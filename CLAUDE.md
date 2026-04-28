# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build / Run

```bash
# Configure and build (from project root)
cmake -B build -DCMAKE_PREFIX_PATH="<Qt6 install>/mingw_64" -G "MinGW Makefiles"
cmake --build build

# Release packaging (requires windeployqt on PATH)
build_release.bat
```

Qt 6.10.2, MinGW 64-bit, C++17. The project links Qt::Widgets and Qt::Network.

There are no automated tests or linting configured.

## Architecture

**JMCampusNetLinker** is a Windows desktop app for campus network authentication:

### Tab 1 — Campus network authentication (Eportal)
- `NetworkChecker` — Connectivity probe: HTTP GET to `http://g.cn/generate_204`, expects 204. 10s timeout. Emits `statusChecked(bool)` / `networkError(QString)`.
- `EportalAuth` — Multi-step auth flow against the campus Eportal gateway:
  1. **Login**: GET the probe URL → extract redirect URL for the login page → POST credentials (userId/password/service/queryString) to the `InterFace.do?method=login` endpoint.
  2. **Logout**: GET the logout probe URL → extract `userIndex` from redirect → POST to `InterFace.do?method=logout`.
  - Service types: Edu, Telecom, Unicom, Mobile (mapped from a QComboBox).
  - Constants (probe URL, logout URL, cookie header, user-agent) are private to the `.cpp` file.
- `MainWindow` — QMainWindow that owns everything. Coordinates `NetworkChecker`, `EportalAuth`, and a 60s online-check QTimer. Handles auto-relogin (up to 3 attempts on connection loss), persistent settings via `QSettings` (org/app = `JMCampusNetLinker`), password encryption via Windows DPAPI (calls PowerShell), tray icon minimize-to-tray, and Windows registry autostart. The `ActionState` enum (`None`/`Login`/`AutoRelogin`/`Logout`) drives button enable/disable and re-login gating.

### Entry point
`main.cpp` accepts `--minimized` to start hidden (used for autostart). The app runs in the system tray; closing the window hides to tray — only the "quit" tray action triggers `QApplication::quit()`.

## Commit style
Conventional commits (`feat:`, `fix:`, etc.).

## Notes
- UI is Chinese-language.
- Password encryption uses Windows DPAPI via invoking PowerShell, not a cross-platform crypto library.
