export DISPLAY=:99
DB=/root/.local/share/QEstoqueLoja/estoque.db
cd /tmp
Xvfb :99 -screen 0 1400x850x24 >/dev/null 2>&1 &
sleep 2
openbox >/dev/null 2>&1 &
sleep 1
QESTOQUELOJA_DEV_AUTOLOGIN=1 timeout 8 /build/QEstoqueLoja >/tmp/first.log 2>&1 || true
for n in Nalva "Maria Souza" "Joao Pedro" Ana Carlos Beatriz; do
sqlite3 $DB "insert into operadores (nome,pin_hash,pin_salt,ativo,tentativas_falhas,bloqueado,adicionado_em,atualizado_em,gerente) values ('$n','x','y',1,0,0,datetime('now'),datetime('now'),0);"
done
sqlite3 $DB "update operadores set gerente=1 where nome='Nalva';"
sqlite3 $DB "insert or replace into config_caixa (chave,valor) values ('gerente_pin_hash','x'),('gerente_pin_salt','y');" 2>&1
/build/QEstoqueLoja >/tmp/app.log 2>&1 &
sleep 6
import -window root /out/l_login.png
