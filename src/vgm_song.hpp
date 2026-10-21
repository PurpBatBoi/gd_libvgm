#pragma once

#include <godot_cpp/variant/packed_byte_array.hpp>

#include "player/playera.hpp"

using namespace godot;

// Owns a libvgm PlayerA (with every format engine registered) playing an in-memory file.
// Not thread-safe: callers serialize access.
class VGMSong {
public:
	// Frames PlayerA renders at most per Render() call.
	static constexpr uint32_t RENDER_BUFFER_FRAMES = 2048;

	VGMSong();
	~VGMSong();
	VGMSong(const VGMSong &) = delete;
	VGMSong &operator=(const VGMSong &) = delete;

	// Loads raw (or gzip-compressed) file bytes and configures 32-bit stereo output at p_sample_rate.
	// Device options must be set after each load, before PlayerA::Start().
	bool load(const PackedByteArray &p_data, uint32_t p_sample_rate);
	// Loads the current file again. Needed before every Start(): libvgm's Stop() discards the
	// device configuration that only LoadFile() builds.
	bool reload();
	// Releases the file and its devices. Safe to call when nothing is loaded.
	void unload();

	// A file holds one song, except an NSF, which holds a number of them. Songs are numbered from 0.
	static int get_song_count(PlayerBase *p_engine);
	static int get_current_song(PlayerBase *p_engine);
	// Chooses the song of the loaded file. Before Start() it only sets what Start() plays; the engine
	// picks the file's own first song again whenever the file is loaded.
	static bool select_song(PlayerBase *p_engine, int p_song);

	PlayerA player;

private:
	PackedByteArray data; // Backing memory for loader; must outlive it.
	PackedByteArray requested_file; // Backing memory for the loader handed to libvgm by on_file_request().
	uint32_t sample_rate = 0;
	DATA_LOADER *loader = nullptr;

	// libvgm asks for external ROM files by name while it starts a song's chips.
	static DATA_LOADER *on_file_request(void *p_user, PlayerBase *p_player, const char *p_name);
};
