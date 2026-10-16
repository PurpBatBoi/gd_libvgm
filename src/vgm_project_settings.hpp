#pragma once

#include <cstdint>

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

private:
	static void reload();
};
