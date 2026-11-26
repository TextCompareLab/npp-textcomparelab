# TextCompareLab for Notepad++

Compare two open tabs in Notepad++ line by line. Free, open source (GPL v2 or later). Works locally; no text leaves your computer.

**Status: beta. Version 1.1.3 has not been tested inside Notepad++ yet** (new dialogs, menu items and file export). An earlier version loaded and drew highlights. The diff and patch engine is tested separately on Linux.

## Features
- Mark a tab, then compare another tab with it, or use "Choose two tabs to compare..."
- Four toolbar buttons and default keys Ctrl+Alt+T (choose tabs), Ctrl+Alt+Down / Ctrl+Alt+Up (next / previous difference), Ctrl+Alt+P (Export: asks patch or HTML), changeable in the Shortcut Mapper; they avoid Notepad++ built-in keys and ComparePlus defaults, but other plugins or user remaps may conflict
- Exports open automatically for viewing (patch in a tab, HTML in the browser); nothing is applied or run
- Red/green/yellow line highlights, orange boxes on changed words
- Next / previous difference, ignore spaces, clear comparison
- Export a unified patch (`git apply`, `patch -p1`). Only saved when both tabs are UTF-8 or plain ASCII (tested with Hebrew, emoji, CRLF, mixed line endings). For UTF-16 or non-ASCII ANSI tabs nothing is saved and a message explains why.
- Export a self-contained HTML report

## Install
Installer option: TextCompareLab-Npp-Installer-1.1.3.exe from the release. **The installer has never been run on Windows (completely untested)**; the ZIP below is the safer route. It is unsigned (check the SHA-256), you must close Notepad++ first (it never closes it for you), and it can also uninstall. Install by hand from the ZIP:

1. Close Notepad++. Download the zip for your build (x64, x86, ARM64). Each zip holds TextCompareLab.dll, README.txt, LICENSE-GPL2.txt and THIRD-PARTY-NOTICES.txt.
2. Check the SHA-256 against the release notes. If it differs, do not use the file.
3. Put `TextCompareLab.dll` in `...\Notepad++\plugins\TextCompareLab\`.
4. Start Notepad++, open Plugins > TextCompareLab.

The DLL is not code-signed, so Windows may warn about it.

## Build
`build.sh` (Zig cross-compile) and `validate.sh` build and test the plugin; `installer/` holds the installer source (`build_installer.sh`, tests in `core_test.cpp`).

## Source and licenses
This repository holds the complete source of what the release binaries are built from (`src/`, `build.sh`, tests). `LICENSE-GPL2.txt` is the GPL v2 text; you may use it under GPL v2 or any later version. `THIRD-PARTY-NOTICES.txt` carries the full texts of the licenses for the bundled headers (Notepad++ plugin template, Scintilla) and for the runtime libraries linked into the binaries by the Zig toolchain (LLVM libc++, libc++abi, libunwind, mingw-w64, Zig compiler-rt); the same files are in `LICENSES/`.

## License
GPL v2 or later. Built on the Notepad++ plugin template. See LICENSE-GPL2.txt and THIRD-PARTY-NOTICES.txt.
Copyright TextCompareLab project. Website page: https://textcomparelab.com/notepad-plugin
