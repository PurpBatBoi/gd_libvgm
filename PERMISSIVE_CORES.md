# Permissively licensed emulation cores

The bundled libvgm (`thirdparty/libvgm`, [the fork's `godot_libvgm` branch](https://github.com/PurpBatBoi/libvgm/tree/godot_libvgm))
contains only sound cores whose license is MIT, BSD-3-Clause or ISC. A game can ship them without releasing its
own source code; it only has to include their copyright notice and license text (for example in its
credits or a third-party licenses file).

Chips with no permissive core are left out of the build. Songs that use any of them
[don't load](#unsupported-chips).

Each chip has exactly one core. Where libvgm offered several permissive ones, the fork keeps the one that
runs fastest (MAME for the AY-3-8910, Game Boy and HuC6280). The **core name** is libvgm's name for
that core.

> This list covers the emulation cores only. See [Not covered here](#not-covered-here): libvgm's own
> framework code has no license yet, and that blocks shipping regardless of the cores chosen.
> Not legal advice.

## Supported chips

| Chip | Typical use | Core | License | Source |
|---|---|---|---|---|
| SN76489 / SN76496 | Master System, Game Gear, Genesis PSG, BBC Micro | `MAME` | BSD-3-Clause | `sn76496.c` |
| AY-3-8910 / YM2149 | MSX, ZX Spectrum, Atari ST, Amstrad CPC | `MAME` | BSD-3-Clause | `ay8910.c` |
| YM2413 (OPLL) | Master System FM, MSX-MUSIC, VRC7 | `EMU2413` | MIT ([upstream](https://github.com/digital-sound-antiques/emu2413)) | `emu2413.c` |
| YM2612 / YM3438 | Genesis / Mega Drive FM | `ares` | ISC ([upstream](https://github.com/ares-emulator/ares/tree/master/ares/component/audio/ym2612)) | `ym2612.c` (C port of the ares core by Talarubi) |
| HuC6280 | PC Engine / TurboGrafx-16 | `MAME` | BSD-3-Clause | `c6280_mame.c` |
| Game Boy DMG | Game Boy | `MAME` | BSD-3-Clause | `gb_mame.c` |
| NES APU (+ FDS) | NES / Famicom, Famicom Disk System | `apu2a03` | 2A03: MIT (written for this project; its renderer reuses the band-limited synth and filters from [TetaNES](https://github.com/lukexor/tetanes)). FDS: written for this project from the [nesdev wiki](https://www.nesdev.org/wiki/FDS_audio), same license as this project (not chosen yet). | `apu2a03.c`, `nes_apu_2a03.c`, `nes_fds_2a03.c`, `nes_synth_2a03.h` (in the libvgm fork) |
| RF5C68 / RF5C164 | Sega CD, FM Towns | `MAME` | BSD-3-Clause | `rf5c68.c` (not written for the RF5C164; untested there) |
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

`apu2a03` applies the console's output filters, so bass and the DPCM channel come out quieter than with
other NES emulators. Its FDS volume envelope follows the wiki's timing, so some FDS attacks ramp up
more slowly than in NSFPlay.

The `ares` YM2612 core was first written for higan (GPLv3) in 2017. ares relicensed it to ISC in v121
(2021), and ares maintainer Luke Usher confirmed on 2026-10-16 that all ares code outside its `thirdparty/`
directory is ISC. The core matches ares's output sample for sample. It leaves out ares's Mega Drive board
filters (20 Hz high-pass, 2840 Hz low-pass), CSM mode and the busy flag.

## Unsupported chips

These chips only have GPL, LGPL or unlicensed cores, so the fork removes them. Loading a song that uses
one of them fails with an error naming the chip, for now.

| Chip | Typical use | Removed cores and licenses |
|---|---|---|
| YM2151 (OPM) | Arcade, X68000 | `MAME` GPL-2.0+, `Nuked OPM` LGPL-2.1+ |
| YM3812 / YM3526 / Y8950 (OPL) | AdLib, arcade, MSX-AUDIO | `MAME` GPL-2.0+, `AdLibEmu` / `Nuked OPL3` LGPL-2.1+ |
| YMF262 (OPL3) | Sound Blaster 16 / Pro 2 | `MAME` GPL-2.0+, `AdLibEmu` / `Nuked OPL3` LGPL-2.1+ |
| YMF278B (OPL4) | MSX MoonSound | `openMSX` GPL-2.0 |
| YM2203 / YM2608 / YM2610 (OPN family) | PC-88/98, Neo Geo | `MAME` GPL-2.0+ |
| 32X PWM | Sega 32X | `Gens/GS` GPL |
| VSU | Virtual Boy | `Mednafen` GPL |
| MSM5232 | Arcade | `MAME` GPL-2.0+ |
| ES5505 / ES5506 | Arcade, Ensoniq | no license stated |
| WonderSwan | WonderSwan | no license stated |

DRO files (OPL) can't load at all until those chips have cores.

A possible future fix: [ymfm](https://github.com/aaronsgiles/ymfm) (BSD-3-Clause, already bundled for the
YM2414) also emulates the YM2151, the OPL family and the OPN family, but libvgm doesn't use it
for those chips yet. Caveat: ymfm is template-heavy C++, while ValleyBell (libvgm's author) recommends
that cores be written in C90. A C90 port of the relevant ymfm code would fit libvgm better, the same
way the `apu2a03` NES core is written in C90.

## Not covered here

These also ship inside the extension and need to be cleared before release:

- **libvgm's framework** (`player/`, `emu/*.c`, the `*intf.c` chip interfaces, `utils/`): no license
  file or headers. Needs a license or permission from libvgm's author before any distribution.
- **zlib** (`thirdparty/zlib`): zlib license, permissive.
- **Godot / godot-cpp**: MIT.

## Shipping

Include the copyright notice and license text of each core you ship. MAME cores carry their copyright
holders at the top of each source file, `nes_apu_2a03.c` and `nes_synth_2a03.h` carry TetaNES's MIT notice, and `ym2612.c` carries the ares ISC notice.
