#ifndef DIVIDIRPAGAMENTODIALOG_H
#define DIVIDIRPAGAMENTODIALOG_H

#include <QDialog>
#include <QList>
#include "dto/Vendas_dto.h"
#include "services/pagamentodividido_service.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class SeletorMaquinaCartao;

// Pagamento em mais de uma forma (ex.: Dinheiro + Pix, Cartão + Dinheiro).
// O operador digita quanto o cliente entregou em cada forma; o troco sai do dinheiro.
class DividirPagamentoDialog : public QDialog
{
    Q_OBJECT
public:
    DividirPagamentoDialog(double valorFinal, double taxaCreditoPadrao, double taxaDebitoPadrao,
                           QWidget *parent = nullptr);

    // partes prontas para a venda (valor = o que abate a venda; dinheiro traz o recebido e o troco)
    QList<PagamentoVendaDTO> pagamentos() const { return partes; }

    // Confere e monta as partes a partir do que foi entregue em cada forma. Texto vazio = ok.
    using Entrega = PagamentoDividido::Entrega;
    static QString montar(double valorFinal, const QList<Entrega> &entregas, QList<PagamentoVendaDTO> *saida);

private:
    struct Linha {
        QComboBox *forma = nullptr;
        QLineEdit *valor = nullptr;
    };
    double valorFinal;
    double taxaCredito;
    double taxaDebito;
    QList<Linha> linhas;
    QLabel *lblSituacao = nullptr;
    QLabel *lblMaquina = nullptr;
    QComboBox *cbMaquina = nullptr;
    SeletorMaquinaCartao *seletor = nullptr;
    QPushButton *btnOk = nullptr;
    QList<PagamentoVendaDTO> partes;
    bool ultimaManual = false;

    class QGridLayout *grade = nullptr;
    void adicionarLinha(const QString &formaInicial);
    QList<Entrega> entregas() const;
    void atualizar();
    void aoEditar(QLineEdit *quem);
    void confirmar();
    static double lerValor(const QString &texto);
};

#endif // DIVIDIRPAGAMENTODIALOG_H
