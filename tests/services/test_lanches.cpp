#include <QtTest>
#include <QCryptographicHash>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include "test_lanches.h"
#include "util/lanches.h"
#include "util/comandalanche.h"
#include "util/cardapiolanches.h"
#include "infra/atualizador_service.h"
#include "services/Produto_service.h"
#include "dto/Produto_dto.h"

namespace {

using Lanches::codigoEhLanche;
using Lanches::TipoVenda;

// servidor HTTP mínimo: responde caminhos fixos; guarda o que foi pedido
class ServidorFalso : public QObject
{
public:
    QMap<QString, QByteArray> respostas; // caminho -> corpo
    QStringList pedidos;
    ServidorFalso()
    {
        server.listen(QHostAddress::LocalHost, 0);
        connect(&server, &QTcpServer::newConnection, this, [this]() {
            while (QTcpSocket *s = server.nextPendingConnection()) {
                connect(s, &QTcpSocket::readyRead, s, [this, s]() {
                    const QByteArray req = s->readAll();
                    const QString caminho = QString::fromLatin1(req.split(' ').value(1));
                    pedidos << caminho;
                    const bool tem = respostas.contains(caminho);
                    const QByteArray corpo = tem ? respostas.value(caminho) : QByteArray("nao encontrado");
                    s->write(QByteArray(tem ? "HTTP/1.1 200 OK\r\n" : "HTTP/1.1 404 Not Found\r\n") +
                             "Content-Length: " + QByteArray::number(corpo.size()) + "\r\nConnection: close\r\n\r\n" + corpo);
                    s->disconnectFromHost();
                });
            }
        });
    }
    QString base() const { return QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()); }
    // publica uma release "latest" com o instalador e o hash (o hash pode ser forçado errado)
    void publicar(const QString &versao, const QByteArray &instalador, const QString &hashForcado = QString())
    {
        const QString nome = QStringLiteral("MercadoPro-Setup-%1.exe").arg(versao);
        const QString hash = hashForcado.isEmpty() ? QString::fromLatin1(QCryptographicHash::hash(instalador, QCryptographicHash::Sha256).toHex()) : hashForcado;
        respostas["/exe"] = instalador;
        respostas["/hash"] = (hash + "  " + nome + "\n").toUtf8();
        QJsonObject release{
            {"tag_name", "v" + versao}, {"draft", false}, {"prerelease", false},
            {"assets", QJsonArray{QJsonObject{{"name", nome}, {"browser_download_url", base() + "/exe"}, {"size", instalador.size()}},
                                  QJsonObject{{"name", nome + ".sha256"}, {"browser_download_url", base() + "/hash"}}}}};
        respostas["/api"] = QJsonDocument(release).toJson();
    }
private:
    QTcpServer server;
};

// espera um dos sinais do atualizador (até 8 s); devolve qual veio
QString esperar(Atualizador_service *a, int ms = 8000)
{
    QString quem;
    QEventLoop loop;
    QTimer limite;
    limite.setSingleShot(true);
    QObject::connect(&limite, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(a, &Atualizador_service::atualizacaoPronta, &loop, [&](const QString &, const QString &) { quem = "pronta"; loop.quit(); });
    QObject::connect(a, &Atualizador_service::semAtualizacao, &loop, [&]() { quem = "sem"; loop.quit(); });
    QObject::connect(a, &Atualizador_service::falhou, &loop, [&](const QString &) { quem = "falhou"; loop.quit(); });
    limite.start(ms);
    a->verificarAgora();
    loop.exec();
    return quem;
}

void limparTemp()
{
    QDir(QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation)).filePath("MercadoPro-atualizacao")).removeRecursively();
}
} // namespace

void TestLanches::faixa_200_a_500_e_inclusiva_nas_pontas()
{
    QVERIFY(!codigoEhLanche("199"));
    QVERIFY(codigoEhLanche("200"));
    QVERIFY(codigoEhLanche("350"));
    QVERIFY(codigoEhLanche("500"));
    QVERIFY(!codigoEhLanche("501"));
    // faixa configurável
    Lanches::Faixa f;
    f.minimo = 10;
    f.maximo = 20;
    QVERIFY(codigoEhLanche("10", f));
    QVERIFY(codigoEhLanche("20", f));
    QVERIFY(!codigoEhLanche("200", f));
}

void TestLanches::codigo_que_nao_e_inteiro_curto_nunca_vira_lanche()
{
    QVERIFY(codigoEhLanche("  250  "));              // espaços do leitor são ignorados
    QVERIFY(!codigoEhLanche("0250"));                // zero à esquerda é outro código
    QVERIFY(!codigoEhLanche("25a"));
    QVERIFY(!codigoEhLanche("2.5"));
    QVERIFY(!codigoEhLanche("-250"));
    QVERIFY(!codigoEhLanche(""));
    QVERIFY(!codigoEhLanche("7891000100103"));       // EAN-13 de produto comum
    QVERIFY(!codigoEhLanche("0000000000250"));
    QVERIFY(!codigoEhLanche("99999999999999999999")); // não estoura a conversão
}

void TestLanches::carrinho_e_classificado_em_normal_lanche_ou_misto()
{
    QCOMPARE(Lanches::classificar({}), TipoVenda::Normal);
    QCOMPARE(Lanches::classificar({"7891000100103", "100", "501"}), TipoVenda::Normal);
    QCOMPARE(Lanches::classificar({"200"}), TipoVenda::Lanche);
    QCOMPARE(Lanches::classificar({"201", "350", "500"}), TipoVenda::Lanche);
    QCOMPARE(Lanches::classificar({"201", "7891000100103"}), TipoVenda::Misto);
}

void TestLanches::senha_do_dia_conta_so_vendas_de_lanche_ate_a_venda_atual()
{
    // vendas do dia: 10 (normal), 11 (lanche), 12 (lanche com 2 itens), 13 (normal), 14 (lanche)
    const QList<QPair<qlonglong, QString>> itens{
        {10, "7891000100103"}, {11, "205"}, {12, "210"}, {12, "211"}, {13, "999"}, {14, "300"}};
    QCOMPARE(Lanches::senhaDoDia(itens, 11), 1);
    QCOMPARE(Lanches::senhaDoDia(itens, 12), 2);    // 2 itens da mesma venda contam uma vez só
    QCOMPARE(Lanches::senhaDoDia(itens, 14), 3);
    QCOMPARE(Lanches::senhaDoDia({}, 5), 0);
}

void TestLanches::comanda_tem_senha_itens_e_corte_e_nenhum_dado_fiscal()
{
    ComandaLanche::Dados d;
    d.empresa = "Lanchonete do Zé";
    d.senha = 7;
    d.quando = QDateTime(QDate(2026, 10, 3), QTime(12, 30));
    d.itens = {{2, "X-BURGUER", "", "chapa", "sem cebola"}, {1, "BATATA FRITA", "", "chapa", ""}};
    const QByteArray b = ComandaLanche::montar(d);

    QVERIFY(b.startsWith(QByteArray("\x1B@", 2)));                 // ESC @ (inicializa)
    QVERIFY(b.contains("007"));                                   // senha com zeros à esquerda
    QVERIFY(b.contains("COZINHA / CHAPA"));
    QVERIFY(b.contains("2x X-BURGUER"));
    QVERIFY(b.contains("1x BATATA FRITA"));
    QVERIFY(b.contains("> sem cebola"));
    QVERIFY(b.contains("OBS:"));
    QVERIFY(b.contains("03/10/2026 12:30"));
    QVERIFY(b.contains(QByteArray("\x1D!\x55", 3)));              // GS ! 0x55: senha em fonte 6x6
    QVERIFY(b.endsWith(QByteArray("\x1D" "V", 2)) || b.right(4).contains('V')); // termina com o corte
    for (const char *proibido : {"CNPJ", "CUPOM", "Valor", "Total", "R$", "Troco", "Assinatura"})
        QVERIFY2(!b.contains(proibido), proibido);                 // comanda não é documento fiscal
}

void TestLanches::comanda_quebra_nome_comprido_sem_estourar_a_largura()
{
    const QStringList l = ComandaLanche::quebrarLinhas("X-TUDO ESPECIAL DA CASA COM OVO BACON E CATUPIRY EXTRA", 15);
    QVERIFY(l.size() >= 3);
    for (const QString &linha : l)
        QVERIFY2(linha.size() <= 15, qPrintable(linha));
    QCOMPARE(ComandaLanche::quebrarLinhas("ABCDEFGHIJKLMNOPQRST", 8), (QStringList{"ABCDEFGH", "IJKLMNOP", "QRST"}));
    QVERIFY(ComandaLanche::quebrarLinhas("", 10).isEmpty());

    ComandaLanche::Dados d;
    d.senha = 1;
    d.itens = {{1, "SANDUICHE NATURAL DE FRANGO COM CENOURA E MAIONESE"}};
    const QByteArray b = ComandaLanche::montar(d);
    for (const QByteArray &linha : b.split('\n'))
        QVERIFY2(linha.size() <= 60, linha.constData()); // sem linha absurda (ESC/POS quebra sozinho, mas o texto já vem quebrado)
}

void TestLanches::comparar_versoes_trata_numeros_e_nao_texto()
{
    using A = Atualizador_service;
    QVERIFY(A::compararVersoes("1.0.10", "1.0.9") > 0);
    QVERIFY(A::compararVersoes("1.0.9", "1.0.10") < 0);
    QCOMPARE(A::compararVersoes("v1.2.0", "1.2"), 0);
    QVERIFY(A::compararVersoes("2.0.0", "1.99.99") > 0);
    QVERIFY(A::compararVersoes("1.0.0-rc1", "1.0.0") == 0);
    QVERIFY(A::compararVersoes("0.0.0", "1.0.1") < 0);
}

void TestLanches::release_so_vale_com_instalador_e_hash_publicados()
{
    ServidorFalso s;
    s.publicar("1.0.5", "dados");
    QVERIFY(Atualizador_service::interpretarRelease(s.respostas["/api"]).has_value());

    auto release = QJsonDocument::fromJson(s.respostas["/api"]).object();
    auto semHash = release;
    auto assets = release["assets"].toArray();
    assets.removeLast();
    semHash["assets"] = assets;
    QVERIFY(!Atualizador_service::interpretarRelease(QJsonDocument(semHash).toJson()).has_value());

    auto rascunho = release;
    rascunho["draft"] = true;
    QVERIFY(!Atualizador_service::interpretarRelease(QJsonDocument(rascunho).toJson()).has_value());
    auto previa = release;
    previa["prerelease"] = true;
    QVERIFY(!Atualizador_service::interpretarRelease(QJsonDocument(previa).toJson()).has_value());
    QVERIFY(!Atualizador_service::interpretarRelease("lixo").has_value());
    QVERIFY(!Atualizador_service::interpretarRelease("{}").has_value());
    // só hosts do GitHub, só HTTPS
    QVERIFY(Atualizador_service::urlPermitida(QUrl("https://github.com/x/y/releases/download/v1/a.exe")));
    QVERIFY(Atualizador_service::urlPermitida(QUrl("https://objects.githubusercontent.com/abc")));
    QVERIFY(!Atualizador_service::urlPermitida(QUrl("http://github.com/x")));
    QVERIFY(!Atualizador_service::urlPermitida(QUrl("https://github.com.malicioso.com/x")));
    QVERIFY(!Atualizador_service::urlPermitida(QUrl("https://exemplo.com/a.exe")));
}

void TestLanches::hash_e_lido_do_arquivo_sha256()
{
    const QString h(64, 'a');
    QCOMPARE(Atualizador_service::hashDoTexto(h + "  MercadoPro-Setup-1.0.5.exe\n"), h);
    QCOMPARE(Atualizador_service::hashDoTexto(h.toUpper()), h);          // normaliza para minúsculo
    QVERIFY(Atualizador_service::hashDoTexto("curto").isEmpty());
    QVERIFY(Atualizador_service::hashDoTexto("").isEmpty());
}

void TestLanches::atualizacao_publicada_e_baixada_e_conferida()
{
    limparTemp();
    ServidorFalso s;
    const QByteArray instalador = QByteArray(300000, 'M') + "fim";
    s.publicar("99.0.0", instalador);
    Atualizador_service::definirUrlApiParaTeste(QUrl(s.base() + "/api"));
    auto *a = Atualizador_service::instancia();
    a->resetarEstadoParaTeste();
    QString arquivo;
    connect(a, &Atualizador_service::atualizacaoPronta, this, [&](const QString &, const QString &c) { arquivo = c; });
    QCOMPARE(esperar(a), QString("pronta"));
    QVERIFY(QFile::exists(arquivo));
    QCOMPARE(QFile(arquivo).size(), qint64(instalador.size()));
    QCOMPARE(Atualizador_service::sha256DoArquivo(arquivo), QString::fromLatin1(QCryptographicHash::hash(instalador, QCryptographicHash::Sha256).toHex()));
    limparTemp();
}

void TestLanches::instalador_com_hash_errado_e_descartado()
{
    limparTemp();
    ServidorFalso s;
    s.publicar("99.0.1", "instalador adulterado", QString(64, 'b'));
    Atualizador_service::definirUrlApiParaTeste(QUrl(s.base() + "/api"));
    auto *a = Atualizador_service::instancia();
    a->resetarEstadoParaTeste();
    QCOMPARE(esperar(a), QString("falhou"));
    const QString pasta = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation)).filePath("MercadoPro-atualizacao");
    QVERIFY(QDir(pasta).entryList({"*.exe"}, QDir::Files).isEmpty());   // nada que possa ser executado ficou no disco
    limparTemp();
}

void TestLanches::servidor_fora_do_ar_nao_trava_nem_levanta_erro()
{
    // porta fechada: a conexão é recusada na hora; offline de verdade cai no tempo limite da consulta
    Atualizador_service::definirUrlApiParaTeste(QUrl("http://127.0.0.1:1/api"));
    auto *a = Atualizador_service::instancia();
    a->resetarEstadoParaTeste();
    QElapsedTimer t;
    t.start();
    QCOMPARE(esperar(a, 12000), QString("falhou"));
    QVERIFY(t.elapsed() < 11000);
    // e o programa pode tentar de novo depois (o estado não ficou "em andamento")
    QCOMPARE(esperar(a, 12000), QString("falhou"));
}

void TestLanches::versao_igual_ou_menor_nao_baixa_nada()
{
    ServidorFalso s;
    s.publicar(Atualizador_service::versaoAtual(), "dados");    // mesma versão que está rodando
    Atualizador_service::definirUrlApiParaTeste(QUrl(s.base() + "/api"));
    auto *a = Atualizador_service::instancia();
    a->resetarEstadoParaTeste();
    QCOMPARE(esperar(a), QString("sem"));
    QVERIFY(!s.pedidos.contains("/exe"));
    QVERIFY(!s.pedidos.contains("/hash"));
}

void TestLanches::nenhuma_versao_publicada_404_nao_e_erro()
{
    ServidorFalso s;                       // sem nenhuma resposta cadastrada: tudo vira 404, como o GitHub sem Release
    Atualizador_service::definirUrlApiParaTeste(QUrl(s.base() + "/api"));
    auto *a = Atualizador_service::instancia();
    a->resetarEstadoParaTeste();
    QCOMPARE(esperar(a), QString("sem"));
    QVERIFY(!s.pedidos.contains("/exe"));
}

void TestLanches::venda_e_devolucao_de_lanche_nao_mexem_no_estoque_mas_mercadoria_sim()
{
    Produto_Service servico;
    auto novo = [&](const QString &desc, const QString &codigo, double qtd) {
        ProdutoDTO p;
        p.quantidade = qtd;
        p.descricao = desc;
        p.preco = 10;
        p.codigoBarras = codigo;
        p.nf = false;
        p.uCom = "UN";
        p.precoFornecedor = 4;
        p.percentLucro = 20;
        p.ncm = "00000000";
        p.csosn = "102";
        p.pis = "49";
        if (!servico.inserir(p).ok)
            return qlonglong(-1);
        return servico.getProdutoPeloCodBarras(codigo).id;
    };
    const qlonglong idLanche = novo("X-BURGUER TESTE ESTOQUE", "205", 9999);
    const qlonglong idComum = novo("MERCADORIA TESTE ESTOQUE", "7898765431001", 10);
    QVERIFY(idLanche > 0 && idComum > 0);
    QVERIFY(servico.ehLanche(idLanche));
    QVERIFY(!servico.ehLanche(idComum));

    // venda: o lanche continua com 9999; a mercadoria baixa
    QVERIFY(servico.updateDiminuirQuantidadeProduto(idLanche, 3).ok);
    QVERIFY(servico.updateDiminuirQuantidadeProduto(idComum, 3).ok);
    QCOMPARE(servico.getProduto(idLanche).quantidade, 9999.0);
    QCOMPARE(servico.getProduto(idComum).quantidade, 7.0);
    // devolução/cancelamento: o lanche não passa de 9999; a mercadoria volta
    QVERIFY(servico.updateAumentarQuantidadeProduto(idLanche, 3).ok);
    QVERIFY(servico.updateAumentarQuantidadeProduto(idComum, 3).ok);
    QCOMPARE(servico.getProduto(idLanche).quantidade, 9999.0);
    QCOMPARE(servico.getProduto(idComum).quantidade, 10.0);
}

void TestLanches::comanda_mostra_pago_com_forma_e_valor_e_a_pagar_quando_nao_pago()
{
    ComandaLanche::Dados d;
    d.senha = 12;
    d.statusPagamento = "PAGO";
    d.formaPagamento = "Pix";
    d.total = 15.0;
    d.itens = {{1, "X-TUDO"}};
    const QByteArray pago = ComandaLanche::montar(d);
    QVERIFY(pago.contains("*** PAGO ***"));
    QVERIFY(pago.contains("Pix"));
    QVERIFY(pago.contains("R$ 15,00"));
    QVERIFY(pago.contains(QByteArray("\x1D!\x22", 3)));          // GS ! 0x22: "PAGO" em fonte 3x3
    QVERIFY(!pago.contains("A PAGAR"));
    QVERIFY(!pago.contains("CNPJ"));                               // continua sem dado fiscal

    d.statusPagamento = "A PAGAR";
    d.formaPagamento = "Prazo";
    const QByteArray naoPago = ComandaLanche::montar(d);
    QVERIFY(naoPago.contains("*** A PAGAR ***"));
    QVERIFY(!naoPago.contains("*** PAGO ***"));

    // sem status de pagamento (comanda antiga) o bloco some
    ComandaLanche::Dados semPag;
    semPag.senha = 1;
    semPag.itens = {{1, "MISTO QUENTE"}};
    const QByteArray s = ComandaLanche::montar(semPag);
    QVERIFY(!s.contains("PAGO") && !s.contains("R$"));
}

void TestLanches::comanda_traz_ingredientes_e_separa_chapa_de_bebidas()
{
    ComandaLanche::Dados d;
    d.senha = 3;
    ComandaLanche::Item tudo{1, "X-TUDO", "Pão francês, hambúrguer, presunto, mussarela, ovo, alface, tomate, cebola", "chapa", ""};
    ComandaLanche::Item suco{2, "SUCO NATURAL", "Suco natural batido na hora", "bebida", ""};
    d.itens = {suco, tudo};                                      // a bebida vem primeiro no pedido, mas sai depois
    const QByteArray b = ComandaLanche::montar(d);
    QVERIFY(b.contains("hambúrguer") || b.contains(QString("hambúrguer").toLocal8Bit()));
    QVERIFY(b.contains("presunto,"));                          // o texto longo quebra em linhas: confere pedaços
    QVERIFY(b.contains("alface") && b.contains("cebola"));
    QVERIFY(b.contains("2x SUCO NATURAL"));
    const int iChapa = b.indexOf("[ CHAPA / LANCHES ]");
    const int iBebida = b.indexOf("[ BEBIDAS ]");
    QVERIFY(iChapa > 0 && iBebida > iChapa);                     // chapa antes de bebidas
    QVERIFY(b.indexOf("1x X-TUDO") > iChapa && b.indexOf("1x X-TUDO") < iBebida);
    QVERIFY(b.indexOf("2x SUCO NATURAL") > iBebida);

    // um bloco só: sem rótulos
    ComandaLanche::Dados um;
    um.senha = 4;
    um.itens = {tudo};
    const QByteArray so = ComandaLanche::montar(um);
    QVERIFY(!so.contains("[ CHAPA") && !so.contains("[ BEBIDAS"));
    // ingrediente comprido quebra em linhas sem estourar a largura de 42 colunas
    for (const QByteArray &linha : so.split('\n'))
        QVERIFY2(linha.size() <= 60, linha.constData());
}

void TestLanches::cardapio_padrao_tem_os_14_itens_com_ingredientes_e_grupo()
{
    CardapioLanches::recarregar();
    for (int cod = 201; cod <= 214; ++cod) {
        const auto i = CardapioLanches::buscar(QString::number(cod));
        QVERIFY2(!i.nome.isEmpty(), qPrintable(QString::number(cod)));
        QVERIFY2(!i.ingredientes.isEmpty(), qPrintable(QString::number(cod)));
        QCOMPARE(i.grupo, QString(cod >= 212 ? "bebida" : "chapa"));
    }
    QVERIFY(CardapioLanches::buscar("211").ingredientes.contains("ovo"));        // X-Tudo leva ovo
    QVERIFY(CardapioLanches::buscar("203").ingredientes.contains("2 fatias"));
    QVERIFY(CardapioLanches::buscar(" 208 ").ingredientes.contains("hambúrguer")); // espaços do leitor
    QVERIFY(CardapioLanches::buscar("999").ingredientes.isEmpty());               // fora do cardápio: sem ingredientes
    QVERIFY(CardapioLanches::buscar("7891000100103").ingredientes.isEmpty());
}

void TestLanches::cardapio_do_usuario_e_interpretado_e_arquivo_quebrado_nao_derruba()
{
    QString erro;
    const auto m = CardapioLanches::carregarDeJson(
        R"({"300": {"nome": "Tapioca", "grupo": "Bebidas", "ingredientes": "Goma, coco"}, "301": {"nome": "Crepioca"}, "x": 5})", &erro);
    QVERIFY2(erro.isEmpty(), qPrintable(erro));
    QCOMPARE(m.size(), 2);                                       // a entrada que não é objeto é ignorada
    QCOMPARE(m.value("300").grupo, QString("bebida"));           // grupo é aceito em qualquer caixa
    QCOMPARE(m.value("301").grupo, QString("chapa"));            // sem grupo = chapa
    QVERIFY(m.value("301").ingredientes.isEmpty());

    QString erro2;
    QVERIFY(CardapioLanches::carregarDeJson("{ quebrado", &erro2).isEmpty());
    QVERIFY(!erro2.isEmpty());
    QVERIFY(CardapioLanches::carregarDeJson("[1,2]", &erro2).isEmpty());
}
