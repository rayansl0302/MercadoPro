#include "nuvemexportador.h"
#include "../infra/databaseconnection_service.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>
#include <QTimeZone>
#include <QDebug>

qint64 Exportacao::totalLinhas() const
{
    qint64 n = 0;
    for (const TabelaExportada &t : tabelas)
        n += t.linhas.size();
    return n;
}

QVariant NuvemExportador::normalizar(const QVariant &v, const QString &tipo)
{
    if (v.isNull() || !v.isValid())
        return QVariant(QMetaType(QMetaType::QString));            // nulo de verdade

    if (tipo == QLatin1String("int"))
        return QString::number(v.toLongLong());
    if (tipo == QLatin1String("dec"))
        return QString::number(v.toDouble(), 'f', 2);
    if (tipo == QLatin1String("real"))
        return QString::number(v.toDouble(), 'g', 12);
    if (tipo == QLatin1String("bool")) {
        if (v.typeId() == QMetaType::Bool)
            return v.toBool() ? QStringLiteral("1") : QStringLiteral("0");
        const QString t = v.toString().trimmed().toLower();
        const bool verdadeiro = t == "1" || t == "t" || t == "true" || t == "sim" || t == "yes";
        return verdadeiro ? QStringLiteral("1") : QStringLiteral("0");
    }
    if (tipo == QLatin1String("ts")) {
        if (v.typeId() == QMetaType::QDateTime)
            return v.toDateTime().toString("yyyy-MM-dd HH:mm:ss");
        if (v.typeId() == QMetaType::QDate)
            return v.toDate().toString(Qt::ISODate);
        return v.toString();
    }
    return v.toString();
}

QString NuvemExportador::checksum(const QList<LinhaExportada> &linhas)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    const QByteArray sepValor(1, char(0x1F));
    const QByteArray fimLinha(1, char(0x1E));
    for (const LinhaExportada &linha : linhas) {
        for (int i = 0; i < linha.size(); ++i) {
            if (i > 0)
                hash.addData(sepValor);
            const QVariant &v = linha.at(i);
            if (v.isNull())
                hash.addData(QByteArrayView("\\N", 2));
            else
                hash.addData(v.toString().toUtf8());
        }
        hash.addData(fimLinha);
    }
    return QString::fromLatin1(hash.result().toHex());
}

QString NuvemExportador::impressaoDigital()
{
    const NuvemContrato &contrato = NuvemContrato::instancia();
    if (!contrato.valido() || !DatabaseConnection_service::open())
        return QString();
    QSqlDatabase db = DatabaseConnection_service::db();
    QCryptographicHash hash(QCryptographicHash::Sha256);
    for (const TabelaContrato &t : contrato.tabelas()) {
        QStringList campos{"count(*)"};
        if (t.pk.size() == 1)
            campos << QStringLiteral("max(%1)").arg(t.pk.first());
        for (const ColunaContrato &col : t.colunas)
            if (col.nome == QLatin1String("atualizado_em"))
                campos << QStringLiteral("max(atualizado_em)");
        QSqlQuery q(db);
        if (!q.exec(QStringLiteral("SELECT %1 FROM %2").arg(campos.join(", "), t.nome)) || !q.next())
            return QString();
        hash.addData(t.nome.toUtf8());
        for (int i = 0; i < campos.size(); ++i) {
            hash.addData(QByteArrayView("\x1f", 1));
            hash.addData(q.value(i).toString().toUtf8());
        }
        hash.addData(QByteArrayView("\x1e", 1));
    }
    return QString::fromLatin1(hash.result().toHex());
}

Exportacao NuvemExportador::exportarTudo()
{
    Exportacao r;
    const NuvemContrato &contrato = NuvemContrato::instancia();
    if (!contrato.valido()) {
        r.erro = "Contrato de dados da nuvem não encontrado.";
        return r;
    }
    if (!DatabaseConnection_service::open()) {
        r.erro = "Banco de dados indisponível.";
        return r;
    }
    QSqlDatabase db = DatabaseConnection_service::db();
    const bool postgres = DatabaseConnection_service::isPostgres();

    // foto única: nenhuma escrita feita durante a leitura aparece pela metade
    QSqlQuery inicio(db);
    if (!inicio.exec(postgres ? "BEGIN ISOLATION LEVEL REPEATABLE READ READ ONLY" : "BEGIN")) {
        r.erro = "Não foi possível iniciar a leitura dos dados: " + inicio.lastError().text();
        return r;
    }
    auto fechar = [&]() { QSqlQuery fim(db); fim.exec("COMMIT"); };

    r.geradoEm = QDateTime::currentDateTime().toOffsetFromUtc(
                     QTimeZone::systemTimeZone().offsetFromUtc(QDateTime::currentDateTime())).toString(Qt::ISODate);

    for (const TabelaContrato &t : contrato.tabelas()) {
        TabelaExportada saida;
        saida.nome = t.nome;
        QStringList nomes;
        for (const ColunaContrato &c : t.colunas) {
            saida.colunas << c.nome;
            nomes << c.nome;
        }
        const QString ordem = t.pk.isEmpty() ? QStringLiteral("1") : t.pk.join(", ");
        QSqlQuery q(db);
        q.setForwardOnly(true);
        if (!q.exec(QString("SELECT %1 FROM %2 ORDER BY %3").arg(nomes.join(", "), t.nome, ordem))) {
            fechar();
            r.erro = QString("Não foi possível ler a tabela %1: %2").arg(t.nome, q.lastError().text());
            return r;
        }
        while (q.next()) {
            LinhaExportada linha;
            for (int i = 0; i < t.colunas.size(); ++i)
                linha.append(normalizar(q.value(i), t.colunas.at(i).tipo));
            saida.linhas.append(linha);
        }
        saida.checksum = checksum(saida.linhas);
        r.tabelas.append(saida);
    }
    fechar();
    r.ok = true;
    return r;
}
