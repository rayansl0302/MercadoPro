#ifndef TEMA_H
#define TEMA_H

#include <QString>

class QApplication;

// Tema visual do programa: fonte, paleta e folha de estilo global.
// Telas que já têm estilo próprio (.ui) continuam mandando nas propriedades que definem;
// o tema cobre o que elas não definem (campos, tabelas, menus, barras de rolagem, diálogos...).
namespace Tema {
void aplicar(QApplication &app);

// ---- tamanho da letra (acessibilidade: operador com dificuldade de ler) ----
// Percentual de aumento: 0, 5, 10 ou 20. Fica em config.ini ([interface] fonte_percentual) e vale
// na próxima abertura do programa.
int percentualFonte();
void definirPercentualFonte(int percentual);   // aceita 0/5/10/20; outro valor vira 0
double fatorFonte();                           // 1.0, 1.05, 1.10 ou 1.20

// Multiplica todo "font-size: Npt|Npx" de uma folha de estilo pelo fator (função pura, testável).
QString escalarFolha(const QString &css, double fator);

// No Windows (e em outros temas) o Qt dá fonte própria a tabelas, cabeçalhos, menus, caixas de mensagem,
// botões, campos etc., e a fonte geral do aplicativo NÃO vale para eles. Esta função multiplica a fonte
// de cada uma dessas classes pelo fator. Deve ser chamada ANTES de mudar a fonte geral.
// "familia" (opcional) troca a família da letra nessas classes.
void escalarFontesDeClasse(double fator, const QString &familia = QString());
}

#endif // TEMA_H
