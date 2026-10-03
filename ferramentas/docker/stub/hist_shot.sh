export DISPLAY=:99
DB=/root/.local/share/QEstoqueLoja/estoque.db
cd /tmp
Xvfb :99 -screen 0 1400x850x24 >/dev/null 2>&1 &
sleep 2
openbox >/dev/null 2>&1 &
sleep 1
export QESTOQUELOJA_DEV_AUTOLOGIN=1
timeout 8 /build/QEstoqueLoja >/tmp/first.log 2>&1 || true
TERM_NAME=$(hostname | tr a-z A-Z)
sqlite3 $DB "
insert into operadores (nome,pin_hash,pin_salt,ativo,tentativas_falhas,bloqueado,adicionado_em,atualizado_em) values ('Maria Souza','x','y',1,0,0,datetime('now'),datetime('now'));
insert into caixas (id_operador,terminal,status,aberto_em,troco_inicial,troco_sugerido) values (1,'$TERM_NAME','ABERTO',datetime('now'),50,50);
insert into caixas (id,id_operador,terminal,status,aberto_em,fechado_em,troco_inicial,troco_sugerido) values (50,1,'BALCAO-2','FECHADO',datetime('now','-1 day'),datetime('now','-1 day','+8 hours'),20,20);
insert into caixas (id,id_operador,terminal,status,aberto_em,fechado_em,troco_inicial,troco_sugerido) values (51,1,'BALCAO-3','FECHADO',datetime('now','-2 day'),datetime('now','-2 day','+8 hours'),20,20);
insert into caixas (id,id_operador,terminal,status,aberto_em,fechado_em,troco_inicial,troco_sugerido) values (52,1,'BALCAO-4','FECHADO',datetime('now','-3 day'),datetime('now','-3 day','+8 hours'),20,20);
insert into fechamentos_caixa (id_caixa,forma_pagamento,valor_esperado,valor_informado,diferenca,ocorrencia_tecnica) values (50,'Dinheiro',100,105.5,5.5,0),(51,'Dinheiro',100,92,-8,0),(52,'Dinheiro',80,80,0,0);"
sqlite3 $DB "update operadores set gerente=1;"
/build/QEstoqueLoja >/tmp/app.log 2>&1 &
sleep 6
import -window root /out/h_home.png
pkill QEstoqueLoja; sleep 1
/build/QEstoqueLoja --preview-caixa historico >/tmp/app2.log 2>&1 &
sleep 5
import -window root /out/h_hist.png
