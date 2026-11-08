#include "promocao_service.h"
#include <cmath>

namespace {
double arredondar2(double v) { return std::round(v * 100.0) / 100.0; }
double comTaxa(double base, double taxa)
{
    if (base <= 0 || taxa <= 0 || taxa >= 100)
        return base;
    return std::round(base / (1.0 - taxa / 100.0) * 100.0) / 100.0;   // mesma conta de PrecoTaxa::comTaxa
}
}

QList<PromocaoDTO> Promocao_service::listar()
{
    return repo.listar();
}

double Promocao_service::baseParaGondola(double precoGondola, double taxaPercentual)
{
    if (precoGondola <= 0 || taxaPercentual <= 0 || taxaPercentual >= 100)
        return arredondar2(precoGondola);
    const double inicio = arredondar2(precoGondola * (1.0 - taxaPercentual / 100.0));
    for (int delta : {0, 1, -1, 2, -2}) {
        const double candidato = arredondar2(inicio + delta / 100.0);
        if (candidato > 0 && std::abs(comTaxa(candidato, taxaPercentual) - precoGondola) < 0.005)
            return candidato;
    }
    return inicio;
}

Promocao_service::Resultado Promocao_service::definir(qlonglong idProduto, double precoPorBase, const QDate &ate)
{
    PromocaoDTO p;
    bool emPromocao = false;
    if (!repo.produto(idProduto, &p, &emPromocao))
        return {false, "Produto não encontrado."};
    precoPorBase = arredondar2(precoPorBase);
    if (precoPorBase <= 0)
        return {false, "Informe o preço promocional."};
    if (precoPorBase >= p.precoDe - 0.005)
        return {false, "O preço promocional precisa ser menor que o preço de antes."};
    if (ate.isValid() && ate < QDate::currentDate())
        return {false, "A data final da promoção já passou."};
    const QString dataFim = ate.isValid() ? ate.toString(Qt::ISODate) : QString();
    QString erro;
    const bool ok = emPromocao ? repo.alterar(idProduto, precoPorBase, dataFim, &erro)
                               : repo.iniciar(idProduto, p.precoDe, precoPorBase, dataFim, &erro);
    if (!ok)
        return {false, erro};
    return {true, emPromocao ? "Promoção atualizada." : "Promoção iniciada."};
}

Promocao_service::Resultado Promocao_service::encerrar(qlonglong idProduto)
{
    QString erro;
    if (!repo.encerrar(idProduto, &erro))
        return {false, erro};
    return {true, "Promoção encerrada: o produto voltou ao preço de antes."};
}

int Promocao_service::encerrarVencidas(const QDate &hoje)
{
    int n = 0;
    for (qlonglong id : repo.vencidas(hoje.toString(Qt::ISODate)))
        if (encerrar(id).ok)
            ++n;
    return n;
}
