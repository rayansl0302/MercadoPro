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
insert into operadores (nome,pin_hash,pin_salt,ativo,tentativas_falhas,bloqueado,adicionado_em,atualizado_em,gerente) values ('Nalva','x','y',1,0,0,datetime('now','localtime'),datetime('now','localtime'),1),('Maria','x','y',1,0,0,datetime('now','localtime'),datetime('now','localtime'),0);
insert into caixas (id_operador,terminal,status,aberto_em,troco_inicial,troco_sugerido) values (2,'CAIXA-1','ABERTO',datetime('now','localtime','-3 hour'),50,50);
insert into clientes (nome,telefone,whatsapp,limite_credito,adicionado_em,atualizado_em) values ('Maria da Silva','71988887777','71988887777',200,datetime('now','localtime'),datetime('now','localtime')),('João Pereira','7133334444',NULL,NULL,datetime('now','localtime'),datetime('now','localtime'));
insert into dividas (id_cliente,id_empresa,data_compra,descricao,valor_total,status,id_operador_sessao,criado_em) values
 (2,1,date('now','localtime','-20 day'),'Compras do mês',320.5,'ABERTA',1,datetime('now','localtime')),
 (2,1,date('now','localtime','-3 day'),'2 sacos de arroz',90,'ABERTA',1,datetime('now','localtime'));
insert into pagamentos_divida (id_divida,data_pagamento,valor,forma_pagamento,id_operador_sessao,id_caixa,cancelado,criado_em) values (1,datetime('now','localtime','-10 day'),100,'Pix',1,1,0,datetime('now','localtime'));
insert into vendas2 (cliente,total,data_hora,forma_pagamento,valor_recebido,troco,taxa,valor_final,desconto,esta_pago,id_caixa,id_empresa,id_operador_sessao) values
 ('',100,datetime('now','localtime','-2 hour'),'Pix',100,0,0,100,0,1,1,1,2),
 ('',50,datetime('now','localtime','-1 hour'),'Dinheiro',50,0,0,50,0,1,1,1,2),
 ('',70,datetime('now','localtime','-1 day'),'Cartão',70,0,0,70,0,1,1,1,1),
 ('',230,datetime('now','localtime','-3 day'),'Pix',230,0,0,230,0,1,1,1,2);
insert into contas_pagar (id_empresa,descricao,fornecedor,categoria,valor,vencimento,status,parcela,total_parcelas,criado_em) values
 (1,'Mercadoria','Distribuidora Norte','Fornecedor',1250.5,date('now','localtime','-4 day'),'ABERTA',1,1,datetime('now','localtime')),
 (1,'Aluguel','Imobiliária','Aluguel',2800,date('now','localtime'),'ABERTA',1,1,datetime('now','localtime')),
 (1,'Energia','Coelba','Energia',430.9,date('now','localtime','+3 day'),'ABERTA',1,1,datetime('now','localtime')),
 (1,'Internet','Provedor','Internet',150,date('now','localtime','-10 day'),'PAGA',1,1,datetime('now','localtime'));
" 2>&1 | head -5
CFG=$(find /root -name config.ini 2>/dev/null | head -1)
[ -z "$CFG" ] && { mkdir -p /root/.config/QEstoqueLoja; CFG=/root/.config/QEstoqueLoja/config.ini; }
echo "config em $CFG"
cat >> $CFG <<EOF

[nuvem]
ativa=true
url=http://host.docker.internal:3000
token=$TOKEN_LOJA
intervalo_min=15
sincroniza_neste_computador=true
EOF
/build/QEstoqueLoja >/tmp/app.log 2>&1 &
sleep 40
import -window root /out/n_tela.png
tail -5 /tmp/app.log
# fica aberto para aplicar pedidos do painel
sleep ${FICAR:-600}
