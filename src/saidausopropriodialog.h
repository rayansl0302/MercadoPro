#ifndef SAIDAUSOPROPRIODIALOG_H
#define SAIDAUSOPROPRIODIALOG_H

#include <QDialog>

class QComboBox;
class QDateEdit;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QTableWidget;

// Dar saída de um produto do estoque para uso da própria loja (consumo, limpeza, quebra...).
class SaidaUsoProprioDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SaidaUsoProprioDialog(qlonglong idProduto, QWidget *parent = nullptr);

    bool registrou() const { return registrou_; }

private slots:
    void confirmar();
    void atualizarCusto();

private:
    qlonglong idProduto_;
    double estoque_ = 0;
    double custo_ = 0;
    bool semControle_ = false;
    bool registrou_ = false;
    QDoubleSpinBox *quantidade_ = nullptr;
    QComboBox *motivo_ = nullptr;
    QLineEdit *observacao_ = nullptr;
    QLabel *lblCusto_ = nullptr;
};

// Histórico das saídas para uso próprio, com filtro de período e motivo e o total a custo.
class HistoricoSaidasDialog : public QDialog
{
    Q_OBJECT
public:
    explicit HistoricoSaidasDialog(QWidget *parent = nullptr);

private slots:
    void carregar();
    void novaSaida();
    void removerSelecionada();

private:
    QDateEdit *de_ = nullptr;
    QDateEdit *ate_ = nullptr;
    QComboBox *motivo_ = nullptr;
    QTableWidget *tabela_ = nullptr;
    QLabel *lblTotal_ = nullptr;
};

#endif // SAIDAUSOPROPRIODIALOG_H
