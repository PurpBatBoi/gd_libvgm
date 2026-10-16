from pathlib import Path
from typing import Any

from tools.paths import PROJECT_ROOT

LIBVGM_DIR = PROJECT_ROOT / "thirdparty" / "libvgm"
ZLIB_DIR = PROJECT_ROOT / "thirdparty" / "zlib"
OBJ_DIR = PROJECT_ROOT / "build"

# Textually #included by other cores, never compiled on their own.
_INCLUDED_ONLY = {"adlibemu_opl_inc.c", "scsplfo.c"}


def _libvgm_sources(platform: str) -> list[str]:
    files = sorted(LIBVGM_DIR.glob("emu/*.c"))
    files += sorted(p for p in LIBVGM_DIR.glob("emu/cores/*.c") if p.name not in _INCLUDED_ONLY)
    files += [LIBVGM_DIR / "libs/blep_filter/blep_filter.c"]
    files += [LIBVGM_DIR / "emu/cores/ymfmintf.cpp", LIBVGM_DIR / "emu/cores/ymfm/ymfm_opz.cpp"]
    files += sorted(LIBVGM_DIR.glob("player/*.c")) + sorted(LIBVGM_DIR.glob("player/*.cpp"))
    files += [LIBVGM_DIR / "utils/DataLoader.c", LIBVGM_DIR / "utils/MemoryLoader.c"]
    # libvgm only inflates in memory; zlib's gz* file API is not needed.
    files += sorted(p for p in ZLIB_DIR.glob("*.c") if not p.name.startswith("gz"))

    # Charset conversion for song tags. Android (API 24) has no iconv: src/libvgm_strutils_stub.c is used there.
    if platform == "windows":
        files.append(LIBVGM_DIR / "utils/StrUtils-CPConv_Win.c")
    elif platform != "android":
        files.append(LIBVGM_DIR / "utils/StrUtils-CPConv_IConv.c")
    return [str(f) for f in files]


def add_libvgm(env: Any) -> list[Any]:
    """Configures env to use libvgm headers and returns libvgm + zlib objects to link into the extension."""
    if not (LIBVGM_DIR / "stdtype.h").exists():
        raise RuntimeError("thirdparty/libvgm is missing. Run: git submodule update --init --recursive thirdparty/libvgm")
    if not (ZLIB_DIR / "zlib.h").exists():
        raise RuntimeError("thirdparty/zlib is missing. Run: git submodule update --init thirdparty/zlib")

    # All Godot target platforms are little endian.
    env.Append(CPPPATH=[str(LIBVGM_DIR), str(LIBVGM_DIR / "libs/blep_filter")], CPPDEFINES=["VGM_LITTLE_ENDIAN", "HAVE_STDINT_H"])
    if env["platform"] in ("macos", "ios"):
        env.Append(LIBS=["iconv"])

    # Third-party C90/C++98 code: silence warnings; ymfm needs C++14 and libvgm uses `register` (removed in C++17).
    vgm_env = env.Clone()
    vgm_env.Append(CPPPATH=[str(ZLIB_DIR)])
    if vgm_env.get("is_msvc", False):
        vgm_env.Append(CCFLAGS=["/w"])
    else:
        vgm_env.Append(CCFLAGS=["-w"])
        vgm_env["CXXFLAGS"] = [f for f in vgm_env["CXXFLAGS"] if f != "-std=c++17"] + ["-std=c++14"]
    # Objects go under build/ rather than next to their sources, so the submodules stay clean.
    objects = []
    obj_suffix = vgm_env.subst("$SHOBJSUFFIX")  # MSVC defines it as "$OBJSUFFIX", so expand it.
    for source in _libvgm_sources(env["platform"]):
        target = OBJ_DIR / Path(source).relative_to(PROJECT_ROOT).with_suffix(obj_suffix)
        objects += vgm_env.SharedObject(target=str(target), source=source)
    return objects
