#ifndef VENDAS_DTO_H
#define VENDAS_DTO_H
#include <QString>

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
};

#endif // VENDAS_DTO_H
