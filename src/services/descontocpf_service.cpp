#include "descontocpf_service.h"
#include "../repository/caixa_repository.h"
#include <QLocale>
#include <cmath>

namespace {
const char *kAtivo = "desconto_cpf_ativo";
const char *kPercentual = "desconto_cpf_percentual";
const char *kDe = "desconto_cpf_de";
const char *kAte = "desconto_cpf_ate";
const char *kTeto = "desconto_cpf_teto";
}

DescontoCpf_service::Config DescontoCpf_service::carregar()
{
    Caixa_repository repo;
    Config c;
    QString v;
    if (repo.lerConfig(kAtivo, &v))
        c.ativo = (v == QLatin1String("1"));
    if (repo.lerConfig(kPercentual, &v))
        c.percentual = v.toDouble();
    if (repo.lerConfig(kTeto, &v))
        c.tetoReais = v.toDouble();
    QString de, ate;
    repo.lerConfig(kDe, &de);
    repo.lerConfig(kAte, &ate);
    c.de = QDate::fromString(de, Qt::ISODate);
    c.ate = QDate::fromString(ate, Qt::ISODate);
    c.comPeriodo = c.de.isValid() && c.ate.isValid();
    return c;
}

bool DescontoCpf_service::salvar(const Config &c, QString *erro)
{
    if (c.percentual < 0 || c.percentual > 50) {
        if (erro) *erro = QStringLiteral("O percentual deve ficar entre 0 e 50.");
        return false;
    }
    if (c.tetoReais < 0) {
        if (erro) *erro = QStringLiteral("O teto não pode ser negativo.");
        return false;
    }
    if (c.comPeriodo && (!c.de.isValid() || !c.ate.isValid() || c.ate < c.de)) {
        if (erro) *erro = QStringLiteral("Período inválido: a data final deve ser igual ou depois da inicial.");
        return false;
    }
    Caixa_repository repo;
    const bool ok = repo.gravarConfig(kAtivo, c.ativo ? "1" : "0")
        && repo.gravarConfig(kPercentual, QString::number(c.percentual, 'f', 2))
        && repo.gravarConfig(kTeto, QString::number(c.tetoReais, 'f', 2))
        && repo.gravarConfig(kDe, c.comPeriodo ? c.de.toString(Qt::ISODate) : QString())
        && repo.gravarConfig(kAte, c.comPeriodo ? c.ate.toString(Qt::ISODate) : QString());
    if (!ok && erro)
        *erro = QStringLiteral("Não foi possível salvar a configuração.");
    return ok;
}

bool DescontoCpf_service::vigente(const Config &c, const QDate &hoje)
{
    if (!c.ativo || c.percentual <= 0)
        return false;
    if (c.comPeriodo && (hoje < c.de || hoje > c.ate))
        return false;
    return true;
}

double DescontoCpf_service::calcular(const Config &c, double totalVenda, const QDate &hoje)
{
    if (!vigente(c, hoje) || totalVenda <= 0)
        return 0;
    double d = std::round(totalVenda * c.percentual) / 100.0;   // centavos
    if (c.tetoReais > 0 && d > c.tetoReais)
        d = c.tetoReais;
    return std::round(d * 100.0) / 100.0;
}

QString DescontoCpf_service::rotulo(const Config &c)
{
    return QStringLiteral("Desconto cliente cadastrado (%1%)").arg(QLocale(QLocale::Portuguese, QLocale::Brazil).toString(c.percentual, 'f', c.percentual == std::floor(c.percentual) ? 0 : 1));
}
