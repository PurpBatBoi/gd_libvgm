#include "vgm_stream_playback.hpp"

#include <algorithm>
#include <chrono>
#include <iterator>

#include <godot_cpp/classes/audio_server.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>

#include "player/vgmplayer.hpp"
#include "vgm_project_settings.hpp"
#include "vgm_util.hpp"

// The sampled editor-preview render: PREVIEW_SEGMENTS evenly spaced windows of real audio.
static constexpr int PREVIEW_SEGMENTS = 256;
static constexpr double PREVIEW_WINDOW_SECONDS = 0.01;

// Time constant of the get_cpu_usage() moving average.
static constexpr double CPU_USAGE_SMOOTHING_SECONDS = 0.5;

void VGMStreamPlayback::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_cpu_usage"), &VGMStreamPlayback::get_cpu_usage);
	ClassDB::bind_method(D_METHOD("get_memory_usage"), &VGMStreamPlayback::get_memory_usage);

	ADD_SIGNAL(MethodInfo("looped", PropertyInfo(Variant::INT, "loop")));
	ADD_SIGNAL(MethodInfo("fade_started"));
}

// Runs on the audio thread, inside Render() (or Seek()) with the mutex held. Never stops or alters the engine.
UINT8 VGMStreamPlayback::on_player_event(PlayerBase *p_player, void *p_user, UINT8 p_type, void *p_param) {
	// The engine also reports loops it passes while seeking; those are not played.
	if (p_type == PLREVT_LOOP && !(p_player->GetState() & PLAYSTATE_SEEK)) {
		static_cast<VGMStreamPlayback *>(p_user)->pending_loop = *static_cast<UINT32 *>(p_param);
	}
	return 0;
}

void VGMStreamPlayback::emit_looped(int p_loop) {
	emit_signal("looped", p_loop);
}

void VGMStreamPlayback::emit_fade_started() {
	emit_signal("fade_started");
}

bool VGMStreamPlayback::init(const Ref<VGMStream> &p_stream) {
	stream = p_stream;
	sample_rate = (uint32_t)AudioServer::get_singleton()->get_mix_rate();
	in_editor = Engine::get_singleton()->is_editor_hint();
	render_buffer.resize(VGMSong::RENDER_BUFFER_FRAMES * 2);
	song.player.SetEventCallback(on_player_event, this); // The player outlives song reloads.
	return song.load(stream->get_data(), sample_rate);
}

// Caller holds mutex.
VGMStream::PlaybackSettings VGMStreamPlayback::merged_settings() const {
	VGMStream::PlaybackSettings merged = stream->get_playback_settings();
	merged.speed *= parameters.speed;
	for (size_t i = 0; i < merged.devices.size() && i < parameters.muted_channels.size(); i++) {
		merged.devices[i].muted_channels |= parameters.muted_channels[i];
	}
	return merged;
}

// Caller holds mutex.
void VGMStreamPlayback::apply_settings(const VGMStream::PlaybackSettings &p_settings) {
	PlayerA &player = song.player;
	PlayerBase *engine = player.GetPlayer();

	PlayerA::Config config = player.GetConfiguration();
	config.pbSpeed = p_settings.speed;
	config.loopCount = p_settings.loop_count; // 0 = loop forever.
	if (p_settings.loop_count > 0 && engine->GetPlayerType() == FCC_VGM) {
		// VGM headers can scale the loop count for songs with very short or long loops.
		config.loopCount = static_cast<VGMPlayer *>(engine)->GetModifiedLoopCount(p_settings.loop_count);
	}
	config.fadeSmpls = (UINT32)(p_settings.fade_time * sample_rate);
	config.ignoreVolGain = p_settings.ignore_song_volume;
	player.SetConfiguration(config);

	// Main devices carry the core options; a linked device shares its parent's options in slot 1.
	for (const VGMStream::DeviceSettings &main : p_settings.devices) {
		if (main.slot != 0) {
			continue;
		}
		PLR_DEV_OPTS opts;
		if (engine->GetDeviceOptions(main.player_id, opts)) {
			continue;
		}
		opts.resmplMode = (UINT8)p_settings.resampler_mode;
		opts.coreOpts = main.core_options;
		opts.muteOpts.chnMute[0] = main.muted_channels;
		std::copy(std::begin(main.panning), std::end(main.panning), opts.panOpts.chnPan[0]);
		for (const VGMStream::DeviceSettings &linked : p_settings.devices) {
			if (linked.slot == 1 && linked.player_id == main.player_id) {
				opts.muteOpts.chnMute[1] = linked.muted_channels;
				std::copy(std::begin(linked.panning), std::end(linked.panning), opts.panOpts.chnPan[1]);
			}
		}
		engine->SetDeviceOptions(main.player_id, opts);
	}
	applied = p_settings;
}

// Caller holds mutex. Starts from the beginning with the applied settings; libvgm needs a fresh load for that.
bool VGMStreamPlayback::restart() {
	song.unload(); // So the heap growth below is this song alone.
	const size_t heap_before = heap_bytes_in_use();
	if (!song.reload()) {
		return false;
	}
	apply_settings(applied);
	song.player.Start(); // Creates the chip emulators.
	memory_usage = (int64_t)heap_bytes_in_use() - (int64_t)heap_before;
	pending_loop = 0;
	fade_reported = false;
	cpu_usage = 0.0;
	return true;
}

// Caller holds mutex.
void VGMStreamPlayback::sync_settings() {
	const uint64_t version = stream->get_settings_version();
	const uint64_t project_version = VGMProjectSettings::get_version();
	if (version == applied_version && project_version == applied_project_version && parameter_version == applied_parameter_version) {
		return;
	}
	// Read the versions first: a change racing with the snapshot is picked up on the next sync.
	const uint64_t synced_parameter_version = parameter_version;
	const VGMStream::PlaybackSettings settings = merged_settings();

	// The resampler mode is only used when devices are created, i.e. on start.
	const bool needs_restart = settings.resampler_mode != applied.resampler_mode || settings.devices.size() != applied.devices.size();

	if (needs_restart && active) {
		const UINT32 position = song.player.GetCurPos(PLAYPOS_SAMPLE);
		applied = settings;
		if (restart()) {
			song.player.Seek(PLAYPOS_SAMPLE, position);
		} else {
			active = false;
		}
	} else {
		apply_settings(settings);
	}
	applied_version = version;
	applied_project_version = project_version;
	applied_parameter_version = synced_parameter_version;
}

// Caller holds mutex. p_position is in song time (seconds).
void VGMStreamPlayback::seek_to(double p_position) {
	const double speed = song.player.GetPlaybackSpeed();
	song.player.Seek(PLAYPOS_SAMPLE, (UINT32)(MAX(p_position, 0.0) / speed * sample_rate));
}

void VGMStreamPlayback::_start(double p_from_pos) {
	{
		std::lock_guard<std::mutex> lock(mutex);
		applied_version = stream->get_settings_version();
		applied_project_version = VGMProjectSettings::get_version();
		applied_parameter_version = parameter_version;
		applied = merged_settings(); // Includes parameters set before play().
		preview_position = 0;
		active = restart();
		ERR_FAIL_COND_MSG(!active, "VGMStreamPlayback: failed to load song data.");
		if (p_from_pos > 0.0) {
			seek_to(p_from_pos);
		}
	}
	prefilling = true;
	begin_resample(); // Pre-fills by calling _mix_resampled, so the lock must be released.
	prefilling = false;
}

void VGMStreamPlayback::_stop() {
	std::lock_guard<std::mutex> lock(mutex);
	song.player.Stop();
	active = false;
}

bool VGMStreamPlayback::_is_playing() const {
	std::lock_guard<std::mutex> lock(mutex);
	return active;
}

int32_t VGMStreamPlayback::_get_loop_count() const {
	std::lock_guard<std::mutex> lock(mutex);
	return (int32_t)song.player.GetCurLoop();
}

double VGMStreamPlayback::_get_playback_position() const {
	std::lock_guard<std::mutex> lock(mutex);
	return MAX(song.player.GetCurTime(PLAYTIME_LOOP_EXCL | PLAYTIME_TIME_FILE), 0.0);
}

void VGMStreamPlayback::_seek(double p_position) {
	{
		std::lock_guard<std::mutex> lock(mutex);
		if (!active) {
			return;
		}
		seek_to(p_position);
	}
	prefilling = true;
	begin_resample();
	prefilling = false;
}

// The editor's FileSystem thumbnail generator mixes the whole song in a row of calls on the main thread. With renderers that can't
// create resources asynchronously (GL Compatibility), it does so on the main thread, freezing the editor
// for as long as emulating every new song takes. The audio server never mixes on the main thread; only
// begin_resample() does, from _start() and _seek().
bool VGMStreamPlayback::is_editor_preview() const {
	return in_editor && OS::get_singleton()->get_thread_caller_id() == OS::get_singleton()->get_main_thread_id() && !prefilling;
}

// Caller holds mutex. Renders up to p_frame_count frames of the playing song; returns how many.
int32_t VGMStreamPlayback::render_frames(AudioFrame *p_dst, int32_t p_frame_count) {
	PlayerA &player = song.player;
	const float scale = 1.0f / 2147483648.0f; // S32 -> [-1, 1)
	int32_t mixed = 0;
	while (mixed < p_frame_count && !(player.GetState() & PLAYSTATE_FIN)) {
		const UINT32 frames = (UINT32)MIN(p_frame_count - mixed, (int32_t)VGMSong::RENDER_BUFFER_FRAMES);
		const UINT32 rendered = player.Render(frames * 2 * sizeof(int32_t), render_buffer.data()) / (2 * sizeof(int32_t));
		for (UINT32 i = 0; i < rendered; i++) {
			p_dst[mixed + i] = AudioFrame{ render_buffer[i * 2] * scale, render_buffer[i * 2 + 1] * scale };
		}
		mixed += (int32_t)rendered;
		if (rendered == 0) {
			break;
		}
	}
	return mixed;
}

// Caller holds mutex.
void VGMStreamPlayback::record_cpu_usage(double p_busy_seconds, int32_t p_frames) {
	if (p_frames <= 0) {
		return;
	}
	const double audio_seconds = (double)p_frames / sample_rate;
	const double weight = MIN(audio_seconds / CPU_USAGE_SMOOTHING_SECONDS, 1.0);
	cpu_usage += (p_busy_seconds / audio_seconds - cpu_usage) * weight;
}

// Smoothed over about CPU_USAGE_SMOOTHING_SECONDS. A mutex wait is not counted; preemption is.
double VGMStreamPlayback::get_cpu_usage() const {
	std::lock_guard<std::mutex> lock(mutex);
	return active ? cpu_usage : 0.0;
}

int64_t VGMStreamPlayback::get_memory_usage() const {
	std::lock_guard<std::mutex> lock(mutex);
	return active ? MAX(memory_usage, (int64_t)0) : 0;
}

// Caller holds mutex.
int32_t VGMStreamPlayback::mix_song(AudioFrame *p_dst, int32_t p_frame_count) {
	const auto start = std::chrono::steady_clock::now();
	const int32_t mixed = render_frames(p_dst, p_frame_count);
	record_cpu_usage(std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count(), mixed);
	PlayerA &player = song.player;
	// Signals are deferred: this runs on the audio thread.
	if (pending_loop) {
		callable_mp(this, &VGMStreamPlayback::emit_looped).call_deferred((int)pending_loop);
		pending_loop = 0;
	}
	if (!fade_reported && (player.GetState() & PLAYSTATE_FADE)) {
		fade_reported = true;
		callable_mp(this, &VGMStreamPlayback::emit_fade_started).call_deferred();
	}
	if (player.GetState() & PLAYSTATE_FIN) {
		active = false;
	}
	return mixed;
}

// Caller holds mutex. Spreads a short window of real audio over each of PREVIEW_SEGMENTS segments of the song
// and leaves the rest silent, so a whole-song mix emulates ~2.5 s of audio whatever the song's length. A forward
// seek only parses commands, so it is cheap.
int32_t VGMStreamPlayback::mix_preview(AudioFrame *p_dst, int32_t p_frame_count) {
	const uint64_t segment_frames = MAX((uint64_t)(stream->get_length() * sample_rate / PREVIEW_SEGMENTS), (uint64_t)1);
	const uint64_t window_frames = (uint64_t)(PREVIEW_WINDOW_SECONDS * sample_rate);
	int32_t done = 0;
	while (done < p_frame_count) {
		const uint64_t segment = preview_position / segment_frames;
		const uint64_t offset = preview_position % segment_frames;
		const int32_t run = (int32_t)MIN((uint64_t)(p_frame_count - done), segment_frames - offset);
		int32_t real = 0;
		if (segment < PREVIEW_SEGMENTS && offset < window_frames) {
			if (offset == 0) {
				seek_to((double)preview_position / sample_rate);
			}
			real = render_frames(p_dst + done, MIN(run, (int32_t)(window_frames - offset)));
		}
		for (int32_t i = real; i < run; i++) {
			p_dst[done + i] = AudioFrame{ 0.0f, 0.0f };
		}
		done += run;
		preview_position += run;
	}
	pending_loop = 0; // Previews send no signals.
	return done;
}

int32_t VGMStreamPlayback::_mix_resampled(AudioFrame *p_dst_buffer, int32_t p_frame_count) {
	std::lock_guard<std::mutex> lock(mutex);
	int32_t mixed = 0;
	if (active) {
		sync_settings();
	}
	if (active) {
		mixed = is_editor_preview() ? mix_preview(p_dst_buffer, p_frame_count) : mix_song(p_dst_buffer, p_frame_count);
	}
	for (int32_t i = mixed; i < p_frame_count; i++) {
		p_dst_buffer[i] = AudioFrame{ 0.0f, 0.0f };
	}
	return mixed;
}

void VGMStreamPlayback::_set_parameter(const StringName &p_name, const Variant &p_value) {
	int device;
	String field;
	std::lock_guard<std::mutex> lock(mutex);
	if (p_name == StringName("speed")) {
		parameters.speed = MAX((double)p_value, 0.01);
	} else if (stream->parse_chip_property(p_name, device, field) && field == "muted_channels") {
		if (parameters.muted_channels.size() <= (size_t)device) {
			parameters.muted_channels.resize(device + 1, 0);
		}
		parameters.muted_channels[device] = (uint32_t)(int64_t)p_value;
	} else {
		return;
	}
	parameter_version++;
}

Variant VGMStreamPlayback::_get_parameter(const StringName &p_name) const {
	int device;
	String field;
	std::lock_guard<std::mutex> lock(mutex);
	if (p_name == StringName("speed")) {
		return parameters.speed;
	}
	if (stream->parse_chip_property(p_name, device, field) && field == "muted_channels") {
		return (size_t)device < parameters.muted_channels.size() ? parameters.muted_channels[device] : 0u;
	}
	return Variant();
}

float VGMStreamPlayback::_get_stream_sampling_rate() const {
	return sample_rate;
}
