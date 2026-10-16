#pragma once

#include <cstdint>

#include <godot_cpp/variant/char_utils.hpp>
#include <godot_cpp/variant/string.hpp>

using namespace godot;

// Lowercase ASCII identifier for property names, e.g. "FM 1" -> "fm_1".
inline String to_identifier(const String &p_text) {
	String result = p_text.to_lower();
	for (int64_t i = 0; i < result.length(); i++) {
		if (!is_ascii_alphanumeric_char(result[i])) {
			result[i] = '_';
		}
	}
	return result;
}
