#pragma once

#include <mutex>
#include <vector>

#include <godot_cpp/classes/audio_frame.hpp>
#include <godot_cpp/classes/audio_stream_playback_resampled.hpp>

#include "vgm_song.hpp"
#include "vgm_stream.hpp"

using namespace godot;

// Plays a VGMStream. libvgm renders at the mix rate; the Resampled base handles pitch_scale.
// The stream's settings, merged with this playback's player parameters (speed multiplier, extra muted
// channels), are re-applied whenever either changes.
//
// Signals are emitted when audio is mixed, i.e. ahead of what the listener hears by about
// AudioServer.get_output_latency().
class VGMStreamPlayback : public AudioStreamPlaybackResampled {
	GDCLASS(VGMStreamPlayback, AudioStreamPlaybackResampled)

public:
	bool init(const Ref<VGMStream> &p_stream);

	void _start(double p_from_pos) override;
	void _stop() override;
	bool _is_playing() const override;
	int32_t _get_loop_count() const override;
	double _get_playback_position() const override;
	void _seek(double p_position) override;
	int32_t _mix_resampled(AudioFrame *p_dst_buffer, int32_t p_frame_count);
	float _get_stream_sampling_rate() const override;
	void _set_parameter(const StringName &p_name, const Variant &p_value) override;
	Variant _get_parameter(const StringName &p_name) const override;

	double get_cpu_usage() const;
	int64_t get_memory_usage() const;

protected:
	static void _bind_methods();

private:
	// Per-player values set through AudioStreamPlayer's "parameters/*" properties.
	struct PlayerParameters {
		double speed = 1.0; // Multiplies the stream's speed.
		std::vector<uint32_t> muted_channels; // Per device (missing entries = 0), OR'ed with the stream's masks.
	};

	// Library event callback; runs inside Render() with the mutex held.
	static UINT8 on_player_event(PlayerBase *p_player, void *p_user, UINT8 p_type, void *p_param);

	Ref<VGMStream> stream;
	uint32_t sample_rate = 44100;
	bool in_editor = false;
	bool prefilling = false; // Main thread only: begin_resample() is mixing.

	// Guards everything below: _mix_resampled runs on the audio thread, the rest on the main thread.
	mutable std::mutex mutex;
	VGMSong song;
	bool active = false;
	uint64_t applied_version = 0; // Stream settings version.
	uint64_t applied_project_version = 0; // VGMProjectSettings version.
	PlayerParameters parameters;
	uint64_t parameter_version = 0;
	uint64_t applied_parameter_version = 0;
	VGMStream::PlaybackSettings applied; // Stream settings merged with the parameters.
	std::vector<int32_t> render_buffer;
	uint32_t pending_loop = 0; // Loop number reported by the library, not yet signaled.
	bool fade_reported = false;
	uint64_t preview_position = 0; // Frames produced so far by the editor-preview render.
	double cpu_usage = 0.0; // Smoothed emulation time / audio time; see get_cpu_usage().
	int64_t memory_usage = 0; // Heap growth of the last restart(); see get_memory_usage().

	VGMStream::PlaybackSettings merged_settings() const;
	void record_cpu_usage(double p_busy_seconds, int32_t p_frames);
	void apply_settings(const VGMStream::PlaybackSettings &p_settings);
	bool restart();
	void sync_settings();
	void seek_to(double p_position);
	bool is_editor_preview() const;
	int32_t render_frames(AudioFrame *p_dst, int32_t p_frame_count);
	int32_t mix_song(AudioFrame *p_dst, int32_t p_frame_count);
	int32_t mix_preview(AudioFrame *p_dst, int32_t p_frame_count);
	void emit_looped(int p_loop);
	void emit_fade_started();
};
