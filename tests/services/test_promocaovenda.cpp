#include <QtTest>
#include <QSqlQuery>
#include "test_promocaovenda.h"
#include "dto/Vendas_dto.h"
#include "infra/databaseconnection_service.h"
#include "repository/vendas_repository.h"
#include "services/precotaxa_service.h"
#include "services/promocaovenda_service.h"

void TestPromocaoVenda::tirar_a_taxa_do_preco_de_venda_devolve_o_preco_base()
{
    // é o caminho inverso de PrecoTaxa::comTaxa: o que a gôndola mostra volta ao preço do cadastro
    QCOMPARE(PrecoTaxa::semTaxa(10.41, 3.9), 10.0);
    QCOMPARE(PrecoTaxa::semTaxa(41.13, 3.0), 39.9);
    QCOMPARE(PrecoTaxa::semTaxa(105.26, 5.0), 100.0);
    for (double base : {1.99, 4.5, 12.9, 19.99, 59.9})
        QCOMPARE(PrecoTaxa::semTaxa(PrecoTaxa::comTaxa(base, 3.9), 3.9), base);

    // taxa inválida ou preço zero não alteram nada
    QCOMPARE(PrecoTaxa::semTaxa(10.0, 0.0), 10.0);
    QCOMPARE(PrecoTaxa::semTaxa(10.0, 100.0), 10.0);
    QCOMPARE(PrecoTaxa::semTaxa(0.0, 3.0), 0.0);
}

void TestPromocaoVenda::desconto_e_a_taxa_embutida_nos_itens_da_venda()
{
    // 2 un. de R$ 10,41 (base R$ 10,00 com 3,9%): o cliente pagaria 20,82 e passa a pagar 20,00
    QList<PromocaoVenda_service::Item> itens;
    itens.append({2.0, 10.41});
    QCOMPARE(PromocaoVenda_service::descontoAnulaTaxa(itens, 3.9), 0.82);

    // mistura de itens, inclusive fracionado (kg)
    itens.append({0.35, 19.99});    // base 19,25: 0,35 x 19,99 = 6,9965 ; 0,35 x 19,21 = 6,7235
    const double esperado = (2.0 * 10.41 + 0.35 * 19.99) - (2.0 * PrecoTaxa::semTaxa(10.41, 3.9) + 0.35 * PrecoTaxa::semTaxa(19.99, 3.9));
    QCOMPARE(PromocaoVenda_service::descontoAnulaTaxa(itens, 3.9), qRound(esperado * 100.0) / 100.0);

    // carrinho vazio
    QCOMPARE(PromocaoVenda_service::descontoAnulaTaxa({}, 3.9), 0.0);
}

void TestPromocaoVenda::sem_taxa_de_maquininha_nao_ha_desconto()
{
    QList<PromocaoVenda_service::Item> itens;
    itens.append({3.0, 12.5});
    QCOMPARE(PromocaoVenda_service::descontoAnulaTaxa(itens, 0.0), 0.0);
    QCOMPARE(PromocaoVenda_service::descontoAnulaTaxa(itens, -1.0), 0.0);
}

void TestPromocaoVenda::cadastro_cria_recusa_repetido_desativa_e_exclui()
{
    PromocaoVenda_service s;

    QVERIFY(!s.salvar(0, "   ", true).ok);                       // nome vazio

    const auto criada = s.salvar(0, "  Promoção   rede social (teste)  ", true);
    QVERIFY2(criada.ok, qPrintable(criada.msg));

    qlonglong id = 0;
    for (const auto &p : s.listar(true))
        if (p.nome == "Promoção rede social (teste)")           // espaços sobrando são limpos
            id = p.id;
    QVERIFY(id > 0);

    QVERIFY(!s.salvar(0, "PROMOÇÃO REDE SOCIAL (TESTE)", true).ok);   // repetido, ignorando maiúsculas
    QVERIFY(s.salvar(id, "Promoção rede social (teste)", true).ok);   // regravar a si mesma é permitido

    // desativada: some da tela de venda, mas continua no cadastro
    QVERIFY(s.salvar(id, "Promoção rede social (teste)", false).ok);
    bool aindaAtiva = false, noCadastro = false;
    for (const auto &p : s.listar(true))
        aindaAtiva |= (p.id == id);
    for (const auto &p : s.listar(false))
        noCadastro |= (p.id == id);
    QVERIFY(!aindaAtiva);
    QVERIFY(noCadastro);

    QVERIFY(s.excluir(id).ok);
    for (const auto &p : s.listar(false))
        QVERIFY(p.id != id);
}

void TestPromocaoVenda::venda_grava_o_nome_da_promocao_usada()
{
    QVERIFY(DatabaseConnection_service::open());

    auto grava = [](const QString &promocao) {
        VendasDTO v;
        v.id = 0;
        v.clienteNome = "Cliente teste promocao";
        v.dataHora = "2031-05-05 10:00:00";
        v.total = 20.82;
        v.desconto = 0.82;
        v.valorFinal = 20.0;
        v.formaPagamento = "Dinheiro";
        v.estaPago = true;
        v.idCliente = 1;
        v.promocaoVenda = promocao;
        return Vendas_repository().inserir(v);
    };
    auto promocaoDaVenda = [](qlonglong id) {
        QSqlQuery q(DatabaseConnection_service::db());
        q.prepare("SELECT promocao_venda FROM vendas2 WHERE id = :id");
        q.bindValue(":id", id);
        return q.exec() && q.next() ? q.value(0) : QVariant(QStringLiteral("(sem linha)"));
    };

    const qlonglong comPromo = grava("Promoção rede social");
    QVERIFY(comPromo > 0);
    QCOMPARE(promocaoDaVenda(comPromo).toString(), QString("Promoção rede social"));

    const qlonglong semPromo = grava(QString());
    QVERIFY(semPromo > 0);
    QVERIFY(promocaoDaVenda(semPromo).isNull());               // sem promoção fica NULL, não texto vazio
}
