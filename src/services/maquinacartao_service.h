#ifndef MAQUINACARTAO_SERVICE_H
#define MAQUINACARTAO_SERVICE_H

#include <QDate>
#include <QList>
#include "../repository/maquinacartao_repository.h"

// Cadastro das maquininhas de cartão e o custo das taxas.
class MaquinaCartao_service
{
public:
    struct Resultado {
        bool ok = false;
        QString msg;
        qlonglong id = 0;
    };

    QList<MaquinaCartaoDTO> listar(bool incluirInativas = false);
    MaquinaCartaoDTO get(qlonglong id);

    // valida (nome obrigatório e único, taxas entre 0 e 100) e grava; id 0 = nova máquina
    Resultado salvar(const MaquinaCartaoDTO &maquina);
    Resultado definirAtiva(qlonglong id, bool ativa);

    QList<CustoMaquinaDTO> custoPorMaquina(const QDate &de, const QDate &ate);

private:
    MaquinaCartao_repository repo;
};

#endif // MAQUINACARTAO_SERVICE_H
