#pragma once

#include <cstdint>

#include <godot_cpp/variant/packed_byte_array.hpp>

// Project-wide defaults under "libvgm_settings/" in Project Settings, used by every VGMStream that doesn't
// override them. Values are cached here so the audio thread can read them.
class VGMProjectSettings {
public:
	struct Defaults {
		int loop_count = 0;
		double fade_time = 0.0;
		bool ignore_song_volume = false;
		int resampler_mode = 0;
	};

	// Main thread, at extension init/deinit.
	static void initialize();
	static void uninitialize();

	// Any thread.
	static Defaults get();
	static uint64_t get_version();
	// The YM2608 rhythm ROM chosen in the project settings; empty when none is set (the built-in sounds play).
	static godot::PackedByteArray get_ym2608_rhythm_rom();

private:
	static void reload();
};
