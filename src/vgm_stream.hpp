#pragma once

#include <atomic>
#include <mutex>
#include <optional>
#include <vector>

#include <godot_cpp/classes/audio_stream.hpp>
#include <godot_cpp/classes/audio_stream_playback.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>

#include "emu/EmuStructs.h"

using namespace godot;

// A song in any format libvgm plays (VGM/VGZ, S98, DRO, GYM, SPC), plus the playback settings that every
// VGMStreamPlayback of it applies live: speed, looping, volume, resampler, and per sound-chip settings.
// Per-chip settings are also exposed to the inspector as dynamic "chip_<index>_*" properties.
// Song info (format, loop points, chip clocks) is read-only. Per-player speed and muted channels are Godot
// stream parameters ("parameters/speed", "parameters/chip_<index>_muted_channels"), applied by VGMStreamPlayback.
// Looping, volume and resampler follow Project Settings (VGMProjectSettings) unless overridden.
// Each chip has exactly one emulation core in the bundled libvgm fork, so cores are not selectable.
class VGMStream : public AudioStream {
	GDCLASS(VGMStream, AudioStream)

public:
	enum ResamplerMode {
		RESAMPLER_HIGH_QUALITY = 0,
		RESAMPLER_LOW_QUALITY = 1,
		RESAMPLER_LQ_DOWN_HQ_UP = 2,
	};

	static constexpr int MAX_CHANNELS = 32; // libvgm mute masks and pan tables.
	static constexpr int PAN_RANGE = 256; // libvgm pan values: -256 (left) .. 256 (right).

	// One sound chip of the song. Linked devices (e.g. the SSG part of a YM2608) are listed as their own
	// entries; libvgm addresses them through their parent's player ID in slot 1.
	struct Device {
		String name;
		PackedStringArray channel_names;
		PackedStringArray pan_property_names; // Per channel, unique within the device.
		const DEV_DEF *core = nullptr; // The emulation core libvgm runs for this chip.
		uint32_t default_core_options = 0;
		bool linked = false;
		uint32_t clock = 0; // Hz; a linked device reports its parent's clock.
	};

	struct DeviceSettings {
		uint32_t player_id = 0;
		int slot = 0; // 0 = main device, 1 = linked device.
		uint8_t type = 0; // libvgm DEV_ID.
		uint32_t core_options = 0; // Only used for main devices (slot 0).
		uint32_t muted_channels = 0; // Bit mask.
		int16_t panning[MAX_CHANNELS] = {};
	};

	// Stored settings; an empty optional follows the project setting.
	struct Settings {
		double speed = 1.0;
		std::optional<int> loop_count; // 0 = loop forever.
		std::optional<double> fade_time;
		std::optional<bool> ignore_song_volume;
		std::optional<ResamplerMode> resampler_mode;
		std::vector<DeviceSettings> devices; // Parallel to the device list.
	};

	// Settings with project defaults resolved, as applied by playbacks.
	struct PlaybackSettings {
		double speed = 1.0;
		int loop_count = 0;
		double fade_time = 0.0;
		bool ignore_song_volume = false;
		ResamplerMode resampler_mode = RESAMPLER_HIGH_QUALITY;
		std::vector<DeviceSettings> devices;
	};

	void set_data(const PackedByteArray &p_data);
	PackedByteArray get_data() const;

	void set_speed(double p_speed);
	double get_speed() const;
	void set_loop_count(int p_count);
	int get_loop_count() const;
	void set_fade_time(double p_seconds);
	double get_fade_time() const;
	void set_ignore_song_volume(bool p_ignore);
	bool get_ignore_song_volume() const;
	void set_resampler_mode(ResamplerMode p_mode);
	ResamplerMode get_resampler_mode() const;

	String get_format() const;
	String get_format_version() const;
	double get_loop_start() const;
	double get_loop_length() const;
	int get_device_clock(int p_device) const;

	int get_device_count() const;
	String get_device_name(int p_device) const;
	PackedStringArray get_channel_names(int p_device) const;
	void set_channel_muted(int p_device, int p_channel, bool p_muted);
	bool is_channel_muted(int p_device, int p_channel) const;
	void set_device_muted(int p_device, bool p_muted);
	void set_channel_pan(int p_device, int p_channel, double p_pan);
	double get_channel_pan(int p_device, int p_channel) const;
	bool has_channel_panning(int p_device) const;
	void set_device_core_options(int p_device, int64_t p_options);
	int64_t get_device_core_options(int p_device) const;

	// Persisted form of the per-device settings (an Array of Dictionaries).
	void set_device_settings(const Array &p_settings);
	Array get_device_settings() const;
	// Persisted form of the settings that override project settings.
	void set_overrides(const Dictionary &p_overrides);
	Dictionary get_overrides() const;

	// Splits "chip_<index>_<field>"; returns false for other names or an invalid chip index.
	bool parse_chip_property(const StringName &p_name, int &r_device, String &r_field) const;

	// For VGMStreamPlayback (audio thread). The version changes when this stream's settings change;
	// project setting changes are tracked by VGMProjectSettings::get_version().
	uint64_t get_settings_version() const { return settings_version.load(); }
	PlaybackSettings get_playback_settings() const;

	Ref<AudioStreamPlayback> _instantiate_playback() const override;
	TypedArray<Dictionary> _get_parameter_list() const override;
	String _get_stream_name() const override;
	double _get_length() const override;
	bool _has_loop() const override;
	Dictionary _get_tags() const override;

protected:
	static void _bind_methods();
	void _get_property_list(List<PropertyInfo> *p_list) const;
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	bool _property_can_revert(const StringName &p_name) const;
	bool _property_get_revert(const StringName &p_name, Variant &r_property) const;

private:
	// Song info, replaced by set_data(). Main thread only.
	PackedByteArray data;
	std::vector<Device> devices;
	Dictionary tags;
	double length = 0.0;
	bool has_loop = false;
	String format;
	String format_version;
	double loop_start = 0.0; // File time, seconds; 0 without a loop.
	double loop_length = 0.0;

	// Read by playbacks on the audio thread; guarded by settings_mutex. Written on the main thread only,
	// so main-thread reads need no lock.
	mutable std::mutex settings_mutex;
	Settings settings;
	std::atomic<uint64_t> settings_version{ 0 };

	bool is_valid_device(int p_device) const;
	// Applies p_modify to the settings under the lock, then bumps the version (picked up by playbacks)
	// and emits `changed`.
	template <typename F>
	void modify_settings(F p_modify);
};

VARIANT_ENUM_CAST(VGMStream::ResamplerMode);
