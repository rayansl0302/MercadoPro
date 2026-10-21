#include "atualizador_service.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>

#include "apppath_service.h"

#ifndef MERCADOPRO_VERSAO
#define MERCADOPRO_VERSAO "0.0.0"
#endif

namespace {
constexpr qint64 kTamanhoMaximo = 400LL * 1024 * 1024; // instalador acima disso é recusado
constexpr int kTempoApiMs = 10 * 1000;                  // consulta à API
constexpr int kTempoDownloadMs = 60 * 1000;             // sem receber nenhum byte por 60 s = desiste
constexpr int kMaximoTentativas = 2;
QUrl urlApiDeTeste;

QSettings config()
{
    return QSettings(AppPath_service::configPath(), QSettings::IniFormat);
}
}

Atualizador_service *Atualizador_service::instancia()
{
    static Atualizador_service s;
    return &s;
}

Atualizador_service::Atualizador_service(QObject *parent) : QObject(parent) {}

QString Atualizador_service::versaoAtual()
{
    return QStringLiteral(MERCADOPRO_VERSAO);
}

int Atualizador_service::compararVersoes(const QString &a, const QString &b)
{
    auto partes = [](QString v) {
        v = v.trimmed();
        if (v.startsWith('v', Qt::CaseInsensitive))
            v.remove(0, 1);
        v = v.section('-', 0, 0); // ignora sufixo ("-rc1")
        QList<int> n;
        for (const QString &p : v.split('.', Qt::SkipEmptyParts))
            n << p.toInt();
        return n;
    };
    const QList<int> x = partes(a), y = partes(b);
    const int total = qMax(x.size(), y.size());
    for (int i = 0; i < total; ++i) {
        const int xi = i < x.size() ? x.at(i) : 0, yi = i < y.size() ? y.at(i) : 0;
        if (xi != yi)
            return xi < yi ? -1 : 1;
    }
    return 0;
}

bool Atualizador_service::urlPermitida(const QUrl &url)
{
    if (url.scheme() != QLatin1String("https"))
        return false;
    const QString h = url.host().toLower();
    return h == "api.github.com" || h == "github.com" || h.endsWith(".githubusercontent.com");
}

std::optional<Atualizador_service::Release> Atualizador_service::interpretarRelease(const QByteArray &json)
{
    const QJsonObject o = QJsonDocument::fromJson(json).object();
    if (o.isEmpty() || o.value("draft").toBool() || o.value("prerelease").toBool())
        return std::nullopt;
    Release r;
    r.versao = o.value("tag_name").toString().trimmed();
    if (r.versao.startsWith('v', Qt::CaseInsensitive))
        r.versao.remove(0, 1);
    if (r.versao.isEmpty())
        return std::nullopt;

    QString nomeInstalador;
    const QJsonArray assets = o.value("assets").toArray();
    for (const QJsonValue &v : assets) {
        const QJsonObject a = v.toObject();
        const QString nome = a.value("name").toString();
        if (nome.startsWith("MercadoPro-Setup-") && nome.endsWith(".exe")) {
            nomeInstalador = nome;
            r.urlInstalador = QUrl(a.value("browser_download_url").toString());
            r.tamanho = static_cast<qint64>(a.value("size").toDouble());
        }
    }
    if (nomeInstalador.isEmpty())
        return std::nullopt;
    for (const QJsonValue &v : assets) {
        const QJsonObject a = v.toObject();
        if (a.value("name").toString() == nomeInstalador + ".sha256")
            r.urlHash = QUrl(a.value("browser_download_url").toString());
    }
    // sem o arquivo de hash não há como conferir o instalador: não atualiza
    if (!r.urlInstalador.isValid() || !r.urlHash.isValid())
        return std::nullopt;
    return r;
}

QString Atualizador_service::hashDoTexto(const QString &conteudo)
{
    static const QRegularExpression re(QStringLiteral("\\b([0-9a-fA-F]{64})\\b"));
    const auto m = re.match(conteudo);
    return m.hasMatch() ? m.captured(1).toLower() : QString();
}

QString Atualizador_service::sha256DoArquivo(const QString &caminho)
{
    QFile f(caminho);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    QCryptographicHash h(QCryptographicHash::Sha256);
    if (!h.addData(&f))
        return {};
    return QString::fromLatin1(h.result().toHex());
}

void Atualizador_service::definirUrlApiParaTeste(const QUrl &url)
{
    urlApiDeTeste = url;
}

bool Atualizador_service::ativo() const
{
#ifdef QEL_MODO_DESENVOLVIMENTO
    return false; // build de desenvolvimento nunca se atualiza sozinha
#endif
    if (!urlApiDeTeste.isEmpty())
        return true;
    if (versaoAtual() == QLatin1String("0.0.0"))
        return false; // compilação local sem versão definida
    return config().value("atualizacao/ativa", true).toBool();
}

bool Atualizador_service::manualPermitido() const
{
    if (!urlApiDeTeste.isEmpty())
        return true;
    return versaoAtual() != QLatin1String("0.0.0"); // compilação local sem versão não tem o que comparar
}

void Atualizador_service::iniciar(std::function<bool()> aplicarSePuder)
{
    podeAplicar = std::move(aplicarSePuder);
    if (!ativo())
        return;
    if (!relogio) {
        relogio = new QTimer(this);
        connect(relogio, &QTimer::timeout, this, [this]() { verificarAgora(false); });
        aplicador = new QTimer(this);
        aplicador->setInterval(30 * 1000);
        connect(aplicador, &QTimer::timeout, this, &Atualizador_service::tentarAplicar);
    }
    relogio->start(6 * 60 * 60 * 1000);
    QTimer::singleShot(20 * 1000, this, [this]() { verificarAgora(false); }); // sem atrapalhar a abertura
}

void Atualizador_service::parar()
{
    if (relogio)
        relogio->stop();
    if (aplicador)
        aplicador->stop();
}

void Atualizador_service::resetarEstadoParaTeste()
{
    emAndamento = false;
    versaoPronta.clear();
    instaladorPronto.clear();
}

void Atualizador_service::falha(const QString &motivo)
{
    emAndamento = false;
    qWarning().noquote() << "Atualizador:" << motivo;
    emit falhou(motivo);
}

void Atualizador_service::verificarAgora(bool manual)
{
    if (emAndamento) {
        if (manual)
            emit falhou("Já existe uma verificação em andamento. Tente de novo em instantes.");
        return;
    }
    if (manual ? !manualPermitido() : !ativo()) {
        if (manual)
            emit falhou("Esta é uma versão de desenvolvimento, sem número de versão para comparar.");
        return;
    }
    if (!instaladorPronto.isEmpty()) { // já baixada e conferida: só falta um momento ocioso
        if (manual)
            emit atualizacaoPronta(versaoPronta, instaladorPronto);
        else if (aplicador && !aplicador->isActive())
            aplicador->start();
        return;
    }
    if (!rede)
        rede = new QNetworkAccessManager(this);

    QUrl api = urlApiDeTeste;
    if (api.isEmpty()) {
        const QString repo = config().value("atualizacao/repo", "rayansl0302/MercadoPro").toString();
        api = QUrl(QStringLiteral("https://api.github.com/repos/%1/releases/latest").arg(repo));
    }
    const bool teste = !urlApiDeTeste.isEmpty();
    if (!teste && !urlPermitida(api)) {
        falha("endereço da API não permitido");
        return;
    }
    emAndamento = true;
    QNetworkRequest req(api);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("MercadoPro/%1").arg(versaoAtual()));
    req.setRawHeader("Accept", "application/vnd.github+json");
    req.setTransferTimeout(kTempoApiMs);
    QNetworkReply *reply = rede->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, manual]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) { // offline, DNS, 403 (limite da API), 404 (sem release)...
            falha("consulta falhou: " + reply->errorString());
            return;
        }
        const auto release = interpretarRelease(reply->readAll());
        if (!release) {
            emAndamento = false;
            emit semAtualizacao();
            return;
        }
        if (compararVersoes(release->versao, versaoAtual()) <= 0) {
            emAndamento = false;
            emit semAtualizacao();
            return;
        }
        QSettings ini = config();
        if (!manual && ini.value("atualizacao/tentativas_versao").toString() == release->versao &&
            ini.value("atualizacao/tentativas").toInt() >= kMaximoTentativas) {
            falha("versão " + release->versao + " já falhou " + QString::number(kMaximoTentativas) + " vezes; aguardando uma versão mais nova");
            return;
        }
        baixar(*release);
    });
}

void Atualizador_service::baixar(const Release &r)
{
    if (r.tamanho > kTamanhoMaximo) {
        falha("instalador grande demais");
        return;
    }
    if (!urlPermitida(r.urlInstalador) || !urlPermitida(r.urlHash)) {
        if (urlApiDeTeste.isEmpty()) {
            falha("endereço do instalador não permitido");
            return;
        }
    }
    auto abrir = [this](const QUrl &u, int tempo) {
        QNetworkRequest req(u);
        req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("MercadoPro/%1").arg(versaoAtual()));
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        req.setTransferTimeout(tempo);
        QNetworkReply *rep = rede->get(req);
        // o GitHub redireciona os arquivos para outro host: só aceita hosts conhecidos
        connect(rep, &QNetworkReply::redirected, this, [this, rep](const QUrl &destino) {
            if (urlApiDeTeste.isEmpty() && !urlPermitida(destino))
                rep->abort();
        });
        return rep;
    };

    // 1) o hash publicado
    QNetworkReply *hashReply = abrir(r.urlHash, kTempoApiMs);
    connect(hashReply, &QNetworkReply::finished, this, [this, r, hashReply, abrir]() {
        hashReply->deleteLater();
        const QString esperado = hashDoTexto(QString::fromUtf8(hashReply->readAll()));
        if (hashReply->error() != QNetworkReply::NoError || esperado.isEmpty()) {
            falha("não foi possível obter o hash do instalador");
            return;
        }
        // 2) o instalador, gravado aos poucos (não carrega 100+ MB na memória)
        const QString pasta = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation)).filePath("MercadoPro-atualizacao");
        QDir().mkpath(pasta);
        const QString destino = QDir(pasta).filePath(QStringLiteral("MercadoPro-Setup-%1.exe").arg(r.versao));
        if (sha256DoArquivo(destino) == esperado) { // já baixado antes (ex.: reabriu o programa)
            emAndamento = false;
            versaoPronta = r.versao;
            instaladorPronto = destino;
            emit atualizacaoPronta(r.versao, destino);
            if (aplicador)
                aplicador->start();
            return;
        }
        auto *arquivo = new QFile(destino + ".parte", this);
        if (!arquivo->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            arquivo->deleteLater();
            falha("não foi possível gravar o instalador em " + pasta);
            return;
        }
        QNetworkReply *rep = abrir(r.urlInstalador, kTempoDownloadMs);
        connect(rep, &QNetworkReply::readyRead, this, [this, rep, arquivo]() {
            arquivo->write(rep->readAll());
            if (arquivo->size() > kTamanhoMaximo)
                rep->abort();
        });
        connect(rep, &QNetworkReply::finished, this, [this, r, rep, arquivo, destino, esperado]() {
            rep->deleteLater();
            arquivo->write(rep->readAll());
            arquivo->close();
            const bool ok = rep->error() == QNetworkReply::NoError;
            if (!ok) {
                arquivo->remove();
                arquivo->deleteLater();
                falha("download falhou: " + rep->errorString());
                return;
            }
            QFile::remove(destino);
            arquivo->rename(destino);
            arquivo->deleteLater();
            // 3) confere o hash: arquivo corrompido ou trocado nunca é executado
            if (sha256DoArquivo(destino) != esperado) {
                QFile::remove(destino);
                falha("hash do instalador não confere; descartado");
                return;
            }
            emAndamento = false;
            versaoPronta = r.versao;
            instaladorPronto = destino;
            emit atualizacaoPronta(r.versao, destino);
            if (aplicador)
                aplicador->start();
        });
    });
}

void Atualizador_service::tentarAplicar()
{
    if (instaladorPronto.isEmpty() || !QFile::exists(instaladorPronto))
        return;
    // só instala quando o PDV está ocioso (sem venda aberta nem diálogo modal); senão tenta de novo em 30 s
    if (podeAplicar && !podeAplicar())
        return;
    executarInstalador();
}

bool Atualizador_service::aplicarAgora()
{
    return executarInstalador();
}

bool Atualizador_service::executarInstalador()
{
    if (instaladorPronto.isEmpty() || !QFile::exists(instaladorPronto))
        return false;
    if (!urlApiDeTeste.isEmpty())
        return false; // em teste nunca executa o instalador

    QSettings ini = config();
    const int tentativas = ini.value("atualizacao/tentativas_versao").toString() == versaoPronta
                               ? ini.value("atualizacao/tentativas").toInt() : 0;
    ini.setValue("atualizacao/tentativas_versao", versaoPronta);
    ini.setValue("atualizacao/tentativas", tentativas + 1);
    ini.sync();

    const QString log = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation)).filePath("MercadoPro-atualizacao.log");
    // /REINICIAR=1: o instalador reabre o programa quando terminar (veja [Run] em instalador/MercadoPro.iss)
    const QStringList args{"/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART", "/CLOSEAPPLICATIONS", "/REINICIAR=1", "/LOG=" + log};
    if (!QProcess::startDetached(instaladorPronto, args)) {
        falha("não foi possível iniciar o instalador");
        return false;
    }
    if (aplicador)
        aplicador->stop();
    QTimer::singleShot(1500, qApp, &QCoreApplication::quit); // sai para o instalador poder trocar os arquivos
    return true;
}
