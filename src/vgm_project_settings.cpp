#include "vgm_project_settings.hpp"

#include <atomic>
#include <mutex>

#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/dictionary.hpp>

using namespace godot;

static const char *LOOP_COUNT = "libvgm_settings/playback/loop_count";
static const char *FADE_TIME = "libvgm_settings/playback/fade_time";
static const char *IGNORE_SONG_VOLUME = "libvgm_settings/playback/ignore_song_volume";
static const char *RESAMPLER_MODE = "libvgm_settings/playback/resampler_mode";

static std::mutex defaults_mutex;
static VGMProjectSettings::Defaults defaults;
static std::atomic<uint64_t> defaults_version{ 0 };

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
	add_setting(LOOP_COUNT, 0, Variant::INT, PROPERTY_HINT_RANGE, "0,100,1,or_greater");
	add_setting(FADE_TIME, 0.0, Variant::FLOAT, PROPERTY_HINT_RANGE, "0,30,0.1,or_greater,suffix:s");
	add_setting(IGNORE_SONG_VOLUME, false, Variant::BOOL, PROPERTY_HINT_NONE, "");
	add_setting(RESAMPLER_MODE, 0, Variant::INT, PROPERTY_HINT_ENUM, "High Quality,Low Quality,LQ Down HQ Up");

	reload();
	ProjectSettings::get_singleton()->connect("settings_changed", callable_mp_static(&VGMProjectSettings::reload));
}

void VGMProjectSettings::uninitialize() {
	ProjectSettings::get_singleton()->disconnect("settings_changed", callable_mp_static(&VGMProjectSettings::reload));
}

void VGMProjectSettings::reload() {
	ProjectSettings *ps = ProjectSettings::get_singleton();
	Defaults loaded;
	loaded.loop_count = MAX((int)ps->get_setting_with_override(LOOP_COUNT), 0);
	loaded.fade_time = MAX((double)ps->get_setting_with_override(FADE_TIME), 0.0);
	loaded.ignore_song_volume = ps->get_setting_with_override(IGNORE_SONG_VOLUME);
	loaded.resampler_mode = CLAMP((int)ps->get_setting_with_override(RESAMPLER_MODE), 0, 2);
	{
		std::lock_guard<std::mutex> lock(defaults_mutex);
		defaults = loaded;
	}
	defaults_version++;
}

VGMProjectSettings::Defaults VGMProjectSettings::get() {
	std::lock_guard<std::mutex> lock(defaults_mutex);
	return defaults;
}

uint64_t VGMProjectSettings::get_version() {
	return defaults_version.load();
}
