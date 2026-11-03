#ifndef SAIDAUSOPROPRIO_SERVICE_H
#define SAIDAUSOPROPRIO_SERVICE_H

#include <QDate>
#include <QList>
#include "../dto/SaidaUsoProprio_dto.h"
#include "../repository/saidausoproprio_repository.h"

// Dar saída em um produto do estoque para uso da própria loja, com histórico.
class SaidaUsoProprio_service
{
public:
    struct Resultado {
        bool ok = false;
        QString msg;
        qlonglong id = 0;
    };

    // idOperadorSessao/nomeOperador: quem estava logado (a tela passa o da sessão).
    Resultado registrar(qlonglong idProduto, double quantidade, const QString &motivo, const QString &observacao,
                        qlonglong idOperadorSessao, const QString &nomeOperador);

    QList<SaidaUsoProprioDTO> listar(const QDate &de, const QDate &ate, const QString &motivo = QString());

    // soma dos custos (quantidade x custo do momento)
    static double custoTotal(const QList<SaidaUsoProprioDTO> &saidas);

private:
    SaidaUsoProprio_repository repo;
};

#endif // SAIDAUSOPROPRIO_SERVICE_H
