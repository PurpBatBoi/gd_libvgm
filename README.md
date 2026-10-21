# gd_libvgm

Native playback of sound chip music in Godot 4.5+, powered by [libvgm](https://github.com/ValleyBell/libvgm).

Songs are not converted to audio files: libvgm imitates the original sound chips (for example the SN76489, AY-3-8910, HuC6280, NES, Game Boy, and Yamaha's FM chips such as the YM2612 and YM2151) while the game runs. Files stay tiny, and you can change tempo, mute channels and switch chip filters on and off while a song plays.

**Formats:** VGM / VGZ, S98, DRO (DOSBox OPL), GYM, SPC (Super Nintendo), NSF / NSFe (NES, with the expansion chips).

**Chips:** only chips whose emulation code has a permissive license (MIT / BSD), so you can ship them in a game. See [Supported chips](#supported-chips).

---

## Installation

Copy the `addons/gd_libvgm` folder into your Godot project, then restart the editor.
You get it from a release zip (see [Releasing](#releasing)) or by [building](#building) it yourself.

## Quick start (no code)

1. Put a `.vgz` / `.vgm` / `.s98` / `.dro` / `.gym` / `.spc` / `.nsf` / `.nsfe` file in your project.
2. Add an `AudioStreamPlayer` (or the 2D / 3D variant).
3. Drag the song from the FileSystem dock onto its **Stream** property.
4. Tick **Autoplay** (or **Playing** to hear it in the editor).

Click the assigned stream in the inspector to see its settings.
For music that keeps playing across scenes, put the player in an autoload scene.

## Supported chips

Only chips whose emulation code (the "core") has a permissive license (MIT / BSD-3-Clause / ISC) are built in. A song that uses any other chip fails to load, with an error naming the chip.

| Chip | Used in | Cores |
|---|---|---|
| SN76489 / SN76496 | Master System, Game Gear, Genesis PSG, BBC Micro | MAME |
| AY-3-8910 / YM2149 | MSX, ZX Spectrum, Atari ST, Amstrad CPC | MAME |
| YM2413 (OPLL) | Master System FM, MSX-MUSIC, VRC7 | EMU2413 |
| YM2612 / YM3438 | Genesis / Mega Drive FM | ymfm |
| YM2151 (OPM) | Arcade, X68000 | ymfm |
| YM3812 / YM3526 / Y8950 (OPL, OPL2, MSX-AUDIO) | AdLib, arcade, MSX-AUDIO | ymfm |
| YMF262 (OPL3) | Sound Blaster 16 / Pro 2 | ymfm |
| YM2203 / YM2608 / YM2610 / YM2610B (OPN family) | PC-88/98, Neo Geo, arcade | ymfm |
| YMF278B (OPL4) | MSX MoonSound | ymfm (needs the YRW801 wave ROM, see below) |
| HuC6280 | PC Engine / TurboGrafx-16 | MAME |
| Game Boy DMG | Game Boy | MAME |
| NES APU + FDS | NES / Famicom, Famicom Disk System | BeesNES |
| VRC6, Namco 163, Sunsoft 5B, MMC5 | NES / Famicom cartridges (NSF / NSFe files only) | BeesNES (VRC6, N163, 5B), ares (MMC5, 5B envelope) |
| RF5C68 / RF5C164 | Sega CD, FM Towns | MAME |
| 32X PWM | Sega 32X | ares |
| WonderSwan sound | WonderSwan, WonderSwan Color | ares |
| QSound | Capcom CPS-2 | MAME |
| SAA1099 | SAM Coupé, Creative Music System | MAME |
| Sega PCM | Sega arcade | MAME |
| SCSP (YMF292) | Saturn, Sega arcade | MAME |
| YMW258 (MultiPCM) | Sega Model 1/2 | MAME |
| YMZ280B | Arcade | MAME |
| YMF271 (OPX) | Arcade | superctr |
| YM2414 (OPZ) | Yamaha TX81Z | ymfm |
| C140 / C219 | Namco System 2 / 21 / ND-1 | MAME |
| C352 | Namco System 11/12/22 | superctr |
| K051649 (SCC), K053260, K054539, K005289, K007232 | Konami MSX, Konami arcade | MAME |
| POKEY | Atari 8-bit, Atari arcade | MAME |
| uPD7759 | Sega Genesis (some), arcade | MAME |
| MSM6258 | X68000 | MAME |
| MSM6295 | Arcade | MAME |
| MSM5205 | Arcade | eito |
| X1-010 | Seta arcade | MAME |
| Irem GA20 | Irem arcade | MAME |
| ES5503 | Apple IIGS | MAME |
| ICS2115 | Arcade (PGM) | MAME |
| BSMT2000 | Data East / pinball | MAME |
| Mikey | Atari Lynx | laoo |

**NES expansion chips** (extra sound chips inside some cartridges): a VGM / VGZ file can use the NES's own sound chip (2A03, with the FDS) and the VRC7 only. The other expansion chips (VRC6, Namco 163, Sunsoft 5B, MMC5) play from an NSF / NSFe file, which contains the game's own music program and runs it on an emulated NES CPU.

**Not supported for now:** Virtual Boy VSU, MSM5232, ES5505 / ES5506. The only emulators available for them have restrictive licenses (GPL, LGPL) or no license, so they are left out.

The YM2608's drum sounds are stored in a ROM chip that belongs to Yamaha. A free replacement is built in, so PC-98 drums sound different from the real chip.

The YMF278B plays its FM instruments without any extra file. Its sampled instruments need the YRW801 wave ROM (a file of instrument sounds), which is Yamaha's
and can't be shipped: without it those instruments are silent (see [YMF278B wave ROM](#ymf278b-wave-rom)).

Licenses and source files per core: [PERMISSIVE_CORES.md](PERMISSIVE_CORES.md).

## Settings

All settings live on the song (a `VGMStream` resource) and apply immediately, even while it plays. Every player using the same song shares them. To give players of the same song different speeds or mutes, use the [per-player parameters](#per-player-parameters) instead of `duplicate()`.

### Per song

| Setting | What it does |
|---|---|
| Speed | Tempo multiplier. Pitch stays the same (use the player's Pitch Scale to change pitch). For SPC and NSF files, the game's music program runs faster, but the chips themselves keep their normal speed. |
| Loop Count | How many times the loop plays before the song ends. `0` = loop forever. |
| Fade Time | Fade-out length after the last loop. |
| Ignore Song Volume | Ignore the volume adjustment stored in the file. |
| Resampler Mode | Sound quality when each chip's audio is converted to the output sample rate. |
| Track | The song to play, for an NSF / NSFe file that holds several. Changing it restarts the players of the song. |

### Per chip

Each chip used by the song gets its own group in the inspector:

- **Muted Channels**: one checkbox per channel, with the chip's channel names.
- **Options**: on/off switches for the chip's emulation, including the filters of the real hardware (for example the NES mixer).
- **Panning**: a left / right slider per channel, shown when the chip supports it.

### Per-player parameters

Each `AudioStreamPlayer` (Godot 4.3+) has a **Parameters** section for the song it plays. These belong to that player only:

- `parameters/speed`: multiplies the song's Speed.
- `parameters/chip_<index>_muted_channels`: channels to mute on top of the song's own muted channels.

### Project-wide defaults

*Project Settings → Libvgm Settings* holds the defaults for loop count, fade time, song volume handling, and resampler quality.
Songs follow these until you change that setting on the song itself. Reverting it in the inspector makes it follow the project again.

### YM2608 rhythm ROM

The YM2608 (PC-98) plays its drums from an 8 KB file of drum sounds (the rhythm ROM). The extension has a free replacement built in
([Tetorica OPNA rhythm sounds](https://github.com/kyorohiro/hello_ymfm_wasm/tree/main/assets/opna-rhythm), BSD-3-Clause),
which sounds different from the real chip. To use another ROM, for example one you made with Tetorica's
[editor page](https://kyorohiro.github.io/hello_ymfm_wasm/demos/opna-rhythm-compare.html):

1. Put the 8 KB file in your project folder.
2. Pick it in *Project Settings → Libvgm Settings → Roms → Ym2608 Rhythm Rom*.
3. Godot doesn't export `.bin` / `.rom` files by itself: add the file (or `*.bin`) to the export preset's
   *Resources → Filters to export non-resource files/folders*.

A new ROM applies to songs started afterwards. A file of the wrong size is ignored with a warning. Only ship a ROM
you are allowed to distribute; Yamaha's own ROM is copyrighted.

### YMF278B wave ROM

The YMF278B (MoonSound) plays its sampled instruments from the YRW801 wave ROM, a 2 MB file (2097152 bytes) that you must supply
yourself. There is no built-in replacement: without it the sampled instruments are silent and only the FM instruments play. Songs that
carry their own samples in the file play without it.

1. Put the 2 MB file in your project folder.
2. Pick it in *Project Settings → Libvgm Settings → Roms → Yrw801 Rom*.
3. Godot doesn't export `.bin` / `.rom` files by itself: add the file (or `*.rom`) to the export preset's
   *Resources → Filters to export non-resource files/folders*.

A new ROM applies to songs started afterwards. A file of the wrong size is ignored with a warning. The YRW801 is
copyrighted by Yamaha: exporting it with your game means your game redistributes it, which is your own legal
responsibility.

### Filtering

The filters that real hardware had are imitated by libvgm and shown as chip **Options**. For general sound shaping, such as cutting high or low tones (low-pass / high-pass) or EQ, use Godot's audio bus effects (`AudioEffectLowPassFilter`, `AudioEffectEQ`, ...).

### Saving settings

A song loaded straight from a `.vgz` file can't save settings back into that file. To keep inspector changes, either right-click the player's Stream → **Make Unique** (stored in the scene), or save the stream as a `.tres` resource. Both store the song data and its settings.

## Scripting

```gdscript
var song: VGMStream = load("res://music/stage1.vgz")
$AudioStreamPlayer.stream = song
$AudioStreamPlayer.play()

song.speed = 1.25                    # Faster tempo, same pitch
song.loop_count = 2                  # Play the loop twice...
song.fade_time = 4.0                 # ...then fade out and emit `finished`

print(song.get_song_tags())          # TITLE, GAME, ARTIST, DATE, ...
for d in song.get_device_count():
    print(song.get_device_name(d), song.get_channel_names(d))

song.set_channel_muted(0, 2, true)   # Mute channel 3 of the first chip
song.set_device_core_options(0, song.get_device_core_options(0) & ~VGMStream.CORE_OPTION_NES_NONLINEAR_MIXER)
if song.has_channel_panning(0):
    song.set_channel_pan(0, 0, -1.0) # Channel 1 hard left
```

An NSF / NSFe file can hold several songs, called tracks. Speed changes a track's tempo just as it does for other songs, and the pitch stays the same. The exception is the NES's recorded samples (DPCM, often used for drums), which always play at normal speed. A track with no length in the file plays until you stop it:

```gdscript
var nsf: VGMStream = load("res://music/game.nsf")
print(nsf.get_track_count())         # Tracks in the file
nsf.track = 4                        # Fifth track (from 0); restarts it if it is playing
```

Song info (read-only):

```gdscript
print(song.format, " ", song.format_version)        # "VGM 1.71"
print(song.loop_start, " ", song.loop_length)       # In seconds; 0 without a loop
print(song.get_device_clock(0))                     # Chip clock in Hz
$AudioStreamPlayer.seek(song.get_loop_start())      # Jump to the loop
```

The playback reports loops and the fade-out with signals. `finished` marks the end of the song:

```gdscript
$AudioStreamPlayer.play()
var playback := $AudioStreamPlayer.get_stream_playback() as VGMStreamPlayback
playback.looped.connect(func(loop): print("loop ", loop))   # 1 = first repeat
playback.fade_started.connect(func(): print("fading out"))
```

Signals fire a little before you hear the matching sound, by about `AudioServer.get_output_latency()`.

`playback.get_cpu_usage()` returns how much of the audio thread's time the song takes (`0.1` = 10%), and `playback.get_memory_usage()` the song's memory use in bytes (Linux/glibc only, `0` elsewhere and in the editor).

`test_project/demo.tscn` shows both calls. Everything else in it is plain GDScript in `demo.gd`, specific to the demo and not part of the extension: the process and Godot RAM/CPU readouts (Linux only), the seekable timeline, and drag and drop. Assign a song to its `AudioStreamPlayer` and run the scene, or drop a song file onto the running window; the demo loads it with `load()` and plays it.

Per-player values, without `duplicate()`:

```gdscript
$PlayerA.set("parameters/speed", 2.0)
$PlayerB.set("parameters/chip_0_muted_channels", 0b100)   # Mute channel 3 of the first chip, on B only
```

The full API is on the `VGMStream` page of Godot's built-in help.
`test_project/main.gd` is a small demo: assign a song to the `Main` node's `song` property and use the keys listed at the top of the script.

## Notes

- Each playing song imitates its chips the whole time it plays, but this is light on the CPU.
- Changing the resampler mode restarts that song's emulation, but playback continues from the same position.
- Songs without a loop point end on their own and emit `finished`. An NSF track that has no length in its file (most have none) never ends: it plays until you stop it, like a looping background track.

---

## Building

### Requirements

- Python 3 and [SCons](https://scons.org/)
- A C++17 compiler: GCC or Clang on Linux, Xcode command line tools on macOS, MSVC on Windows
- Git

### Steps

```sh
git clone --recursive <this repository>
cd gd_libvgm
scons target=template_debug     # or template_release
```

If you cloned without `--recursive`, run `git submodule update --init --recursive` first (`godot-cpp`, `thirdparty/libvgm` and `thirdparty/zlib` are submodules).

The library is built into `bin/<platform>/` and installed into `test_project/addons/gd_libvgm/`. Open `test_project` in Godot to try it; put test songs in `test_project/songs/`, which git ignores.
`python setup.py` opens a menu for the other build tasks (target Godot version, target project folder, release / LTO builds, regenerating the class docs).

### Project layout

| Path | Contents |
|---|---|
| `src/` | The extension: `VGMStream` (resource + settings), `VGMStreamPlayback` (rendering), file loader, project settings |
| `thirdparty/libvgm/` | libvgm submodule: [a modified copy](https://github.com/PurpBatBoi/libvgm/tree/godot_libvgm) of libvgm with only permissively licensed cores, one per chip. It adds the `BeesNES` NES core (the main NES sound chip plus VRC6, N163 and 5B; `emu/cores/beesnes_*`, `emu/cores/purps_fds.*`, `emu/cores/nes_filter.h`, `emu/cores/nes_expout.h`), the `ares` MMC5 core, the NSF player (`player/nsfplayer.*`, `player/nsf_cpu6502.*`) and a filter that avoids harsh digital noise (`libs/blep_filter`) |
| `thirdparty/zlib/` | zlib submodule, for compressed `.vgz` files |
| `tools/libvgm_build.py` | Compiles libvgm and zlib into the extension |
| `doc_classes/` | Class reference shown in Godot's help |
| `gd_libvgm.gdextension` | Tells Godot which library to load per platform |

## Releasing

The **Build GDExtension Cross Platform Plugin** GitHub Actions workflow builds Windows, macOS, Linux, Android, iOS and Web. Run it from the Actions tab, then download `finished_unzip_me` from the run's summary: it contains the ready-to-ship zip with the `addons` folder.

So far the extension has only been built and tested on Linux x86_64; other platforms are first compiled by that workflow.

## Credits

- [libvgm](https://github.com/ValleyBell/libvgm) by Valley Bell and the authors of its sound cores
- [BeesNES](https://github.com/L-Spiro/BeesNES) by L-Spiro, a very accurate NES emulator (MIT): the `BeesNES` NES core is its main sound chip, and the VRC6, N163 and 5B cores are its expansion chips, rewritten in C.
- [ares](https://github.com/ares-emulator/ares), a multi-system emulator (ISC): the `ares` 32X PWM, WonderSwan sound and MMC5 cores are its sound code rewritten in C, the 5B core's volume envelope comes from its YM2149, and the NSF player's NES CPU (6502) is its MOS 6502 core rewritten in C.
- [Snaggletooth](https://github.com/etroimcasso/Snaggletooth) by Eric Tomasso (MIT): the `Snaggletooth` SNES core, which plays `.spc` files, is its audio unit (SPC700 and S-DSP) rewritten in C.
- [zlib](https://zlib.net/)
- Build setup based on the [Godot++](https://github.com/nikoladevelops/godot-plus-plus) GDExtension template
