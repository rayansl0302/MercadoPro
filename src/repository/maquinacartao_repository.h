#ifndef MAQUINACARTAO_REPOSITORY_H
#define MAQUINACARTAO_REPOSITORY_H

#include <QDate>
#include <QList>
#include <QSqlDatabase>
#include "../dto/MaquinaCartao_dto.h"

// Maquininhas de cartão (migração 27).
class MaquinaCartao_repository
{
public:
    MaquinaCartao_repository();

    QList<MaquinaCartaoDTO> listar(bool incluirInativas);
    MaquinaCartaoDTO get(qlonglong id);
    // id novo, ou -1 com a mensagem em `erro`
    qlonglong inserir(const MaquinaCartaoDTO &m, QString *erro = nullptr);
    bool atualizar(const MaquinaCartaoDTO &m, QString *erro = nullptr);
    bool nomeJaExiste(const QString &nome, qlonglong ignorarId);

    QList<CustoMaquinaDTO> custoPorMaquina(const QDate &de, const QDate &ate);

private:
    QSqlDatabase db;
};

#endif // MAQUINACARTAO_REPOSITORY_H
