# 3D Pinball Space Cadet Talking 3.0.32

Creatiel fork by Gerard Esperinas, based on SpaceCadetPinball 2.1.0 by
Andrey Muzychenko and community contributors.
Project: https://creatiel.info/logiciels/3d-pinball-space-cadet-talking/

## What this archive contains

Complete C++ sources, CMake modules, platform integration files, translation
and glyph tools, changelog, custom Talking media, Unifont fonts and the
Freepats instrument set required by Windows software MIDI.
No executable, DLL, object file, CMake cache, build directory or user scores.
This is a source archive, not a ready-to-run installation.

Version 3.0.32 fixes a Windows audio bug inherited from upstream 2.1.0:
native MIDI music volume also changed the whole application's Windows mixer
volume. Windows now uses TiMidity software synthesis with bundled instruments.
Linux MIDI configuration is unchanged. The Windows mixer volume is respected.

## Historical game data

The original table data, MIDI music and original sound effects are excluded.
To run a build, supply those resources from a copy you are entitled to use:
CADET.DAT (or PINBALL.DAT), PINBALL.MID and the original SOUND files.
Keep the custom SOUND/SOUND2.WAV supplied here, which is the Talking Game Over.
Do not use PINBALL2.MID as music: it is original bitmap font data.
See licenses/RESOURCE-NOTICE.txt; the MIT license of the code does not grant
rights over the historical game resources or other separately licensed media.

## Build on Ubuntu 24.04 x64

Install the development dependencies:

```sh
sudo apt install build-essential cmake libsdl2-dev libsdl2-mixer-dev timidity fluid-soundfont-gm
cmake -S . -B build
cmake --build build -j2
```

CMake places the executable in bin/SpaceCadetPinball. Once historical game
resources have been supplied, run from the project directory:

```sh
./bin/SpaceCadetPinball
```

If MIDI needs a SoundFont, the installed FluidR3 file can be selected:

```sh
SDL_SOUNDFONTS=/usr/share/sounds/sf2/FluidR3_GM.sf2 ./bin/SpaceCadetPinball
```

Preferences and scores use SDL_GetPrefPath, normally
~/.local/share/SpaceCadetPinball/. For isolated tests:

```sh
mkdir -p "$HOME/SpaceCadet-source-test/data"
XDG_DATA_HOME="$HOME/SpaceCadet-source-test/data" ./bin/SpaceCadetPinball
```

## Build on Windows x64

Use a C++11 compiler and CMake, with matching SDL 2.30.0 and SDL_mixer 2.8.0
x64 development packages. CMake searches Libs/SDL2 and Libs/SDL2_mixer;
SDL2_PATH and SDL2_MIXER_PATH can also be specified at configuration time.
For Visual Studio, use the developer terminal and configure an x64 build:

```powershell
cmake -S . -B build-win64 -A x64
cmake --build build-win64 --config Release
```

Copy the matching SDL2.dll and SDL2_mixer.dll beside the resulting executable.
Depending on the generator, it may be under bin/Release: copy the runtime
media there too. In particular MIDI/timidity.cfg and the Tone_000 and Drum_000
folders must remain next to the executable, under MIDI/.
Historical game data and custom media are also required at runtime.
The official 3.0.32 binary was cross-compiled with LLVM-MinGW and tested on
Windows by Gerard. The Visual Studio commands are build guidance, not a claim
that this particular toolchain was tested for this source package.
For libc++ toolchains, explicit pre-includes may be needed:
-include functional -include cfloat -include new.

## Rebuild the Windows installer

The NSIS script in packaging/ targets Windows 10/11 x64 and per-user setup.
Prepare dist/windows/ with the compiled executable, matching DLLs and runtime
resources, then run makensis packaging/installer-3.0.32.nsi with NSIS 3.09+.
The script creates Start menu shortcuts, an optional Desktop shortcut and
an uninstaller. User preferences and scores are preserved on uninstall.

## Validation and supported platforms

Windows 3.0.32 audio fix validated by Gerard. Ubuntu 24.04 runtime validated
with 3.0.31; version 3.0.32 leaves Linux audio unchanged. macOS is not validated.
Archive checked for CRC integrity, source-list completeness, version consistency,
corrected Windows MIDI code and all instrument configuration references.
A fresh end-to-end build from this cleaned archive has not been run here.

## En francais

Cette archive contient les sources completes 3.0.32, les medias propres au
fork et les instruments MIDI Windows. Les ressources historiques du jeu sont
exclues et doivent etre fournies separement pour jouer. Aucune compilation
n'est necessaire pour les installateurs proposes sur la page officielle.
Les commandes ci-dessus expliquent la compilation Ubuntu et Windows.
Le fichier CHANGELOG.TXT contient l'historique, dont le bogue MIDI Windows.
