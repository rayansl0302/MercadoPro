#include "promocao_repository.h"
#include "../infra/databaseconnection_service.h"
#include "../util/datautil.h"
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {
PromocaoDTO ler(QSqlQuery &q)
{
    PromocaoDTO p;
    p.idProduto = q.value("id").toLongLong();
    p.descricao = q.value("descricao").toString();
    p.codigoBarras = q.value("codigo_barras").toString().trimmed();
    p.unidade = q.value("un_comercial").toString();
    p.precoPor = q.value("preco").toDouble();
    p.precoDe = q.value("preco_de").toDouble();
    p.ate = q.value("promocao_ate").toString();
    return p;
}
void setErro(QString *erro, const QString &t)
{
    if (erro)
        *erro = t;
}
}

Promocao_repository::Promocao_repository()
    : db(DatabaseConnection_service::db())
{
}

QList<PromocaoDTO> Promocao_repository::listar()
{
    QList<PromocaoDTO> lista;
    if (!DatabaseConnection_service::open())
        return lista;
    QSqlQuery q(db);
    q.prepare("SELECT id, descricao, codigo_barras, un_comercial, preco, preco_de, promocao_ate FROM produtos "
              "WHERE em_promocao = :t ORDER BY descricao");
    q.bindValue(":t", true);
    if (q.exec())
        while (q.next())
            lista.append(ler(q));
    return lista;
}

bool Promocao_repository::produto(qlonglong id, PromocaoDTO *p, bool *emPromocao)
{
    if (!DatabaseConnection_service::open())
        return false;
    QSqlQuery q(db);
    q.prepare("SELECT id, descricao, codigo_barras, un_comercial, preco, preco_de, promocao_ate, em_promocao "
              "FROM produtos WHERE id = :id");
    q.bindValue(":id", id);
    if (!q.exec() || !q.next())
        return false;
    *p = ler(q);
    *emPromocao = q.value("em_promocao").toBool();
    if (!*emPromocao) {
        p->precoDe = p->precoPor;
        p->ate.clear();
    }
    return true;
}

bool Promocao_repository::iniciar(qlonglong id, double precoDe, double precoPor, const QString &ate, QString *erro)
{
    if (!DatabaseConnection_service::open()) {
        setErro(erro, "Banco de dados indisponível.");
        return false;
    }
    QSqlQuery q(db);
    q.prepare("UPDATE produtos SET preco = :por, preco_de = :de, em_promocao = :t, promocao_ate = :ate, "
              "atualizado_em = :agora WHERE id = :id AND em_promocao = :f");
    q.bindValue(":por", precoPor);
    q.bindValue(":de", precoDe);
    q.bindValue(":t", true);
    q.bindValue(":f", false);
    q.bindValue(":ate", ate.isEmpty() ? QVariant(QMetaType(QMetaType::QString)) : QVariant(ate));
    q.bindValue(":agora", DataUtil::getDataAgoraUS());
    q.bindValue(":id", id);
    if (!q.exec()) {
        setErro(erro, q.lastError().text());
        return false;
    }
    if (q.numRowsAffected() != 1) {
        setErro(erro, "O produto não foi encontrado ou já está em promoção.");
        return false;
    }
    return true;
}

bool Promocao_repository::alterar(qlonglong id, double precoPor, const QString &ate, QString *erro)
{
    if (!DatabaseConnection_service::open()) {
        setErro(erro, "Banco de dados indisponível.");
        return false;
    }
    QSqlQuery q(db);
    q.prepare("UPDATE produtos SET preco = :por, promocao_ate = :ate, atualizado_em = :agora "
              "WHERE id = :id AND em_promocao = :t");
    q.bindValue(":por", precoPor);
    q.bindValue(":ate", ate.isEmpty() ? QVariant(QMetaType(QMetaType::QString)) : QVariant(ate));
    q.bindValue(":agora", DataUtil::getDataAgoraUS());
    q.bindValue(":id", id);
    q.bindValue(":t", true);
    if (!q.exec()) {
        setErro(erro, q.lastError().text());
        return false;
    }
    if (q.numRowsAffected() != 1) {
        setErro(erro, "O produto não está em promoção.");
        return false;
    }
    return true;
}

bool Promocao_repository::encerrar(qlonglong id, QString *erro)
{
    if (!DatabaseConnection_service::open()) {
        setErro(erro, "Banco de dados indisponível.");
        return false;
    }
    QSqlQuery q(db);
    q.prepare("UPDATE produtos SET preco = preco_de, preco_de = NULL, em_promocao = :f, promocao_ate = NULL, "
              "atualizado_em = :agora WHERE id = :id AND em_promocao = :t AND preco_de IS NOT NULL");
    q.bindValue(":f", false);
    q.bindValue(":t", true);
    q.bindValue(":agora", DataUtil::getDataAgoraUS());
    q.bindValue(":id", id);
    if (!q.exec()) {
        setErro(erro, q.lastError().text());
        return false;
    }
    if (q.numRowsAffected() != 1) {
        setErro(erro, "O produto não está em promoção.");
        return false;
    }
    return true;
}

QList<qlonglong> Promocao_repository::vencidas(const QString &hoje)
{
    QList<qlonglong> ids;
    if (!DatabaseConnection_service::open())
        return ids;
    QSqlQuery q(db);
    q.prepare("SELECT id FROM produtos WHERE em_promocao = :t AND promocao_ate IS NOT NULL AND promocao_ate <> '' "
              "AND promocao_ate < :hoje");
    q.bindValue(":t", true);
    q.bindValue(":hoje", hoje);
    if (q.exec())
        while (q.next())
            ids << q.value(0).toLongLong();
    return ids;
}
