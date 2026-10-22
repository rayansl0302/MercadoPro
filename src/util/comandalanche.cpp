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
    const QString tracejado(cols, QLatin1Char('-'));

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

    // pagamento: o cozinheiro e o balcão veem de longe se o pedido já está pago
    if (!d.statusPagamento.trimmed().isEmpty()) {
        const QString status = QStringLiteral("*** %1 ***").arg(d.statusPagamento.trimmed().toUpper());
        b += EscPosComandos::tamanhoFonte(status.size() * 3 <= cols ? 3 : 2, status.size() * 3 <= cols ? 3 : 2);
        for (const QString &l : quebrarLinhas(status, status.size() * 3 <= cols ? cols / 3 : colsGrande))
            b += linha(l);
        b += EscPosComandos::tamanhoFonte(1, 1);
        QString detalhe = d.formaPagamento.trimmed();
        if (d.total > 0) {
            const QString valor = QLocale(QLocale::Portuguese, QLocale::Brazil).toString(d.total, 'f', 2);
            detalhe += (detalhe.isEmpty() ? QString() : QStringLiteral("  -  ")) + "R$ " + valor;
        }
        if (!detalhe.isEmpty())
            b += linha(detalhe);
    }
    b += EscPosComandos::bold(false);

    const QString hora = d.quando.isValid() ? d.quando.toString("dd/MM/yyyy HH:mm") : QString();
    if (!hora.isEmpty())
        b += linha(hora);
    if (!d.empresa.trimmed().isEmpty())
        b += linha(centralizar(d.empresa.trimmed(), cols).trimmed());

    // itens em dois blocos (chapa e bebidas): cada um vai para uma estação diferente
    b += EscPosComandos::alinharEsquerda();
    b += linha(separador);
    const QStringList ordem{"chapa", "bebida"};
    int blocos = 0;
    for (const QString &g : ordem)
        for (const Item &i : d.itens)
            if ((g == "bebida") == (i.grupo == "bebida")) { ++blocos; break; }
    bool primeiroBloco = true;
    for (const QString &g : ordem) {
        QList<Item> doGrupo;
        for (const Item &i : d.itens)
            if ((g == "bebida") == (i.grupo == "bebida"))
                doGrupo << i;
        if (doGrupo.isEmpty())
            continue;
        if (!primeiroBloco)
            b += linha(tracejado);
        primeiroBloco = false;
        if (blocos > 1) { // só rotula os blocos quando há mais de um
            b += EscPosComandos::bold(true);
            b += linha(g == "bebida" ? QStringLiteral("[ BEBIDAS ]") : QStringLiteral("[ CHAPA / LANCHES ]"));
            b += EscPosComandos::bold(false);
        }
        for (const Item &item : doGrupo) {
            b += EscPosComandos::bold(true);
            b += EscPosComandos::tamanhoFonte(2, 2);
            const QString prefixo = quantidadeTexto(item.quantidade) + "x ";
            const QStringList partes = quebrarLinhas(item.descricao, colsGrande - prefixo.size());
            for (int i = 0; i < partes.size(); ++i)
                b += linha((i == 0 ? prefixo : QString(prefixo.size(), QLatin1Char(' '))) + partes.at(i));
            b += EscPosComandos::tamanhoFonte(1, 1);
            // ingredientes (do cardápio) em negrito normal, logo abaixo do nome
            if (!item.ingredientes.trimmed().isEmpty())
                for (const QString &l : quebrarLinhas(item.ingredientes.trimmed(), cols - 2))
                    b += linha("  " + l);
            b += EscPosComandos::bold(false);
            if (!item.observacao.trimmed().isEmpty())
                for (const QString &l : quebrarLinhas(item.observacao.trimmed(), cols - 4))
                    b += linha("  > " + l);
        }
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
