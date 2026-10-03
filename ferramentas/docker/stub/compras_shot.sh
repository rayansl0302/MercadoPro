export DISPLAY=:99
Xvfb :99 -screen 0 1400x850x24 >/dev/null 2>&1 &
sleep 2
openbox >/dev/null 2>&1 &
sleep 1
timeout 8 /build/QEstoqueLoja >/tmp/first.log 2>&1 || true
/build/QEstoqueLoja >/tmp/app.log 2>&1 &
sleep 6
xdotool mousemove 1230 169 click 1
sleep 2
import -window root /out/c02_compras.png
