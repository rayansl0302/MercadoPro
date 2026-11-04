#include "precotaxa_service.h"
#include "maquinacartao_service.h"

#include <QDateTime>
#include <QLocale>
#include <QtGlobal>
#include <cmath>

namespace {
double taxaEmCache = 0;
qint64 validoAte = 0;   // ms desde a época; 0 = cache vazio
constexpr qint64 kValidadeMs = 3000;
}

void PrecoTaxa::invalidar()
{
    validoAte = 0;
}

double PrecoTaxa::maiorTaxa()
{
    const qint64 agora = QDateTime::currentMSecsSinceEpoch();
    if (validoAte != 0 && agora < validoAte)
        return taxaEmCache;
    double maior = 0;
    const QList<MaquinaCartaoDTO> maquinas = MaquinaCartao_service().listar(false);
    for (const MaquinaCartaoDTO &m : maquinas)
        maior = qMax(maior, qMax(m.taxaDebito, m.taxaCredito));
    taxaEmCache = maior;
    validoAte = agora + kValidadeMs;
    return maior;
}

double PrecoTaxa::comTaxa(double precoBase, double taxaPercentual)
{
    if (precoBase <= 0 || taxaPercentual <= 0 || taxaPercentual >= 100)
        return precoBase;
    return std::round(precoBase / (1.0 - taxaPercentual / 100.0) * 100.0) / 100.0;
}

QString PrecoTaxa::textoPrecoDeVenda(double precoBase)
{
    const double taxa = maiorTaxa();
    if (taxa <= 0 || precoBase <= 0)
        return {};
    const QLocale ptBR(QLocale::Portuguese, QLocale::Brazil);
    return QStringLiteral("Preço de venda com a taxa da maquininha (%1%): %2 — é o valor da gôndola e da venda.")
        .arg(ptBR.toString(taxa, 'f', 2), ptBR.toCurrencyString(comTaxa(precoBase, taxa), QStringLiteral("R$ ")));
}

double PrecoTaxa::comTaxa(double precoBase)
{
    return comTaxa(precoBase, maiorTaxa());
}
