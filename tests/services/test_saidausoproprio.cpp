#include <QtTest>
#include "test_saidausoproprio.h"
#include "services/Produto_service.h"
#include "services/saidausoproprio_service.h"
#include "dto/Produto_dto.h"

namespace {
qlonglong novoProduto(Produto_Service &s, const QString &desc, const QString &codigo, double qtd, double custo, double preco,
                      bool semControle = false)
{
    ProdutoDTO p;
    p.quantidade = qtd;
    p.descricao = desc;
    p.preco = preco;
    p.codigoBarras = codigo;
    p.nf = false;
    p.uCom = "UN";
    p.precoFornecedor = custo;
    p.percentLucro = 50;
    p.ncm = "21069090";
    p.csosn = "102";
    p.pis = "49";
    p.semControleEstoque = semControle;
    if (!s.inserir(p).ok)
        return -1;
    return s.getProdutoPeloCodBarras(codigo).id;
}
}

void TestSaidaUsoProprio::saida_baixa_o_estoque_e_grava_custo_e_preco_do_momento()
{
    Produto_Service produtos;
    SaidaUsoProprio_service saidas;
    const qlonglong id = novoProduto(produtos, "CAFE PARA A EQUIPE", "7898100000011", 10, 6.0, 9.0);
    QVERIFY(id > 0);

    const auto r = saidas.registrar(id, 2, "Consumo próprio", "  café da manhã  ", 3, "Maria");
    QVERIFY2(r.ok, qPrintable(r.msg));
    QVERIFY(r.id > 0);
    QCOMPARE(produtos.getProduto(id).quantidade, 8.0);

    const auto lista = saidas.listar(QDate::currentDate(), QDate::currentDate());
    QCOMPARE(lista.size(), 1);
    QCOMPARE(lista[0].idProduto, id);
    QCOMPARE(lista[0].descricaoProduto, QString("CAFE PARA A EQUIPE"));
    QCOMPARE(lista[0].quantidade, 2.0);
    QCOMPARE(lista[0].custoUnitario, 6.0);
    QCOMPARE(lista[0].precoVenda, 9.0);
    QCOMPARE(lista[0].custoTotal(), 12.0);
    QCOMPARE(lista[0].observacao, QString("café da manhã"));
    QCOMPARE(lista[0].idOperadorSessao, 3LL);
    QCOMPARE(lista[0].nomeOperador, QString("Maria"));
}

void TestSaidaUsoProprio::quantidade_invalida_motivo_invalido_e_estoque_insuficiente_sao_recusados()
{
    Produto_Service produtos;
    SaidaUsoProprio_service saidas;
    const qlonglong id = novoProduto(produtos, "SACOLA GRANDE", "7898100000028", 5, 0.1, 0.2);
    QVERIFY(id > 0);
    const int antes = saidas.listar(QDate::currentDate(), QDate::currentDate()).size();

    QVERIFY(!saidas.registrar(id, 0, "Consumo próprio", "", 1, "Op").ok);
    QVERIFY(!saidas.registrar(id, -1, "Consumo próprio", "", 1, "Op").ok);
    QVERIFY(!saidas.registrar(id, 1, "motivo inventado", "", 1, "Op").ok);
    const auto muito = saidas.registrar(id, 5.5, "Quebra / avaria", "", 1, "Op");
    QVERIFY(!muito.ok);
    QVERIFY(muito.msg.contains("insuficiente"));
    QVERIFY(!saidas.registrar(999999, 1, "Consumo próprio", "", 1, "Op").ok);

    QCOMPARE(produtos.getProduto(id).quantidade, 5.0);                              // nada mudou
    QCOMPARE(saidas.listar(QDate::currentDate(), QDate::currentDate()).size(), antes);  // nada gravado

    QVERIFY(saidas.registrar(id, 5, "Quebra / avaria", "", 1, "Op").ok);           // tirar tudo pode
    QCOMPARE(produtos.getProduto(id).quantidade, 0.0);
    QVERIFY(!saidas.registrar(id, 1, "Quebra / avaria", "", 1, "Op").ok);           // e não passa de zero
}

void TestSaidaUsoProprio::mudar_o_preco_depois_nao_reescreve_o_historico()
{
    Produto_Service produtos;
    SaidaUsoProprio_service saidas;
    const qlonglong id = novoProduto(produtos, "DETERGENTE", "7898100000035", 20, 2.0, 4.0);
    QVERIFY(id > 0);
    QVERIFY(saidas.registrar(id, 1, "Limpeza / uso na loja", "", 1, "Op").ok);

    ProdutoDTO p = produtos.getProduto(id);
    p.precoFornecedor = 3.5;
    p.preco = 7.0;
    QVERIFY(produtos.alterarVerificarCodigoBarras(p, p.codigoBarras, QString::number(id)).ok);

    const auto lista = saidas.listar(QDate::currentDate(), QDate::currentDate(), "Limpeza / uso na loja");
    QCOMPARE(lista.size(), 1);
    QCOMPARE(lista[0].custoUnitario, 2.0);
    QCOMPARE(lista[0].precoVenda, 4.0);
}

void TestSaidaUsoProprio::produto_sem_controle_de_estoque_registra_sem_baixar()
{
    Produto_Service produtos;
    SaidaUsoProprio_service saidas;
    const qlonglong id = novoProduto(produtos, "OVOS A GRANEL", "7898100000042", 999, 0.5, 1.0, true);
    QVERIFY(id > 0);
    const auto r = saidas.registrar(id, 30, "Consumo próprio", "", 1, "Op");
    QVERIFY2(r.ok, qPrintable(r.msg));
    QCOMPARE(produtos.getProduto(id).quantidade, 999.0);       // saldo de produto sem controle não anda
    QVERIFY(saidas.listar(QDate::currentDate(), QDate::currentDate(), "Consumo próprio").size() >= 1);
}

void TestSaidaUsoProprio::remover_saida_devolve_o_estoque_e_so_remove_uma_vez()
{
    Produto_Service produtos;
    SaidaUsoProprio_service saidas;
    const qlonglong id = novoProduto(produtos, "PALITO DE DENTE", "7898100000099", 10, 1.0, 2.0);
    QVERIFY(id > 0);

    // o operador lançou duas vezes sem querer
    const auto a = saidas.registrar(id, 2, "Consumo próprio", "", 1, "Op");
    const auto b = saidas.registrar(id, 2, "Consumo próprio", "", 1, "Op");
    QVERIFY(a.ok && b.ok);
    QCOMPARE(produtos.getProduto(id).quantidade, 6.0);

    const auto r = saidas.remover(b.id);
    QVERIFY2(r.ok, qPrintable(r.msg));
    QCOMPARE(produtos.getProduto(id).quantidade, 8.0);          // voltou o que o lançamento repetido tirou

    int doProduto = 0;
    for (const auto &s : saidas.listar(QDate::currentDate(), QDate::currentDate()))
        if (s.idProduto == id)
            ++doProduto;
    QCOMPARE(doProduto, 1);                                     // sobrou só a saída certa

    QVERIFY(!saidas.remover(b.id).ok);                          // remover de novo não devolve estoque em dobro
    QCOMPARE(produtos.getProduto(id).quantidade, 8.0);
    QVERIFY(!saidas.remover(999999).ok);
}

void TestSaidaUsoProprio::remover_saida_de_produto_sem_controle_nao_mexe_no_estoque()
{
    Produto_Service produtos;
    SaidaUsoProprio_service saidas;
    const qlonglong id = novoProduto(produtos, "OVO DA CASA", "7898100000100", 0, 0.5, 1.0, true);
    QVERIFY(id > 0);
    const auto a = saidas.registrar(id, 3, "Consumo próprio", "", 1, "Op");
    QVERIFY(a.ok);
    const double antes = produtos.getProduto(id).quantidade;
    QVERIFY(saidas.remover(a.id).ok);
    QCOMPARE(produtos.getProduto(id).quantidade, antes);
}

void TestSaidaUsoProprio::historico_filtra_por_periodo_e_motivo_e_soma_o_custo()
{
    Produto_Service produtos;
    SaidaUsoProprio_service saidas;
    const qlonglong id = novoProduto(produtos, "PAPEL TOALHA", "7898100000059", 50, 4.0, 8.0);
    QVERIFY(id > 0);
    QVERIFY(saidas.registrar(id, 2, "Doação", "", 1, "Op").ok);
    QVERIFY(saidas.registrar(id, 3, "Doação", "", 1, "Op").ok);
    QVERIFY(saidas.registrar(id, 1, "Vencido / estragado", "", 1, "Op").ok);

    const auto doacoes = saidas.listar(QDate::currentDate(), QDate::currentDate(), "Doação");
    QCOMPARE(doacoes.size(), 2);
    QCOMPARE(SaidaUsoProprio_service::custoTotal(doacoes), 20.0);
    QCOMPARE(saidas.listar(QDate::currentDate().addDays(-30), QDate::currentDate().addDays(-1), "Doação").size(), 0);
    QCOMPARE(saidas.listar(QDate::currentDate().addDays(1), QDate::currentDate().addDays(5)).size(), 0);
    QVERIFY(saidas.listar(QDate::currentDate(), QDate::currentDate()).size() >= 3);   // sem filtro de motivo traz todos
}
