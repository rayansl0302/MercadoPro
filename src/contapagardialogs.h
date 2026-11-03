#ifndef CONTAPAGARDIALOGS_H
#define CONTAPAGARDIALOGS_H

#include <QDialog>
#include "dto/ContasPagar_dto.h"
#include "services/contaspagar_service.h"

class QCheckBox;
class QComboBox;
class QDateEdit;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QSpinBox;

// Lançar ou editar uma conta a pagar (com parcelas no lançamento).
class NovaContaDialog : public QDialog
{
    Q_OBJECT
public:
    // conta.id > 0: edição de conta em aberto (sem parcelas). Fornecedor/descrição vêm preenchidos se
    // a conta veio de outra tela (ex.: Compras).
    explicit NovaContaDialog(const ContaPagarDTO &conta = ContaPagarDTO(), QWidget *parent = nullptr);

    ContaPagarDTO conta() const;
    int parcelas() const;
    ContasPagar_service::Periodicidade periodicidade() const;

private:
    qlonglong idConta = 0;
    QLineEdit *edDescricao;
    QLineEdit *edFornecedor;
    QComboBox *cbCategoria;
    QLineEdit *edDocumento;
    QDoubleSpinBox *spValor;
    QDateEdit *dtVencimento;
    QSpinBox *spParcelas;
    QCheckBox *chValorPorParcela;
    QLabel *lblValor;
    QComboBox *cbPeriodicidade;
    QPlainTextEdit *edObservacao;
    QLabel *lblPrevia;
    QLabel *lblAviso;

    void atualizarPrevia();
    void confirmar();
};

// Baixa (pagamento) de uma conta.
class BaixaContaDialog : public QDialog
{
    Q_OBJECT
public:
    explicit BaixaContaDialog(const ContaPagarDTO &conta, QWidget *parent = nullptr);

    double valorPago() const;
    QString forma() const;
    QDateTime quando() const;

private:
    QDoubleSpinBox *spValor;
    QComboBox *cbForma;
    QDateEdit *dtPagamento;
};

#endif // CONTAPAGARDIALOGS_H
