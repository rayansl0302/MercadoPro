#include "comandalanche.h"

#include <QLocale>

#include "escposcomandos.h"

namespace ComandaLanche {

namespace {

QByteArray linha(const QString &texto)
{
    return texto.toLocal8Bit() + '\n';
}

QString quantidadeTexto(double q)
{
    // 2 -> "2", 1.5 -> "1,5" (lanche costuma ser unidade inteira)
    if (qAbs(q - qRound(q)) < 0.0005)
        return QString::number(qRound(q));
    return QLocale(QLocale::Portuguese, QLocale::Brazil).toString(q, 'f', 2);
}

QString centralizar(const QString &texto, int largura)
{
    if (texto.size() >= largura)
        return texto.left(largura);
    return QString((largura - texto.size()) / 2, QLatin1Char(' ')) + texto;
}

} // namespace

QStringList quebrarLinhas(const QString &texto, int largura)
{
    QStringList linhas;
    QString atual;
    for (const QString &palavra : texto.simplified().split(' ', Qt::SkipEmptyParts)) {
        QString resto = palavra;
        // palavra maior que a linha: parte em pedaços
        while (resto.size() > largura) {
            if (!atual.isEmpty()) {
                linhas << atual;
                atual.clear();
            }
            linhas << resto.left(largura);
            resto = resto.mid(largura);
        }
        const QString candidata = atual.isEmpty() ? resto : atual + ' ' + resto;
        if (candidata.size() <= largura) {
            atual = candidata;
        } else {
            linhas << atual;
            atual = resto;
        }
    }
    if (!atual.isEmpty())
        linhas << atual;
    return linhas;
}

QByteArray montar(const Dados &d)
{
    const int cols = qMax(20, d.colunas);
    const int colsGrande = cols / 2; // fonte com largura 2x cabe metade das colunas
    const QString separador(cols, QLatin1Char('='));

    QByteArray b;
    b += EscPosComandos::inicializar();
    b += QByteArray("\x1B\x74\x10", 3); // página de códigos WPC1252 (acentos), igual ao cupom padrão

    // cabeçalho: setor + senha em destaque
    b += EscPosComandos::alinharCentro();
    b += EscPosComandos::bold(true);
    b += EscPosComandos::tamanhoFonte(2, 2);
    for (const QString &l : quebrarLinhas(d.setor, colsGrande))
        b += linha(l);
    b += EscPosComandos::tamanhoFonte(1, 1);
    b += linha("PEDIDO / SENHA");
    b += EscPosComandos::tamanhoFonte(6, 6);
    b += linha(QStringLiteral("%1").arg(d.senha, 3, 10, QLatin1Char('0')));
    b += EscPosComandos::tamanhoFonte(1, 1);
    b += EscPosComandos::bold(false);

    const QString hora = d.quando.isValid() ? d.quando.toString("dd/MM/yyyy HH:mm") : QString();
    if (!hora.isEmpty())
        b += linha(hora);
    if (!d.empresa.trimmed().isEmpty())
        b += linha(centralizar(d.empresa.trimmed(), cols).trimmed());

    // itens: letra grande e em negrito, uma linha de leitura por lanche
    b += EscPosComandos::alinharEsquerda();
    b += linha(separador);
    for (const Item &item : d.itens) {
        b += EscPosComandos::bold(true);
        b += EscPosComandos::tamanhoFonte(2, 2);
        const QString prefixo = quantidadeTexto(item.quantidade) + "x ";
        const QStringList partes = quebrarLinhas(item.descricao, colsGrande - prefixo.size());
        for (int i = 0; i < partes.size(); ++i)
            b += linha((i == 0 ? prefixo : QString(prefixo.size(), QLatin1Char(' '))) + partes.at(i));
        b += EscPosComandos::tamanhoFonte(1, 1);
        b += EscPosComandos::bold(false);
        if (!item.observacao.trimmed().isEmpty())
            for (const QString &l : quebrarLinhas(item.observacao.trimmed(), cols - 4))
                b += linha("  > " + l);
    }
    b += linha(separador);

    // observações de preparo: as já registradas ou um espaço para escrever à mão
    b += EscPosComandos::bold(true);
    b += linha("OBS:");
    b += EscPosComandos::bold(false);
    if (!d.observacao.trimmed().isEmpty()) {
        for (const QString &l : quebrarLinhas(d.observacao.trimmed(), cols))
            b += linha(l);
    } else {
        b += linha(QString(cols, QLatin1Char('_')));
        b += linha(QString(cols, QLatin1Char('_')));
    }

    b += EscPosComandos::feed(4);
    b += EscPosComandos::cortar();
    return b;
}

} // namespace ComandaLanche
