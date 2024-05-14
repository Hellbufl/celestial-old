<h1 align="center">🌌 celestial ✨</h1>
<h3 align="center">A path timing tool for NieR: Automata speedrunning</h3>

It's probably still very buggy so please report any weird behavior in the [Issues](https://github.com/Hellbufl/celestial/issues) section or on Discord (hellbufl) \
\
A big big thank you to [Woeful Wolf](https://github.com/WoefulWolf/) for helping me with the hooking, rendering and just generally getting me started on this!

# Features
- Precice Timing of routes
- Automatic comparison and sorting of times
- 3D visualization with depth-buffer rendering.

# Planned Features
- customizing trigger size and color stuff
- custom category tabs to organize comparisons
- live timer onscreen
- highlighting latest run

If you have an idea for a feature please feel free to message me on discord!\
I'm also happy to receive pull requests :)

# Installation
Warning: This tool might not yet be compatible with other mods using ImGUI like FAR or NARoIP (which is outdated, go check out [Kajiki](https://github.com/WoefulWolf/kajiki-mod)!).
1. Download the [latest release](https://github.com/Hellbufl/celestial/releases)
2. Extract it in your game folder next to NieRAutomata.exe
3. Rename "celestial.dll" to one of the [Supported Files](#supported-files)

# Supported Files
| Game          | Working Proxies                           |
| ---           | ---                                       |
| NieR:Automata | `dxgi.dll`, `d3d11.dll`, `dinput8.dll`    |

# Usage
- Toggle the menu with the default keybind `HOME` (look up the other keybinds in the Config tab)

### Comparisons (default)
- Place a start and an end trigger
- Start recording a path by leaving the start trigger and finish by entering the end trigger
- Highlight a path by clicking on the time
- Use the buttons labeled "S" ("Solo") and "M" ("Mute") to only show / not show the selected path


After multiple recordings with the same set of triggers, the different paths will be sorted from fastest to slowest and colored on a gradient (default: green -> red) with the fastest being highlighted (default: gold).

Comparisons are stored as `.pcomp` files in `/Paths` in your game directory.

### Separate Paths
- Activate "Direct Mode" in the Config tab
- The "Create Trigger" keybinds now directly start and end the recording respectively

Separate Paths are stored as `.p` files in `/Paths` in your game directory.

# Known Issues
Unwanted behaviour I am aware of and that I will work on:
- The game freezes with a black screen on startup if the mouse curser is where the ImGUI window would be
- The screen freezes when changing window settings like resolution
