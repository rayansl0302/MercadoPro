export DISPLAY=:99
cd /tmp
Xvfb :99 -screen 0 1400x850x24 >/dev/null 2>&1 &
sleep 2
openbox >/dev/null 2>&1 &
sleep 1
export QESTOQUELOJA_DEV_AUTOLOGIN=1
timeout 8 /build/QEstoqueLoja >/tmp/first.log 2>&1 || true
/build/QEstoqueLoja --preview-caixa produto >/tmp/app.log 2>&1 &
sleep 5
import -window root /out/p_principal.png
xdotool mousemove 600 300
xdotool key ctrl+Tab
sleep 1
import -window root /out/p_imposto.png
