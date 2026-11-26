#!/bin/sh
# Rebuild: pip install ziglang, mkdir -p out/x64 out/x86 out/arm64, then:
for t in x86_64:x64 x86:x86 aarch64:arm64; do
  python3 -m ziglang c++ -std=c++17 -target ${t%%:*}-windows-gnu -shared -Os -w -DUNICODE -D_UNICODE -DNOMINMAX \
   -o out/${t##*:}/TextCompareLab.dll src/PluginMain.cpp src/PluginDefinition.cpp src/version.rc -lshell32 -luser32 -lcomdlg32 -lgdi32 -lcomctl32 -Wl,--strip-all
done
# Unit test of the diff engine (Linux/any g++): g++ -std=c++17 -O2 test.cpp -o test && ./test
