export DISPLAY=:99
DB=/root/.local/share/QEstoqueLoja/estoque.db
cd /tmp
Xvfb :99 -screen 0 1500x900x24 >/dev/null 2>&1 &
sleep 2
openbox >/dev/null 2>&1 &
sleep 1
export QESTOQUELOJA_DEV_AUTOLOGIN=1
timeout 8 /build/QEstoqueLoja >/tmp/first.log 2>&1 || true
sqlite3 $DB "
insert into operadores (nome,pin_hash,pin_salt,ativo,tentativas_falhas,bloqueado,adicionado_em,atualizado_em,gerente) values ('Nalva','x','y',1,0,0,datetime('now'),datetime('now'),1);
insert into caixas (id_operador,terminal,status,aberto_em,troco_inicial,troco_sugerido) values (1,'$(hostname | tr a-z A-Z)','ABERTO',datetime('now'),50,50);
insert into clientes (nome,telefone,whatsapp,limite_credito,adicionado_em,atualizado_em) values ('Maria da Silva','71988887777','71988887777',500,datetime('now'),datetime('now')),('João Pereira','7133334444',NULL,NULL,datetime('now'),datetime('now')),('Ana Souza',NULL,NULL,200,datetime('now'),datetime('now'));
insert into dividas (id_cliente,id_empresa,data_compra,descricao,valor_total,status,id_operador_sessao,criado_em) values
 (2,1,date('now','-20 day'),'Compras do mês',320.5,'ABERTA',1,datetime('now')),
 (2,1,date('now','-3 day'),'2 sacos de arroz',90,'ABERTA',1,datetime('now')),
 (4,1,date('now','-9 day'),'Caderneta',260,'ABERTA',1,datetime('now'));
insert into pagamentos_divida (id_divida,data_pagamento,valor,forma_pagamento,id_operador_sessao,id_caixa,cancelado,criado_em) values (1,datetime('now','-10 day'),100,'Pix',1,1,0,datetime('now'));
insert into vendas2 (cliente,total,data_hora,forma_pagamento,valor_recebido,troco,taxa,valor_final,desconto,id_cliente,esta_pago,id_caixa,id_empresa) values ('Maria da Silva',180,datetime('now','-15 day'),'Prazo',0,0,0,180,0,3,0,1,1);
" 2>&1 | head -5
sqlite3 $DB "select id,nome from clientes;"
/build/QEstoqueLoja --preview-caixa receber >/tmp/app.log 2>&1 &
sleep 5
import -window root /out/r_lista.png
xdotool mousemove 400 357 click 1 click 1
sleep 1
sleep 2
import -window root /out/r_extrato.png
