#include <QtTest>
#include "test_descontocpf.h"
#include "services/descontocpf_service.h"

static DescontoCpf_service::Config cfg5()
{
    DescontoCpf_service::Config c;
    c.ativo = true;
    c.percentual = 5.0;
    return c;
}

void TestDescontoCpf::desconto_e_o_percentual_do_total_em_centavos()
{
    const QDate hoje(2026, 11, 22);
    QCOMPARE(DescontoCpf_service::calcular(cfg5(), 100.0, hoje), 5.0);
    QCOMPARE(DescontoCpf_service::calcular(cfg5(), 12.34, hoje), 0.62);   // 0,617 arredonda para 0,62
    QCOMPARE(DescontoCpf_service::calcular(cfg5(), 0.0, hoje), 0.0);
}

void TestDescontoCpf::desligado_ou_percentual_zero_nao_da_desconto()
{
    const QDate hoje(2026, 11, 22);
    auto c = cfg5();
    c.ativo = false;
    QCOMPARE(DescontoCpf_service::calcular(c, 100.0, hoje), 0.0);
    c = cfg5();
    c.percentual = 0;
    QCOMPARE(DescontoCpf_service::calcular(c, 100.0, hoje), 0.0);
}

void TestDescontoCpf::campanha_so_vale_dentro_do_periodo()
{
    auto c = cfg5();
    c.comPeriodo = true;
    c.de = QDate(2026, 12, 1);
    c.ate = QDate(2026, 12, 31);
    QCOMPARE(DescontoCpf_service::calcular(c, 100.0, QDate(2026, 11, 30)), 0.0);
    QCOMPARE(DescontoCpf_service::calcular(c, 100.0, QDate(2026, 12, 1)), 5.0);
    QCOMPARE(DescontoCpf_service::calcular(c, 100.0, QDate(2026, 12, 31)), 5.0);
    QCOMPARE(DescontoCpf_service::calcular(c, 100.0, QDate(2027, 1, 1)), 0.0);
}

void TestDescontoCpf::teto_limita_o_desconto_por_venda()
{
    auto c = cfg5();
    c.tetoReais = 10.0;
    const QDate hoje(2026, 11, 22);
    QCOMPARE(DescontoCpf_service::calcular(c, 100.0, hoje), 5.0);
    QCOMPARE(DescontoCpf_service::calcular(c, 500.0, hoje), 10.0);   // 25,00 cortado no teto
}

void TestDescontoCpf::configuracao_salva_e_volta_igual()
{
    DescontoCpf_service::Config c;
    c.ativo = true;
    c.percentual = 7.5;
    c.tetoReais = 20;
    c.comPeriodo = true;
    c.de = QDate(2026, 12, 1);
    c.ate = QDate(2026, 12, 25);
    QString erro;
    QVERIFY2(DescontoCpf_service::salvar(c, &erro), qPrintable(erro));
    const auto lida = DescontoCpf_service::carregar();
    QVERIFY(lida.ativo);
    QCOMPARE(lida.percentual, 7.5);
    QCOMPARE(lida.tetoReais, 20.0);
    QVERIFY(lida.comPeriodo);
    QCOMPARE(lida.de, c.de);
    QCOMPARE(lida.ate, c.ate);

    // sem período: volta a valer sempre
    c.comPeriodo = false;
    QVERIFY(DescontoCpf_service::salvar(c, &erro));
    QVERIFY(!DescontoCpf_service::carregar().comPeriodo);
}

void TestDescontoCpf::configuracao_invalida_e_recusada()
{
    auto c = cfg5();
    QString erro;
    c.percentual = 80;
    QVERIFY(!DescontoCpf_service::salvar(c, &erro));
    c = cfg5();
    c.comPeriodo = true;
    c.de = QDate(2026, 12, 31);
    c.ate = QDate(2026, 12, 1);
    QVERIFY(!DescontoCpf_service::salvar(c, &erro));
}
