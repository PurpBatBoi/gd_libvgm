#pragma once

#include <cstdint>
#include <cstdlib>
// mallinfo2() exists from glibc 2.33. __GLIBC_PREREQ is only defined on glibc, so it can't share an #if with defined().
#if defined(__GLIBC__)
#include <malloc.h>
#if __GLIBC_PREREQ(2, 33)
#define VGM_HAS_MALLINFO2 1
#endif
#endif

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

// Bytes the C heap holds in live allocations. 0 where the platform cannot tell (only glibc 2.33+ can).
inline size_t heap_bytes_in_use() {
#ifdef VGM_HAS_MALLINFO2
	const struct mallinfo2 info = mallinfo2();
	return info.uordblks + info.hblkhd;
#else
	return 0;
#endif
}
