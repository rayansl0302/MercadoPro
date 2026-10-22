#include "nuvem_service.h"
#include "nuvemclient.h"
#include "nuvemcontrato.h"
#include "nuvemexportador.h"
#include "pedidoaplicador.h"
#include "../services/caixa_service.h"
#include "../infra/empresaativa.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimeZone>
#include <QTimer>
#include <QDebug>

namespace {
constexpr int kFalhasParaAvisar = 3;                         // queda de conexão só vira "erro" na 3ª vez seguida
constexpr int kMinimoEntreEnviosPorMudancaSeg = 5 * 60;   // envio por mudança: no máximo a cada 5 min
constexpr int kTiqueMs = 60 * 1000;
constexpr int kValidadeTravaSegundos = 180;

QString agora() { return QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"); }

QJsonArray linhaParaJson(const LinhaExportada &linha)
{
    QJsonArray a;
    for (const QVariant &v : linha)
        a.append(v.isNull() ? QJsonValue(QJsonValue::Null) : QJsonValue(v.toString()));
    return a;
}
}

Nuvem_service *Nuvem_service::instancia()
{
    static Nuvem_service *unica = new Nuvem_service(nullptr);
    return unica;
}

Nuvem_service::Nuvem_service(QObject *parent)
    : QObject(parent)
{
}

Nuvem_service::~Nuvem_service() = default;

void Nuvem_service::definirConfigParaTeste(const NuvemConfig &cfg) { configTeste = std::make_unique<NuvemConfig>(cfg); }
void Nuvem_service::definirTerminalParaTeste(const QString &t) { terminalTeste = t; }

NuvemConfig Nuvem_service::configAtual() const
{
    return configTeste ? *configTeste : NuvemConfig::carregar();
}

QString Nuvem_service::terminal() const
{
    return terminalTeste.isEmpty() ? Caixa_service::terminalAtual() : terminalTeste;
}

void Nuvem_service::iniciar()
{
    if (!relogio) {
        relogio = new QTimer(this);
        relogio->setInterval(kTiqueMs);
        connect(relogio, &QTimer::timeout, this, &Nuvem_service::tique);
    }
    if (!relogio->isActive())
        relogio->start();
    // primeira rodada logo depois que o programa abre (sem travar a abertura)
    QTimer::singleShot(15 * 1000, this, &Nuvem_service::tique);
}

void Nuvem_service::parar()
{
    if (relogio)
        relogio->stop();
    abortar();
    controle.liberarTrava(terminal());
}

void Nuvem_service::abortar()
{
    abortado = true;
    if (clienteAtual)
        clienteAtual->abortar();
}

void Nuvem_service::tique()
{
    if (emExecucao)
        return;
    const NuvemConfig cfg = configAtual();
    if (!cfg.pronta())
        return;
    // envio completo só de tempos em tempos; entre eles, só os pedidos (leve, a cada minuto)
    const QDateTime ultimo = QDateTime::fromString(controle.estado("ultimo_envio_ok"), "yyyy-MM-dd HH:mm:ss");
    const bool horaDeEnviar = !ultimo.isValid() || ultimo.secsTo(QDateTime::currentDateTime()) >= cfg.intervaloMinutos * 60;
    // Mudou algo nos dados (venda feita ou cancelada em qualquer caixa, estoque, conta paga...)? Envia sem esperar a hora
    // cheia, mas no máximo a cada 5 minutos. A checagem é barata (contagens), roda a cada minuto.
    bool mudou = false;
    if (!horaDeEnviar && ultimo.isValid() && ultimo.secsTo(QDateTime::currentDateTime()) >= kMinimoEntreEnviosPorMudancaSeg) {
        const QString atual = NuvemExportador::impressaoDigital();
        mudou = !atual.isEmpty() && atual != controle.estado("impressao_digital");
    }
    executar(horaDeEnviar || mudou);
}

Nuvem_service::Status Nuvem_service::status()
{
    Status s;
    s.ativa = configAtual().pronta();
    s.emExecucao = emExecucao;
    s.ultimoCicloOk = controle.estado("ultimo_ciclo_ok");
    s.ultimoEnvioOk = controle.estado("ultimo_envio_ok");
    s.ultimoErro = controle.estado("ultimo_erro");
    s.ultimoResumo = controle.estado("ultimo_resumo");
    s.donoDaTrava = controle.donoDaTrava();
    return s;
}

void Nuvem_service::falhar(ResultadoCiclo &r, const QString &mensagem)
{
    r.ok = false;
    r.mensagem = mensagem;
    // Queda de internet/servidor por um instante é normal. Só mostra erro (chip vermelho) quando se repete;
    // até lá fica só no histórico como aviso. Erros "de verdade" (chave recusada, dados recusados) avisam na hora.
    const bool passageira = mensagem.contains("Tempo esgotado", Qt::CaseInsensitive) || mensagem.contains("Sem conexão", Qt::CaseInsensitive);
    const int seguidas = passageira ? controle.estado("falhas_seguidas", "0").toInt() + 1 : kFalhasParaAvisar;
    controle.definirEstado("falhas_seguidas", QString::number(seguidas));
    if (seguidas < kFalhasParaAvisar) {
        controle.registrarLog("AVISO", QStringLiteral("%1 (tentativa %2 de %3; tenta de novo em 1 minuto)").arg(mensagem).arg(seguidas).arg(kFalhasParaAvisar));
        return;
    }
    controle.definirEstado("ultimo_erro", QStringLiteral("%1 — %2").arg(agora(), mensagem));
    controle.registrarLog("ERRO", mensagem);
}

Nuvem_service::ResultadoCiclo Nuvem_service::testarConexao()
{
    ResultadoCiclo r;
    r.executou = true;
    const NuvemConfig cfg = configAtual();
    const QString erro = cfg.validar();
    if (!erro.isEmpty()) {
        r.mensagem = erro;
        return r;
    }
    NuvemClient cliente(cfg.url, cfg.token);
    const RespostaHttp resp = cliente.get("/api/loja/ping", 15000);
    if (!resp.ok()) {
        r.mensagem = resp.status == 401 ? QStringLiteral("A API recusou a chave da loja (token inválido).")
                                        : QStringLiteral("Não foi possível falar com a API: %1").arg(resp.mensagem());
        return r;
    }
    r.ok = true;
    const QString loja = resp.json.object().value("loja").toString();
    r.mensagem = loja.isEmpty() ? QStringLiteral("Conexão com a nuvem funcionando.")
                                : QStringLiteral("Conexão funcionando. Loja: %1.").arg(loja);
    return r;
}

Nuvem_service::ResultadoCiclo Nuvem_service::executar(bool comDados)
{
    ResultadoCiclo r;
    if (emExecucao)
        return r;
    const NuvemConfig cfg = configAtual();
    if (!cfg.pronta())
        return r;

    // só um computador sincroniza por vez
    if (!controle.adquirirTrava(terminal(), kValidadeTravaSegundos)) {
        r.mensagem = QStringLiteral("Outro computador (%1) está sincronizando.").arg(controle.donoDaTrava());
        return r;
    }

    emExecucao = true;
    abortado = false;
    r.executou = true;
    emit statusMudou();

    NuvemClient cliente(cfg.url, cfg.token);
    clienteAtual = &cliente;
    auto terminar = [&]() {
        clienteAtual = nullptr;
        emExecucao = false;
        emit statusMudou();
    };

    // 1. loja online
    QJsonObject hb;
    hb["terminal"] = terminal();
    hb["versaoApp"] = QCoreApplication::applicationVersion();
    hb["contratoVersao"] = NuvemContrato::instancia().versao();
    hb["empresaAtiva"] = double(EmpresaAtiva::id());
    RespostaHttp respHb = cliente.enviar("POST", "/api/loja/heartbeat", hb, 20000);
    if (!respHb.ok() && respHb.status == 0 && !abortado)   // sem resposta (tempo esgotado/queda): tenta mais uma vez antes de desistir
        respHb = cliente.enviar("POST", "/api/loja/heartbeat", hb, 15000);
    if (!respHb.ok()) {
        falhar(r, respHb.status == 401 ? QStringLiteral("A API recusou a chave da loja (token inválido).")
                                       : QStringLiteral("Sem conexão com a nuvem: %1").arg(respHb.mensagem()));
        terminar();
        return r;
    }

    // 2 e 3. pedidos do painel
    QString erroPedidos;
    r.pedidosAplicados = processarPedidos(cliente, &erroPedidos);
    if (!erroPedidos.isEmpty()) {
        falhar(r, erroPedidos);
        terminar();
        return r;
    }

    // 4. foto completa dos dados
    if ((comDados || r.pedidosAplicados > 0) && !abortado) {
        const QString digitalAntes = NuvemExportador::impressaoDigital(); // antes de ler: mudança durante o envio gera outro envio
        QString erroDados;
        if (!enviarDados(cliente, &r.linhasEnviadas, &erroDados)) {
            falhar(r, erroDados);
            terminar();
            return r;
        }
        controle.definirEstado("ultimo_envio_ok", agora());
        controle.definirEstado("impressao_digital", digitalAntes);
    }

    r.ok = true;
    r.mensagem = QStringLiteral("%1 pedido(s) aplicado(s), %2 linha(s) enviada(s).")
                     .arg(r.pedidosAplicados).arg(r.linhasEnviadas);
    controle.definirEstado("ultimo_ciclo_ok", agora());
    controle.definirEstado("ultimo_erro", QString());
    controle.definirEstado("falhas_seguidas", "0");
    controle.definirEstado("ultimo_resumo", r.mensagem);
    if (r.pedidosAplicados > 0 || r.linhasEnviadas > 0)
        controle.registrarLog("INFO", r.mensagem);
    controle.limparLogsAntigos();
    terminar();
    return r;
}

int Nuvem_service::processarPedidos(NuvemClient &cliente, QString *erro)
{
    auto enviarResultado = [&](const PedidoAplicadoDTO &p) -> bool {
        QJsonObject corpo;
        corpo["status"] = p.status;
        corpo["mensagem"] = p.mensagem;
        corpo["detalhe"] = QJsonDocument::fromJson(p.detalhe.toUtf8()).object();
        const RespostaHttp resp = cliente.enviar("POST", "/api/loja/pedidos/" + p.idPedido + "/resultado", corpo);
        if (resp.ok() || resp.status == 404 || resp.status == 409) {   // 404/409: a nuvem já resolveu esse pedido
            controle.marcarPedidoEnviado(p.idPedido);
            return true;
        }
        return false;
    };

    // resultados que ficaram para trás (a internet caiu depois de aplicar)
    for (const PedidoAplicadoDTO &p : controle.pedidosNaoEnviados())
        enviarResultado(p);

    const RespostaHttp resp = cliente.get("/api/loja/pedidos");
    if (!resp.ok()) {
        *erro = QStringLiteral("Não foi possível buscar os pedidos do painel: %1").arg(resp.mensagem());
        return 0;
    }
    int aplicados = 0;
    PedidoAplicador aplicador;
    for (const QJsonValue &v : resp.json.object().value("pedidos").toArray()) {
        if (abortado)
            break;
        const QJsonObject pedido = v.toObject();
        const QString id = pedido.value("id").toString();
        if (id.isEmpty())
            continue;

        PedidoAplicadoDTO guardado = controle.pedido(id);
        if (!guardado.existe()) {
            // nunca visto: aplica pelos serviços do sistema e guarda o resultado ANTES de avisar a nuvem
            // (se a internet cair agora, o pedido não é aplicado de novo: só o aviso é reenviado)
            const ResultadoPedido res = aplicador.aplicar(pedido);
            guardado.idPedido = id;
            guardado.tipo = pedido.value("tipo").toString();
            guardado.status = res.status;
            guardado.mensagem = res.mensagem;
            guardado.detalhe = QString::fromUtf8(QJsonDocument(res.detalhe).toJson(QJsonDocument::Compact));
            if (!controle.registrarPedidoAplicado(guardado)) {
                *erro = QStringLiteral("Não foi possível registrar o pedido %1 no banco da loja.").arg(id);
                return aplicados;
            }
            if (res.aplicado())
                ++aplicados;
            controle.registrarLog(res.aplicado() ? "INFO" : "AVISO",
                                  QStringLiteral("Pedido %1 (%2): %3 — %4").arg(id, guardado.tipo, res.status, res.mensagem));
        }
        if (!guardado.enviado)
            enviarResultado(guardado);
    }
    return aplicados;
}

bool Nuvem_service::enviarDados(NuvemClient &cliente, qint64 *linhasEnviadas, QString *erro)
{
    NuvemExportador exportador;
    const Exportacao exp = exportador.exportarTudo();
    if (!exp.ok) {
        *erro = "Não foi possível ler os dados da loja: " + exp.erro;
        return false;
    }
    const NuvemContrato &contrato = NuvemContrato::instancia();

    QJsonArray tabelasAviso;
    for (const TabelaExportada &t : exp.tabelas) {
        QJsonObject o;
        o["nome"] = t.nome;
        o["linhas"] = double(t.linhas.size());
        o["checksum"] = t.checksum;
        QJsonArray cols;
        for (const QString &c : t.colunas)
            cols.append(c);
        o["colunas"] = cols;
        tabelasAviso.append(o);
    }
    QJsonObject abrir;
    abrir["contratoVersao"] = contrato.versao();
    abrir["contratoHash"] = QString::fromLatin1(QCryptographicHash::hash(contrato.bruto(), QCryptographicHash::Sha256).toHex());
    abrir["versaoApp"] = QCoreApplication::applicationVersion();
    abrir["terminal"] = terminal();
    abrir["fuso"] = QString::fromUtf8(QTimeZone::systemTimeZoneId());
    abrir["geradoEm"] = exp.geradoEm;
    abrir["tabelas"] = tabelasAviso;
    const RespostaHttp resAbrir = cliente.enviar("POST", "/api/loja/ciclo/iniciar", abrir);
    if (!resAbrir.ok()) {
        *erro = QStringLiteral("A nuvem recusou iniciar o envio: %1").arg(resAbrir.mensagem());
        return false;
    }
    const QString ciclo = resAbrir.json.object().value("cicloId").toString();
    if (ciclo.isEmpty()) {
        *erro = "A nuvem não devolveu o identificador do ciclo.";
        return false;
    }

    auto cancelar = [&]() { cliente.enviar("POST", "/api/loja/ciclo/" + ciclo + "/cancelar", QJsonObject(), 10000); };

    qint64 enviadas = 0;
    for (const TabelaExportada &t : exp.tabelas) {
        QJsonArray cols;
        for (const QString &c : t.colunas)
            cols.append(c);
        int lote = 0;
        for (int i = 0; i < t.linhas.size(); i += linhasPorLote, ++lote) {
            if (abortado) {
                *erro = "Envio cancelado.";
                cancelar();
                return false;
            }
            // mantém a trava viva durante envios demorados
            controle.adquirirTrava(terminal(), kValidadeTravaSegundos);
            QJsonArray linhas;
            for (int j = i; j < qMin(i + linhasPorLote, int(t.linhas.size())); ++j)
                linhas.append(linhaParaJson(t.linhas.at(j)));
            QJsonObject corpo;
            corpo["lote"] = lote;
            corpo["colunas"] = cols;
            corpo["linhas"] = linhas;
            const RespostaHttp resLote = cliente.enviar("PUT", QStringLiteral("/api/loja/ciclo/%1/tabela/%2").arg(ciclo, t.nome), corpo);
            if (!resLote.ok()) {
                *erro = QStringLiteral("Falha ao enviar a tabela %1 (lote %2): %3").arg(t.nome).arg(lote).arg(resLote.mensagem());
                cancelar();
                return false;
            }
            enviadas += linhas.size();
        }
    }

    QJsonObject fim;
    fim["tabelas"] = tabelasAviso;
    const RespostaHttp resFim = cliente.enviar("POST", "/api/loja/ciclo/" + ciclo + "/concluir", fim);
    if (!resFim.ok()) {
        *erro = QStringLiteral("A nuvem não confirmou o envio: %1").arg(resFim.mensagem());
        cancelar();
        return false;
    }
    *linhasEnviadas = enviadas;
    return true;
}
