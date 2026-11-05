#ifndef VENDAS_DTO_H
#define VENDAS_DTO_H
#include <QString>
#include <QList>

// Uma parte do pagamento de uma venda dividida em mais de uma forma (ex.: Dinheiro + Pix).
// "valor" é o que abate a venda (sem o troco); em Dinheiro, valorRecebido é o que o cliente entregou.
struct PagamentoVendaDTO {
    QString forma;
    double valor = 0;
    double valorRecebido = 0;
    double troco = 0;
    qlonglong idMaquina = 0;     // só no cartão
    double taxaMaquina = 0;      // % da máquina; custo da loja
};

struct VendasDTO {
    qlonglong id;
    QString clienteNome;
    QString dataHora;
    double total = 0;
    QString formaPagamento;
    double valorRecebido = 0;
    double troco = 0;
    double taxa = 0;
    // maquininha usada no cartão (0 = nenhuma escolhida) e a taxa dela em %. É CUSTO da loja: não entra no valorFinal.
    qlonglong idMaquina = 0;
    double taxaMaquina = 0;
    double valorFinal = 0;
    double desconto = 0;
    bool estaPago;
    qlonglong idCliente;
    qlonglong idCaixa = 0;
    // empresa (CNPJ) da venda; 0 = a empresa ativa na hora de gravar
    qlonglong idEmpresa = 0;
    // quem estava logado na venda. idCaixa aponta para o dono do caixa, que pode ser outra pessoa.
    // -1 = sem sessão (venda fora de uma sessão de operador, ex.: gravação avulsa);
    // 0 = gerente que entrou com o PIN geral; >= 1 = operador do cadastro.
    qlonglong idOperadorSessao = -1;
    QString adicionadoEm;
    QString atualizadoEm;
    // venda paga em mais de uma forma: formaPagamento = "Misto" e as partes ficam aqui (tabela venda_pagamentos)
    QList<PagamentoVendaDTO> pagamentos;
    static constexpr const char *FormaMista = "Misto";
    bool misto() const { return formaPagamento == QLatin1String(FormaMista); }
};

#endif // VENDAS_DTO_H
