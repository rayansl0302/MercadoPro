#include "maquinacartao_service.h"
#include "precotaxa_service.h"

QList<MaquinaCartaoDTO> MaquinaCartao_service::listar(bool incluirInativas)
{
    return repo.listar(incluirInativas);
}

MaquinaCartaoDTO MaquinaCartao_service::get(qlonglong id)
{
    return repo.get(id);
}

MaquinaCartao_service::Resultado MaquinaCartao_service::salvar(const MaquinaCartaoDTO &m)
{
    PrecoTaxa::invalidar();   // a taxa mais alta pode mudar: o preço de venda recalcula na hora
    MaquinaCartaoDTO maquina = m;
    maquina.nome = maquina.nome.trimmed();
    if (maquina.nome.isEmpty())
        return {false, "Informe o nome da maquininha."};
    if (maquina.taxaDebito < 0 || maquina.taxaDebito > 100 || maquina.taxaCredito < 0 || maquina.taxaCredito > 100)
        return {false, "As taxas devem estar entre 0% e 100%."};
    if (repo.nomeJaExiste(maquina.nome, maquina.id))
        return {false, "Já existe uma maquininha com esse nome."};

    QString erro;
    if (maquina.id > 0) {
        if (!repo.get(maquina.id).valida())
            return {false, "Maquininha não encontrada."};
        if (!repo.atualizar(maquina, &erro))
            return {false, erro.isEmpty() ? QStringLiteral("Não foi possível salvar.") : erro};
        return {true, "Maquininha atualizada.", maquina.id};
    }
    const qlonglong id = repo.inserir(maquina, &erro);
    if (id < 0)
        return {false, erro.isEmpty() ? QStringLiteral("Não foi possível salvar.") : erro};
    return {true, "Maquininha cadastrada.", id};
}

MaquinaCartao_service::Resultado MaquinaCartao_service::definirAtiva(qlonglong id, bool ativa)
{
    PrecoTaxa::invalidar();
    MaquinaCartaoDTO m = repo.get(id);
    if (!m.valida())
        return {false, "Maquininha não encontrada."};
    m.ativa = ativa;
    QString erro;
    if (!repo.atualizar(m, &erro))
        return {false, erro.isEmpty() ? QStringLiteral("Não foi possível salvar.") : erro};
    return {true, ativa ? "Maquininha reativada." : "Maquininha desativada.", id};
}

QList<CustoMaquinaDTO> MaquinaCartao_service::custoPorMaquina(const QDate &de, const QDate &ate)
{
    return repo.custoPorMaquina(de, ate);
}
