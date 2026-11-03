#ifndef SAIDAUSOPROPRIO_DTO_H
#define SAIDAUSOPROPRIO_DTO_H

#include <QString>
#include <QStringList>

// Saída de mercadoria do estoque para uso da própria loja (consumo, limpeza, quebra, doação...).
// Fica registrada com o custo e o preço do produto NO MOMENTO da saída: mudar o preço depois não reescreve o histórico.
struct SaidaUsoProprioDTO {
    qlonglong idProduto = 0;
    QString descricaoProduto;
    double quantidade = 0;
    QString motivo;
    QString observacao;
    double custoUnitario = 0;
    double precoVenda = 0;
    qlonglong idOperadorSessao = -1;
    QString nomeOperador;          // só leitura (listagens)

    // preenchidos na leitura
    qlonglong id = 0;
    QString dataHora;              // yyyy-MM-dd HH:mm:ss

    double custoTotal() const { return quantidade * custoUnitario; }
    double valorDeVenda() const { return quantidade * precoVenda; }
};

// Motivos pré-definidos: dá para somar por motivo no histórico e no painel.
inline QStringList motivosSaidaUsoProprio()
{
    return {QStringLiteral("Consumo próprio"), QStringLiteral("Limpeza / uso na loja"),
            QStringLiteral("Quebra / avaria"), QStringLiteral("Vencido / estragado"),
            QStringLiteral("Doação"), QStringLiteral("Outro")};
}

#endif // SAIDAUSOPROPRIO_DTO_H
