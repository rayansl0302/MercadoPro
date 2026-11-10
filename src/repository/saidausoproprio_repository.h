#ifndef SAIDAUSOPROPRIO_REPOSITORY_H
#define SAIDAUSOPROPRIO_REPOSITORY_H

#include <QDate>
#include <QList>
#include <QSqlDatabase>
#include "../dto/SaidaUsoProprio_dto.h"

// Saídas de estoque para uso próprio (migração 26).
class SaidaUsoProprio_repository
{
public:
    SaidaUsoProprio_repository();

    // Grava a saída E baixa o estoque na mesma transação (ou nada). O estoque só é baixado se
    // `baixarEstoque`; quando baixa, não deixa o saldo ficar negativo (a condição vai no UPDATE, então
    // dois caixas dando saída do mesmo produto ao mesmo tempo não passam do que existe).
    // Devolve o id da saída, ou -1 com a mensagem em `erro`.
    qlonglong registrar(const SaidaUsoProprioDTO &saida, bool baixarEstoque, QString *erro = nullptr);

    QList<SaidaUsoProprioDTO> listar(const QDate &de, const QDate &ate, const QString &motivo = QString());

    // Uma saída pelo id (id <= 0 no resultado se não existir).
    SaidaUsoProprioDTO obter(qlonglong id);

    // Remove a saída (lançada por engano) e, se `devolverEstoque`, devolve a quantidade ao produto, tudo na mesma
    // transação. Só remove uma vez: se outra tela já removeu, recusa em vez de devolver o estoque em dobro.
    bool remover(qlonglong id, bool devolverEstoque, QString *erro = nullptr);

private:
    QSqlDatabase db;
};

#endif // SAIDAUSOPROPRIO_REPOSITORY_H
