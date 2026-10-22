#ifndef CARDAPIOLANCHES_H
#define CARDAPIOLANCHES_H

#include <QMap>
#include <QString>

// Cardápio dos lanches (códigos 200 a 500): ingredientes e grupo de cada item, para a comanda da cozinha.
// O cardápio padrão vai dentro do programa. Para mudar, crie o arquivo "cardapio_lanches.json" na pasta
// de configuração do programa (a mesma do config.ini): ele tem prioridade sobre o padrão.
//   { "201": { "nome": "Pão na Chapa", "grupo": "chapa", "ingredientes": "Pão francês, manteiga" } }
// grupo: "chapa" (lanches) ou "bebida".
namespace CardapioLanches {

struct Item {
    QString nome;
    QString grupo = "chapa";
    QString ingredientes;
};

// Interpreta o JSON do cardápio (função pura). Entradas inválidas são ignoradas; erro (se pedido) descreve o problema.
QMap<QString, Item> carregarDeJson(const QByteArray &json, QString *erro = nullptr);

// Dados do item pelo código de barras; Item vazio (sem ingredientes) se o código não está no cardápio.
Item buscar(const QString &codigo);

// Descarta o cache (para reler o arquivo do usuário).
void recarregar();

} // namespace CardapioLanches

#endif // CARDAPIOLANCHES_H
