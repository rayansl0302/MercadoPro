export DISPLAY=:99
DB=/root/.local/share/QEstoqueLoja/estoque.db
CFG=/root/.local/share/QEstoqueLoja/config.ini
cd /tmp
Xvfb :99 -screen 0 1400x850x24 >/dev/null 2>&1 &
sleep 2
openbox >/dev/null 2>&1 &
sleep 1
export QESTOQUELOJA_DEV_AUTOLOGIN=1
timeout 8 /build/QEstoqueLoja >/tmp/first.log 2>&1 || true
sqlite3 $DB "
insert into operadores (nome,pin_hash,pin_salt,ativo,tentativas_falhas,bloqueado,adicionado_em,atualizado_em,gerente) values ('Nalva','x','y',1,0,0,datetime('now'),datetime('now'),1);
update empresas set apelido='Emassa Alimentos', cnpj='69133888000137', razao_social='Emassa Comercial de Alimentos' where id=1;
insert into empresas (apelido,cnpj,razao_social,ativa,criado_em) values ('Studio Bela','11222333000181','Studio Bela LTDA',1,datetime('now'));
insert into contas_pagar (id_empresa,descricao,fornecedor,categoria,documento,valor,vencimento,status,parcela,total_parcelas,criado_em) values
 (1,'Compra de mercadoria','Distribuidora Sul','Fornecedor / mercadoria','NF 1234',1250.5,date('now','-4 day'),'ABERTA',1,1,datetime('now')),
 (1,'Aluguel','Imobiliaria Central','Aluguel','',2800,date('now'),'ABERTA',1,1,datetime('now')),
 (1,'Energia','Concessionaria','Energia','',430.9,date('now','+3 day'),'ABERTA',1,1,datetime('now')),
 (1,'Fornecedor X parcela','Fornecedor X','Fornecedor / mercadoria','',500,date('now','+30 day'),'ABERTA',1,3,datetime('now')),
 (1,'Internet','Provedor','Internet e telefone','',150,date('now','-10 day'),'PAGA',1,1,datetime('now')),
 (2,'Produtos de beleza','Cosmeticos SA','Fornecedor / mercadoria','',900,date('now','-1 day'),'ABERTA',1,1,datetime('now'));
update contas_pagar set valor_pago=150, forma_pagamento='Pix', pago_em=datetime('now','-9 day') where status='PAGA';"
/build/QEstoqueLoja >/tmp/app.log 2>&1 &
sleep 6
import -window root /out/e_home.png
pkill QEstoqueLoja; sleep 1
/build/QEstoqueLoja --preview-caixa contas >/tmp/app2.log 2>&1 &
sleep 5
import -window root /out/e_contas.png
pkill QEstoqueLoja; sleep 1
/build/QEstoqueLoja --preview-caixa empresa >/tmp/app3.log 2>&1 &
sleep 5
import -window root /out/e_empresa.png
