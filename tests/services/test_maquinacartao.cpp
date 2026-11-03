#include <QtTest>
#include <QSqlQuery>
#include "test_maquinacartao.h"
#include "infra/databaseconnection_service.h"
#include "repository/relatorios_repository.h"
#include "repository/vendas_repository.h"
#include "services/Produto_service.h"
#include "services/maquinacartao_service.h"
#include "dto/Vendas_dto.h"

namespace {
MaquinaCartaoDTO maquina(const QString &nome, double deb, double cred)
{
    MaquinaCartaoDTO m;
    m.nome = nome;
    m.taxaDebito = deb;
    m.taxaCredito = cred;
    return m;
}

VendasDTO venda(const QString &forma, double total, double valorFinal, double taxa, qlonglong idMaquina, double taxaMaquina,
                const QString &dataHora = "2026-03-10 10:00:00")
{
    VendasDTO v;
    v.clienteNome = "Teste";
    v.dataHora = dataHora;
    v.total = total;
    v.formaPagamento = forma;
    v.valorFinal = valorFinal;
    v.valorRecebido = valorFinal;
    v.taxa = taxa;
    v.idMaquina = idMaquina;
    v.taxaMaquina = taxaMaquina;
    v.estaPago = true;
    v.idCliente = 1;
    return v;
}

QVariantMap linhaDaVenda(qlonglong id)
{
    QVariantMap r;
    DatabaseConnection_service::open();
    QSqlQuery q(DatabaseConnection_service::db());
    q.prepare("SELECT valor_final, taxa, id_maquina, taxa_maquina FROM vendas2 WHERE id = :id");
    q.bindValue(":id", id);
    if (q.exec() && q.next()) {
        r["valor_final"] = q.value(0);
        r["taxa"] = q.value(1);
        r["id_maquina"] = q.value(2);
        r["taxa_maquina"] = q.value(3);
    }
    return r;
}

void produtoVendido(qlonglong idVenda, qlonglong idProduto, double qtd, double preco)
{
    QSqlQuery q(DatabaseConnection_service::db());
    q.prepare("INSERT INTO produtos_vendidos (id_produto, id_venda, quantidade, preco_vendido) VALUES (:p, :v, :q, :pr)");
    q.bindValue(":p", idProduto);
    q.bindValue(":v", idVenda);
    q.bindValue(":q", qtd);
    q.bindValue(":pr", preco);
    q.exec();
}
}

void TestMaquinaCartao::cadastro_valida_nome_taxas_e_nome_repetido()
{
    MaquinaCartao_service s;
    QVERIFY(!s.salvar(maquina("   ", 1, 2)).ok);
    QVERIFY(!s.salvar(maquina("Stone", -1, 2)).ok);
    QVERIFY(!s.salvar(maquina("Stone", 1, 100.5)).ok);

    const auto r = s.salvar(maquina("Stone", 1.99, 3.5));
    QVERIFY2(r.ok, qPrintable(r.msg));
    QVERIFY(r.id > 0);
    const MaquinaCartaoDTO lida = s.get(r.id);
    QCOMPARE(lida.nome, QString("Stone"));
    QCOMPARE(lida.taxaDebito, 1.99);
    QCOMPARE(lida.taxaCredito, 3.5);
    QVERIFY(lida.ativa);

    QVERIFY(!s.salvar(maquina(" stone ", 1, 1)).ok);                // mesmo nome (sem diferenciar maiúsculas)
    MaquinaCartaoDTO alterada = lida;                               // alterar a própria máquina com o mesmo nome pode
    alterada.taxaCredito = 4.1;
    QVERIFY(s.salvar(alterada).ok);
    QCOMPARE(s.get(r.id).taxaCredito, 4.1);
    QCOMPARE(lida.taxaDaForma("Débito"), 1.99);
    QCOMPARE(lida.taxaDaForma("Crédito"), 3.5);
}

void TestMaquinaCartao::desativar_tira_da_lista_de_ativas_e_reativar_volta()
{
    MaquinaCartao_service s;
    const auto r = s.salvar(maquina("Cielo", 1.5, 2.9));
    QVERIFY(r.ok);
    auto contem = [&](bool inativas) {
        for (const auto &m : s.listar(inativas))
            if (m.id == r.id)
                return true;
        return false;
    };
    QVERIFY(contem(false));
    QVERIFY(s.definirAtiva(r.id, false).ok);
    QVERIFY(!contem(false));
    QVERIFY(contem(true));
    QVERIFY(s.definirAtiva(r.id, true).ok);
    QVERIFY(contem(false));
    QVERIFY(!s.definirAtiva(999999, false).ok);
}

void TestMaquinaCartao::venda_no_cartao_grava_maquina_e_taxa_sem_mexer_no_valor_final()
{
    MaquinaCartao_service s;
    const auto m = s.salvar(maquina("PagBank", 1.0, 3.0));
    QVERIFY(m.ok);
    Vendas_repository repo;
    const qlonglong id = repo.inserir(venda("Crédito", 50.0, 50.0, 0, m.id, 3.0));
    QVERIFY(id > 0);
    const QVariantMap l = linhaDaVenda(id);
    QCOMPARE(l["valor_final"].toDouble(), 50.0);          // o cliente paga o preço de gôndola
    QCOMPARE(l["taxa"].toDouble(), 0.0);                   // sem acréscimo
    QCOMPARE(l["id_maquina"].toLongLong(), m.id);
    QCOMPARE(l["taxa_maquina"].toDouble(), 3.0);
}

void TestMaquinaCartao::venda_fora_do_cartao_nao_grava_taxa_de_maquina()
{
    Vendas_repository repo;
    const qlonglong id = repo.inserir(venda("Dinheiro", 20.0, 20.0, 0, 5, 3.0));   // mesmo com lixo no DTO
    QVERIFY(id > 0);
    const QVariantMap l = linhaDaVenda(id);
    QVERIFY(l["id_maquina"].isNull());
    QVERIFY(l["taxa_maquina"].isNull());
}

void TestMaquinaCartao::lucro_da_venda_desconta_a_taxa_da_maquina_e_mantem_o_legado()
{
    // produto com preco 10 e porcent_lucro 50 no relatório -> custo 5; 2 un. a 10 = lucro bruto 10
    Produto_Service ps;
    ProdutoDTO p;
    p.quantidade = 100; p.descricao = "LUCRO TESTE"; p.preco = 10.0; p.codigoBarras = "7898200000014";
    p.nf = false; p.uCom = "UN"; p.precoFornecedor = 5.0; p.percentLucro = 50; p.ncm = "21069090";
    p.csosn = "102"; p.pis = "49";
    QVERIFY(ps.inserir(p).ok);
    const qlonglong idProd = ps.getProdutoPeloCodBarras("7898200000014").id;
    QVERIFY(idProd > 0);

    Vendas_repository repo;
    Relatorios_repository rel;

    const qlonglong dinheiro = repo.inserir(venda("Dinheiro", 20.0, 20.0, 0, 0, 0));
    produtoVendido(dinheiro, idProd, 2, 10.0);
    QVERIFY(qAbs(rel.buscarLucroVenda(dinheiro) - 10.0) < 0.001);

    const qlonglong cartao = repo.inserir(venda("Crédito", 20.0, 20.0, 0, 0, 2.0));       // taxa da máquina 2% de 20 = 0,40
    produtoVendido(cartao, idProd, 2, 10.0);
    QVERIFY2(qAbs(rel.buscarLucroVenda(cartao) - 9.6) < 0.001, qPrintable(QString::number(rel.buscarLucroVenda(cartao))));

    // venda antiga: taxa somada ao valor (3%) e sem máquina -> o cálculo de antes continua igual
    const qlonglong legado = repo.inserir(venda("Débito", 20.0, 20.6, 3.0, 0, 0));
    produtoVendido(legado, idProd, 2, 10.0);
    QVERIFY(qAbs(rel.buscarLucroVenda(legado) - 9.4) < 0.001);
}

void TestMaquinaCartao::custo_por_maquina_soma_as_taxas_do_periodo()
{
    MaquinaCartao_service s;
    const auto a = s.salvar(maquina("Máquina A", 1.0, 2.0));
    const auto b = s.salvar(maquina("Máquina B", 0.5, 4.0));
    QVERIFY(a.ok && b.ok);
    Vendas_repository repo;
    QVERIFY(repo.inserir(venda("Crédito", 100.0, 100.0, 0, a.id, 2.0, "2026-07-05 10:00:00")) > 0);   // 2,00
    QVERIFY(repo.inserir(venda("Débito", 200.0, 200.0, 0, a.id, 1.0, "2026-07-06 11:00:00")) > 0);    // 2,00
    QVERIFY(repo.inserir(venda("Crédito", 50.0, 50.0, 0, b.id, 4.0, "2026-07-07 12:00:00")) > 0);     // 2,00
    QVERIFY(repo.inserir(venda("Dinheiro", 80.0, 80.0, 0, 0, 0, "2026-07-07 13:00:00")) > 0);          // sem taxa
    QVERIFY(repo.inserir(venda("Crédito", 100.0, 100.0, 0, a.id, 2.0, "2026-08-01 10:00:00")) > 0);    // fora do período

    const auto custos = s.custoPorMaquina(QDate(2026, 7, 1), QDate(2026, 7, 31));
    double totalA = -1, totalB = -1;
    int vendasA = 0;
    for (const auto &c : custos) {
        if (c.idMaquina == a.id) { totalA = c.custoTaxas; vendasA = c.vendas; }
        if (c.idMaquina == b.id) totalB = c.custoTaxas;
    }
    QCOMPARE(vendasA, 2);
    QVERIFY(qAbs(totalA - 4.0) < 0.001);
    QVERIFY(qAbs(totalB - 2.0) < 0.001);
}
