#include "promocaovenda_service.h"
#include "precotaxa_service.h"
#include "../infra/databaseconnection_service.h"

#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <cmath>

QList<PromocaoVendaDTO> PromocaoVenda_service::listar(bool somenteAtivas) const
{
    QList<PromocaoVendaDTO> lista;
    if (!DatabaseConnection_service::open())
        return lista;
    QSqlQuery q(DatabaseConnection_service::db());
    QString sql = QStringLiteral("SELECT id, nome, ativa FROM promocoes_venda");
    if (somenteAtivas)
        sql += QStringLiteral(" WHERE ativa = :ativa");
    sql += QStringLiteral(" ORDER BY nome");
    q.prepare(sql);
    if (somenteAtivas)
        q.bindValue(":ativa", true);
    if (q.exec()) {
        while (q.next()) {
            PromocaoVendaDTO p;
            p.id = q.value(0).toLongLong();
            p.nome = q.value(1).toString();
            p.ativa = q.value(2).toBool();
            lista.append(p);
        }
    }
    return lista;
}

PromocaoVenda_service::Resultado PromocaoVenda_service::salvar(qlonglong id, const QString &nomeInformado,
                                                               bool ativa) const
{
    const QString nome = nomeInformado.simplified();
    if (nome.isEmpty())
        return {false, QStringLiteral("Informe o nome da promoção.")};
    if (!DatabaseConnection_service::open())
        return {false, QStringLiteral("Não foi possível abrir o banco de dados.")};

    for (const PromocaoVendaDTO &outra : listar(false)) {
        if (outra.id != id && outra.nome.compare(nome, Qt::CaseInsensitive) == 0)
            return {false, QStringLiteral("Já existe uma promoção com esse nome.")};
    }

    QSqlQuery q(DatabaseConnection_service::db());
    if (id > 0) {
        q.prepare("UPDATE promocoes_venda SET nome = :nome, ativa = :ativa WHERE id = :id");
        q.bindValue(":id", id);
    } else {
        q.prepare("INSERT INTO promocoes_venda (nome, ativa, criada_em) VALUES (:nome, :ativa, :criada)");
        q.bindValue(":criada", QDateTime::currentDateTime().toString(Qt::ISODate));
    }
    q.bindValue(":nome", nome);
    q.bindValue(":ativa", ativa);
    if (!q.exec())
        return {false, QStringLiteral("Erro ao salvar a promoção: ") + q.lastError().text()};
    return {true, QString()};
}

PromocaoVenda_service::Resultado PromocaoVenda_service::excluir(qlonglong id) const
{
    if (id <= 0 || !DatabaseConnection_service::open())
        return {false, QStringLiteral("Promoção inválida.")};
    // as vendas já feitas guardam o NOME da promoção, então apagar o cadastro não perde o histórico
    QSqlQuery q(DatabaseConnection_service::db());
    q.prepare("DELETE FROM promocoes_venda WHERE id = :id");
    q.bindValue(":id", id);
    if (!q.exec())
        return {false, QStringLiteral("Erro ao excluir a promoção: ") + q.lastError().text()};
    return {true, QString()};
}

double PromocaoVenda_service::descontoAnulaTaxa(const QList<Item> &itens, double taxaPercentual)
{
    if (taxaPercentual <= 0)
        return 0;
    double comTaxa = 0, semTaxa = 0;
    for (const Item &i : itens) {
        comTaxa += i.quantidade * i.precoVenda;
        semTaxa += i.quantidade * PrecoTaxa::semTaxa(i.precoVenda, taxaPercentual);
    }
    const double desconto = std::round((comTaxa - semTaxa) * 100.0) / 100.0;
    return desconto > 0 ? desconto : 0;
}
