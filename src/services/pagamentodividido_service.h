#ifndef PAGAMENTODIVIDIDO_SERVICE_H
#define PAGAMENTODIVIDIDO_SERVICE_H

#include <QList>
#include <QString>
#include "../dto/Vendas_dto.h"

// Regra do pagamento dividido (ex.: Dinheiro + Pix): confere o que o cliente entregou em cada forma
// e monta as partes da venda. O troco só sai do dinheiro.
namespace PagamentoDividido {

struct Entrega {
    QString forma;
    double entregue = 0;
};

// Texto vazio = ok (e `saida` recebe as partes: valor = o que abate a venda; dinheiro traz recebido e troco).
QString montar(double valorFinal, const QList<Entrega> &entregas, QList<PagamentoVendaDTO> *saida);

}

#endif // PAGAMENTODIVIDIDO_SERVICE_H
