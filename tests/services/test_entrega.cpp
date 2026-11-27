#include <QtTest>
#include "test_entrega.h"
#include "services/entrega_service.h"
#include "../db/test_db_factory.h"
#include "infra/databaseconnection_service.h"
#include <QSqlQuery>

namespace {

EntregaItemDTO itemPorDescricao(const QList<EntregaItemDTO> &itens, const QString &trecho)
{
    for (const EntregaItemDTO &i : itens) {
        if (i.descricao.contains(trecho))
            return i;
    }
    return {};
}

EntregaItemDTO paoCaseiro()
{
    return itemPorDescricao(Entrega_service::itensPadrao(), "PAO CASEIRO");
}

} // namespace

void TestEntrega::init()
{
    db = DatabaseConnection_service::db();
    TestDbFactory::garantirCaixaAberto();
}

void TestEntrega::cleanup()
{
    if (!DatabaseConnection_service::open())
        return;
    QSqlQuery q(db);
    q.exec("DELETE FROM entrega_venda");
    q.exec("DELETE FROM produtos_vendidos");
    q.exec("DELETE FROM vendas2");
    q.exec("DELETE FROM entrega_item");
    q.exec("DELETE FROM produtos");
    db.close();
}

void TestEntrega::faixa_de_150_paes_muda_o_preco_para_030()
{
    const EntregaItemDTO pao = paoCaseiro();
    QCOMPARE(Entrega_service::precoAplicado(pao, 1), 0.40);
    QCOMPARE(Entrega_service::precoAplicado(pao, 149), 0.40);
    QCOMPARE(Entrega_service::precoAplicado(pao, 150), 0.30);
    QCOMPARE(Entrega_service::precoAplicado(pao, 400), 0.30);
}

void TestEntrega::total_da_linha_usa_o_preco_da_faixa()
{
    const EntregaItemDTO pao = paoCaseiro();
    QCOMPARE(Entrega_service::totalLinha(pao, 149), 59.60);
    QCOMPARE(Entrega_service::totalLinha(pao, 150), 45.00);
    QCOMPARE(Entrega_service::totalLinha(pao, 0), 0.0);
}

void TestEntrega::valor_em_reais_vira_quantidade_de_paes()
{
    const EntregaItemDTO pao = paoCaseiro();
    // R$ 30 de pão caseiro a R$ 0,40 = 75 pães
    QCOMPARE(Entrega_service::quantidadePorValor(pao, 30.0), 75.0);
    // sobra de centavos não vira meio pão
    QCOMPARE(Entrega_service::quantidadePorValor(pao, 30.30), 75.0);
    QCOMPARE(Entrega_service::quantidadePorValor(pao, 0), 0.0);
}

void TestEntrega::valor_em_reais_acima_da_faixa_usa_o_preco_da_faixa()
{
    const EntregaItemDTO pao = paoCaseiro();
    // R$ 90 a R$ 0,40 daria 225 pães (>= 150), então vale R$ 0,30: 300 pães
    QCOMPARE(Entrega_service::quantidadePorValor(pao, 90.0), 300.0);
}

void TestEntrega::kg_aceita_quantidade_fracionada()
{
    const EntregaItemDTO hot = itemPorDescricao(Entrega_service::itensPadrao(), "HOT DOG");
    QVERIFY(hot.fracionado());
    QCOMPARE(Entrega_service::totalLinha(hot, 0.25), 3.75);  // 0,25 kg x 14,99 = 3,7475 -> 3,75
    QCOMPARE(Entrega_service::quantidadePorValor(hot, 14.99), 1.0);
}

void TestEntrega::total_do_pedido_exemplo_fecha_em_49_08()
{
    const QList<EntregaItemDTO> itens = Entrega_service::itensPadrao();
    QList<EntregaLinhaDTO> linhas;
    linhas << EntregaLinhaDTO{itemPorDescricao(itens, "PAO CASEIRO"), 75};
    linhas << EntregaLinhaDTO{itemPorDescricao(itens, "DELICIA SEM RECHEIO"), 1};
    linhas << EntregaLinhaDTO{itemPorDescricao(itens, "MINI ESFIRRA"), 1};
    linhas << EntregaLinhaDTO{itemPorDescricao(itens, "MINI ENROLADINHO"), 1};
    linhas << EntregaLinhaDTO{itemPorDescricao(itens, "PAO PIZZA"), 1};
    QCOMPARE(Entrega_service::totalPedido(linhas), 49.08);
}

void TestEntrega::itens_padrao_sao_cadastrados_e_ligados_ao_estoque()
{
    Entrega_service serv;
    const QList<EntregaItemDTO> itens = serv.carregarItens();
    QCOMPARE(itens.size(), Entrega_service::itensPadrao().size());
    for (const EntregaItemDTO &i : itens)
        QVERIFY2(i.idProduto > 0, qPrintable(i.descricao));

    // chamar de novo não duplica nem itens nem produtos
    const QList<EntregaItemDTO> denovo = serv.carregarItens();
    QCOMPARE(denovo.size(), itens.size());
    QCOMPARE(denovo.first().idProduto, itens.first().idProduto);
}

void TestEntrega::pedido_gravado_como_venda_marcada_como_entrega_e_baixa_estoque()
{
    Entrega_service serv;
    const QList<EntregaItemDTO> itens = serv.carregarItens();

    EntregaPedidoDTO pedido;
    pedido.idCliente = 1;
    pedido.formaPagamento = "Dinheiro";
    pedido.endereco = "Fazenda Grande 1";
    pedido.linhas << EntregaLinhaDTO{itemPorDescricao(itens, "PAO CASEIRO"), 75};
    pedido.linhas << EntregaLinhaDTO{itemPorDescricao(itens, "MINI ESFIRRA"), 1};

    auto r = serv.registrarPedido(pedido);
    QVERIFY2(r.ok, qPrintable(r.msg));
    QVERIFY(r.idVenda > 0);
    QVERIFY(!r.avisoMarca);
    QVERIFY(serv.vendaEhEntrega(r.idVenda));
    // a tela usa estes dados para imprimir o cupom e emitir a nota fiscal
    QCOMPARE(r.venda.id, r.idVenda);
    QCOMPARE(r.produtos.size(), 2);

    DatabaseConnection_service::open();
    QSqlQuery q(db);
    q.exec(QString("SELECT total, valor_final FROM vendas2 WHERE id = %1").arg(r.idVenda));
    QVERIFY(q.next());
    QCOMPARE(q.value("total").toDouble(), 34.50);   // 75 x 0,40 + 4,50
    QCOMPARE(q.value("valor_final").toDouble(), 34.50);

    // pão feito na hora: o produto é sem controle de estoque, então a venda não mexe na quantidade
    QSqlQuery q2(db);
    q2.exec(QString("SELECT quantidade, sem_controle_estoque FROM produtos WHERE id = %1")
                .arg(itemPorDescricao(itens, "PAO CASEIRO").idProduto));
    QVERIFY(q2.next());
    QCOMPARE(q2.value(0).toDouble(), 0.0);
    QVERIFY(q2.value(1).toBool());
    db.close();
}

void TestEntrega::pedido_sem_quantidade_e_recusado()
{
    Entrega_service serv;
    const QList<EntregaItemDTO> itens = serv.carregarItens();

    EntregaPedidoDTO pedido;
    pedido.linhas << EntregaLinhaDTO{itemPorDescricao(itens, "PAO CASEIRO"), 0};

    auto r = serv.registrarPedido(pedido);
    QVERIFY(!r.ok);
    QVERIFY(r.idVenda <= 0);
}
