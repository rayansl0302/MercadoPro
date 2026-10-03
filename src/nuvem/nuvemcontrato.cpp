#include "nuvemcontrato.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <QtGlobal>

static void iniciarRecursosNuvem()
{
    Q_INIT_RESOURCE(nuvem);
}

const NuvemContrato &NuvemContrato::instancia()
{
    static const NuvemContrato unico;
    return unico;
}

NuvemContrato::NuvemContrato()
{
    iniciarRecursosNuvem();
    QFile f(QStringLiteral(":/nuvem/contrato.json"));
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning() << "Nuvem: contrato.json não encontrado nos recursos";
        return;
    }
    // o hash não pode depender de quebra de linha (checkout no Windows vira CRLF); a API normaliza igual
    jsonBruto = f.readAll();
    jsonBruto.replace("\r\n", "\n");
    QJsonParseError erro;
    const QJsonDocument doc = QJsonDocument::fromJson(jsonBruto, &erro);
    if (erro.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning() << "Nuvem: contrato.json inválido:" << erro.errorString();
        return;
    }
    const QJsonObject raiz = doc.object();
    versaoContrato = raiz.value("versao").toInt();
    const QJsonObject tabelas = raiz.value("tabelas").toObject();
    for (auto it = tabelas.constBegin(); it != tabelas.constEnd(); ++it) {
        TabelaContrato t;
        t.nome = it.key();
        for (const QJsonValue &v : it.value().toObject().value("pk").toArray())
            t.pk << v.toString();
        for (const QJsonValue &v : it.value().toObject().value("colunas").toArray()) {
            const QJsonObject c = v.toObject();
            t.colunas.append({c.value("n").toString(), c.value("t").toString()});
        }
        if (!t.nome.isEmpty() && !t.colunas.isEmpty())
            tabelasLista.append(t);
    }
}

const TabelaContrato *NuvemContrato::tabela(const QString &nome) const
{
    for (const TabelaContrato &t : tabelasLista)
        if (t.nome == nome)
            return &t;
    return nullptr;
}
