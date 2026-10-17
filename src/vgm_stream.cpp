#include "vgm_stream.hpp"

#include <godot_cpp/core/class_db.hpp>

#include "emu/SoundDevs.h"
#include "emu/SoundEmu.h"
#include "emu/cores/c352.h"
#include "emu/cores/gbintf.h"
#include "emu/cores/iremga20.h"
#include "emu/cores/nesintf.h"
#include "emu/cores/okim6258.h"
#include "emu/cores/scsp.h"
#include "vgm_project_settings.hpp"
#include "vgm_song.hpp"
#include "vgm_stream_playback.hpp"
#include "vgm_util.hpp"

// Sample rate used only to inspect a song; playback uses the mix rate.
static const uint32_t PROBE_SAMPLE_RATE = 44100;

// Named core option bits per chip type, shown as checkboxes in the inspector.
struct CoreOption {
	const char *name;
	uint32_t bit;
};
struct ChipCoreOptions {
	uint8_t type;
	std::vector<CoreOption> options;
};
static const std::vector<ChipCoreOptions> CHIP_CORE_OPTIONS = {
	{ DEVID_C352, { { "Mute Rear Speakers", OPT_C352_MUTE_REAR } } },
	{ DEVID_GB_DMG, {
							{ "No Wave RAM Corruption", OPT_GB_DMG_NO_WAVE_CORRUPT },
							{ "Legacy Mode", OPT_GB_DMG_LEGACY_MODE },
					} },
	{ DEVID_GA20, { { "Interpolate Samples", OPT_GA20_INTERPOLATE } } },
	{ DEVID_NES_APU, {
							 { "Unmute On Reset", OPT_NES_UNMUTE_ON_RESET },
							 { "Nonlinear Mixer", OPT_NES_NONLINEAR_MIXER },
							 { "Duty Swap", OPT_NES_DUTY_SWAP },
							 { "Enable $4011 Writes", OPT_NES_ENABLE_4011 },
							 { "Loop Noise", OPT_NES_ENABLE_PNOISE },
							 { "$4085 Reset", OPT_NES_4085_RESET },
							 { "FDS Write Protect", OPT_NES_FDS_DISABLE },
					 } },
	{ DEVID_MSM6258, { { "Force 12-Bit Output", OPT_MSM6258_FORCE_12BIT } } },
	{ DEVID_SCSP, { { "Bypass DSP", OPT_SCSP_BYPASS_DSP } } },
};

static const std::vector<CoreOption> *find_core_options(uint8_t p_type) {
	for (const ChipCoreOptions &chip : CHIP_CORE_OPTIONS) {
		if (chip.type == p_type) {
			return &chip.options;
		}
	}
	return nullptr;
}

// Chips of the bundled libvgm fork that have no permissively licensed core, so the fork leaves them out.
// libvgm can't name a chip it has no declaration for.
static const char *unsupported_chip_name(DEV_ID p_type) {
	switch (p_type) {
		case DEVID_YMF278B: return "YMF278B";
		case DEVID_32X_PWM: return "32X PWM";
		case DEVID_WSWAN: return "WonderSwan";
		case DEVID_VBOY_VSU: return "Virtual Boy VSU";
		case DEVID_ES5506: return "ES5506";
		case DEVID_MSM5232: return "MSM5232";
		default: return nullptr;
	}
}

// Comma-separated names of the song's chips this build can't emulate; empty if all are supported.
static String find_unsupported_chips(PlayerBase *p_engine) {
	std::vector<PLR_DEV_INFO> infos;
	p_engine->GetSongDeviceInfo(infos);
	PackedStringArray names;
	for (const PLR_DEV_INFO &info : infos) {
		if (info.parentIdx != (UINT32)-1 || info.devDecl) {
			continue;
		}
		const char *name = unsupported_chip_name(info.type);
		const String chip = name ? String(name) : vformat("chip type 0x%02X", info.type);
		if (!names.has(chip)) {
			names.push_back(chip);
		}
	}
	return String(", ").join(names);
}

// Inspector hint strings use ',' and ':' as separators.
static String hint_safe(const String &p_text) {
	return p_text.replace(",", " ").replace(":", " ");
}

// One flag per channel, for the mute mask properties and parameters.
static String channel_flags_hint(const VGMStream::Device &p_device) {
	PackedStringArray hint;
	for (const String &channel : p_device.channel_names) {
		hint.push_back(hint_safe(channel));
	}
	return String(",").join(hint);
}

// A four-character code like FCC_VGM ("VGM\0") as text.
static String fcc_to_string(uint32_t p_fcc) {
	String result;
	for (int shift = 24; shift >= 0; shift -= 8) {
		const char32_t c = (p_fcc >> shift) & 0xFF;
		if (c) {
			result += c;
		}
	}
	return result;
}

void VGMStream::set_data(const PackedByteArray &p_data) {
	data = PackedByteArray();
	devices.clear();
	tags.clear();
	length = 0.0;
	has_loop = false;
	format = String();
	format_version = String();
	loop_start = 0.0;
	loop_length = 0.0;
	std::vector<DeviceSettings> device_settings;

	VGMSong song;
	if (!p_data.is_empty()) {
		if (song.load(p_data, PROBE_SAMPLE_RATE)) {
			const String unsupported = find_unsupported_chips(song.player.GetPlayer());
			if (unsupported.is_empty()) {
				data = p_data;
			} else {
				ERR_PRINT("VGMStream: the song uses chips with no permissively licensed core, which this build leaves out: " + unsupported + ".");
			}
		} else {
			ERR_PRINT("VGMStream: data is not a file format supported by libvgm (VGM/VGZ, S98, DRO, GYM).");
		}
	}

	if (!data.is_empty()) {
		PlayerA &player = song.player;
		PlayerBase *engine = player.GetPlayer();

		PLR_SONG_INFO song_info;
		if (!engine->GetSongInfo(song_info)) {
			has_loop = song_info.loopTick != (UINT32)-1;
			format = fcc_to_string(song_info.format);
			format_version = vformat("%X.%02X", song_info.fileVerMaj, song_info.fileVerMin);
		}
		for (const char *const *tag = engine->GetTags(); tag && *tag; tag += 2) {
			if (tag[1] && tag[1][0]) {
				tags[String::utf8(tag[0])] = String::utf8(tag[1]);
			}
		}
		length = MAX(player.GetTotalTime(PLAYTIME_LOOP_EXCL), 0.0);
		if (has_loop) {
			// PLR_SONG_INFO::loopTick is the loop's length, not its start. The probe plays at speed 1, so these are file times.
			loop_length = engine->Tick2Second(engine->GetLoopTicks());
			loop_start = engine->Tick2Second(engine->GetTotalTicks() - engine->GetLoopTicks());
		}

		// Started devices report their actual core and linked-device configs.
		player.Start();
		std::vector<PLR_DEV_INFO> infos;
		engine->GetSongDeviceInfo(infos);
		for (const PLR_DEV_INFO &info : infos) {
			const bool linked = info.parentIdx != (UINT32)-1;
			// libvgm only addresses the first linked device of a chip (option slot 1).
			if (!info.devDecl || !info.devCfg || (linked && info.instance > 0)) {
				continue;
			}
			const DEV_DECL *decl = info.devDecl;
			// Address options by chip type + instance: runtime indices only resolve while devices are running,
			// but options must be set before Start(). A linked device is addressed through its parent.
			const PLR_DEV_INFO &owner = linked ? infos[info.parentIdx] : info;

			Device device;
			device.linked = linked;
			device.name = String::utf8(decl->name(info.devCfg));
			if (linked) {
				// No '/': the inspector would split the group name into nested groups. The only linked devices
				// are the OPN chips' SSG parts (AY8910 core).
				device.name = String::utf8(owner.devDecl->name(owner.devCfg)) + " SSG (" + device.name + ")";
			}
			if (owner.instance > 0 && owner.instance != 0xFFFF) {
				device.name += " #" + itos(owner.instance + 1);
			}
			device.clock = owner.devCfg ? owner.devCfg->clock : 0;
			const UINT16 channel_count = MIN(decl->channelCount(info.devCfg), (UINT16)MAX_CHANNELS);
			const char **names = decl->channelNames ? decl->channelNames(info.devCfg) : nullptr;
			for (UINT16 ch = 0; ch < channel_count; ch++) {
				const String channel_name = names && names[ch] ? String::utf8(names[ch]) : "Channel " + itos(ch + 1);
				device.channel_names.push_back(channel_name);
				String pan_name = to_identifier(channel_name);
				if (device.pan_property_names.has(pan_name)) {
					pan_name += "_" + itos(ch + 1);
				}
				device.pan_property_names.push_back(pan_name);
			}
			for (const DEV_DEF *const *core = decl->cores; *core; core++) {
				if ((*core)->coreID == info.core) {
					device.core = *core;
				}
			}

			DeviceSettings ds;
			ds.player_id = PLR_DEV_ID(owner.type, (owner.instance == 0xFFFF ? 0u : (UINT32)owner.instance));
			ds.slot = linked ? 1 : 0;
			ds.type = info.type;
			PLR_DEV_OPTS opts;
			if (!linked && !engine->GetDeviceOptions(ds.player_id, opts)) {
				device.default_core_options = opts.coreOpts;
				ds.core_options = opts.coreOpts;
			}
			devices.push_back(device);
			device_settings.push_back(ds);
		}
	}

	modify_settings([&](Settings &s) { s.devices = device_settings; });
	notify_property_list_changed();
	emit_signal("parameter_list_changed");
}

PackedByteArray VGMStream::get_data() const {
	return data;
}

template <typename F>
void VGMStream::modify_settings(F p_modify) {
	{
		std::lock_guard<std::mutex> lock(settings_mutex);
		p_modify(settings);
	}
	settings_version++;
	emit_changed();
}

VGMStream::PlaybackSettings VGMStream::get_playback_settings() const {
	Settings stored;
	{
		std::lock_guard<std::mutex> lock(settings_mutex);
		stored = settings;
	}
	const VGMProjectSettings::Defaults defaults = VGMProjectSettings::get();
	PlaybackSettings resolved;
	resolved.speed = stored.speed;
	resolved.loop_count = stored.loop_count.value_or(defaults.loop_count);
	resolved.fade_time = stored.fade_time.value_or(defaults.fade_time);
	resolved.ignore_song_volume = stored.ignore_song_volume.value_or(defaults.ignore_song_volume);
	resolved.resampler_mode = stored.resampler_mode.value_or((ResamplerMode)defaults.resampler_mode);
	resolved.devices = stored.devices;
	return resolved;
}

// Stores p_value as an override, or follows the project setting when it equals p_default
// (this is also how the inspector's revert button clears an override).
template <typename T>
static void set_override(std::optional<T> &r_setting, const T &p_value, const T &p_default) {
	if (p_value == p_default) {
		r_setting.reset();
	} else {
		r_setting = p_value;
	}
}

void VGMStream::set_speed(double p_speed) {
	ERR_FAIL_COND_MSG(p_speed <= 0.0, "Speed must be greater than 0.");
	modify_settings([&](Settings &s) { s.speed = p_speed; });
}

double VGMStream::get_speed() const {
	return settings.speed;
}

void VGMStream::set_loop_count(int p_count) {
	ERR_FAIL_COND_MSG(p_count < 0, "Loop count must be 0 (loop forever) or more.");
	modify_settings([&](Settings &s) { set_override(s.loop_count, p_count, VGMProjectSettings::get().loop_count); });
}

int VGMStream::get_loop_count() const {
	return settings.loop_count.value_or(VGMProjectSettings::get().loop_count);
}

void VGMStream::set_fade_time(double p_seconds) {
	ERR_FAIL_COND_MSG(p_seconds < 0.0, "Fade time can't be negative.");
	modify_settings([&](Settings &s) { set_override(s.fade_time, p_seconds, VGMProjectSettings::get().fade_time); });
}

double VGMStream::get_fade_time() const {
	return settings.fade_time.value_or(VGMProjectSettings::get().fade_time);
}

void VGMStream::set_ignore_song_volume(bool p_ignore) {
	modify_settings([&](Settings &s) { set_override(s.ignore_song_volume, p_ignore, VGMProjectSettings::get().ignore_song_volume); });
}

bool VGMStream::get_ignore_song_volume() const {
	return settings.ignore_song_volume.value_or(VGMProjectSettings::get().ignore_song_volume);
}

void VGMStream::set_resampler_mode(ResamplerMode p_mode) {
	ERR_FAIL_INDEX((int)p_mode, 3);
	modify_settings([&](Settings &s) { set_override(s.resampler_mode, p_mode, (ResamplerMode)VGMProjectSettings::get().resampler_mode); });
}

VGMStream::ResamplerMode VGMStream::get_resampler_mode() const {
	return settings.resampler_mode.value_or((ResamplerMode)VGMProjectSettings::get().resampler_mode);
}

void VGMStream::set_overrides(const Dictionary &p_overrides) {
	modify_settings([&](Settings &s) {
		s.loop_count.reset();
		s.fade_time.reset();
		s.ignore_song_volume.reset();
		s.resampler_mode.reset();
		if (p_overrides.has("loop_count")) {
			s.loop_count = MAX((int)p_overrides["loop_count"], 0);
		}
		if (p_overrides.has("fade_time")) {
			s.fade_time = MAX((double)p_overrides["fade_time"], 0.0);
		}
		if (p_overrides.has("ignore_song_volume")) {
			s.ignore_song_volume = (bool)p_overrides["ignore_song_volume"];
		}
		if (p_overrides.has("resampler_mode")) {
			s.resampler_mode = (ResamplerMode)CLAMP((int)p_overrides["resampler_mode"], 0, 2);
		}
	});
}

Dictionary VGMStream::get_overrides() const {
	Dictionary result;
	if (settings.loop_count) {
		result["loop_count"] = *settings.loop_count;
	}
	if (settings.fade_time) {
		result["fade_time"] = *settings.fade_time;
	}
	if (settings.ignore_song_volume) {
		result["ignore_song_volume"] = *settings.ignore_song_volume;
	}
	if (settings.resampler_mode) {
		result["resampler_mode"] = (int)*settings.resampler_mode;
	}
	return result;
}

bool VGMStream::is_valid_device(int p_device) const {
	return p_device >= 0 && p_device < (int)devices.size();
}

String VGMStream::get_format() const {
	return format;
}

String VGMStream::get_format_version() const {
	return format_version;
}

double VGMStream::get_loop_start() const {
	return loop_start;
}

double VGMStream::get_loop_length() const {
	return loop_length;
}

int VGMStream::get_device_clock(int p_device) const {
	ERR_FAIL_COND_V(!is_valid_device(p_device), 0);
	return (int)devices[p_device].clock;
}

int VGMStream::get_device_count() const {
	return (int)devices.size();
}

String VGMStream::get_device_name(int p_device) const {
	ERR_FAIL_COND_V(!is_valid_device(p_device), String());
	return devices[p_device].name;
}

PackedStringArray VGMStream::get_channel_names(int p_device) const {
	ERR_FAIL_COND_V(!is_valid_device(p_device), PackedStringArray());
	return devices[p_device].channel_names;
}

void VGMStream::set_channel_muted(int p_device, int p_channel, bool p_muted) {
	ERR_FAIL_COND(!is_valid_device(p_device));
	ERR_FAIL_INDEX(p_channel, devices[p_device].channel_names.size());
	modify_settings([&](Settings &s) {
		uint32_t &mask = s.devices[p_device].muted_channels;
		mask = p_muted ? (mask | (1u << p_channel)) : (mask & ~(1u << p_channel));
	});
}

bool VGMStream::is_channel_muted(int p_device, int p_channel) const {
	ERR_FAIL_COND_V(!is_valid_device(p_device), false);
	ERR_FAIL_INDEX_V(p_channel, devices[p_device].channel_names.size(), false);
	return settings.devices[p_device].muted_channels & (1u << p_channel);
}

void VGMStream::set_device_muted(int p_device, bool p_muted) {
	ERR_FAIL_COND(!is_valid_device(p_device));
	const int64_t channel_count = devices[p_device].channel_names.size();
	modify_settings([&](Settings &s) { s.devices[p_device].muted_channels = p_muted ? (uint32_t)((1ull << channel_count) - 1) : 0; });
}

void VGMStream::set_channel_pan(int p_device, int p_channel, double p_pan) {
	ERR_FAIL_COND(!is_valid_device(p_device));
	ERR_FAIL_INDEX(p_channel, devices[p_device].channel_names.size());
	modify_settings([&](Settings &s) { s.devices[p_device].panning[p_channel] = (int16_t)Math::round(CLAMP(p_pan, -1.0, 1.0) * PAN_RANGE); });
}

double VGMStream::get_channel_pan(int p_device, int p_channel) const {
	ERR_FAIL_COND_V(!is_valid_device(p_device), 0.0);
	ERR_FAIL_INDEX_V(p_channel, devices[p_device].channel_names.size(), 0.0);
	return (double)settings.devices[p_device].panning[p_channel] / PAN_RANGE;
}

bool VGMStream::has_channel_panning(int p_device) const {
	ERR_FAIL_COND_V(!is_valid_device(p_device), false);
	const DEV_DEF *core = devices[p_device].core;
	void *pan_func = nullptr;
	return core && SndEmu_GetDeviceFunc(core, RWF_CHN_PAN | RWF_WRITE, DEVRW_ALL, 0, &pan_func) == EERR_OK;
}

void VGMStream::set_device_core_options(int p_device, int64_t p_options) {
	ERR_FAIL_COND(!is_valid_device(p_device));
	ERR_FAIL_COND_MSG(devices[p_device].linked, "Linked devices share their parent device's core options.");
	modify_settings([&](Settings &s) { s.devices[p_device].core_options = (uint32_t)p_options; });
}

int64_t VGMStream::get_device_core_options(int p_device) const {
	ERR_FAIL_COND_V(!is_valid_device(p_device), 0);
	return settings.devices[p_device].core_options;
}

void VGMStream::set_device_settings(const Array &p_settings) {
	// Saved settings only apply to the song they were saved with.
	if (p_settings.size() != (int64_t)devices.size()) {
		return;
	}
	modify_settings([&](Settings &s) {
		for (int64_t i = 0; i < p_settings.size(); i++) {
			const Dictionary entry = p_settings[i];
			DeviceSettings &ds = s.devices[i];
			ds.muted_channels = (uint32_t)(int64_t)entry.get("muted_channels", 0);
			if (!devices[i].linked) {
				ds.core_options = (uint32_t)(int64_t)entry.get("core_options", devices[i].default_core_options);
			}
			const PackedInt32Array panning = entry.get("panning", PackedInt32Array());
			for (int64_t ch = 0; ch < MIN(panning.size(), (int64_t)MAX_CHANNELS); ch++) {
				ds.panning[ch] = (int16_t)CLAMP(panning[ch], -PAN_RANGE, PAN_RANGE);
			}
		}
	});
	notify_property_list_changed();
}

Array VGMStream::get_device_settings() const {
	Array result;
	for (size_t i = 0; i < settings.devices.size(); i++) {
		const DeviceSettings &ds = settings.devices[i];
		Dictionary entry;
		entry["core_options"] = ds.core_options;
		entry["muted_channels"] = ds.muted_channels;
		PackedInt32Array panning;
		for (int64_t ch = 0; ch < devices[i].channel_names.size(); ch++) {
			panning.push_back(ds.panning[ch]);
		}
		entry["panning"] = panning;
		result.push_back(entry);
	}
	return result;
}

bool VGMStream::parse_chip_property(const StringName &p_name, int &r_device, String &r_field) const {
	const String name = p_name;
	if (!name.begins_with("chip_")) {
		return false;
	}
	const int64_t separator = name.find("_", 5);
	if (separator < 0) {
		return false;
	}
	const String index = name.substr(5, separator - 5);
	if (!index.is_valid_int()) {
		return false;
	}
	r_device = (int)index.to_int();
	r_field = name.substr(separator + 1);
	return is_valid_device(r_device);
}

void VGMStream::_get_property_list(List<PropertyInfo> *p_list) const {
	for (int i = 0; i < (int)devices.size(); i++) {
		const Device &device = devices[i];
		const String prefix = vformat("chip_%d_", i);
		p_list->push_back(PropertyInfo(Variant::NIL, device.name, PROPERTY_HINT_NONE, prefix, PROPERTY_USAGE_GROUP));

		p_list->push_back(PropertyInfo(Variant::INT, prefix + String("muted_channels"), PROPERTY_HINT_FLAGS, channel_flags_hint(device), PROPERTY_USAGE_EDITOR));

		const std::vector<CoreOption> *options = device.linked ? nullptr : find_core_options(settings.devices[i].type);
		if (options) {
			PackedStringArray option_hint;
			for (const CoreOption &option : *options) {
				option_hint.push_back(vformat("%s:%d", option.name, option.bit));
			}
			p_list->push_back(PropertyInfo(Variant::INT, prefix + String("options"), PROPERTY_HINT_FLAGS, String(",").join(option_hint), PROPERTY_USAGE_EDITOR));
		}

		if (has_channel_panning(i)) {
			p_list->push_back(PropertyInfo(Variant::NIL, "Panning", PROPERTY_HINT_NONE, prefix + String("pan_"), PROPERTY_USAGE_SUBGROUP));
			for (const String &pan_name : device.pan_property_names) {
				p_list->push_back(PropertyInfo(Variant::FLOAT, prefix + String("pan_") + pan_name, PROPERTY_HINT_RANGE, "-1,1,0.01", PROPERTY_USAGE_EDITOR));
			}
		}
	}
}

bool VGMStream::_set(const StringName &p_name, const Variant &p_value) {
	int device;
	String field;
	if (!parse_chip_property(p_name, device, field)) {
		return false;
	}
	if (field == "muted_channels") {
		const uint32_t mask = (uint32_t)(int64_t)p_value;
		modify_settings([&](Settings &s) { s.devices[device].muted_channels = mask; });
	} else if (field == "options") {
		set_device_core_options(device, p_value);
	} else if (field.begins_with("pan_")) {
		const int64_t channel = devices[device].pan_property_names.find(field.substr(4));
		if (channel < 0) {
			return false;
		}
		set_channel_pan(device, (int)channel, p_value);
	} else {
		return false;
	}
	return true;
}

bool VGMStream::_get(const StringName &p_name, Variant &r_ret) const {
	int device;
	String field;
	if (!parse_chip_property(p_name, device, field)) {
		return false;
	}
	if (field == "muted_channels") {
		r_ret = settings.devices[device].muted_channels;
	} else if (field == "options") {
		r_ret = settings.devices[device].core_options;
	} else if (field.begins_with("pan_")) {
		const int64_t channel = devices[device].pan_property_names.find(field.substr(4));
		if (channel < 0) {
			return false;
		}
		r_ret = get_channel_pan(device, (int)channel);
	} else {
		return false;
	}
	return true;
}

// Settings that follow Project Settings revert to the project value.
static bool get_project_default(const StringName &p_name, Variant &r_value) {
	const VGMProjectSettings::Defaults defaults = VGMProjectSettings::get();
	if (p_name == StringName("loop_count")) {
		r_value = defaults.loop_count;
	} else if (p_name == StringName("fade_time")) {
		r_value = defaults.fade_time;
	} else if (p_name == StringName("ignore_song_volume")) {
		r_value = defaults.ignore_song_volume;
	} else if (p_name == StringName("resampler_mode")) {
		r_value = defaults.resampler_mode;
	} else {
		return false;
	}
	return true;
}

bool VGMStream::_property_can_revert(const StringName &p_name) const {
	int device;
	String field;
	Variant unused;
	return get_project_default(p_name, unused) || parse_chip_property(p_name, device, field);
}

bool VGMStream::_property_get_revert(const StringName &p_name, Variant &r_property) const {
	if (get_project_default(p_name, r_property)) {
		return true;
	}
	int device;
	String field;
	if (!parse_chip_property(p_name, device, field)) {
		return false;
	}
	if (field == "options") {
		r_property = devices[device].default_core_options;
	} else if (field.begins_with("pan_")) {
		r_property = 0.0;
	} else if (field == "muted_channels") {
		r_property = 0;
	} else {
		return false;
	}
	return true;
}

TypedArray<Dictionary> VGMStream::_get_parameter_list() const {
	TypedArray<Dictionary> result;
	Dictionary speed = PropertyInfo(Variant::FLOAT, "speed", PROPERTY_HINT_RANGE, "0.1,4.0,0.01,or_greater");
	speed["default_value"] = 1.0;
	result.push_back(speed);
	for (int i = 0; i < (int)devices.size(); i++) {
		Dictionary muted = PropertyInfo(Variant::INT, vformat("chip_%d_muted_channels", i), PROPERTY_HINT_FLAGS, channel_flags_hint(devices[i]));
		muted["default_value"] = 0;
		result.push_back(muted);
	}
	return result;
}

Ref<AudioStreamPlayback> VGMStream::_instantiate_playback() const {
	ERR_FAIL_COND_V_MSG(data.is_empty(), Ref<AudioStreamPlayback>(), "VGMStream has no song data.");
	Ref<VGMStreamPlayback> playback;
	playback.instantiate();
	ERR_FAIL_COND_V(!playback->init(Ref<VGMStream>(const_cast<VGMStream *>(this))), Ref<AudioStreamPlayback>());
	return playback;
}

String VGMStream::_get_stream_name() const {
	return tags.get("TITLE", get_name());
}

double VGMStream::_get_length() const {
	return length;
}

bool VGMStream::_has_loop() const {
	return has_loop;
}

Dictionary VGMStream::_get_tags() const {
	return tags;
}

void VGMStream::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_data", "data"), &VGMStream::set_data);
	ClassDB::bind_method(D_METHOD("get_data"), &VGMStream::get_data);
	ClassDB::bind_method(D_METHOD("set_speed", "speed"), &VGMStream::set_speed);
	ClassDB::bind_method(D_METHOD("get_speed"), &VGMStream::get_speed);
	ClassDB::bind_method(D_METHOD("set_loop_count", "count"), &VGMStream::set_loop_count);
	ClassDB::bind_method(D_METHOD("get_loop_count"), &VGMStream::get_loop_count);
	ClassDB::bind_method(D_METHOD("set_fade_time", "seconds"), &VGMStream::set_fade_time);
	ClassDB::bind_method(D_METHOD("get_fade_time"), &VGMStream::get_fade_time);
	ClassDB::bind_method(D_METHOD("set_ignore_song_volume", "ignore"), &VGMStream::set_ignore_song_volume);
	ClassDB::bind_method(D_METHOD("get_ignore_song_volume"), &VGMStream::get_ignore_song_volume);
	ClassDB::bind_method(D_METHOD("set_resampler_mode", "mode"), &VGMStream::set_resampler_mode);
	ClassDB::bind_method(D_METHOD("get_resampler_mode"), &VGMStream::get_resampler_mode);
	ClassDB::bind_method(D_METHOD("set_device_settings", "settings"), &VGMStream::set_device_settings);
	ClassDB::bind_method(D_METHOD("get_device_settings"), &VGMStream::get_device_settings);
	ClassDB::bind_method(D_METHOD("set_overrides", "overrides"), &VGMStream::set_overrides);
	ClassDB::bind_method(D_METHOD("get_overrides"), &VGMStream::get_overrides);

	ClassDB::bind_method(D_METHOD("get_format"), &VGMStream::get_format);
	ClassDB::bind_method(D_METHOD("get_format_version"), &VGMStream::get_format_version);
	ClassDB::bind_method(D_METHOD("get_loop_start"), &VGMStream::get_loop_start);
	ClassDB::bind_method(D_METHOD("get_loop_length"), &VGMStream::get_loop_length);
	ClassDB::bind_method(D_METHOD("get_device_clock", "device"), &VGMStream::get_device_clock);
	ClassDB::bind_method(D_METHOD("get_song_tags"), &VGMStream::_get_tags);
	ClassDB::bind_method(D_METHOD("get_device_count"), &VGMStream::get_device_count);
	ClassDB::bind_method(D_METHOD("get_device_name", "device"), &VGMStream::get_device_name);
	ClassDB::bind_method(D_METHOD("get_channel_names", "device"), &VGMStream::get_channel_names);
	ClassDB::bind_method(D_METHOD("set_channel_muted", "device", "channel", "muted"), &VGMStream::set_channel_muted);
	ClassDB::bind_method(D_METHOD("is_channel_muted", "device", "channel"), &VGMStream::is_channel_muted);
	ClassDB::bind_method(D_METHOD("set_device_muted", "device", "muted"), &VGMStream::set_device_muted);
	ClassDB::bind_method(D_METHOD("set_channel_pan", "device", "channel", "pan"), &VGMStream::set_channel_pan);
	ClassDB::bind_method(D_METHOD("get_channel_pan", "device", "channel"), &VGMStream::get_channel_pan);
	ClassDB::bind_method(D_METHOD("has_channel_panning", "device"), &VGMStream::has_channel_panning);
	ClassDB::bind_method(D_METHOD("set_device_core_options", "device", "options"), &VGMStream::set_device_core_options);
	ClassDB::bind_method(D_METHOD("get_device_core_options", "device"), &VGMStream::get_device_core_options);

	// data first: device_settings is only accepted once the song's devices are known.
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_BYTE_ARRAY, "data", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR), "set_data", "get_data");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "speed", PROPERTY_HINT_RANGE, "0.1,4.0,0.01,or_greater"), "set_speed", "get_speed");
	// Shown with their effective value; saved through `overrides` so un-overridden ones keep following Project Settings.
	ADD_PROPERTY(PropertyInfo(Variant::INT, "loop_count", PROPERTY_HINT_RANGE, "0,100,1,or_greater", PROPERTY_USAGE_EDITOR), "set_loop_count", "get_loop_count");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "fade_time", PROPERTY_HINT_RANGE, "0,30,0.1,or_greater,suffix:s", PROPERTY_USAGE_EDITOR), "set_fade_time", "get_fade_time");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "ignore_song_volume", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_EDITOR), "set_ignore_song_volume", "get_ignore_song_volume");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "resampler_mode", PROPERTY_HINT_ENUM, "High Quality,Low Quality,LQ Down HQ Up", PROPERTY_USAGE_EDITOR), "set_resampler_mode", "get_resampler_mode");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "overrides", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR), "set_overrides", "get_overrides");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "device_settings", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR), "set_device_settings", "get_device_settings");

	// Read-only song info, shown (never saved) in its own inspector group.
	const uint32_t info_usage = PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_READ_ONLY;
	ADD_GROUP("Song Info", "");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "format", PROPERTY_HINT_NONE, "", info_usage), "", "get_format");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "format_version", PROPERTY_HINT_NONE, "", info_usage), "", "get_format_version");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "loop_start", PROPERTY_HINT_NONE, "suffix:s", info_usage), "", "get_loop_start");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "loop_length", PROPERTY_HINT_NONE, "suffix:s", info_usage), "", "get_loop_length");

	BIND_ENUM_CONSTANT(RESAMPLER_HIGH_QUALITY);
	BIND_ENUM_CONSTANT(RESAMPLER_LOW_QUALITY);
	BIND_ENUM_CONSTANT(RESAMPLER_LQ_DOWN_HQ_UP);

	// Filter-related core option bits (see set_device_core_options).
	ClassDB::bind_integer_constant(get_class_static(), "", "CORE_OPTION_NES_NONLINEAR_MIXER", OPT_NES_NONLINEAR_MIXER);
	ClassDB::bind_integer_constant(get_class_static(), "", "CORE_OPTION_GA20_INTERPOLATE", OPT_GA20_INTERPOLATE);
}
