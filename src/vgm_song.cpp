#include "vgm_song.hpp"

#include "player/droplayer.hpp"
#include "player/gymplayer.hpp"
#include "player/s98player.hpp"
#include "player/vgmplayer.hpp"
#include "utils/MemoryLoader.h"

VGMSong::VGMSong() {
	player.RegisterPlayerEngine(new VGMPlayer);
	player.RegisterPlayerEngine(new S98Player);
	player.RegisterPlayerEngine(new DROPlayer);
	player.RegisterPlayerEngine(new GYMPlayer);
}

VGMSong::~VGMSong() {
	unload();
}

void VGMSong::unload() {
	player.Stop();
	player.UnloadFile();
	if (loader) {
		DataLoader_Deinit(loader);
		loader = nullptr;
	}
}

bool VGMSong::load(const PackedByteArray &p_data, uint32_t p_sample_rate) {
	unload();
	data = p_data;
	sample_rate = p_sample_rate;
	if (data.is_empty()) {
		return false;
	}
	if (player.SetOutputSettings(p_sample_rate, 2, 32, RENDER_BUFFER_FRAMES)) {
		return false;
	}
	loader = MemoryLoader_Init(data.ptr(), (UINT32)data.size());
	if (!loader) {
		return false;
	}
	if (DataLoader_Load(loader) || player.LoadFile(loader)) {
		unload();
		return false;
	}
	return true;
}

bool VGMSong::reload() {
	const PackedByteArray current = data;
	return load(current, sample_rate);
}
