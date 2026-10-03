#include "contaspagar_service.h"
#include "sessao_service.h"
#include "../infra/empresaativa.h"
#include <QDateTime>
#include <QLocale>
#include <QUuid>
#include <cmath>

std::function<bool()> ContasPagar_service::autorizador;

namespace {
double arredondar2(double v) { return std::round(v * 100.0) / 100.0; }

QString dinheiro(double v)
{
    return QLocale(QLocale::Portuguese, QLocale::Brazil).toCurrencyString(v, "R$ ");
}
}

ContasPagar_service::ContasPagar_service(QObject *parent)
    : QObject(parent)
{
}

void ContasPagar_service::definirAutorizador(std::function<bool()> a)
{
    autorizador = std::move(a);
}

bool ContasPagar_service::autorizado()
{
    return !autorizador || autorizador();
}

QStringList ContasPagar_service::formasDePagamento()
{
    return {"Dinheiro", "Pix", "Transferência", "Boleto", "Cartão de débito", "Cartão de crédito", "Cheque", "Outro"};
}

QStringList ContasPagar_service::categoriasSugeridas()
{
    return {"Fornecedor / mercadoria", "Aluguel", "Energia", "Água", "Internet e telefone", "Impostos e taxas",
            "Salários e encargos", "Manutenção", "Marketing", "Contador", "Outros"};
}

QList<double> ContasPagar_service::dividirValor(double total, int parcelas)
{
    QList<double> valores;
    if (parcelas < 1)
        return valores;
    const qint64 centavos = qRound64(total * 100.0);
    const qint64 base = centavos / parcelas;
    qint64 acumulado = 0;
    for (int i = 0; i < parcelas; ++i) {
        const qint64 c = (i == parcelas - 1) ? centavos - acumulado : base;   // sobra de centavos na última
        valores.append(c / 100.0);
        acumulado += c;
    }
    return valores;
}

ContasPagar_service::Resultado ContasPagar_service::lancar(const ContaPagarDTO &base, int parcelas,
                                                           Periodicidade periodicidade, const QString &origem)
{
    if (base.descricao.trimmed().size() < 2)
        return {false, "Informe a descrição da conta."};
    if (base.valor <= 0)
        return {false, "Informe um valor maior que zero."};
    if (parcelas < 1 || parcelas > 120)
        return {false, "O número de parcelas deve ser de 1 a 120."};
    const QDate primeiro = QDate::fromString(base.vencimento, Qt::ISODate);
    if (!primeiro.isValid())
        return {false, "Informe um vencimento válido."};
    if (arredondar2(base.valor) / parcelas < 0.01)
        return {false, "O valor é pequeno demais para esse número de parcelas."};

    const QList<double> valores = dividirValor(arredondar2(base.valor), parcelas);
    const QString grupo = parcelas > 1 ? QUuid::createUuid().toString(QUuid::WithoutBraces) : QString();
    const qlonglong empresa = base.idEmpresa > 0 ? base.idEmpresa : EmpresaAtiva::id();

    QList<ContaPagarDTO> contas;
    for (int i = 0; i < parcelas; ++i) {
        ContaPagarDTO c = base;
        c.id = 0;
        c.idEmpresa = empresa;
        c.descricao = base.descricao.trimmed();
        c.fornecedor = base.fornecedor.trimmed();
        c.categoria = base.categoria.trimmed();
        c.documento = base.documento.trimmed();
        c.valor = valores.at(i);
        c.status = kContaAberta;
        c.parcela = i + 1;
        c.totalParcelas = parcelas;
        c.grupo = grupo;
        // sempre a partir do primeiro vencimento: somar mês a mês acumularia o recuo do dia 31 -> 30 -> 28
        QDate venc = primeiro;
        switch (periodicidade) {
        case Periodicidade::Mensal: venc = primeiro.addMonths(i); break;
        case Periodicidade::Quinzenal: venc = primeiro.addDays(15 * i); break;
        case Periodicidade::Semanal: venc = primeiro.addDays(7 * i); break;
        }
        c.vencimento = venc.toString(Qt::ISODate);
        c.idOperadorSessao = Sessao_service::instancia()->ativa() ? Sessao_service::instancia()->idOperador() : -1;
        contas.append(c);
    }

    QString erro;
    QList<qlonglong> ids;
    if (!repo.inserirVarias(contas, &ids, &erro))
        return {false, "Não foi possível lançar a conta: " + erro};
    if (!origem.isEmpty())
        Sessao_service::instancia()->registrarAcao(
            QStringLiteral("CONTA_PAGAR_LANCADA_PAINEL"),
            QStringLiteral("[%1] Conta #%2 (%3, %4) lançada em %5 parcela(s)")
                .arg(origem).arg(ids.value(0)).arg(base.descricao.trimmed(), dinheiro(base.valor)).arg(parcelas));
    return {true, parcelas > 1 ? QString("%1 parcelas lançadas.").arg(parcelas) : QStringLiteral("Conta lançada."),
            ids.value(0), parcelas};
}

ContasPagar_service::Resultado ContasPagar_service::alterar(const ContaPagarDTO &conta)
{
    if (conta.descricao.trimmed().size() < 2)
        return {false, "Informe a descrição da conta."};
    const ContaPagarDTO atual = repo.getPorId(conta.id);
    if (atual.id <= 0)
        return {false, "Conta não encontrada."};

    ContaPagarDTO c = conta;
    c.descricao = conta.descricao.trimmed();
    c.fornecedor = conta.fornecedor.trimmed();
    c.categoria = conta.categoria.trimmed();
    c.documento = conta.documento.trimmed();
    c.observacao = conta.observacao.trimmed();
    QString erro;

    if (atual.status == kContaPaga) {
        // conta paga: só os textos (corrigir descrição, fornecedor...). Valor e vencimento mudam
        // estornando o pagamento antes.
        if (!repo.atualizarTextos(c, &erro))
            return {false, erro};
        return {true, "Conta atualizada.", c.id, 1};
    }
    if (conta.valor <= 0)
        return {false, "Informe um valor maior que zero."};
    if (!QDate::fromString(conta.vencimento, Qt::ISODate).isValid())
        return {false, "Informe um vencimento válido."};
    c.valor = arredondar2(conta.valor);
    if (!repo.atualizarAberta(c, &erro))
        return {false, erro};
    return {true, "Conta atualizada.", c.id, 1};
}

ContasPagar_service::Resultado ContasPagar_service::baixar(qlonglong id, double valorPago, const QString &forma,
                                                           const QDateTime &quando, const QString &origem)
{
    const ContaPagarDTO c = repo.getPorId(id);
    if (c.id <= 0)
        return {false, "Conta não encontrada."};
    if (!c.aberta())
        return {false, "Esta conta já foi baixada ou cancelada."};
    if (valorPago <= 0)
        return {false, "Informe o valor pago."};
    if (forma.trimmed().isEmpty())
        return {false, "Informe a forma de pagamento."};

    const QDateTime em = quando.isValid() ? quando : QDateTime::currentDateTime();
    QString erro;
    if (!repo.baixar(id, arredondar2(valorPago), forma.trimmed(), em.toString("yyyy-MM-dd HH:mm:ss"), &erro))
        return {false, erro};
    Sessao_service::instancia()->registrarAcao(
        origem.isEmpty() ? QStringLiteral("CONTA_PAGAR_BAIXA") : QStringLiteral("CONTA_PAGAR_BAIXA_PAINEL"),
        QStringLiteral("%1Conta #%2 (%3) paga: %4 em %5")
            .arg(origem.isEmpty() ? QString() : QStringLiteral("[%1] ").arg(origem))
            .arg(id).arg(c.descricao, dinheiro(valorPago), forma.trimmed()));
    return {true, "Conta baixada.", id, 1};
}

ContasPagar_service::Resultado ContasPagar_service::estornarBaixa(qlonglong id)
{
    if (!autorizado())
        return {false, "Estornar um pagamento é restrito a gerentes."};
    const ContaPagarDTO c = repo.getPorId(id);
    if (c.id <= 0)
        return {false, "Conta não encontrada."};
    QString erro;
    if (!repo.estornarBaixa(id, &erro))
        return {false, erro};
    Sessao_service::instancia()->registrarAcao(
        QStringLiteral("CONTA_PAGAR_ESTORNO"),
        QStringLiteral("Pagamento da conta #%1 (%2, %3) estornado").arg(id).arg(c.descricao, dinheiro(c.valorPago)));
    return {true, "Pagamento estornado: a conta voltou para em aberto.", id, 1};
}

ContasPagar_service::Resultado ContasPagar_service::cancelar(qlonglong id, const QString &motivo)
{
    if (!autorizado())
        return {false, "Cancelar uma conta é restrito a gerentes."};
    if (motivo.trimmed().size() < 3)
        return {false, "Informe o motivo do cancelamento."};
    const ContaPagarDTO c = repo.getPorId(id);
    if (c.id <= 0)
        return {false, "Conta não encontrada."};
    QString erro;
    if (!repo.cancelar(id, motivo.trimmed(), &erro))
        return {false, erro};
    Sessao_service::instancia()->registrarAcao(
        QStringLiteral("CONTA_PAGAR_CANCELAMENTO"),
        QStringLiteral("Conta #%1 (%2, %3) cancelada: %4").arg(id).arg(c.descricao, dinheiro(c.valor), motivo.trimmed()));
    return {true, "Conta cancelada.", id, 1};
}

QList<ContaPagarDTO> ContasPagar_service::listar(const FiltroContasPagarDTO &filtro)
{
    return repo.listar(filtro);
}

ContaPagarDTO ContasPagar_service::getConta(qlonglong id)
{
    return repo.getPorId(id);
}

ResumoContasPagarDTO ContasPagar_service::resumo(qlonglong idEmpresa)
{
    return repo.resumo(idEmpresa, QDate::currentDate().toString(Qt::ISODate));
}
