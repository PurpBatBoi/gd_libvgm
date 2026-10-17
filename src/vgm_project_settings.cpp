#include "vgm_project_settings.hpp"

#include <atomic>
#include <mutex>

#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/dictionary.hpp>

using namespace godot;

static const char *LOOP_COUNT = "libvgm_settings/playback/loop_count";
static const char *FADE_TIME = "libvgm_settings/playback/fade_time";
static const char *IGNORE_SONG_VOLUME = "libvgm_settings/playback/ignore_song_volume";
static const char *RESAMPLER_MODE = "libvgm_settings/playback/resampler_mode";
static const char *YM2608_RHYTHM_ROM = "libvgm_settings/roms/ym2608_rhythm_rom";

static constexpr int64_t YM2608_RHYTHM_ROM_SIZE = 0x2000;

static std::mutex defaults_mutex;
static VGMProjectSettings::Defaults defaults;
static std::atomic<uint64_t> defaults_version{ 0 };
// Godot types can't be built before the extension is initialized (at library load), so this lives on the heap
// between initialize() and uninitialize().
struct RomState {
	PackedByteArray ym2608_rhythm_rom; // guarded by defaults_mutex
	String ym2608_rhythm_rom_path; // main thread only: the path last read
};
static RomState *roms = nullptr;

// Reads the ROM file a setting points to; an empty path or a bad file gives no ROM.
static PackedByteArray read_rom(const String &p_path, int64_t p_size) {
	if (p_path.is_empty()) {
		return PackedByteArray();
	}
	const PackedByteArray data = FileAccess::get_file_as_bytes(p_path);
	if (data.size() != p_size) {
		WARN_PRINT(vformat("libvgm: %s must be a %d-byte ROM file; using the built-in sounds instead.", p_path, p_size));
		return PackedByteArray();
	}
	return data;
}

static void add_setting(const String &p_name, const Variant &p_default, Variant::Type p_type, PropertyHint p_hint, const String &p_hint_string) {
	ProjectSettings *ps = ProjectSettings::get_singleton();
	if (!ps->has_setting(p_name)) {
		ps->set_setting(p_name, p_default);
	}
	ps->set_initial_value(p_name, p_default);
	ps->set_as_basic(p_name, true);
	Dictionary info;
	info["name"] = p_name;
	info["type"] = p_type;
	info["hint"] = p_hint;
	info["hint_string"] = p_hint_string;
	ps->add_property_info(info);
}

void VGMProjectSettings::initialize() {
	roms = new RomState;
	add_setting(LOOP_COUNT, 0, Variant::INT, PROPERTY_HINT_RANGE, "0,100,1,or_greater");
	add_setting(FADE_TIME, 0.0, Variant::FLOAT, PROPERTY_HINT_RANGE, "0,30,0.1,or_greater,suffix:s");
	add_setting(IGNORE_SONG_VOLUME, false, Variant::BOOL, PROPERTY_HINT_NONE, "");
	add_setting(RESAMPLER_MODE, 0, Variant::INT, PROPERTY_HINT_ENUM, "High Quality,Low Quality,LQ Down HQ Up");
	add_setting(YM2608_RHYTHM_ROM, String(), Variant::STRING, PROPERTY_HINT_FILE, "*.bin,*.rom");

	reload();
	ProjectSettings::get_singleton()->connect("settings_changed", callable_mp_static(&VGMProjectSettings::reload));
}

void VGMProjectSettings::uninitialize() {
	ProjectSettings::get_singleton()->disconnect("settings_changed", callable_mp_static(&VGMProjectSettings::reload));
	std::lock_guard<std::mutex> lock(defaults_mutex);
	delete roms;
	roms = nullptr;
}

void VGMProjectSettings::reload() {
	ProjectSettings *ps = ProjectSettings::get_singleton();
	Defaults loaded;
	loaded.loop_count = MAX((int)ps->get_setting_with_override(LOOP_COUNT), 0);
	loaded.fade_time = MAX((double)ps->get_setting_with_override(FADE_TIME), 0.0);
	loaded.ignore_song_volume = ps->get_setting_with_override(IGNORE_SONG_VOLUME);
	loaded.resampler_mode = CLAMP((int)ps->get_setting_with_override(RESAMPLER_MODE), 0, 2);
	// Settings change one at a time, so only read the ROM again when its path changes.
	const String rom_path = ps->get_setting_with_override(YM2608_RHYTHM_ROM);
	const bool rom_changed = rom_path != roms->ym2608_rhythm_rom_path;
	const PackedByteArray rom = rom_changed ? read_rom(rom_path, YM2608_RHYTHM_ROM_SIZE) : PackedByteArray();
	roms->ym2608_rhythm_rom_path = rom_path;
	{
		std::lock_guard<std::mutex> lock(defaults_mutex);
		defaults = loaded;
		if (rom_changed) {
			roms->ym2608_rhythm_rom = rom;
		}
	}
	defaults_version++;
}

VGMProjectSettings::Defaults VGMProjectSettings::get() {
	std::lock_guard<std::mutex> lock(defaults_mutex);
	return defaults;
}

PackedByteArray VGMProjectSettings::get_ym2608_rhythm_rom() {
	std::lock_guard<std::mutex> lock(defaults_mutex);
	return roms ? roms->ym2608_rhythm_rom : PackedByteArray();
}

uint64_t VGMProjectSettings::get_version() {
	return defaults_version.load();
}
