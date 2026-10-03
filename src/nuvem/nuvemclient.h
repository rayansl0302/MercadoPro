#ifndef NUVEMCLIENT_H
#define NUVEMCLIENT_H

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

struct RespostaHttp {
    int status = 0;                       // 0 = não chegou resposta (rede, tempo esgotado, abortado)
    QByteArray corpo;
    QJsonDocument json;
    QString erroRede;                     // vazio quando houve resposta HTTP
    bool ok() const { return erroRede.isEmpty() && status >= 200 && status < 300; }
    // texto para mostrar/guardar: usa a mensagem que a API mandou, se houver
    QString mensagem() const;
};

// Cliente HTTPS da API do painel. Cada chamada espera a resposta (com um laço de eventos interno, então a
// tela não trava) e termina sempre: sucesso, erro HTTP, falha de rede, tempo esgotado ou abortada.
class NuvemClient : public QObject
{
    Q_OBJECT
public:
    NuvemClient(const QString &urlBase, const QString &token, QObject *parent = nullptr);

    RespostaHttp get(const QString &caminho, int timeoutMs = 20000);
    RespostaHttp enviar(const QByteArray &metodo, const QString &caminho, const QJsonObject &corpo,
                        int timeoutMs = 60000);
    // chamada em andamento termina com erro "abortado" (ex.: o programa vai fechar)
    void abortar();

private:
    QString base;
    QString token;
    QNetworkAccessManager *rede;
    QNetworkReply *atual = nullptr;
    bool abortado = false;

    RespostaHttp executar(const QByteArray &metodo, const QString &caminho, const QByteArray &corpo, int timeoutMs);
};

#endif // NUVEMCLIENT_H
