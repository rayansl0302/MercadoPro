#include "lanches.h"

#include <QRegularExpression>
#include <QSet>
#include <QSettings>

#include "../infra/apppath_service.h"

namespace Lanches {

Faixa faixaConfigurada()
{
    QSettings ini(AppPath_service::configPath(), QSettings::IniFormat);
    Faixa f;
    bool okMin = false, okMax = false;
    const int minimo = ini.value("pdv/lanche_codigo_min").toInt(&okMin);
    const int maximo = ini.value("pdv/lanche_codigo_max").toInt(&okMax);
    if (okMin && okMax && minimo > 0 && maximo >= minimo) {
        f.minimo = minimo;
        f.maximo = maximo;
    }
    return f;
}

bool codigoEhLanche(const QString &codigo, const Faixa &faixa)
{
    static const QRegularExpression soDigitos(QStringLiteral("^[1-9]\\d{0,3}$"));
    const QString c = codigo.trimmed();
    if (!soDigitos.match(c).hasMatch())
        return false;
    const int valor = c.toInt();
    return valor >= faixa.minimo && valor <= faixa.maximo;
}

TipoVenda classificar(const QStringList &codigos, const Faixa &faixa)
{
    int lanches = 0;
    for (const QString &c : codigos)
        if (codigoEhLanche(c, faixa))
            ++lanches;
    if (lanches == 0)
        return TipoVenda::Normal;
    return lanches == codigos.size() ? TipoVenda::Lanche : TipoVenda::Misto;
}

int senhaDoDia(const QList<QPair<qlonglong, QString>> &itensDoDia, qlonglong idVenda, const Faixa &faixa)
{
    QSet<qlonglong> vendasDeLanche;
    for (const auto &item : itensDoDia)
        if (item.first <= idVenda && codigoEhLanche(item.second, faixa))
            vendasDeLanche.insert(item.first);
    return vendasDeLanche.size();
}

} // namespace Lanches
