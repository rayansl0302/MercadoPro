#ifndef PROMOCAOVENDA_SERVICE_H
#define PROMOCAOVENDA_SERVICE_H

#include <QList>
#include <QString>

// Promoção de venda: um desconto que o operador marca no FIM da venda (ex.: "Promoção rede social" para quem
// segue a loja no Instagram e marca o @ num stories). Ela anula a taxa da maquininha que está embutida no preço
// de gôndola (ver PrecoTaxa), então o cliente paga o preço base. Não mexe em estoque nem no preço cadastrado.
struct PromocaoVendaDTO {
    qlonglong id = 0;
    QString nome;
    bool ativa = true;
};

class PromocaoVenda_service
{
public:
    struct Resultado {
        bool ok = false;
        QString msg;
    };

    // ativas = true devolve só as que aparecem na tela de venda
    QList<PromocaoVendaDTO> listar(bool somenteAtivas) const;

    // id <= 0 cria; senão altera nome/ativa. Nome vazio ou repetido (ignorando maiúsculas) é recusado.
    Resultado salvar(qlonglong id, const QString &nome, bool ativa) const;
    Resultado excluir(qlonglong id) const;

    // Desconto (R$) que a promoção dá numa venda cujos itens têm o preço de venda e a quantidade informados:
    // a diferença entre o que o cliente pagaria e o que pagaria sem a taxa da maquininha. Com taxa <= 0
    // (nenhuma maquininha ativa) é zero.
    struct Item {
        double quantidade = 0;
        double precoVenda = 0;
    };
    static double descontoAnulaTaxa(const QList<Item> &itens, double taxaPercentual);
};

#endif // PROMOCAOVENDA_SERVICE_H
