#include "nuvemclient.h"
#include <QCoreApplication>
#include <QEventLoop>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

QString RespostaHttp::mensagem() const
{
    if (!erroRede.isEmpty())
        return erroRede;
    if (json.isObject()) {
        const QString m = json.object().value("mensagem").toString();
        if (!m.isEmpty())
            return m;
    }
    return QStringLiteral("HTTP %1").arg(status);
}

NuvemClient::NuvemClient(const QString &urlBase, const QString &tk, QObject *parent)
    : QObject(parent)
    , base(urlBase.trimmed())
    , token(tk)
    , rede(new QNetworkAccessManager(this))
{
    while (base.endsWith('/'))
        base.chop(1);
}

void NuvemClient::abortar()
{
    abortado = true;
    if (atual)
        atual->abort();
}

RespostaHttp NuvemClient::get(const QString &caminho, int timeoutMs)
{
    return executar("GET", caminho, QByteArray(), timeoutMs);
}

RespostaHttp NuvemClient::enviar(const QByteArray &metodo, const QString &caminho, const QJsonObject &corpo,
                                 int timeoutMs)
{
    return executar(metodo, caminho, QJsonDocument(corpo).toJson(QJsonDocument::Compact), timeoutMs);
}

RespostaHttp NuvemClient::executar(const QByteArray &metodo, const QString &caminho, const QByteArray &corpo,
                                   int timeoutMs)
{
    RespostaHttp r;
    if (abortado) {
        r.erroRede = "Operação cancelada.";
        return r;
    }
    QNetworkRequest req(QUrl(base + caminho));
    req.setRawHeader("Authorization", "Bearer " + token.toUtf8());
    req.setRawHeader("Accept", "application/json");
    req.setRawHeader("User-Agent", QByteArray("QEstoqueLoja/") + QCoreApplication::applicationVersion().toUtf8());
    if (!corpo.isEmpty())
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json; charset=utf-8");
    req.setTransferTimeout(timeoutMs);

    QNetworkReply *reply = metodo == "GET" ? rede->get(req) : rede->sendCustomRequest(req, metodo, corpo);
    atual = reply;

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QTimer limite;
    limite.setSingleShot(true);
    QObject::connect(&limite, &QTimer::timeout, reply, &QNetworkReply::abort);
    limite.start(timeoutMs + 2000);        // rede de segurança além do tempo do Qt
    if (!reply->isFinished())
        loop.exec();
    limite.stop();
    atual = nullptr;

    r.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    r.corpo = reply->readAll();
    if (reply->error() != QNetworkReply::NoError && r.status == 0) {
        r.erroRede = abortado ? QStringLiteral("Operação cancelada.")
                              : (reply->error() == QNetworkReply::OperationCanceledError
                                     ? QStringLiteral("Tempo esgotado ao falar com a API.")
                                     : QStringLiteral("Falha de rede: %1").arg(reply->errorString()));
    }
    if (!r.corpo.isEmpty())
        r.json = QJsonDocument::fromJson(r.corpo);
    reply->deleteLater();
    return r;
}
