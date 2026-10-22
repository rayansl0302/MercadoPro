#include "cardapiolanches.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QMutexLocker>

#include "../infra/apppath_service.h"

static void iniciarRecursoCardapio()
{
    Q_INIT_RESOURCE(cardapio); // fora de namespace, como o Qt exige
}

namespace {
QMutex trava;
QMap<QString, CardapioLanches::Item> cache;
bool carregado = false;

QByteArray lerArquivo(const QString &caminho)
{
    QFile f(caminho);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}
}

namespace CardapioLanches {

QMap<QString, Item> carregarDeJson(const QByteArray &json, QString *erro)
{
    QMap<QString, Item> mapa;
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
        if (erro)
            *erro = pe.error != QJsonParseError::NoError ? pe.errorString() : QStringLiteral("o arquivo deve ser um objeto JSON");
        return mapa;
    }
    const QJsonObject raiz = doc.object();
    for (auto it = raiz.begin(); it != raiz.end(); ++it) {
        if (!it.value().isObject())
            continue;
        const QJsonObject o = it.value().toObject();
        Item item;
        item.nome = o.value("nome").toString().trimmed();
        item.ingredientes = o.value("ingredientes").toString().trimmed();
        const QString g = o.value("grupo").toString().trimmed().toLower();
        item.grupo = (g == "bebida" || g == "bebidas") ? QStringLiteral("bebida") : QStringLiteral("chapa");
        mapa.insert(it.key().trimmed(), item);
    }
    return mapa;
}

static void garantirCarregado()
{
    if (carregado)
        return;
    iniciarRecursoCardapio();
    // o arquivo do usuário vence o padrão; se estiver quebrado, o padrão é usado (a comanda nunca deixa de sair)
    QMap<QString, Item> mapa;
    const QByteArray doUsuario = lerArquivo(QDir(AppPath_service::generalConfigPath()).filePath("cardapio_lanches.json"));
    if (!doUsuario.isEmpty())
        mapa = carregarDeJson(doUsuario);
    if (mapa.isEmpty())
        mapa = carregarDeJson(lerArquivo(":/cardapio/cardapio_lanches.json"));
    cache = mapa;
    carregado = true;
}

Item buscar(const QString &codigo)
{
    QMutexLocker l(&trava);
    garantirCarregado();
    return cache.value(codigo.trimmed());
}

void recarregar()
{
    QMutexLocker l(&trava);
    carregado = false;
    cache.clear();
}

} // namespace CardapioLanches
