#ifndef MAQUINACARTAO_DTO_H
#define MAQUINACARTAO_DTO_H

#include <QString>

// Maquininha de cartão da loja. As taxas (% do valor da venda) são CUSTO da loja: o cliente sempre paga
// o preço de gôndola, e a taxa da máquina escolhida na venda é descontada do lucro nos relatórios.
struct MaquinaCartaoDTO {
    qlonglong id = 0;
    QString nome;
    double taxaDebito = 0;     // %
    double taxaCredito = 0;    // % (crédito à vista)
    bool ativa = true;
    QString criadaEm;

    bool valida() const { return id > 0; }
    double taxaDaForma(const QString &forma) const { return forma == QStringLiteral("Débito") ? taxaDebito : taxaCredito; }
};

// Quanto uma máquina custou de taxa num período (vendas no cartão + recebimentos de fiado no cartão).
struct CustoMaquinaDTO {
    qlonglong idMaquina = 0;      // 0 = vendas sem máquina escolhida (taxa padrão das Configurações)
    QString nome;
    int vendas = 0;
    double valorVendido = 0;
    double custoTaxas = 0;
};

#endif // MAQUINACARTAO_DTO_H
