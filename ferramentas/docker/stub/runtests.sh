cd /build
# 1a configuracao sem o cabecalho forcado (o teste do compilador nao acha o Qt), depois com ele
[ -f /build/CMakeCache.txt ] || cmake -S /src -B /build -G Ninja > /tmp/cfg0.log 2>&1
cmake -S /src -B /build -G Ninja -DCMAKE_CXX_FLAGS="-include /stub/fix.h" > /tmp/cfg.log 2>&1 || { tail -20 /tmp/cfg.log; exit 1; }
ninja -k 0 QEstoqueLoja QEstoqueLojaTests 2>&1 | grep -E "error|FAILED|undefined|warning: unused" | head -40
cd /build/tests && QT_QPA_PLATFORM=offscreen timeout 280 ./QEstoqueLojaTests 2>&1 | grep -E "^(FAIL|Totals|   Actual|   Expected|   Loc)" 
