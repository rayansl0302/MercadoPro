#include <QtTest>
#include <QFileInfo>
#include <QTemporaryDir>
#include "test_precotaxa.h"
#include "services/maquinacartao_service.h"
#include "services/precotaxa_service.h"
#include "util/etiquetaspreco.h"

namespace {
MaquinaCartaoDTO maquina(const QString &nome, double deb, double cred)
{
    MaquinaCartaoDTO m;
    m.nome = nome;
    m.taxaDebito = deb;
    m.taxaCredito = cred;
    return m;
}
}

void TestPrecoTaxa::conta_por_dentro_devolve_o_preco_base_depois_de_tirar_a_taxa()
{
    QCOMPARE(PrecoTaxa::comTaxa(10.0, 3.9), 10.41);
    QCOMPARE(PrecoTaxa::comTaxa(39.9, 3.0), 41.13);
    QCOMPARE(PrecoTaxa::comTaxa(100.0, 5.0), 105.26);
    // tirando a taxa do preço de venda, a loja fica com (quase) o preço cadastrado
    const double venda = PrecoTaxa::comTaxa(10.0, 3.9);
    QVERIFY(qAbs(venda * (1 - 0.039) - 10.0) < 0.01);
}

void TestPrecoTaxa::taxa_invalida_ou_preco_zero_nao_altera_o_preco()
{
    QCOMPARE(PrecoTaxa::comTaxa(10.0, 0.0), 10.0);
    QCOMPARE(PrecoTaxa::comTaxa(10.0, -2.0), 10.0);
    QCOMPARE(PrecoTaxa::comTaxa(10.0, 100.0), 10.0);
    QCOMPARE(PrecoTaxa::comTaxa(0.0, 3.0), 0.0);
}

void TestPrecoTaxa::usa_a_maior_taxa_das_maquininhas_ativas_e_acompanha_o_cadastro()
{
    MaquinaCartao_service s;
    PrecoTaxa::invalidar();
    const double antes = PrecoTaxa::maiorTaxa();
    QVERIFY(antes < 40);

    const auto cara = s.salvar(maquina("Maquina cara (teste preco)", 12.0, 40.0));   // débito 12, crédito 40: vale o maior
    QVERIFY2(cara.ok, qPrintable(cara.msg));
    QCOMPARE(PrecoTaxa::maiorTaxa(), 40.0);                                          // o cadastro invalida o cache
    QCOMPARE(PrecoTaxa::comTaxa(60.0), 100.0);                                       // 60 / 0,60

    QVERIFY(s.definirAtiva(cara.id, false).ok);                                      // desativada: não conta mais
    QVERIFY(PrecoTaxa::maiorTaxa() < 40);
    QCOMPARE(PrecoTaxa::maiorTaxa(), antes);
}

void TestPrecoTaxa::texto_do_cadastro_mostra_taxa_e_preco_final()
{
    MaquinaCartao_service s;
    const auto m = s.salvar(maquina("Maquina texto (teste preco)", 1.0, 50.0));
    QVERIFY(m.ok);
    const QString texto = PrecoTaxa::textoPrecoDeVenda(10.0);
    QVERIFY2(texto.contains("50,00%"), qPrintable(texto));
    QVERIFY2(texto.contains("20,00"), qPrintable(texto));                            // 10 / 0,5
    QVERIFY(PrecoTaxa::textoPrecoDeVenda(0.0).isEmpty());
    QVERIFY(s.definirAtiva(m.id, false).ok);
}

void TestPrecoTaxa::etiquetas_a4_geram_pdf_e_contam_as_folhas()
{
    QCOMPARE(EtiquetasPreco::paginas(0), 1);
    QCOMPARE(EtiquetasPreco::paginas(27), 1);
    QCOMPARE(EtiquetasPreco::paginas(28), 2);

    QList<EtiquetasPreco::Item> itens;
    for (int i = 0; i < 30; ++i)
        itens.append({QStringLiteral("PRODUTO DE TESTE COM NOME BEM COMPRIDO %1").arg(i), i % 2 ? QStringLiteral("KG") : QStringLiteral("UN"), 10.5 + i});
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString arquivo = dir.filePath("etiquetas.pdf");
    QString erro;
    QVERIFY2(EtiquetasPreco::gerarPdf(arquivo, "Loja Teste", "Preços com a taxa incluída.", itens, &erro), qPrintable(erro));
    QVERIFY(QFileInfo(arquivo).size() > 1000);
}
