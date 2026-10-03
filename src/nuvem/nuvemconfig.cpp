#include "nuvemconfig.h"
#include "../infra/apppath_service.h"
#include <QSettings>
#include <QUrl>

namespace {
constexpr int kIntervaloMinimo = 15;
}

bool NuvemConfig::urlPermitida(const QString &texto, QString *motivo)
{
    const QUrl u(texto.trimmed());
    auto nega = [&](const QString &m) { if (motivo) *motivo = m; return false; };
    if (!u.isValid() || u.host().isEmpty())
        return nega("Endereço da API inválido.");
    if (u.scheme() == QLatin1String("https"))
        return true;
    if (u.scheme() == QLatin1String("http")) {
        const QString h = u.host().toLower();
        const bool local = h == "localhost" || h == "127.0.0.1" || h == "::1" || h == "host.docker.internal" ||
                           h.endsWith(".localhost") || h == "api" || h == "painel-api";
        if (local)
            return true;
        return nega("Fora do computador, o endereço da API precisa começar com https:// (conexão segura).");
    }
    return nega("O endereço da API deve começar com https://.");
}

QString NuvemConfig::validar() const
{
    QString motivo;
    if (!urlPermitida(url, &motivo))
        return motivo;
    if (token.trimmed().size() < 16)
        return "Informe a chave da loja (token) fornecida pelo painel.";
    if (intervaloMinutos < kIntervaloMinimo)
        return QString("O intervalo mínimo é de %1 minutos.").arg(kIntervaloMinimo);
    return QString();
}

NuvemConfig NuvemConfig::carregar()
{
    QSettings s(AppPath_service::configPath(), QSettings::IniFormat);
    NuvemConfig c;
    c.ativa = s.value("nuvem/ativa", false).toBool();
    c.url = s.value("nuvem/url").toString().trimmed();
    c.token = s.value("nuvem/token").toString().trimmed();
    c.intervaloMinutos = qMax(kIntervaloMinimo, s.value("nuvem/intervalo_min", 60).toInt());
    c.sincronizaNesteComputador = s.value("nuvem/sincroniza_neste_computador", false).toBool();
    return c;
}

bool NuvemConfig::salvar(const NuvemConfig &c)
{
    QSettings s(AppPath_service::configPath(), QSettings::IniFormat);
    s.setValue("nuvem/ativa", c.ativa);
    s.setValue("nuvem/url", c.url.trimmed());
    s.setValue("nuvem/token", c.token.trimmed());
    s.setValue("nuvem/intervalo_min", qMax(kIntervaloMinimo, c.intervaloMinutos));
    s.setValue("nuvem/sincroniza_neste_computador", c.sincronizaNesteComputador);
    s.sync();
    return s.status() == QSettings::NoError;
}
