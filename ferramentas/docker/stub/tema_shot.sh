export DISPLAY=:99
DB=/root/.local/share/QEstoqueLoja/estoque.db
cd /tmp
Xvfb :99 -screen 0 1400x850x24 >/dev/null 2>&1 &
sleep 2
openbox >/dev/null 2>&1 &
sleep 1
export QESTOQUELOJA_DEV_AUTOLOGIN=1
timeout 8 /build/QEstoqueLoja >/tmp/first.log 2>&1 || true
sqlite3 $DB "delete from produtos; insert into produtos (quantidade,descricao,preco,codigo_barras,nf,un_comercial,preco_fornecedor,porcent_lucro,ncm,cest,aliquota_imposto,csosn,pis,adicionado_em,atualizado_em) values
 (10,'CAFE PILAO 500G',18.90,'789001',0,'UN',10,0,'','',0,'','','2025-01-01 00:00:00','2025-01-01 00:00:00'),
 (25,'ACUCAR REFINADO 1KG',5.49,'789002',0,'UN',3,0,'','',0,'','','2025-01-01 00:00:00','2025-01-01 00:00:00'),
 (0,'CAFE SOLUVEL 100G',12.00,'789003',0,'UN',6,0,'','',0,'','','2025-01-01 00:00:00','2025-01-01 00:00:00'),
 (7,'ARROZ TIPO 1 5KG',24.90,'789004',0,'UN',15,0,'','',0,'','','2025-01-01 00:00:00','2025-01-01 00:00:00');"
sqlite3 $DB "select count(*) from operadores;"
/build/QEstoqueLoja >/tmp/app.log 2>&1 &
sleep 6
import -window root /out/${1:-t}_main.png
xdotool key F5; sleep 3
import -window root /out/${1:-t}_pdv.png
xdotool key Escape; sleep 1
pkill QEstoqueLoja; sleep 1
/build/QEstoqueLoja --preview-caixa abrir >/tmp/app2.log 2>&1 &
sleep 4
import -window root /out/${1:-t}_abrir.png
