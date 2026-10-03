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
insert into produtos (quantidade,descricao,preco,codigo_barras,nf,un_comercial,preco_fornecedor,porcent_lucro,ncm,cest,aliquota_imposto,csosn,pis,adicionado_em,atualizado_em) values
 (10,'CAFE PILAO 500G',18.90,'789001',0,'UN',10,0,'','',0,'','','2025-01-01 00:00:00','2025-01-01 00:00:00');
insert into clientes (nome, adicionado_em, atualizado_em) values ('Maria da Silva', datetime('now'), datetime('now'));
insert into caixas (id_operador,terminal,status,aberto_em,troco_inicial,troco_sugerido) values (1,'$(hostname | tr a-z A-Z)','ABERTO',datetime('now'),50,50);
insert into vendas2 (cliente,total,data_hora,forma_pagamento,valor_recebido,troco,taxa,valor_final,desconto,id_cliente,esta_pago,id_caixa,id_empresa) values ('Maria da Silva',37.8,datetime('now'),'Pix',37.8,0,0,37.8,0,1,1,1,1);
insert into produtos_vendidos (id_produto,id_venda,quantidade,preco_vendido,adicionado_em,atualizado_em,emitido_nf) values (1,1,2,18.9,datetime('now'),datetime('now'),0);" 2>&1 | head -3
for t in $TELAS; do
  /build/QEstoqueLoja --preview-caixa $t >/tmp/app_$t.log 2>&1 &
  sleep 5
  import -window root /out/t_$t.png
  pkill QEstoqueLoja; sleep 1
done
