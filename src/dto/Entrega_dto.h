#ifndef ENTREGA_DTO_H
#define ENTREGA_DTO_H

#include <QString>
#include <QList>

// Item da tabela de preços de entrega (tabela entrega_item).
// Preço de entrega não tem o desconto de revenda: é o preço final do item.
struct EntregaItemDTO {
    qlonglong id = 0;
    QString descricao;
    // "un", "bandeja", "prato" ou "kg"
    QString unidade = "un";
    double preco = 0;
    // a partir desta quantidade vale precoFaixa (0 = sem faixa)
    double qtdFaixa = 0;
    double precoFaixa = 0;
    // produto do estoque que recebe a baixa (0 = ainda não ligado)
    qlonglong idProduto = 0;
    int ordem = 0;
    bool ativo = true;

    bool temFaixa() const { return qtdFaixa > 0 && precoFaixa > 0; }
    bool fracionado() const { return unidade == "kg"; }
};

// Uma linha do pedido: o item e a quantidade pedida.
struct EntregaLinhaDTO {
    EntregaItemDTO item;
    double quantidade = 0;
};

// Dados gravados junto com a venda de entrega (tabela entrega_venda).
struct EntregaPedidoDTO {
    qlonglong idCliente = 1;
    QString clienteNome = "Consumidor";
    QString formaPagamento = "Dinheiro";
    QString endereco;
    QString observacao;
    // quem está logado (-1 = sem sessão); a tela preenche com o operador da sessão
    qlonglong idOperadorSessao = -1;
    QList<EntregaLinhaDTO> linhas;
};

#endif // ENTREGA_DTO_H
