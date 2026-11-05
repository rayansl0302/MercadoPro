#include "pagamentodividido_service.h"

#include <QLocale>
#include <QSet>
#include <QStringList>
#include <QtGlobal>

namespace {
const QStringList kFormas = {QStringLiteral("Dinheiro"), QStringLiteral("Pix"),
                             QStringLiteral("Crédito"), QStringLiteral("Débito")};

QString dinheiro(double v)
{
    return QLocale(QLocale::Portuguese, QLocale::Brazil).toCurrencyString(v, QStringLiteral("R$ "));
}
double arred(double v) { return qRound64(v * 100.0) / 100.0; }
}

QString PagamentoDividido::montar(double valorFinal, const QList<Entrega> &entregas, QList<PagamentoVendaDTO> *saida)
{
    QList<Entrega> validas;
    QSet<QString> vistas;
    for (const Entrega &e : entregas) {
        if (e.entregue <= 0)
            continue;
        if (!kFormas.contains(e.forma))
            return QStringLiteral("Forma de pagamento inválida: %1.").arg(e.forma);
        if (vistas.contains(e.forma))
            return QStringLiteral("%1 aparece mais de uma vez. Junte os valores numa linha só.").arg(e.forma);
        vistas.insert(e.forma);
        validas << e;
    }
    if (validas.size() < 2)
        return QStringLiteral("Informe o valor em pelo menos duas formas de pagamento.");

    double semDinheiro = 0, dinheiroEntregue = 0;
    for (const Entrega &e : validas) {
        if (e.forma == "Dinheiro")
            dinheiroEntregue = arred(e.entregue);
        else
            semDinheiro += arred(e.entregue);
    }
    semDinheiro = arred(semDinheiro);
    const double restante = arred(valorFinal - semDinheiro);
    if (restante < -0.009)
        return QStringLiteral("Pix e cartão somam %1, mais que o valor da venda (%2). Só o dinheiro pode dar troco.")
            .arg(dinheiro(semDinheiro), dinheiro(valorFinal));
    if (dinheiroEntregue <= 0 && qAbs(restante) > 0.009)
        return QStringLiteral("Faltam %1 para fechar o valor da venda.").arg(dinheiro(restante));
    if (dinheiroEntregue > 0 && dinheiroEntregue < restante - 0.009)
        return QStringLiteral("Faltam %1 para fechar o valor da venda.").arg(dinheiro(restante - dinheiroEntregue));

    double troco = dinheiroEntregue > 0 ? arred(dinheiroEntregue - restante) : 0.0;
    if (troco < 0)
        troco = 0;
    const double dinheiroAplicado = arred(dinheiroEntregue - troco);
    if (dinheiroEntregue > 0 && dinheiroAplicado <= 0.009)
        return QStringLiteral("O dinheiro não faz parte do pagamento: Pix e cartão já cobrem a venda. Tire a linha do dinheiro.");

    if (saida) {
        saida->clear();
        for (const Entrega &e : validas) {
            PagamentoVendaDTO p;
            p.forma = e.forma;
            if (e.forma == "Dinheiro") {
                p.valor = dinheiroAplicado;
                p.valorRecebido = dinheiroEntregue;
                p.troco = troco;
            } else {
                p.valor = arred(e.entregue);
                p.valorRecebido = p.valor;
            }
            saida->append(p);
        }
    }
    return QString();
}
