#!/bin/sh
# Build the installer: needs the three plugin DLLs (build.sh output) copied as x64.dll, x86.dll, arm64.dll
# next to installer.rc, plus LICENSE-GPL2.txt and THIRD-PARTY-NOTICES.txt.
python3 -m ziglang c++ -std=c++17 -target x86_64-windows-gnu -mwindows -Os -w -DUNICODE -D_UNICODE -DNOMINMAX -municode \
  -o TextCompareLab-Npp-Installer.exe installer.cpp installer.rc -lshell32 -luser32 -lole32 -ladvapi32 -Wl,--strip-all
# Logic tests (Linux/any g++): g++ -std=c++17 core_test.cpp -o core_test && ./core_test x64.dll x86.dll arm64.dll
