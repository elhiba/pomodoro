<div align="center">

# Pomodoro

**A focus timer with a lo-fi stream, session statistics and native desktop integration.**

Built with C++17 and Qt 6 (Qt Quick / QML). One binary, no runtime dependencies beyond Qt.

![Pomodoro](docs/screenshot.png)

</div>

## Features

- **Pomodoro timer** — focus, short break and long break sessions with configurable
  lengths, a configurable number of rounds before a long break, and optional automatic
  start of the next session. Drift-free: the clock is derived from a monotonic timer,
  not counted down tick by tick.
- **Lo-fi background stream** — play an internet radio stream while you work. If the
  connection drops it reconnects on its own with exponential backoff, and picks straight
  back up when the network returns. The station and current track are shown live next to
  the play button, read from the stream's ICY metadata.
- **OS media controls** — the stream shows up as a real media source in the system:
  Windows **System Media Transport Controls**, **MPRIS** on Linux (GNOME, KDE,
  `playerctl`), and the **Now Playing** centre on macOS. Play, pause and the keyboard's
  media keys all work.
- **Session statistics** — focus minutes and pomodoros today, focus time over the last
  seven days, an all-time total, a daily streak and a seven-day chart. Stored locally.
- **Desktop notifications** — a notification and an alarm sound when a session ends.
- **System tray** — close hides the app to the tray with the timer still running; the
  tray menu and icon bring it back. A single instance is enforced.
- **Custom frameless window** — a compact, resizable landscape layout with a colour that
  follows the current mode.

## Download

Grab the latest build for your platform from the
[**Releases**](../../releases/latest) page:

| Platform | File |
| --- | --- |
| Linux | `pomodoro-*-x86_64.AppImage` — `chmod +x` it and run |
| Windows | `pomodoro-*-windows-x64.zip` — unzip and run `pomodoro.exe` |
| macOS | `pomodoro-*-macos.dmg` — open and drag to Applications |

On macOS the app is unsigned, so the first launch needs right-click → Open. On Linux the
AppImage needs FUSE; most desktops have it, otherwise run it with `--appimage-extract-and-run`.

## Keyboard shortcuts

| Key | Action |
| --- | --- |
| `Space` | Start / pause the timer |
| `R` | Reset the current session |
| `S` | Skip to the next session |
| `Ctrl` + `,` | Open settings |
| `Esc` | Close the open drawer |
| `Ctrl` + `Q` | Quit |

## Building from source

Requirements: CMake ≥ 3.21, Ninja, and Qt ≥ 6.5 with the Core, Gui, Qml, Quick,
QuickControls2, Multimedia, Network, Svg, DBus and Widgets modules.

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/pomodoro          # pomodoro.exe on Windows
```

To build and run the unit tests:

```bash
cmake -S . -B build -G Ninja -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

### Releasing

Continuous integration builds and tests every push on Linux, Windows and macOS. Pushing
a version tag builds the packages and publishes them on a GitHub Release:

```bash
git tag v1.2.3
git push origin v1.2.3
```

## Tech stack

C++17 · Qt 6 (Qt Quick / QML, Multimedia, Network, DBus, Widgets) · CMake · GitHub Actions.

The UI is written in QML; all logic lives in C++ types exposed to QML. Platform
integration is handled natively per OS — SMTC via the WinRT ABI on Windows, MPRIS over
D-Bus on Linux, and the Media Player framework on macOS.
