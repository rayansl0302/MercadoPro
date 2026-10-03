set -e
mkdir -p /usr/include/QtRPT && cp /stub/qtrpt.h /usr/include/QtRPT/
mkdir -p /tmp/qs && cd /tmp/qs
g++ -fPIC -shared -std=c++17 $(pkg-config --cflags Qt6Core Qt6Gui 2>/dev/null || echo "-I/usr/include/x86_64-linux-gnu/qt6 -I/usr/include/x86_64-linux-gnu/qt6/QtCore -I/usr/include/x86_64-linux-gnu/qt6/QtGui") \
  -I/stub /stub/qtrpt.cpp -o /usr/local/lib/libQtRPT.so
# moc for the stub
/usr/lib/qt6/libexec/moc /stub/qtrpt.h -o /tmp/qs/moc_qtrpt.cpp
g++ -fPIC -shared -std=c++17 -I/usr/include/x86_64-linux-gnu/qt6 -I/usr/include/x86_64-linux-gnu/qt6/QtCore -I/usr/include/x86_64-linux-gnu/qt6/QtGui -I/stub /stub/qtrpt.cpp /tmp/qs/moc_qtrpt.cpp -o /usr/local/lib/libQtRPT.so
ldconfig
cmake -S /src -B /build -G Ninja -DCMAKE_CXX_FLAGS="-include /stub/fix.h" > /tmp/cfg.log 2>&1 || (tail -20 /tmp/cfg.log; exit 1)
cd /build && ninja -k 0 QEstoqueLoja QEstoqueLojaTests 2>&1 | grep -E "error|FAILED|undefined|ninja:" | head -40
ls -la /build/QEstoqueLoja /build/tests/QEstoqueLojaTests 2>&1
