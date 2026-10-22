#ifndef COMANDALANCHE_H
#define COMANDALANCHE_H

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QString>

// Comanda térmica de lanches (para a cozinha/chapa). Sem dados fiscais nem valores: só o que o
// cozinheiro precisa ler de longe — senha grande, itens em letra grande e observações.
namespace ComandaLanche {

struct Item {
    double quantidade = 1;
    QString descricao;
    QString ingredientes; // do cardápio (ex.: "Pão francês, hambúrguer, queijo"); opcional
    QString grupo = "chapa"; // "chapa" (lanches) ou "bebida": a comanda separa os dois blocos
    QString observacao;   // preparo deste item (ex.: "sem cebola"); opcional
};

struct Dados {
    QString empresa;                 // opcional
    QString setor = "COZINHA / CHAPA";
    int senha = 0;                   // número do pedido/senha do dia
    QDateTime quando;
    // pagamento: "PAGO" (ou "A PAGAR") em destaque, com a forma e o valor. Vazio = não imprime o bloco.
    QString statusPagamento;
    QString formaPagamento;
    double total = 0;                // 0 = não imprime o valor
    QList<Item> itens;
    QString observacao;              // observação geral do pedido; opcional
    int colunas = 42;                // largura da bobina em caracteres (fonte normal): 42 (80 mm) ou 32 (58 mm)
};

// Monta os bytes ESC/POS prontos para a impressora (inicializa, escreve e corta).
// É uma função pura: não abre impressora nem banco, então dá para testar sem hardware.
QByteArray montar(const Dados &dados);

// Quebra um texto em linhas de no máximo "largura" caracteres, sem cortar palavras (palavra maior
// que a linha é partida). Exposta para teste.
QStringList quebrarLinhas(const QString &texto, int largura);

} // namespace ComandaLanche

#endif // COMANDALANCHE_H
