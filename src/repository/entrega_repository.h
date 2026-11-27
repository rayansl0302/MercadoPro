#ifndef ENTREGA_REPOSITORY_H
#define ENTREGA_REPOSITORY_H

#include <QObject>
#include <QSqlDatabase>
#include <QList>
#include "../dto/Entrega_dto.h"

// Tabela de preços de entrega e marca das vendas de entrega (tabelas entrega_item e entrega_venda, migração 24).
class Entrega_repository : public QObject
{
    Q_OBJECT
public:
    explicit Entrega_repository(QObject *parent = nullptr);

    QList<EntregaItemDTO> listarItens(bool somenteAtivos = true);
    qlonglong contarItens();
    bool inserirItem(const EntregaItemDTO &item, QString *erro = nullptr);
    bool atualizarIdProduto(qlonglong idItem, qlonglong idProduto, QString *erro = nullptr);

    bool marcarVendaEntrega(qlonglong idVenda, const QString &endereco, const QString &observacao,
                            QString *erro = nullptr);
    bool vendaEhEntrega(qlonglong idVenda);

private:
    QSqlDatabase db;
};

#endif // ENTREGA_REPOSITORY_H
