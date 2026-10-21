#include "vgm_song.hpp"

#include <cstring>

#include "player/droplayer.hpp"
#include "player/gymplayer.hpp"
#include "player/nsfplayer.hpp"
#include "player/s98player.hpp"
#include "player/spcplayer.hpp"
#include "player/vgmplayer.hpp"
#include "utils/MemoryLoader.h"
#include "vgm_project_settings.hpp"

VGMSong::VGMSong() {
	player.RegisterPlayerEngine(new VGMPlayer);
	player.RegisterPlayerEngine(new S98Player);
	player.RegisterPlayerEngine(new DROPlayer);
	player.RegisterPlayerEngine(new GYMPlayer);
	player.RegisterPlayerEngine(new SPCPlayer);
	player.RegisterPlayerEngine(new NSFPlayer);
	player.SetFileReqCallback(on_file_request, this); // Reaches only the engines registered above.
}

DATA_LOADER *VGMSong::on_file_request(void *p_user, PlayerBase *p_player, const char *p_name) {
	VGMSong *song = static_cast<VGMSong *>(p_user);
	if (strcmp(p_name, "ym2608_adpcm_rom.bin") == 0) {
		song->requested_file = VGMProjectSettings::get_ym2608_rhythm_rom();
	} else if (strcmp(p_name, "yrw801.rom") == 0) {
		song->requested_file = VGMProjectSettings::get_yrw801_rom();
	} else {
		return nullptr;
	}
	// libvgm copies the file before it frees the loader; without a ROM the chip keeps its built-in sounds (the YM2608)
	// or has no samples (the YMF278B).
	if (song->requested_file.is_empty()) {
		return nullptr;
	}
	DATA_LOADER *file_loader = MemoryLoader_Init(song->requested_file.ptr(), (UINT32)song->requested_file.size());
	if (file_loader && DataLoader_Load(file_loader)) {
		DataLoader_Deinit(file_loader);
		return nullptr;
	}
	return file_loader;
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

// The engine is known by its type code: godot-cpp builds without RTTI, so there is no dynamic_cast.
int VGMSong::get_song_count(PlayerBase *p_engine) {
	return p_engine->GetPlayerType() == FCC_NSF ? static_cast<NSFPlayer *>(p_engine)->GetSongCount() : 1;
}

int VGMSong::get_current_song(PlayerBase *p_engine) {
	return p_engine->GetPlayerType() == FCC_NSF ? static_cast<NSFPlayer *>(p_engine)->GetCurSong() : 0;
}

bool VGMSong::select_song(PlayerBase *p_engine, int p_song) {
	if (p_song < 0 || p_song >= get_song_count(p_engine)) {
		return false;
	}
	if (p_engine->GetPlayerType() != FCC_NSF) {
		return true; // The only song.
	}
	NSFPlayer *nsf = static_cast<NSFPlayer *>(p_engine);
	return nsf->GetCurSong() == p_song || nsf->SelectSong((UINT16)p_song) == 0;
}
