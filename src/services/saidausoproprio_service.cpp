#include "saidausoproprio_service.h"
#include "Produto_service.h"
#include <QLocale>

SaidaUsoProprio_service::Resultado SaidaUsoProprio_service::registrar(qlonglong idProduto, double quantidade,
                                                                      const QString &motivo, const QString &observacao,
                                                                      qlonglong idOperadorSessao, const QString &nomeOperador)
{
    if (quantidade <= 0)
        return {false, "Informe uma quantidade maior que zero."};
    if (!motivosSaidaUsoProprio().contains(motivo))
        return {false, "Escolha o motivo da saída."};

    Produto_Service produtos;
    const ProdutoDTO p = produtos.getProduto(idProduto);
    if (p.id <= 0)
        return {false, "Produto não encontrado."};

    // lanche e produto "sem controle de estoque" não têm saldo para baixar: a saída só fica registrada
    const bool baixar = !produtos.semControleDeEstoque(idProduto);
    if (baixar && quantidade > p.quantidade) {
        return {false, QStringLiteral("Estoque insuficiente: há apenas %1 de \"%2\".")
                           .arg(QLocale(QLocale::Portuguese, QLocale::Brazil).toString(p.quantidade, 'g', 10), p.descricao)};
    }

    SaidaUsoProprioDTO s;
    s.idProduto = idProduto;
    s.descricaoProduto = p.descricao;
    s.quantidade = quantidade;
    s.motivo = motivo;
    s.observacao = observacao.trimmed();
    s.custoUnitario = p.precoFornecedor;
    s.precoVenda = p.preco;
    s.idOperadorSessao = idOperadorSessao;
    s.nomeOperador = nomeOperador;

    QString erro;
    const qlonglong id = repo.registrar(s, baixar, &erro);
    if (id < 0)
        return {false, erro.isEmpty() ? QStringLiteral("Não foi possível registrar a saída.") : erro};
    return {true, "Saída registrada.", id};
}

QList<SaidaUsoProprioDTO> SaidaUsoProprio_service::listar(const QDate &de, const QDate &ate, const QString &motivo)
{
    return repo.listar(de, ate, motivo);
}

double SaidaUsoProprio_service::custoTotal(const QList<SaidaUsoProprioDTO> &saidas)
{
    double total = 0;
    for (const SaidaUsoProprioDTO &s : saidas)
        total += s.custoTotal();
    return total;
}
