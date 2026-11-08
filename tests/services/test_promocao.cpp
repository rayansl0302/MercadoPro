#include <QtTest>
#include <QUuid>
#include "test_promocao.h"
#include "db/test_db_factory.h"
#include "services/promocao_service.h"
#include "services/precotaxa_service.h"
#include "services/Produto_service.h"
#include "util/postpromocao.h"

namespace {
qlonglong novoProduto(const QString &nome, double preco)
{
    Produto_Service ps;
    ProdutoDTO p;
    p.quantidade = 10;
    p.descricao = nome;
    p.preco = preco;
    p.codigoBarras = "PR" + QUuid::createUuid().toString(QUuid::Id128).left(10);
    p.nf = false;
    p.uCom = "UN";
    p.precoFornecedor = 1;
    p.percentLucro = 10;
    p.ncm = "22021000";
    p.aliquotaIcms = 18;
    p.csosn = "102";
    p.pis = "49";
    ps.inserir(p);
    return ps.getProdutoPeloCodBarras(p.codigoBarras).id;
}

double precoAtual(qlonglong id)
{
    return Produto_Service().getProduto(id).preco;
}
}

void TestPromocao::preco_base_da_promocao_fecha_o_valor_exato_da_gondola()
{
    for (double taxa : {0.0, 1.99, 3.9, 4.5}) {
        for (double gondola : {1.99, 3.49, 3.99, 7.5, 12.9, 24.99}) {
            const double base = Promocao_service::baseParaGondola(gondola, taxa);
            QVERIFY2(std::abs(PrecoTaxa::comTaxa(base, taxa) - gondola) < 0.005,
                     qPrintable(QString("taxa %1 gondola %2 base %3").arg(taxa).arg(gondola).arg(base)));
        }
    }
}

void TestPromocao::promocao_troca_o_preco_do_cadastro_e_o_fim_devolve_o_de()
{
    Promocao_service s;
    const qlonglong id = novoProduto("Promo teste A", 10.0);
    QVERIFY(id > 0);

    QVERIFY2(s.definir(id, 7.5, QDate::currentDate().addDays(5)).ok, "iniciar");
    QCOMPARE(precoAtual(id), 7.5);                       // o PDV já vende pelo preço promocional
    const QList<PromocaoDTO> lista = s.listar();
    bool achou = false;
    for (const PromocaoDTO &p : lista)
        if (p.idProduto == id) {
            achou = true;
            QCOMPARE(p.precoDe, 10.0);
            QCOMPARE(p.precoPor, 7.5);
            QCOMPARE(p.ate, QDate::currentDate().addDays(5).toString(Qt::ISODate));
        }
    QVERIFY(achou);

    QVERIFY(s.definir(id, 6.0, QDate()).ok);             // alterar mantém o "de" original
    QCOMPARE(precoAtual(id), 6.0);

    QVERIFY(s.encerrar(id).ok);
    QCOMPARE(precoAtual(id), 10.0);                      // volta ao preço de antes
    QVERIFY(!s.encerrar(id).ok);                         // já encerrada
    for (const PromocaoDTO &p : s.listar())
        QVERIFY(p.idProduto != id);
}

void TestPromocao::promocao_com_preco_invalido_ou_data_passada_e_recusada()
{
    Promocao_service s;
    const qlonglong id = novoProduto("Promo teste B", 10.0);
    QVERIFY(!s.definir(id, 10.0, QDate()).ok);           // igual ao preço de antes
    QVERIFY(!s.definir(id, 12.0, QDate()).ok);           // maior
    QVERIFY(!s.definir(id, 0, QDate()).ok);
    QVERIFY(!s.definir(id, 5.0, QDate::currentDate().addDays(-1)).ok);
    QVERIFY(!s.definir(999999999, 5.0, QDate()).ok);     // produto inexistente
    QCOMPARE(precoAtual(id), 10.0);                      // nada mudou
}

void TestPromocao::promocao_vencida_e_encerrada_sozinha()
{
    Promocao_service s;
    const qlonglong vence = novoProduto("Promo vence", 20.0);
    const qlonglong segue = novoProduto("Promo segue", 20.0);
    QVERIFY(s.definir(vence, 15.0, QDate::currentDate()).ok);
    QVERIFY(s.definir(segue, 15.0, QDate::currentDate().addDays(3)).ok);

    QCOMPARE(s.encerrarVencidas(QDate::currentDate()), 0);                  // hoje ainda vale (até o fim do dia)
    QCOMPARE(precoAtual(vence), 15.0);
    QVERIFY(s.encerrarVencidas(QDate::currentDate().addDays(1)) >= 1);       // amanhã a primeira venceu
    QCOMPARE(precoAtual(vence), 20.0);
    QCOMPARE(precoAtual(segue), 15.0);
    s.encerrar(segue);
}

void TestPromocao::post_e_legenda_trazem_de_por_e_desconto()
{
    using namespace PostPromocao;
    QCOMPARE(descontoPercentual(10, 7.5), 25);
    QCOMPARE(descontoPercentual(10, 10), 0);
    QCOMPARE(descontoPercentual(0, 5), 0);
    QCOMPARE(quantidadeDePosts(0), 0);
    QCOMPARE(quantidadeDePosts(6), 1);
    QCOMPARE(quantidadeDePosts(7), 2);

    const QList<Oferta> ofertas = {{"ARROZ TIPO 1 5KG", "UN", 29.9, 24.9}, {"SALSICHA", "KG", 14.99, 11.99}};
    const QImage img = postEmGrade("Loja Teste", "válido até 15/11", ofertas, 1, 1);
    QCOMPARE(img.width(), kLargura);
    QCOMPARE(img.height(), kAltura);
    QVERIFY(!img.isNull());
    QCOMPARE(postUnico("Loja Teste", "", ofertas.first()).size(), QSize(kLargura, kAltura));

    const QString texto = legenda("Loja Teste", "válido até 15/11", ofertas);
    QVERIFY(texto.contains("ARROZ TIPO 1 5KG"));
    QVERIFY(texto.contains("29,90"));
    QVERIFY(texto.contains("24,90"));
    QVERIFY(texto.contains("17% OFF"));
    QVERIFY(texto.contains("válido até 15/11"));

    QTemporaryDir pasta;
    QVERIFY(pasta.isValid());
    QString erro;
    const QStringList arquivos = salvar(pasta.path(), "Loja Teste", "", ofertas, true, &erro);
    QVERIFY2(erro.isEmpty(), qPrintable(erro));
    QCOMPARE(arquivos.size(), 3);                       // 1 grade + 2 individuais
    for (const QString &a : arquivos)
        QVERIFY(QFileInfo(a).size() > 1000);
}
