#include <QtTest>
#include "test_pagamentodividido.h"
#include "db/test_db_factory.h"
#include "services/pagamentodividido_service.h"
#include "nota/pagamentomisto.h"
#include "repository/vendas_repository.h"
#include "services/caixa_service.h"
#include "services/vendas_service.h"

namespace {
using PagamentoDividido::montar;

VendasDTO vendaMista(double valor, const QList<PagamentoVendaDTO> &partes)
{
    VendasDTO v;
    v.clienteNome = "Consumidor";
    v.dataHora = "2026-03-16 10:00:00";
    v.total = valor;
    v.valorFinal = valor;
    v.desconto = 0;
    v.idCliente = 1;
    v.estaPago = true;
    v.pagamentos = partes;
    return v;
}
}

void TestPagamentoDividido::init()
{
    TestDbFactory::garantirCaixaAberto();
}

void TestPagamentoDividido::partes_conferem_e_o_troco_sai_do_dinheiro()
{
    QList<PagamentoVendaDTO> p;
    // 20 no dinheiro + 30 no Pix = 50, sem troco
    QVERIFY(montar(50, {{"Dinheiro", 20}, {"Pix", 30}}, &p).isEmpty());
    QCOMPARE(p.size(), 2);
    QCOMPARE(p[0].valor, 20.0);
    QCOMPARE(p[0].troco, 0.0);
    QCOMPARE(p[1].valor, 30.0);

    // cliente entrega 50 em dinheiro e paga 30 no Pix de uma venda de 60: o dinheiro cobre 30, troco 20
    QVERIFY(montar(60, {{"Dinheiro", 50}, {"Pix", 30}}, &p).isEmpty());
    QCOMPARE(p[0].valor, 30.0);
    QCOMPARE(p[0].valorRecebido, 50.0);
    QCOMPARE(p[0].troco, 20.0);
    QCOMPARE(p[1].valor, 30.0);
}

void TestPagamentoDividido::partes_invalidas_sao_recusadas()
{
    QList<PagamentoVendaDTO> p;
    QVERIFY(!montar(50, {{"Dinheiro", 50}}, &p).isEmpty());                     // uma forma só
    QVERIFY(!montar(50, {{"Dinheiro", 10}, {"Pix", 20}}, &p).isEmpty());        // falta
    QVERIFY(!montar(50, {{"Pix", 30}, {"Crédito", 30}}, &p).isEmpty());          // cartão não dá troco
    QVERIFY(!montar(50, {{"Pix", 25}, {"Pix", 25}}, &p).isEmpty());              // forma repetida
    QVERIFY(!montar(50, {{"Pix", 50}, {"Dinheiro", 10}}, &p).isEmpty());         // dinheiro sobra todo como troco
    QVERIFY(!montar(50, {{"Prazo", 25}, {"Pix", 25}}, &p).isEmpty());            // Prazo não entra
}

void TestPagamentoDividido::venda_dividida_grava_as_partes_e_entra_no_esperado_do_caixa()
{
    Caixa_service cs;
    const qlonglong idCaixa = cs.caixaAtual().id;
    QVERIFY(idCaixa > 0);
    const ResumoCaixaDTO antes = cs.resumo(idCaixa);

    QList<PagamentoVendaDTO> partes;
    QVERIFY(montar(80, {{"Dinheiro", 50}, {"Pix", 40}}, &partes).isEmpty());   // troco 10

    Vendas_service vs;
    VendasDTO v = vendaMista(80, partes);
    const auto r = vs.inserirVendaRegraDeNegocio(v, {});
    QVERIFY2(r.ok, qPrintable(r.msg));

    const VendasDTO lida = vs.getVenda(r.idVendaInserida);
    QVERIFY(lida.misto());
    QCOMPARE(lida.pagamentos.size(), 2);
    QCOMPARE(lida.valorFinal, 80.0);
    QCOMPARE(lida.valorRecebido, 90.0);     // 50 em dinheiro + 40 no Pix
    QCOMPARE(lida.troco, 10.0);

    const ResumoCaixaDTO depois = cs.resumo(idCaixa);
    QCOMPARE(depois.esperadoPorForma.value("Dinheiro") - antes.esperadoPorForma.value("Dinheiro"), 40.0);
    QCOMPARE(depois.esperadoPorForma.value("Pix") - antes.esperadoPorForma.value("Pix"), 40.0);
    QCOMPARE(depois.totalVendas - antes.totalVendas, 80.0);
    QCOMPARE(depois.quantidadeVendas - antes.quantidadeVendas, 1);
    QVERIFY(!depois.outrasFormas.contains("Misto"));

    // apagar a venda leva as partes junto
    Vendas_repository repo;
    QVERIFY(repo.deletarVenda(r.idVendaInserida));
    QVERIFY(repo.listarPagamentos(r.idVendaInserida).isEmpty());
}

void TestPagamentoDividido::soma_diferente_do_valor_e_recusada_pelo_servico()
{
    PagamentoVendaDTO a;
    a.forma = "Dinheiro";
    a.valor = 20;
    a.valorRecebido = 20;
    PagamentoVendaDTO b;
    b.forma = "Pix";
    b.valor = 20;
    Vendas_service vs;
    VendasDTO v = vendaMista(50, {a, b});
    const auto r = vs.inserirVendaRegraDeNegocio(v, {});
    QVERIFY(!r.ok);
    QVERIFY2(r.msg.contains("soma"), qPrintable(r.msg));
}

void TestPagamentoDividido::nota_fiscal_gera_uma_secao_por_forma_com_troco_na_ultima()
{
    QList<PagamentoVendaDTO> partes;
    QVERIFY(montar(60, {{"Dinheiro", 50}, {"Pix", 30}}, &partes).isEmpty());
    std::stringstream ini;
    PagamentoMistoNf::escrever(ini, PagamentoMistoNf::partes(partes), 20.0, false, 60.0);
    const QString texto = QString::fromStdString(ini.str());
    QVERIFY(texto.contains("[pag001]"));
    QVERIFY(texto.contains("[pag002]"));
    QVERIFY(texto.contains("tPag=01"));
    QVERIFY(texto.contains("tPag=17"));
    QVERIFY(texto.contains("vPag=50,00"));   // dinheiro: o que o cliente entregou
    QVERIFY(texto.contains("vPag=30,00"));
    // troco só na última seção
    const int primeira = texto.indexOf("[pag001]");
    const int segunda = texto.indexOf("[pag002]");
    QVERIFY(texto.mid(primeira, segunda - primeira).contains("vTroco=0,00"));
    QVERIFY(texto.mid(segunda).contains("vTroco=20,00"));

    // só parte dos itens na nota: formas reduzidas na mesma proporção e sem troco
    std::stringstream ini2;
    PagamentoMistoNf::escrever(ini2, PagamentoMistoNf::partes(partes), 20.0, true, 40.0);
    const QString t2 = QString::fromStdString(ini2.str());
    QVERIFY(!t2.contains("vTroco=20,00"));
    QVERIFY(t2.contains("vPag=25,00") && t2.contains("vPag=15,00"));    // 40 * 50/80 e o resto
}
