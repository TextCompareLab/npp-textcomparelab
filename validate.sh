#!/bin/sh
# Validates the patch exporter against git apply and patch on generated fixtures.
# Needs g++, git, patch. Usage: sh validate.sh   (from the source folder)
g++ -std=c++17 -O2 -o patchgen patchgen.cpp && g++ -std=c++17 -O2 -o test test.cpp && ./test
python3 gen_fixtures.py
ok=0; fail=0
for a in fx/*.a; do n=$(basename "$a" .a); b=fx/$n.b
  ./patchgen "$a" "$b" file.txt file.txt "fx/$n.patch"; r=$?
  [ $r = 1 ] && continue
  [ $r != 0 ] && { echo "gen fail $n"; fail=$((fail+1)); continue; }
  d=$(mktemp -d); cp "$a" "$d/file.txt"; P=$(pwd)
  (cd "$d" && git init -q . && git apply --check "$P/fx/$n.patch" && git apply "$P/fx/$n.patch"); cmp -s "$d/file.txt" "$b" && g=1 || g=0
  d2=$(mktemp -d); cp "$a" "$d2/file.txt"; (cd "$d2" && patch -p1 -s < "$P/fx/$n.patch"); cmp -s "$d2/file.txt" "$b" && p=1 || p=0
  if [ $g = 1 ] && [ $p = 1 ]; then ok=$((ok+1)); else echo "FAIL $n git=$g patch=$p"; fail=$((fail+1)); fi
done
echo "patch fixtures ok=$ok fail=$fail"
