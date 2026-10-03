#ifndef PEDIDOAPLICADOR_H
#define PEDIDOAPLICADOR_H

#include <QJsonObject>
#include <QString>

struct ResultadoPedido {
    QString status;                  // APLICADO | RECUSADO | CONFLITO | EXPIRADO
    QString mensagem;
    QJsonObject detalhe;
    bool aplicado() const { return status == QLatin1String("APLICADO"); }
};

// Aplica no banco da loja um pedido vindo do painel, SEMPRE pelos serviços do sistema (mesmas regras de
// validação de qualquer tela). Nunca grava SQL direto.
//
// Pedido: { id, tipo, criadoEm, expiraEm, solicitante, payload }
//  CONTA_PAGAR_BAIXAR : payload { contaId, valorPago, forma, data("yyyy-MM-dd") }
//  CONTA_PAGAR_LANCAR : payload { descricao, fornecedor, categoria, documento, valor, vencimento("yyyy-MM-dd"),
//                                 parcelas, periodicidade(MENSAL|QUINZENAL|SEMANAL), observacao, idEmpresa }
class PedidoAplicador
{
public:
    ResultadoPedido aplicar(const QJsonObject &pedido);

private:
    ResultadoPedido baixarConta(const QJsonObject &payload, const QString &origem);
    ResultadoPedido lancarConta(const QJsonObject &payload, const QString &origem);
};

#endif // PEDIDOAPLICADOR_H
