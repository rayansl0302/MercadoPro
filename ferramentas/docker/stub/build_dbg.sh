cd /build
[ -f /build/CMakeCache.txt ] || cmake -S /src -B /build -G Ninja > /tmp/cfg0.log 2>&1
cmake -S /src -B /build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-include /stub/fix.h" > /tmp/cfg.log 2>&1 || { tail -20 /tmp/cfg.log; exit 1; }
ninja -k 0 QEstoqueLoja 2>&1 | grep -E "error|FAILED|undefined" | head -40
echo built
