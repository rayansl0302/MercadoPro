#ifndef PROMOCAO_REPOSITORY_H
#define PROMOCAO_REPOSITORY_H

#include <QList>
#include <QSqlDatabase>
#include "../dto/Promocao_dto.h"

// Promoções (colunas em_promocao, preco_de e promocao_ate da tabela produtos, migração 30).
class Promocao_repository
{
public:
    Promocao_repository();

    QList<PromocaoDTO> listar();
    // produto (promoção ou não): preencha precoDe = preço atual quando não estiver em promoção
    bool produto(qlonglong id, PromocaoDTO *p, bool *emPromocao);
    bool iniciar(qlonglong id, double precoDe, double precoPor, const QString &ate, QString *erro);
    bool alterar(qlonglong id, double precoPor, const QString &ate, QString *erro);
    bool encerrar(qlonglong id, QString *erro);
    QList<qlonglong> vencidas(const QString &hoje);

private:
    QSqlDatabase db;
};

#endif // PROMOCAO_REPOSITORY_H
