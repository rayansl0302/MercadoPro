set -x
DB=/root/.local/share/QEstoqueLoja/estoque.db
cd /tmp
export DISPLAY=:99
Xvfb :99 -screen 0 1400x850x24 &
sleep 2
openbox &
sleep 1
# 1a execução: cria o banco
timeout 8 /build/QEstoqueLoja --pdv >/tmp/first.log 2>&1 || true
sqlite3 $DB "select count(*) from clientes;" 
sqlite3 $DB "delete from produtos; insert into produtos (quantidade,descricao,preco,codigo_barras,nf,un_comercial,preco_fornecedor,porcent_lucro,ncm,cest,aliquota_imposto,csosn,pis,adicionado_em,atualizado_em) values
 (10,'CAFE PILAO 500G',18.90,'789001',0,'UN',10,0,'','',0,'','','2025-01-01 00:00:00','2025-01-01 00:00:00'),
 (25,'ACUCAR REFINADO 1KG',5.49,'789002',0,'UN',3,0,'','',0,'','','2025-01-01 00:00:00','2025-01-01 00:00:00'),
 (0,'CAFE SOLUVEL 100G (SEM ESTOQUE)',12.00,'789003',0,'UN',6,0,'','',0,'','','2025-01-01 00:00:00','2025-01-01 00:00:00'),
 (7,'ARROZ TIPO 1 5KG',24.90,'789004',0,'UN',15,0,'','',0,'','','2025-01-01 00:00:00','2025-01-01 00:00:00');"
/build/QEstoqueLoja --pdv >/tmp/app.log 2>&1 &
sleep 6
shot(){ import -window root /out/$1.png; }
shot 01_inicial
xdotool type --delay 40 "789001"; xdotool key Return; sleep 1
shot 02_bipou_cafe
xdotool type --delay 40 "3*789002"; xdotool key Return; sleep 1
xdotool type --delay 40 "789001"; xdotool key Return; sleep 1
shot 03_soma_e_multiplicador
xdotool type --delay 40 "cafe"; sleep 1
shot 04_busca_nome_estoque_zero
xdotool key Down; sleep 1
shot 05_seta_baixo_catalogo
xdotool key Escape; sleep 1
xdotool key ctrl+a; xdotool type "789004"; xdotool key Return; sleep 1
xdotool key F9; xdotool key minus; sleep 1
shot 06_menos_no_carrinho
xdotool key F10; sleep 1
shot 07_pagamento
xdotool key alt+1; sleep 1; xdotool type --delay 40 "100"; sleep 1
shot 08_troco
xdotool key Return; sleep 3
shot 09_apos_venda
echo ---- DB
sqlite3 $DB "select id,valor_final,forma_pagamento,troco from vendas2; select * from produtos_vendidos; select descricao,quantidade from produtos;"
grep -i "error\|warn\|QObject::" /tmp/app.log | head -20
cat /root/.config/QEstoqueLoja/config.ini | grep -A3 pdv
