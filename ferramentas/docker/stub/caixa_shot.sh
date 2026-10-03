export DISPLAY=:99
DB=/root/.local/share/QEstoqueLoja/estoque.db
Xvfb :99 -screen 0 1400x850x24 >/dev/null 2>&1 &
sleep 2
openbox >/dev/null 2>&1 &
sleep 1
# 1a execucao: cria o banco e migra ate a versao atual
timeout 8 /build/QEstoqueLoja >/tmp/first.log 2>&1 || true
echo "schema: $(sqlite3 $DB 'PRAGMA user_version;')"
sqlite3 $DB ".indexes caixas"
TERM_NAME=$(hostname | tr a-z A-Z)
sqlite3 $DB "
insert into operadores (nome,pin_hash,pin_salt,ativo,tentativas_falhas,bloqueado,adicionado_em,atualizado_em) values ('Maria Souza','x','y',1,0,0,datetime('now'),datetime('now'));
insert into caixas (id_operador,terminal,status,aberto_em,troco_inicial,troco_sugerido) values (1,'$TERM_NAME','ABERTO',datetime('now'),50,50);
insert into caixas (id_operador,terminal,status,aberto_em,fechado_em,troco_inicial,troco_sugerido) values (1,'BALCAO-2','FECHADO',datetime('now','-1 day'),datetime('now','-1 day','+8 hours'),20,20);
insert into vendas2 (cliente,total,data_hora,forma_pagamento,valor_recebido,troco,taxa,valor_final,desconto,id_cliente,esta_pago,id_caixa) values
 ('T',40,'2026-09-30 10:00:00','Dinheiro',40,0,0,40,0,1,1,1),
 ('T',25,'2026-09-30 10:05:00','Pix',25,0,0,25,0,1,1,1),
 ('T',7,'2026-09-30 10:10:00','Não Sei',7,0,0,7,0,1,1,1);"
/build/QEstoqueLoja --preview-caixa fechar >/tmp/app.log 2>&1 &
sleep 5
import -window root /out/k01_fechar_vazio.png
xdotool type --delay 40 "40"
xdotool key Tab
xdotool type --delay 40 "0"
sleep 1
import -window root /out/k02_fechar_digitando.png
pkill QEstoqueLoja; sleep 1
/build/QEstoqueLoja --preview-caixa historico >/tmp/app2.log 2>&1 &
sleep 5
import -window root /out/k03_historico.png
xdotool mousemove 500 300 click 3
sleep 1
import -window root /out/k04_historico_menu.png
