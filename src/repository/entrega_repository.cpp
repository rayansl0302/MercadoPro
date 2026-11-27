#include "entrega_repository.h"
#include "../infra/databaseconnection_service.h"
#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>
#include <QDebug>

namespace {
void setErro(QString *erro, const QString &t) { if (erro) *erro = t; }
}

Entrega_repository::Entrega_repository(QObject *parent)
    : QObject(parent)
    , db(DatabaseConnection_service::db())
{
}

QList<EntregaItemDTO> Entrega_repository::listarItens(bool somenteAtivos)
{
    QList<EntregaItemDTO> itens;
    if (!DatabaseConnection_service::open()) {
        qDebug() << "db nao aberto ao listarItens de entrega";
        return itens;
    }
    QSqlQuery q(db);
    QString sql = "SELECT id, descricao, unidade, preco, qtd_faixa, preco_faixa, id_produto, ordem, ativo "
                  "FROM entrega_item ";
    if (somenteAtivos)
        sql += "WHERE ativo = 1 ";
    sql += "ORDER BY ordem, id";
    if (!q.exec(sql)) {
        qDebug() << "Erro listarItens de entrega:" << q.lastError().text();
        return itens;
    }
    while (q.next()) {
        EntregaItemDTO i;
        i.id = q.value("id").toLongLong();
        i.descricao = q.value("descricao").toString();
        i.unidade = q.value("unidade").toString();
        i.preco = q.value("preco").toDouble();
        i.qtdFaixa = q.value("qtd_faixa").toDouble();
        i.precoFaixa = q.value("preco_faixa").toDouble();
        i.idProduto = q.value("id_produto").toLongLong();
        i.ordem = q.value("ordem").toInt();
        i.ativo = q.value("ativo").toInt() == 1;
        itens << i;
    }
    return itens;
}

qlonglong Entrega_repository::contarItens()
{
    if (!DatabaseConnection_service::open())
        return 0;
    QSqlQuery q(db);
    if (!q.exec("SELECT COUNT(*) FROM entrega_item") || !q.next())
        return 0;
    return q.value(0).toLongLong();
}

bool Entrega_repository::inserirItem(const EntregaItemDTO &item, QString *erro)
{
    if (!DatabaseConnection_service::open()) {
        setErro(erro, "Banco de dados indisponível.");
        return false;
    }
    QSqlQuery q(db);
    q.prepare("INSERT INTO entrega_item (descricao, unidade, preco, qtd_faixa, preco_faixa, id_produto, ordem, ativo) "
              "VALUES (:desc, :un, :preco, :qtdfaixa, :precofaixa, :idprod, :ordem, :ativo)");
    q.bindValue(":desc", item.descricao);
    q.bindValue(":un", item.unidade);
    q.bindValue(":preco", item.preco);
    q.bindValue(":qtdfaixa", item.qtdFaixa);
    q.bindValue(":precofaixa", item.precoFaixa);
    q.bindValue(":idprod", item.idProduto);
    q.bindValue(":ordem", item.ordem);
    q.bindValue(":ativo", item.ativo ? 1 : 0);
    if (!q.exec()) {
        setErro(erro, q.lastError().text());
        return false;
    }
    return true;
}

bool Entrega_repository::atualizarIdProduto(qlonglong idItem, qlonglong idProduto, QString *erro)
{
    if (!DatabaseConnection_service::open()) {
        setErro(erro, "Banco de dados indisponível.");
        return false;
    }
    QSqlQuery q(db);
    q.prepare("UPDATE entrega_item SET id_produto = :idprod WHERE id = :id");
    q.bindValue(":idprod", idProduto);
    q.bindValue(":id", idItem);
    if (!q.exec()) {
        setErro(erro, q.lastError().text());
        return false;
    }
    return true;
}

bool Entrega_repository::marcarVendaEntrega(qlonglong idVenda, const QString &endereco,
                                            const QString &observacao, QString *erro)
{
    if (!DatabaseConnection_service::open()) {
        setErro(erro, "Banco de dados indisponível.");
        return false;
    }
    QSqlQuery q(db);
    q.prepare("INSERT INTO entrega_venda (id_venda, endereco, observacao, criado_em) "
              "VALUES (:id, :end, :obs, :criado)");
    q.bindValue(":id", idVenda);
    q.bindValue(":end", endereco);
    q.bindValue(":obs", observacao);
    q.bindValue(":criado", QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));
    if (!q.exec()) {
        setErro(erro, q.lastError().text());
        return false;
    }
    return true;
}

bool Entrega_repository::vendaEhEntrega(qlonglong idVenda)
{
    if (!DatabaseConnection_service::open())
        return false;
    QSqlQuery q(db);
    q.prepare("SELECT 1 FROM entrega_venda WHERE id_venda = :id");
    q.bindValue(":id", idVenda);
    return q.exec() && q.next();
}
