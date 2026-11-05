#include "maquinacartao_repository.h"
#include "../infra/databaseconnection_service.h"
#include <QDateTime>
#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>

namespace {
void setErro(QString *erro, const QString &t) { if (erro) *erro = t; }

MaquinaCartaoDTO ler(QSqlQuery &q)
{
    MaquinaCartaoDTO m;
    m.id = q.value("id").toLongLong();
    m.nome = q.value("nome").toString();
    m.taxaDebito = q.value("taxa_debito").toDouble();
    m.taxaCredito = q.value("taxa_credito").toDouble();
    m.ativa = q.value("ativa").toBool();
    m.criadaEm = q.value("criada_em").toString();
    return m;
}
}

MaquinaCartao_repository::MaquinaCartao_repository()
    : db(DatabaseConnection_service::db())
{
}

QList<MaquinaCartaoDTO> MaquinaCartao_repository::listar(bool incluirInativas)
{
    QList<MaquinaCartaoDTO> lista;
    if (!DatabaseConnection_service::open())
        return lista;
    QSqlQuery q(db);
    QString sql = "SELECT id, nome, taxa_debito, taxa_credito, ativa, criada_em FROM maquinas_cartao";
    if (!incluirInativas)
        sql += " WHERE ativa = :ativa";
    sql += " ORDER BY nome, id";
    q.prepare(sql);
    if (!incluirInativas)
        q.bindValue(":ativa", true);
    if (!q.exec()) {
        qDebug() << "Erro ao listar maquinas_cartao:" << q.lastError().text();
        return lista;
    }
    while (q.next())
        lista << ler(q);
    return lista;
}

MaquinaCartaoDTO MaquinaCartao_repository::get(qlonglong id)
{
    MaquinaCartaoDTO m;
    if (!DatabaseConnection_service::open())
        return m;
    QSqlQuery q(db);
    q.prepare("SELECT id, nome, taxa_debito, taxa_credito, ativa, criada_em FROM maquinas_cartao WHERE id = :id");
    q.bindValue(":id", id);
    if (q.exec() && q.next())
        m = ler(q);
    return m;
}

qlonglong MaquinaCartao_repository::inserir(const MaquinaCartaoDTO &m, QString *erro)
{
    if (!DatabaseConnection_service::open()) {
        setErro(erro, "Banco de dados indisponível.");
        return -1;
    }
    QSqlQuery q(db);
    q.prepare("INSERT INTO maquinas_cartao (nome, taxa_debito, taxa_credito, ativa, criada_em) "
              "VALUES (:nome, :deb, :cred, :ativa, :criada)");
    q.bindValue(":nome", m.nome);
    q.bindValue(":deb", m.taxaDebito);
    q.bindValue(":cred", m.taxaCredito);
    q.bindValue(":ativa", m.ativa);
    q.bindValue(":criada", QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));
    if (!q.exec()) {
        setErro(erro, q.lastError().text());
        return -1;
    }
    QVariant id = q.lastInsertId();
    if (!id.isValid()) {
        QSqlQuery q2(db);
        if (q2.exec("SELECT MAX(id) FROM maquinas_cartao") && q2.next())
            id = q2.value(0);
    }
    return id.toLongLong();
}

bool MaquinaCartao_repository::atualizar(const MaquinaCartaoDTO &m, QString *erro)
{
    if (!DatabaseConnection_service::open()) {
        setErro(erro, "Banco de dados indisponível.");
        return false;
    }
    QSqlQuery q(db);
    q.prepare("UPDATE maquinas_cartao SET nome = :nome, taxa_debito = :deb, taxa_credito = :cred, ativa = :ativa "
              "WHERE id = :id");
    q.bindValue(":nome", m.nome);
    q.bindValue(":deb", m.taxaDebito);
    q.bindValue(":cred", m.taxaCredito);
    q.bindValue(":ativa", m.ativa);
    q.bindValue(":id", m.id);
    if (!q.exec()) {
        setErro(erro, q.lastError().text());
        return false;
    }
    return true;
}

bool MaquinaCartao_repository::nomeJaExiste(const QString &nome, qlonglong ignorarId)
{
    if (!DatabaseConnection_service::open())
        return false;
    QSqlQuery q(db);
    q.prepare("SELECT COUNT(*) FROM maquinas_cartao WHERE LOWER(nome) = LOWER(:nome) AND id <> :id");
    q.bindValue(":nome", nome);
    q.bindValue(":id", ignorarId);
    return q.exec() && q.next() && q.value(0).toInt() > 0;
}

QList<CustoMaquinaDTO> MaquinaCartao_repository::custoPorMaquina(const QDate &de, const QDate &ate)
{
    QList<CustoMaquinaDTO> lista;
    if (!DatabaseConnection_service::open())
        return lista;
    // vendas no cartão + recebimentos de fiado no cartão; só entra o que tem taxa de máquina gravada
    QSqlQuery q(db);
    q.prepare(
        "SELECT x.id_maquina, COALESCE(m.nome, ''), COUNT(*), SUM(x.valor_final), "
        "       SUM(x.valor_final * x.taxa_maquina / 100.0) "
        "FROM ("
        "  SELECT id_maquina, taxa_maquina, valor_final, data_hora FROM vendas2 "
        "  WHERE forma_pagamento IN ('Crédito', 'Débito') AND taxa_maquina IS NOT NULL "
        "  UNION ALL "
        "  SELECT id_maquina, taxa_maquina, valor_final, data_hora FROM entradas_vendas "
        "  WHERE forma_pagamento IN ('Crédito', 'Débito') AND taxa_maquina IS NOT NULL "
        "  UNION ALL "
        // parte no cartão de vendas pagas em mais de uma forma
        "  SELECT p.id_maquina, p.taxa_maquina, p.valor AS valor_final, v.data_hora "
        "  FROM venda_pagamentos p JOIN vendas2 v ON v.id = p.id_venda "
        "  WHERE p.forma_pagamento IN ('Crédito', 'Débito') AND p.taxa_maquina IS NOT NULL"
        ") x LEFT JOIN maquinas_cartao m ON m.id = x.id_maquina "
        "WHERE x.data_hora >= :de AND x.data_hora < :ate "
        "GROUP BY x.id_maquina, m.nome ORDER BY 5 DESC");
    q.bindValue(":de", de.toString("yyyy-MM-dd") + " 00:00:00");
    q.bindValue(":ate", ate.addDays(1).toString("yyyy-MM-dd") + " 00:00:00");
    if (!q.exec()) {
        qDebug() << "Erro ao somar custo por maquina:" << q.lastError().text();
        return lista;
    }
    while (q.next()) {
        CustoMaquinaDTO c;
        c.idMaquina = q.value(0).isNull() ? 0 : q.value(0).toLongLong();
        c.nome = q.value(1).toString();
        c.vendas = q.value(2).toInt();
        c.valorVendido = q.value(3).toDouble();
        c.custoTaxas = q.value(4).toDouble();
        lista << c;
    }
    return lista;
}
