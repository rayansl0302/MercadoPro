#include <QtTest>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlQuery>
#include <QSqlError>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUuid>
#include "test_nuvem.h"
#include "nuvem/nuvem_service.h"
#include "nuvem/nuvemconfig.h"
#include "nuvem/nuvemcontrato.h"
#include "nuvem/nuvemexportador.h"
#include "nuvem/nuvemcontrole_repository.h"
#include "services/contaspagar_service.h"
#include "infra/databaseconnection_service.h"

namespace {
QSqlDatabase banco() { return DatabaseConnection_service::db(); }

void sql(const QString &comando)
{
    QSqlQuery q(banco());
    if (!q.exec(comando))
        qWarning() << "sql de teste falhou:" << q.lastError().text() << comando;
}

// Servidor HTTP de mentira que imita a API do painel (só o que a loja usa). Registra tudo que recebe.
class FakeApi : public QObject
{
public:
    struct Req { QString metodo; QString caminho; QByteArray corpo; QJsonObject json; };

    QString token = "token-de-teste-1234567890";
    QList<Req> reqs;
    QJsonArray pedidos;                      // pedidos pendentes que a "nuvem" entrega
    QMap<QString, QJsonObject> resultados;   // resultado que a loja devolveu, por pedido
    bool reentregarPedidos = false;          // continua entregando o pedido mesmo depois do resultado
    bool falharResultado = false;            // a 1ª tentativa de enviar resultado dá erro 500
    int falharNoLote = -1;                   // o lote de número N (contando de 1) dá erro 500
    bool recusarContrato = false;
    int resultadosFalhos = 0;
    int lotesRecebidos = 0;
    int ciclosConcluidos = 0;
    int ciclosCancelados = 0;
    QString erroConferencia;
    QMap<QString, QList<LinhaExportada>> linhas;       // por tabela, do ciclo em andamento
    QMap<QString, QJsonObject> declaradas;              // o que a loja avisou ao abrir o ciclo

    FakeApi()
    {
        server.listen(QHostAddress::LocalHost, 0);
        connect(&server, &QTcpServer::newConnection, this, [this]() { aceitar(); });
    }
    QString url() const { return QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()); }

    QList<Req> com(const QString &metodo, const QString &trecho) const
    {
        QList<Req> r;
        for (const Req &q : reqs)
            if (q.metodo == metodo && q.caminho.contains(trecho))
                r.append(q);
        return r;
    }

private:
    QTcpServer server;

    void aceitar()
    {
        while (QTcpSocket *s = server.nextPendingConnection()) {
            auto buffer = std::make_shared<QByteArray>();
            connect(s, &QTcpSocket::readyRead, this, [this, s, buffer]() {
                buffer->append(s->readAll());
                const int fim = buffer->indexOf("\r\n\r\n");
                if (fim < 0)
                    return;
                const QByteArray cab = buffer->left(fim);
                int tamanho = 0;
                for (const QByteArray &linha : cab.split('\n'))
                    if (linha.toLower().startsWith("content-length:"))
                        tamanho = linha.mid(15).trimmed().toInt();
                if (buffer->size() < fim + 4 + tamanho)
                    return;
                tratar(s, cab, buffer->mid(fim + 4, tamanho));
                buffer->clear();
            });
        }
    }

    void responder(QTcpSocket *s, int status, const QJsonObject &corpo)
    {
        const QByteArray dados = QJsonDocument(corpo).toJson(QJsonDocument::Compact);
        s->write("HTTP/1.1 " + QByteArray::number(status) + " X\r\nContent-Type: application/json\r\n"
                 "Connection: close\r\nContent-Length: " + QByteArray::number(dados.size()) + "\r\n\r\n" + dados);
        s->disconnectFromHost();
    }

    void tratar(QTcpSocket *s, const QByteArray &cab, const QByteArray &corpo)
    {
        const QList<QByteArray> cabecalhos = cab.split('\n');
        const QList<QByteArray> partes = cabecalhos.first().trimmed().split(' ');
        Req r;
        r.metodo = QString::fromLatin1(partes.value(0));
        r.caminho = QString::fromLatin1(partes.value(1));
        r.corpo = corpo;
        r.json = QJsonDocument::fromJson(corpo).object();
        reqs.append(r);

        QString auth;
        for (const QByteArray &l : cabecalhos)
            if (l.toLower().startsWith("authorization:"))
                auth = QString::fromLatin1(l.mid(14).trimmed());
        if (auth != "Bearer " + token)
            return responder(s, 401, {{"mensagem", "token inválido"}});

        const QString c = r.caminho;
        if (c == "/api/loja/ping")
            return responder(s, 200, {{"loja", "Loja de Teste"}});
        if (c == "/api/loja/heartbeat")
            return responder(s, 200, {{"ok", true}});
        if (r.metodo == "GET" && c == "/api/loja/pedidos") {
            QJsonArray a;
            for (const QJsonValue &v : std::as_const(pedidos))
                if (reentregarPedidos || !resultados.contains(v.toObject().value("id").toString()))
                    a.append(v);
            return responder(s, 200, {{"pedidos", a}});
        }
        if (c.startsWith("/api/loja/pedidos/") && c.endsWith("/resultado")) {
            if (falharResultado && resultadosFalhos < 1) {
                ++resultadosFalhos;
                return responder(s, 500, {{"mensagem", "erro de teste"}});
            }
            resultados[c.section('/', 4, 4)] = r.json;
            return responder(s, 200, {{"ok", true}});
        }
        if (c == "/api/loja/ciclo/iniciar") {
            if (recusarContrato)
                return responder(s, 409, {{"mensagem", "Contrato de dados incompatível com a API."}});
            linhas.clear();
            declaradas.clear();
            lotesRecebidos = 0;
            for (const QJsonValue &v : r.json.value("tabelas").toArray())
                declaradas[v.toObject().value("nome").toString()] = v.toObject();
            return responder(s, 200, {{"cicloId", "ciclo-1"}});
        }
        if (r.metodo == "PUT" && c.startsWith("/api/loja/ciclo/ciclo-1/tabela/")) {
            ++lotesRecebidos;
            if (lotesRecebidos == falharNoLote)
                return responder(s, 500, {{"mensagem", "erro de teste no lote"}});
            const QString tabela = c.section('/', 6, 6);
            for (const QJsonValue &lv : r.json.value("linhas").toArray()) {
                LinhaExportada linha;
                for (const QJsonValue &v : lv.toArray())
                    linha.append(v.isNull() ? QVariant(QMetaType(QMetaType::QString)) : QVariant(v.toString()));
                linhas[tabela].append(linha);
            }
            return responder(s, 200, {{"ok", true}});
        }
        if (c == "/api/loja/ciclo/ciclo-1/concluir") {
            erroConferencia.clear();
            for (auto it = declaradas.constBegin(); it != declaradas.constEnd(); ++it) {
                const QList<LinhaExportada> recebidas = linhas.value(it.key());
                if (recebidas.size() != it.value().value("linhas").toInt())
                    erroConferencia += QStringLiteral("%1: linhas %2 != %3; ").arg(it.key()).arg(recebidas.size()).arg(it.value().value("linhas").toInt());
                else if (NuvemExportador::checksum(recebidas) != it.value().value("checksum").toString())
                    erroConferencia += QStringLiteral("%1: checksum difere; ").arg(it.key());
            }
            if (!erroConferencia.isEmpty())
                return responder(s, 422, {{"mensagem", erroConferencia}});
            ++ciclosConcluidos;
            return responder(s, 200, {{"ok", true}});
        }
        if (c == "/api/loja/ciclo/ciclo-1/cancelar") {
            ++ciclosCancelados;
            return responder(s, 200, {{"ok", true}});
        }
        responder(s, 404, {{"mensagem", "rota desconhecida"}});
    }
};

NuvemConfig configPara(const FakeApi &api)
{
    NuvemConfig c;
    c.ativa = true;
    c.url = api.url();
    c.token = api.token;
    c.intervaloMinutos = 60;
    c.sincronizaNesteComputador = true;
    return c;
}

QJsonObject pedido(const QString &id, const QString &tipo, const QJsonObject &payload, int expiraEmHoras = 24)
{
    return QJsonObject{{"id", id}, {"tipo", tipo}, {"solicitante", "Maria"},
                       {"expiraEm", QDateTime::currentDateTimeUtc().addSecs(expiraEmHoras * 3600).toString(Qt::ISODate)},
                       {"payload", payload}};
}

qlonglong novaConta(const QString &descricao, double valor)
{
    ContasPagar_service cp;
    ContaPagarDTO c;
    c.descricao = descricao;
    c.valor = valor;
    c.vencimento = QDate::currentDate().addDays(10).toString(Qt::ISODate);
    return cp.lancar(c).id;
}

int contarAuditoria(const QString &acao, const QString &trecho)
{
    QSqlQuery q(banco());
    q.prepare("SELECT COUNT(*) FROM auditoria_acesso WHERE acao = :a AND detalhe LIKE :d");
    q.bindValue(":a", acao);
    q.bindValue(":d", "%" + trecho + "%");
    return (q.exec() && q.next()) ? q.value(0).toInt() : -1;
}
}

void TestNuvem::init()
{
    sql("DELETE FROM sync_estado");
    sql("DELETE FROM sync_pedidos_aplicados");
    sql("DELETE FROM sync_log");
    sql("UPDATE sync_trava SET terminal = NULL, expira_em = '2000-01-01 00:00:00' WHERE id = 1");
}

void TestNuvem::cleanup()
{
    sql("DELETE FROM operadores WHERE nome = 'Operador Segredo'");
    sql("UPDATE sync_trava SET terminal = NULL, expira_em = '2000-01-01 00:00:00' WHERE id = 1");
}

void TestNuvem::contrato_so_lista_colunas_que_existem_e_nenhum_segredo()
{
    const NuvemContrato &contrato = NuvemContrato::instancia();
    QVERIFY(contrato.valido());
    QVERIFY(contrato.versao() >= 1);

    const QStringList proibidas = {"pin", "hash", "salt", "senha", "password", "token", "cpf", "email", "endereco",
                                   "xml_path", "caminho", "certificado", "tentativas"};
    for (const TabelaContrato &t : contrato.tabelas()) {
        QStringList nomes;
        for (const ColunaContrato &c : t.colunas) {
            nomes << c.nome;
            for (const QString &p : proibidas)
                QVERIFY2(!c.nome.toLower().contains(p), qPrintable(QString("%1.%2 parece segredo").arg(t.nome, c.nome)));
            QVERIFY2(QStringList({"int", "dec", "real", "text", "bool", "ts"}).contains(c.tipo),
                     qPrintable(QString("tipo inválido em %1.%2").arg(t.nome, c.nome)));
        }
        QVERIFY2(!t.pk.isEmpty(), qPrintable(t.nome + " sem pk"));
        // toda coluna do contrato existe no banco (contrato desatualizado quebraria o envio)
        QSqlQuery q(banco());
        QVERIFY2(q.exec(QString("SELECT %1 FROM %2 WHERE 1 = 0").arg(nomes.join(", "), t.nome)),
                 qPrintable(QString("%1: %2").arg(t.nome, q.lastError().text())));
    }
    // tabelas de segredo nunca entram
    for (const QString &proibida : {"config_caixa", "sync_trava", "sync_estado", "rascunho_venda"})
        QVERIFY(contrato.tabela(proibida) == nullptr);
}

void TestNuvem::checksum_bate_com_os_vetores_compartilhados_com_a_api()
{
    // os mesmos casos de src/nuvem/checksum_vetores.json, que a API testa do lado dela
    QCOMPARE(NuvemExportador::checksum({}), QStringLiteral("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
    const QVariant nulo{QMetaType(QMetaType::QString)};
    QCOMPARE(NuvemExportador::checksum({{QString("1"), QString("a"), nulo}}),
             QStringLiteral("0f41b15537ca4807e4d43c1294d8f2935de423769d5a2d524371df97f47e4d00"));
    QCOMPARE(NuvemExportador::checksum({{QString("1"), QString("a"), nulo},
                                        {QString("2"), QString::fromUtf8("João ç"), QString("12.50")},
                                        {QString("3"), QString(""), QString("")}}),
             QStringLiteral("9b84ca5385c27adc94340a07a6147c5da7c6c66df632eb0ebf7771ae21c71f02"));
    // nulo e texto vazio são coisas diferentes
    QVERIFY(NuvemExportador::checksum({{QString("")}}) != NuvemExportador::checksum({{nulo}}));
}

void TestNuvem::normalizar_converte_tipos_para_o_texto_do_contrato()
{
    QCOMPARE(NuvemExportador::normalizar(12.5, "dec").toString(), QStringLiteral("12.50"));
    QCOMPARE(NuvemExportador::normalizar(42.0, "int").toString(), QStringLiteral("42"));
    QCOMPARE(NuvemExportador::normalizar(true, "bool").toString(), QStringLiteral("1"));
    QCOMPARE(NuvemExportador::normalizar(0, "bool").toString(), QStringLiteral("0"));
    QCOMPARE(NuvemExportador::normalizar("true", "bool").toString(), QStringLiteral("1"));
    QCOMPARE(NuvemExportador::normalizar(QDateTime(QDate(2026, 3, 4), QTime(5, 6, 7)), "ts").toString(),
             QStringLiteral("2026-03-04 05:06:07"));
    QCOMPARE(NuvemExportador::normalizar(2.5, "real").toString(), QStringLiteral("2.5"));
    QCOMPARE(NuvemExportador::normalizar(10.0, "real").toString(), QStringLiteral("10"));
    QVERIFY(NuvemExportador::normalizar(QVariant(), "text").isNull());
    QVERIFY(NuvemExportador::normalizar(QVariant(QMetaType(QMetaType::QString)), "int").isNull());
    QCOMPARE(NuvemExportador::normalizar("", "text").toString(), QString(""));
    QVERIFY(!NuvemExportador::normalizar("", "text").isNull());
}

void TestNuvem::configuracao_valida_url_e_token()
{
    QVERIFY(NuvemConfig::urlPermitida("https://painel.exemplo.com"));
    QVERIFY(NuvemConfig::urlPermitida("http://localhost:3000"));
    QVERIFY(NuvemConfig::urlPermitida("http://127.0.0.1:3000"));
    QString motivo;
    QVERIFY(!NuvemConfig::urlPermitida("http://painel.exemplo.com", &motivo));
    QVERIFY(motivo.contains("https"));
    QVERIFY(!NuvemConfig::urlPermitida("ftp://x.com"));
    QVERIFY(!NuvemConfig::urlPermitida("sem-esquema"));

    NuvemConfig c;
    c.url = "https://painel.exemplo.com";
    c.token = "curto";
    QVERIFY(!c.validar().isEmpty());
    c.token = "token-com-mais-de-16-caracteres";
    QVERIFY(c.validar().isEmpty());
    c.intervaloMinutos = 5;
    QVERIFY(!c.validar().isEmpty());
    c.intervaloMinutos = 60;
    // pronta exige ativa + este computador sincronizando
    QVERIFY(!c.pronta());
    c.ativa = true;
    c.sincronizaNesteComputador = true;
    QVERIFY(c.pronta());
}

void TestNuvem::ciclo_completo_confere_contagens_e_nao_vaza_segredos()
{
    FakeApi api;
    sql("INSERT INTO operadores (nome, pin_hash, pin_salt, ativo, tentativas_falhas, bloqueado, adicionado_em, atualizado_em, gerente) "
        "VALUES ('Operador Segredo', 'SEGREDO_PIN_HASH_UNICO', 'SEGREDO_SALT_UNICO', 1, 0, 0, '2026-01-01 00:00:00', '2026-01-01 00:00:00', 0)");

    Nuvem_service servico;
    servico.definirConfigParaTeste(configPara(api));
    servico.definirTerminalParaTeste("CAIXA-A");
    servico.definirTamanhoLoteParaTeste(3);                    // vários lotes por tabela
    const auto r = servico.executar(true);
    QVERIFY2(r.ok, qPrintable(r.mensagem));
    QVERIFY(r.linhasEnviadas > 0);

    // a ordem do ciclo: online, pedidos, abrir, lotes, concluir
    QCOMPARE(api.reqs.first().caminho, QStringLiteral("/api/loja/heartbeat"));
    QVERIFY(api.reqs.at(1).caminho.endsWith("/pedidos"));
    QVERIFY(api.reqs.at(2).caminho.endsWith("/ciclo/iniciar"));
    QVERIFY(api.reqs.last().caminho.endsWith("/concluir"));
    QCOMPARE(api.ciclosConcluidos, 1);
    QVERIFY2(api.erroConferencia.isEmpty(), qPrintable(api.erroConferencia));
    QVERIFY(api.lotesRecebidos > api.declaradas.size());       // houve tabelas em mais de um lote

    // todas as tabelas do contrato foram avisadas
    QCOMPARE(api.declaradas.size(), NuvemContrato::instancia().tabelas().size());

    // nenhum segredo saiu em nenhuma requisição
    for (const auto &req : api.reqs) {
        QVERIFY(!req.corpo.contains("SEGREDO_PIN_HASH_UNICO"));
        QVERIFY(!req.corpo.contains("SEGREDO_SALT_UNICO"));
    }
    // mas o operador (nome) subiu: o painel precisa dele
    bool achouNome = false;
    for (const auto &req : api.com("PUT", "tabela/operadores"))
        achouNome = achouNome || req.corpo.contains("Operador Segredo");
    QVERIFY(achouNome);

    const auto s = servico.status();
    QVERIFY(!s.ultimoCicloOk.isEmpty());
    QVERIFY(!s.ultimoEnvioOk.isEmpty());
    QVERIFY(s.ultimoErro.isEmpty());
}

void TestNuvem::falha_no_meio_do_envio_cancela_e_a_proxima_tentativa_funciona()
{
    FakeApi api;
    api.falharNoLote = 2;
    Nuvem_service servico;
    servico.definirConfigParaTeste(configPara(api));
    servico.definirTerminalParaTeste("CAIXA-A");
    servico.definirTamanhoLoteParaTeste(2);

    const auto falhou = servico.executar(true);
    QVERIFY(falhou.executou);
    QVERIFY(!falhou.ok);
    QVERIFY(falhou.mensagem.contains("lote"));
    QCOMPARE(api.ciclosConcluidos, 0);
    QCOMPARE(api.ciclosCancelados, 1);                         // a loja avisou para descartar o ciclo
    const auto s = servico.status();
    QVERIFY(s.ultimoEnvioOk.isEmpty());                        // não marcou "enviado"
    QVERIFY(!s.ultimoErro.isEmpty());

    api.falharNoLote = -1;
    const auto ok = servico.executar(true);
    QVERIFY2(ok.ok, qPrintable(ok.mensagem));
    QCOMPARE(api.ciclosConcluidos, 1);
    QVERIFY(servico.status().ultimoErro.isEmpty());            // o erro antigo some quando volta a funcionar
}

void TestNuvem::token_recusado_e_contrato_incompativel_viram_mensagem_clara()
{
    FakeApi api;
    NuvemConfig cfg = configPara(api);
    cfg.token = "token-errado-com-mais-de-16";
    Nuvem_service servico;
    servico.definirConfigParaTeste(cfg);
    servico.definirTerminalParaTeste("CAIXA-A");

    const auto teste = servico.testarConexao();
    QVERIFY(!teste.ok);
    QVERIFY(teste.mensagem.contains("chave", Qt::CaseInsensitive));
    const auto ciclo = servico.executar(true);
    QVERIFY(!ciclo.ok);
    QVERIFY(ciclo.mensagem.contains("chave", Qt::CaseInsensitive));

    servico.definirConfigParaTeste(configPara(api));
    const auto bom = servico.testarConexao();
    QVERIFY(bom.ok);
    QVERIFY(bom.mensagem.contains("Loja de Teste"));

    api.recusarContrato = true;
    const auto incompativel = servico.executar(true);
    QVERIFY(!incompativel.ok);
    QVERIFY(incompativel.mensagem.contains("incompatível"));
}

void TestNuvem::pedido_de_baixa_e_aplicado_pelo_servico_e_registrado_como_painel()
{
    FakeApi api;
    ContasPagar_service cp;
    const qlonglong conta = novaConta("Aluguel do painel " + QUuid::createUuid().toString(QUuid::Id128).left(6), 1200);
    QVERIFY(conta > 0);
    api.pedidos.append(pedido("ped-baixa-1", "CONTA_PAGAR_BAIXAR",
        {{"contaId", double(conta)}, {"valorPago", "1210.50"}, {"forma", "Pix"},
         {"data", QDate::currentDate().toString(Qt::ISODate)}}));

    Nuvem_service servico;
    servico.definirConfigParaTeste(configPara(api));
    servico.definirTerminalParaTeste("CAIXA-A");
    const auto r = servico.executar(false);
    QVERIFY2(r.ok, qPrintable(r.mensagem));
    QCOMPARE(r.pedidosAplicados, 1);

    const ContaPagarDTO paga = cp.getConta(conta);
    QCOMPARE(paga.status, QStringLiteral("PAGA"));
    QCOMPARE(paga.valorPago, 1210.5);
    QCOMPARE(paga.formaPagamento, QStringLiteral("Pix"));

    QCOMPARE(api.resultados.value("ped-baixa-1").value("status").toString(), QStringLiteral("APLICADO"));
    QCOMPARE(contarAuditoria("CONTA_PAGAR_BAIXA_PAINEL", "Painel: Maria"), 1);
    // depois de aplicar um pedido a loja manda os dados na hora (o painel não fica mostrando a conta aberta)
    QCOMPARE(api.ciclosConcluidos, 1);
}

void TestNuvem::pedido_repetido_nao_aplica_duas_vezes()
{
    FakeApi api;
    api.reentregarPedidos = true;                              // a nuvem entrega o mesmo pedido de novo
    ContasPagar_service cp;
    const qlonglong conta = novaConta("Luz repetida " + QUuid::createUuid().toString(QUuid::Id128).left(6), 300);
    api.pedidos.append(pedido("ped-repetido", "CONTA_PAGAR_BAIXAR",
        {{"contaId", double(conta)}, {"valorPago", 300}, {"forma", "Dinheiro"},
         {"data", QDate::currentDate().toString(Qt::ISODate)}}));

    Nuvem_service servico;
    servico.definirConfigParaTeste(configPara(api));
    servico.definirTerminalParaTeste("CAIXA-A");
    QCOMPARE(servico.executar(false).pedidosAplicados, 1);
    QCOMPARE(servico.executar(false).pedidosAplicados, 0);     // já aplicado: ignora
    QCOMPARE(servico.executar(false).pedidosAplicados, 0);
    QCOMPARE(cp.getConta(conta).status, QStringLiteral("PAGA"));
    QCOMPARE(contarAuditoria("CONTA_PAGAR_BAIXA_PAINEL", QString("Conta #%1 ").arg(conta)), 1);
}

void TestNuvem::resultado_que_nao_chegou_e_reenviado_sem_reaplicar()
{
    FakeApi api;
    api.falharResultado = true;                                // o aviso de resultado falha na 1ª vez
    ContasPagar_service cp;
    const qlonglong conta = novaConta("Agua reenvio " + QUuid::createUuid().toString(QUuid::Id128).left(6), 80);
    api.pedidos.append(pedido("ped-reenvio", "CONTA_PAGAR_BAIXAR",
        {{"contaId", double(conta)}, {"valorPago", 80}, {"forma", "Pix"},
         {"data", QDate::currentDate().toString(Qt::ISODate)}}));

    Nuvem_service servico;
    servico.definirConfigParaTeste(configPara(api));
    servico.definirTerminalParaTeste("CAIXA-A");
    QCOMPARE(servico.executar(false).pedidosAplicados, 1);
    QVERIFY(!api.resultados.contains("ped-reenvio"));          // a nuvem não recebeu o resultado
    NuvemControle_repository repo;
    QCOMPARE(repo.pedidosNaoEnviados().size(), 1);

    const auto segunda = servico.executar(false);
    QCOMPARE(segunda.pedidosAplicados, 0);                     // não aplicou de novo
    QVERIFY(api.resultados.contains("ped-reenvio"));           // mas agora o resultado chegou
    QCOMPARE(api.resultados.value("ped-reenvio").value("status").toString(), QStringLiteral("APLICADO"));
    QCOMPARE(repo.pedidosNaoEnviados().size(), 0);
    QCOMPARE(cp.getConta(conta).valorPago, 80.0);
}

void TestNuvem::pedido_com_conta_ja_paga_e_conflito_e_pedido_vencido_expira()
{
    FakeApi api;
    ContasPagar_service cp;
    const qlonglong paga = novaConta("Ja paga " + QUuid::createUuid().toString(QUuid::Id128).left(6), 55);
    QVERIFY(cp.baixar(paga, 55, "Pix").ok);                    // baixada na loja antes do pedido chegar
    const qlonglong aberta = novaConta("Vencido " + QUuid::createUuid().toString(QUuid::Id128).left(6), 66);

    api.pedidos.append(pedido("ped-conflito", "CONTA_PAGAR_BAIXAR",
        {{"contaId", double(paga)}, {"valorPago", 55}, {"forma", "Pix"}, {"data", QDate::currentDate().toString(Qt::ISODate)}}));
    api.pedidos.append(pedido("ped-expirado", "CONTA_PAGAR_BAIXAR",
        {{"contaId", double(aberta)}, {"valorPago", 66}, {"forma", "Pix"}, {"data", QDate::currentDate().toString(Qt::ISODate)}}, -2));
    api.pedidos.append(pedido("ped-inexistente", "CONTA_PAGAR_BAIXAR",
        {{"contaId", 999999999.0}, {"valorPago", 1}, {"forma", "Pix"}, {"data", QDate::currentDate().toString(Qt::ISODate)}}));

    Nuvem_service servico;
    servico.definirConfigParaTeste(configPara(api));
    servico.definirTerminalParaTeste("CAIXA-A");
    const auto r = servico.executar(false);
    QVERIFY2(r.ok, qPrintable(r.mensagem));
    QCOMPARE(r.pedidosAplicados, 0);
    QCOMPARE(api.resultados.value("ped-conflito").value("status").toString(), QStringLiteral("CONFLITO"));
    QCOMPARE(api.resultados.value("ped-expirado").value("status").toString(), QStringLiteral("EXPIRADO"));
    QCOMPARE(api.resultados.value("ped-inexistente").value("status").toString(), QStringLiteral("RECUSADO"));
    QCOMPARE(cp.getConta(aberta).status, QStringLiteral("ABERTA"));      // o vencido não foi executado
    QCOMPARE(cp.getConta(paga).valorPago, 55.0);                          // e nada foi duplicado
}

void TestNuvem::pedido_de_lancar_conta_parcelada_e_pedido_invalido_e_recusado()
{
    FakeApi api;
    ContasPagar_service cp;
    const QString marca = "Parcelada painel " + QUuid::createUuid().toString(QUuid::Id128).left(6);
    api.pedidos.append(pedido("ped-lancar", "CONTA_PAGAR_LANCAR",
        {{"descricao", marca}, {"fornecedor", "Fornecedor X"}, {"categoria", "Aluguel"}, {"valor", "300.00"},
         {"vencimento", QDate::currentDate().addDays(5).toString(Qt::ISODate)}, {"parcelas", 3},
         {"periodicidade", "MENSAL"}, {"idEmpresa", 1}}));
    api.pedidos.append(pedido("ped-empresa-ruim", "CONTA_PAGAR_LANCAR",
        {{"descricao", "x empresa"}, {"valor", 10}, {"vencimento", QDate::currentDate().toString(Qt::ISODate)}, {"idEmpresa", 987654}}));
    api.pedidos.append(pedido("ped-sem-valor", "CONTA_PAGAR_BAIXAR", {{"contaId", 1}}));
    api.pedidos.append(pedido("ped-tipo-ruim", "APAGAR_TUDO", {}));

    Nuvem_service servico;
    servico.definirConfigParaTeste(configPara(api));
    servico.definirTerminalParaTeste("CAIXA-A");
    const auto r = servico.executar(false);
    QVERIFY2(r.ok, qPrintable(r.mensagem));
    QCOMPARE(r.pedidosAplicados, 1);

    QCOMPARE(api.resultados.value("ped-lancar").value("status").toString(), QStringLiteral("APLICADO"));
    FiltroContasPagarDTO f;
    f.texto = marca;
    const QList<ContaPagarDTO> contas = cp.listar(f);
    QCOMPARE(contas.size(), 3);
    QCOMPARE(contas.at(0).valor, 100.0);
    QCOMPARE(contas.at(0).fornecedor, QStringLiteral("Fornecedor X"));
    QCOMPARE(contarAuditoria("CONTA_PAGAR_LANCADA_PAINEL", marca), 1);

    QCOMPARE(api.resultados.value("ped-empresa-ruim").value("status").toString(), QStringLiteral("RECUSADO"));
    QCOMPARE(api.resultados.value("ped-sem-valor").value("status").toString(), QStringLiteral("RECUSADO"));
    QCOMPARE(api.resultados.value("ped-tipo-ruim").value("status").toString(), QStringLiteral("RECUSADO"));
}

void TestNuvem::so_um_computador_sincroniza_e_a_trava_vencida_e_assumida()
{
    NuvemControle_repository repo;
    QVERIFY(repo.adquirirTrava("CAIXA-A", 180));
    QVERIFY(repo.adquirirTrava("CAIXA-A", 180));               // o dono renova
    QVERIFY(!repo.adquirirTrava("CAIXA-B", 180));              // outro computador não pega
    QCOMPARE(repo.donoDaTrava(), QStringLiteral("CAIXA-A"));

    FakeApi api;
    Nuvem_service servicoB;
    servicoB.definirConfigParaTeste(configPara(api));
    servicoB.definirTerminalParaTeste("CAIXA-B");
    const auto r = servicoB.executar(true);
    QVERIFY(!r.executou);                                      // sem ciclo, e nem falou com a API
    QVERIFY(r.mensagem.contains("CAIXA-A"));
    QVERIFY(api.reqs.isEmpty());

    // o dono parou de renovar (programa fechou): a trava vence e o outro assume
    sql("UPDATE sync_trava SET expira_em = '2000-01-01 00:00:00' WHERE id = 1");
    QVERIFY(repo.adquirirTrava("CAIXA-B", 180));
    QCOMPARE(repo.donoDaTrava(), QStringLiteral("CAIXA-B"));

    // quem fecha o programa solta a trava
    repo.liberarTrava("CAIXA-B");
    QVERIFY(repo.donoDaTrava().isEmpty());
    QVERIFY(repo.adquirirTrava("CAIXA-A", 180));
}

void TestNuvem::nuvem_desligada_nao_faz_nada()
{
    FakeApi api;
    NuvemConfig cfg = configPara(api);
    cfg.ativa = false;
    Nuvem_service servico;
    servico.definirConfigParaTeste(cfg);
    servico.definirTerminalParaTeste("CAIXA-A");
    const auto r = servico.executar(true);
    QVERIFY(!r.executou);
    QVERIFY(api.reqs.isEmpty());                               // desligada: nenhuma chamada de rede

    cfg.ativa = true;
    cfg.sincronizaNesteComputador = false;                     // ligada, mas este computador não é o sincronizador
    servico.definirConfigParaTeste(cfg);
    QVERIFY(!servico.executar(true).executou);
    QVERIFY(api.reqs.isEmpty());
    QVERIFY(!servico.status().ativa);
}

void TestNuvem::queda_passageira_so_vira_erro_na_terceira_vez_seguida_e_volta_ao_normal()
{
    // servidor fora do ar: porta fechada (a conexão é recusada na hora)
    NuvemConfig morta;
    morta.ativa = true;
    morta.url = "http://127.0.0.1:1";
    morta.token = "token-de-teste-1234567890";
    morta.intervaloMinutos = 60;
    morta.sincronizaNesteComputador = true;
    Nuvem_service servico;
    servico.definirConfigParaTeste(morta);
    servico.definirTerminalParaTeste("CAIXA-A");

    QVERIFY(!servico.executar(true).ok);
    QVERIFY2(servico.status().ultimoErro.isEmpty(), "1ª queda não pode virar erro");
    QVERIFY(!servico.executar(true).ok);
    QVERIFY2(servico.status().ultimoErro.isEmpty(), "2ª queda também não");
    QVERIFY(!servico.executar(true).ok);
    QVERIFY2(!servico.status().ultimoErro.isEmpty(), "3ª queda seguida avisa");

    // volta a conexão: o erro some e o contador zera
    FakeApi api;
    servico.definirConfigParaTeste(configPara(api));
    const auto r = servico.executar(true);
    QVERIFY2(r.ok, qPrintable(r.mensagem));
    QVERIFY(servico.status().ultimoErro.isEmpty());
    servico.definirConfigParaTeste(morta);
    QVERIFY(!servico.executar(true).ok);
    QVERIFY2(servico.status().ultimoErro.isEmpty(), "depois de recuperar, a contagem recomeça do zero");

    // erro de verdade (chave recusada) avisa na hora
    FakeApi recusa;
    NuvemConfig errada = configPara(recusa);
    errada.token = "chave-que-a-api-nao-conhece-0000";
    servico.definirConfigParaTeste(errada);
    QVERIFY(!servico.executar(true).ok);
    QVERIFY(!servico.status().ultimoErro.isEmpty());
}

void TestNuvem::impressao_digital_muda_quando_uma_venda_e_cancelada_ou_o_estoque_mexe()
{
    const QString base = NuvemExportador::impressaoDigital();
    QVERIFY(!base.isEmpty());
    QCOMPARE(NuvemExportador::impressaoDigital(), base);           // sem mudança, igual

    sql("INSERT INTO vendas2 (cliente, total, data_hora, forma_pagamento, valor_recebido, troco, taxa, valor_final, desconto, esta_pago, id_caixa, id_empresa) "
        "VALUES ('x', 10, '2026-10-22 10:00:00', 'Pix', 10, 0, 0, 10, 0, 1, 1, 1)");
    const QString comVenda = NuvemExportador::impressaoDigital();
    QVERIFY(comVenda != base);                                    // venda nova

    sql("DELETE FROM vendas2 WHERE cliente = 'x' AND data_hora = '2026-10-22 10:00:00'");
    const QString cancelada = NuvemExportador::impressaoDigital();
    QVERIFY(cancelada != comVenda);                               // venda cancelada (apagada) também muda
    QCOMPARE(NuvemExportador::impressaoDigital(), cancelada);
}
