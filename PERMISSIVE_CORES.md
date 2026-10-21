# Permissively licensed emulation cores

The bundled libvgm (`thirdparty/libvgm`, [the fork's `godot_libvgm` branch](https://github.com/PurpBatBoi/libvgm/tree/godot_libvgm))
contains only sound cores (the code that imitates a sound chip) whose license is MIT, BSD-3-Clause or ISC. A game can ship them without releasing its
own source code; it only has to include their copyright notice and license text (for example in its
credits or a third-party licenses file).

Chips with no permissive core are left out of the build. Songs that use any of them
[don't load](#unsupported-chips).

Each chip has exactly one core. Where libvgm offered several permissive ones, the fork keeps the one that
runs fastest (MAME for the AY-3-8910, Game Boy and HuC6280). The **core name** is libvgm's name for
that core. A **port** (as in "C port of ...") is the original code rewritten in C to fit into libvgm.

> This list only covers the chip emulators. The other components that ship in the extension, including libvgm's own code (the part that loads and plays songs), are listed under [Not covered here](#not-covered-here).
> This document is not legal advice.

## Supported chips

| Chip | Typical use | Core | License | Source |
|---|---|---|---|---|
| SN76489 / SN76496 | Master System, Game Gear, Genesis PSG, BBC Micro | `MAME` | BSD-3-Clause | `sn76496.c` |
| AY-3-8910 / YM2149 | MSX, ZX Spectrum, Atari ST, Amstrad CPC | `MAME` | BSD-3-Clause | `ay8910.c` |
| YM2413 (OPLL) | Master System FM, MSX-MUSIC, VRC7 | `EMU2413` | MIT ([upstream](https://github.com/digital-sound-antiques/emu2413)) | `emu2413.c` |
| YM2612 / YM3438 | Genesis / Mega Drive FM | `ymfm` | BSD-3-Clause ([upstream](https://github.com/aaronsgiles/ymfm)) | `ymfm_opn.c` (C port of ymfm's YM2612), `ymfm_fm.c` (the FM engine shared by the ymfm-based cores) |
| YM2151 (OPM) | Arcade, X68000 | `ymfm` | BSD-3-Clause ([upstream](https://github.com/aaronsgiles/ymfm)) | `ymfm_opm.c` (C port of ymfm's YM2151), `ymfm_fm.c` |
| YM3812 / YM3526 / Y8950 (OPL, OPL2, MSX-AUDIO) | AdLib, arcade, MSX-AUDIO | `ymfm` | BSD-3-Clause ([upstream](https://github.com/aaronsgiles/ymfm)) | `ymfm_opl.c` (C port of ymfm's YM3526, YM3812 and Y8950), `ymfm_fm.c`, `ymfm_adpcm.c` (sample playback, ADPCM-B) |
| YMF262 (OPL3) | Sound Blaster 16 / Pro 2 | `ymfm` | BSD-3-Clause ([upstream](https://github.com/aaronsgiles/ymfm)) | `ymfm_opl.c` (C port of ymfm's YMF262), `ymfm_fm.c` |
| YM2203 / YM2608 / YM2610 / YM2610B (OPN family) | PC-88/98, Neo Geo, arcade | `ymfm` | BSD-3-Clause ([upstream](https://github.com/aaronsgiles/ymfm)); YM2608 rhythm sounds: BSD-3-Clause ([Tetorica](https://github.com/kyorohiro/hello_ymfm_wasm)) | `ymfm_opn.c` (C port of ymfm's YM2203, YM2608 and YM2610), `ymfm_fm.c`, `ymfm_adpcm.c` (sample playback, ADPCM-A/B), `ymfm_opn_rhythm.h`; the SSG part uses the AY-3-8910 core |
| YMF278B (OPL4) | MSX MoonSound | `ymfm` | BSD-3-Clause ([upstream](https://github.com/aaronsgiles/ymfm)) | `ymfm_opl.c`, `ymfm_pcm.c` (C port of ymfm's sampled-instrument part and how the chip is controlled; the FM part uses the YMF262 core) |
| HuC6280 | PC Engine / TurboGrafx-16 | `MAME` | BSD-3-Clause | `c6280_mame.c` |
| Game Boy DMG | Game Boy | `MAME` | BSD-3-Clause | `gb_mame.c` |
| NES APU (+ FDS) | NES / Famicom, Famicom Disk System | `BeesNES` | 2A03 (the NES's own sound chip): MIT, taken from [BeesNES](https://github.com/L-Spiro/BeesNES) (L-Spiro's very accurate NES emulator) and rewritten in C. FDS (`purps_fds`): written for this project from the [nesdev wiki](https://www.nesdev.org/wiki/FDS_audio), and MIT licensed like this project. | `beesnes_apu.c`, `beesnes_intf.c`, `purps_fds.c`, `nes_filter.h` (in the libvgm fork) |
| RF5C68 / RF5C164 | Sega CD, FM Towns | `MAME` | BSD-3-Clause | `rf5c68.c` (written for the RF5C68; untested on the RF5C164) |
| 32X PWM | Sega 32X | `ares` | ISC ([upstream](https://github.com/ares-emulator/ares)) | `ares_32xpwm.c` (C port of ares' 32X PWM) |
| WonderSwan sound | WonderSwan, WonderSwan Color | `ares` | ISC ([upstream](https://github.com/ares-emulator/ares)) | `ares_wswan.c` (C port of ares' WonderSwan sound unit) |
| QSound | Capcom CPS-2 | `MAME` | BSD-3-Clause | `qsound_mame.c` |
| SAA1099 | SAM Coupé, Creative Music System | `MAME` | BSD-3-Clause | `saa1099_mame.c` |
| Sega PCM | Sega arcade (OutRun, After Burner) | `MAME` | BSD-3-Clause | `segapcm.c` |
| SCSP (YMF292) | Saturn, Sega arcade | `MAME` | BSD-3-Clause | `scsp.c`, `scspdsp.c`, `scsplfo.c` |
| YMW258 (MultiPCM) | Sega Model 1/2 (e.g. Daytona USA) | `MAME` | BSD-3-Clause | `multipcm.c` |
| YMZ280B | Arcade | `MAME` | BSD-3-Clause | `ymz280b.c` |
| YMF271 (OPX) | Arcade | `superctr` | BSD-3-Clause | `ymf271.c` |
| YM2414 (OPZ) | Yamaha TX81Z | `ymfm` | BSD-3-Clause | `ymfm/`, `ymfmintf.cpp` |
| C140 / C219 | Namco System 2 / 21 / ND-1 | `MAME` | BSD-3-Clause | `c140.c`, `c219.c` |
| C352 | Namco System 11/12/22 | `superctr` | BSD-3-Clause | `c352.c` |
| K051649 (SCC) | Konami MSX, arcade | `MAME` | BSD-3-Clause | `k051649.c` |
| K053260 | Konami arcade | `MAME` | BSD-3-Clause | `k053260.c` |
| K054539 | Konami arcade | `MAME` | BSD-3-Clause | `k054539.c` |
| K005289 | Konami arcade | `MAME` | BSD-3-Clause | `k005289.c` |
| K007232 | Konami arcade | `MAME` | BSD-3-Clause | `k007232.c` |
| POKEY | Atari 8-bit, Atari arcade | `MAME` | BSD-3-Clause | `pokey.c` |
| uPD7759 | Sega Genesis (some), arcade | `MAME` | BSD-3-Clause | `upd7759.c` |
| MSM6258 | X68000 | `MAME` | BSD-3-Clause | `okim6258.c`, `okiadpcm.c` |
| MSM6295 | Arcade | `MAME` | BSD-3-Clause | `okim6295.c`, `okiadpcm.c` |
| MSM5205 | Arcade | `eito` | BSD-3-Clause | `msm5205.c` |
| X1-010 | Seta arcade | `MAME` | BSD-3-Clause | `x1_010.c` |
| Irem GA20 | Irem arcade | `MAME` | BSD-3-Clause | `iremga20.c` |
| ES5503 | Apple IIGS | `MAME` | BSD-3-Clause | `es5503.c` |
| ICS2115 | Arcade (PGM) | `MAME` | BSD-3-Clause | `ics2115.c` |
| BSMT2000 | Data East / pinball | `MAME` | BSD-3-Clause | `bsmt2000.c` |
| Mikey | Atari Lynx | `laoo` | MIT | `mikey.c` |
| SNES APU (SPC700 + S-DSP) | Super Nintendo, `.spc` files | `Snaggletooth` | MIT ([upstream](https://github.com/etroimcasso/Snaggletooth)) | `snes_spc700.c`, `snes_dsp.c`, `snes_apu.c` (C port of Snaggletooth's audio unit, pinned at commit `efd5049`) |
| VRC6 | Konami NES / Famicom cartridges, `.nsf` / `.nsfe` files only | `BeesNES` | MIT ([upstream](https://github.com/L-Spiro/BeesNES)) | `beesnes_vrc6.c` (C port of BeesNES' VRC6), `nes_expout.h`, `nes_filter.h` (in the libvgm fork) |
| Namco 163 | Namco NES / Famicom cartridges, `.nsf` / `.nsfe` files only | `BeesNES` | MIT ([upstream](https://github.com/L-Spiro/BeesNES)) | `beesnes_n163.c` (C port of BeesNES' N163), `nes_expout.h`, `nes_filter.h` |
| Sunsoft 5B | Sunsoft NES / Famicom cartridges (Gimmick!), `.nsf` / `.nsfe` files only | `BeesNES` (tones, noise), `ares` (envelope) | MIT ([upstream](https://github.com/L-Spiro/BeesNES)); the volume envelope is ISC ([upstream](https://github.com/ares-emulator/ares)) | `beesnes_5b.c` (C port of BeesNES' 5B with the volume envelope of ares' YM2149), `nes_expout.h`, `nes_filter.h` |
| MMC5 | Nintendo NES cartridges (Castlevania III), `.nsf` / `.nsfe` files only | `ares` | ISC ([upstream](https://github.com/ares-emulator/ares)) | `ares_mmc5.c` (C port of the sound unit of ares' MMC5 board), `nes_expout.h`, `nes_filter.h` |

## Unsupported chips

The only emulators available for these chips have restrictive licenses (GPL, LGPL) or no license, so the fork removes them. Loading a song that uses
one of them fails with an error naming the chip.

| Chip | Typical use | Removed cores and licenses |
|---|---|---|
| VSU | Virtual Boy | `Mednafen` GPL |
| MSM5232 | Arcade | `MAME` GPL-2.0+ |
| ES5505 / ES5506 | Arcade, Ensoniq | no license stated |

## Not covered here

The following components also ship inside the extension:

- **libvgm's own code** (`player/`, `emu/*.c`, the `*intf.c` chip interfaces, `utils/`): MIT.
- **zlib** (`thirdparty/zlib`): zlib license, permissive.
- **Godot / godot-cpp**: MIT.

## Shipping

The copyright notice and license text of each shipped core must be included. Where to find them:

- **MAME cores**: the copyright holders are listed at the top of each source file.
- **ymfm cores**: `ymfm_fm.c`, `ymfm_adpcm.c`, `ymfm_pcm.c`, `ymfm_opn.c`, `ymfm_opm.c` and `ymfm_opl.c` (and their headers) carry ymfm's BSD-3-Clause notice.
- **YM2608 drum sounds**: `ymfm_opn_rhythm.h` carries their BSD-3-Clause notice.
- **ares-based code**: `ares_32xpwm.c`, `ares_wswan.c`, `ares_mmc5.c`, `player/nsf_cpu6502.c` (the NSF player's NES CPU, a C port of ares' MOS 6502 core) and `beesnes_5b.c` carry the ISC notice of ares. `beesnes_5b.c` also has the MIT notice of BeesNES, and takes its volume envelope from ares' YM2149.

[`THIRD_PARTY_LICENSES.txt`](THIRD_PARTY_LICENSES.txt) collects all of these notices and license texts (cores, blep_filter, zlib, godot-cpp, this extension) in one file that can be shipped with a game. It must be updated when a core is added or removed.
