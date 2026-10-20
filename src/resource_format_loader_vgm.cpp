#include "resource_format_loader_vgm.hpp"

#include <godot_cpp/classes/file_access.hpp>

#include "vgm_stream.hpp"

PackedStringArray ResourceFormatLoaderVGM::_get_recognized_extensions() const {
	return PackedStringArray({ "vgm", "vgz", "s98", "dro", "gym", "spc" });
}

bool ResourceFormatLoaderVGM::_handles_type(const StringName &p_type) const {
	return p_type == StringName("VGMStream") || p_type == StringName("AudioStream");
}

String ResourceFormatLoaderVGM::_get_resource_type(const String &p_path) const {
	return _get_recognized_extensions().has(p_path.get_extension().to_lower()) ? "VGMStream" : "";
}

Variant ResourceFormatLoaderVGM::_load(const String &p_path, const String &p_original_path, bool p_use_sub_threads, int32_t p_cache_mode) const {
	const PackedByteArray bytes = FileAccess::get_file_as_bytes(p_path);
	if (bytes.is_empty()) {
		return FileAccess::get_open_error() != OK ? FileAccess::get_open_error() : ERR_FILE_CORRUPT;
	}
	Ref<VGMStream> stream;
	stream.instantiate();
	stream->set_data(bytes);
	if (stream->get_data().is_empty()) {
		return ERR_FILE_UNRECOGNIZED;
	}
	return stream;
}
