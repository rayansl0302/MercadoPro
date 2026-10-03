#include "pedidoaplicador.h"
#include "../services/contaspagar_service.h"
#include "../services/empresa_service.h"
#include <QDate>
#include <QDateTime>
#include <QJsonValue>

namespace {
ResultadoPedido resultado(const QString &status, const QString &mensagem, const QJsonObject &detalhe = QJsonObject())
{
    ResultadoPedido r;
    r.status = status;
    r.mensagem = mensagem;
    r.detalhe = detalhe;
    return r;
}

// números chegam como número ou como texto ("12.50"): aceita os dois
double numero(const QJsonValue &v, bool *ok)
{
    if (v.isDouble()) {
        *ok = true;
        return v.toDouble();
    }
    return v.toString().replace(',', '.').toDouble(ok);
}
}

ResultadoPedido PedidoAplicador::aplicar(const QJsonObject &pedido)
{
    const QString tipo = pedido.value("tipo").toString();
    const QString solicitante = pedido.value("solicitante").toString().trimmed();
    const QString origem = QStringLiteral("Painel: %1").arg(solicitante.isEmpty() ? QStringLiteral("usuário") : solicitante);

    // pedido que esperou demais (a loja ficou fechada) não executa: o contexto de quem pediu já passou
    const QDateTime expira = QDateTime::fromString(pedido.value("expiraEm").toString(), Qt::ISODate);
    if (expira.isValid() && QDateTime::currentDateTimeUtc() > expira)
        return resultado("EXPIRADO", "O pedido expirou antes de a loja processá-lo. Faça de novo se ainda for necessário.");

    const QJsonObject payload = pedido.value("payload").toObject();
    if (tipo == QLatin1String("CONTA_PAGAR_BAIXAR"))
        return baixarConta(payload, origem);
    if (tipo == QLatin1String("CONTA_PAGAR_LANCAR"))
        return lancarConta(payload, origem);
    return resultado("RECUSADO", QStringLiteral("Tipo de pedido desconhecido: %1.").arg(tipo));
}

ResultadoPedido PedidoAplicador::baixarConta(const QJsonObject &p, const QString &origem)
{
    ContasPagar_service servico;
    const qlonglong id = qRound64(p.value("contaId").toDouble(p.value("contaId").toString().toDouble()));
    bool ok = false;
    const double valor = numero(p.value("valorPago"), &ok);
    if (id <= 0 || !ok)
        return resultado("RECUSADO", "Pedido de baixa incompleto (conta ou valor ausentes).");
    const QDate data = QDate::fromString(p.value("data").toString(), Qt::ISODate);
    if (!data.isValid())
        return resultado("RECUSADO", "Data do pagamento inválida.");

    const ContaPagarDTO conta = servico.getConta(id);
    if (conta.id <= 0)
        return resultado("RECUSADO", "A conta não existe mais na loja.");
    // o painel mostrou a conta como em aberto; se na loja ela já mudou, é conflito (nada é duplicado)
    if (!conta.aberta())
        return resultado("CONFLITO",
                         QStringLiteral("A conta já estava %1 na loja.").arg(conta.status == kContaPaga ? "paga" : "cancelada"),
                         QJsonObject{{"statusNaLoja", conta.status}});

    const auto r = servico.baixar(id, valor, p.value("forma").toString(), QDateTime(data, QTime::currentTime()), origem);
    if (!r.ok)
        return resultado("RECUSADO", r.msg);
    return resultado("APLICADO", r.msg, QJsonObject{{"contaId", double(id)}});
}

ResultadoPedido PedidoAplicador::lancarConta(const QJsonObject &p, const QString &origem)
{
    ContasPagar_service servico;
    ContaPagarDTO c;
    bool okValor = false;
    c.valor = numero(p.value("valor"), &okValor);
    if (!okValor)
        return resultado("RECUSADO", "Valor da conta ausente.");
    c.descricao = p.value("descricao").toString();
    c.fornecedor = p.value("fornecedor").toString();
    c.categoria = p.value("categoria").toString();
    c.documento = p.value("documento").toString();
    c.observacao = p.value("observacao").toString();
    c.vencimento = p.value("vencimento").toString();

    c.idEmpresa = qRound64(p.value("idEmpresa").toDouble(p.value("idEmpresa").toString().toDouble()));
    if (c.idEmpresa > 0) {
        const EmpresaDTO e = Empresa_service::instancia()->getPorId(c.idEmpresa);
        if (!e.valida() || !e.ativa)
            return resultado("RECUSADO", "A empresa do pedido não existe ou está desativada na loja.");
    }

    const int parcelas = p.value("parcelas").isDouble() ? p.value("parcelas").toInt() : p.value("parcelas").toString().toInt();
    const QString per = p.value("periodicidade").toString().toUpper();
    const auto periodicidade = per == "SEMANAL"    ? ContasPagar_service::Periodicidade::Semanal
                               : per == "QUINZENAL" ? ContasPagar_service::Periodicidade::Quinzenal
                                                    : ContasPagar_service::Periodicidade::Mensal;

    const auto r = servico.lancar(c, parcelas > 0 ? parcelas : 1, periodicidade, origem);
    if (!r.ok)
        return resultado("RECUSADO", r.msg);
    return resultado("APLICADO", r.msg, QJsonObject{{"contaId", double(r.id)}, {"parcelas", r.quantidade}});
}
