#ifndef MAQUINASCARTAODIALOG_H
#define MAQUINASCARTAODIALOG_H

#include <QDialog>

class QCheckBox;
class QLabel;
class QPushButton;
class QTableWidget;

// Cadastro das maquininhas de cartão (nome e taxas de débito/crédito) e quanto cada uma custou de taxa no mês.
class MaquinasCartaoDialog : public QDialog
{
    Q_OBJECT
public:
    explicit MaquinasCartaoDialog(QWidget *parent = nullptr);

private slots:
    void carregar();
    void nova();
    void editar();
    void alternarAtiva();

private:
    qlonglong idSelecionado() const;
    void atualizarBotoes();

    QTableWidget *tabela_ = nullptr;
    QCheckBox *mostrarInativas_ = nullptr;
    QLabel *lblCustos_ = nullptr;
    QPushButton *btnEditar_ = nullptr;
    QPushButton *btnAtiva_ = nullptr;
};

// Janela de uma maquininha (nova ou existente). Devolve true se salvou.
class EditarMaquinaDialog : public QDialog
{
    Q_OBJECT
public:
    explicit EditarMaquinaDialog(qlonglong idMaquina, QWidget *parent = nullptr);

private slots:
    void salvar();

private:
    qlonglong id_;
    class QLineEdit *nome_ = nullptr;
    class QDoubleSpinBox *debito_ = nullptr;
    class QDoubleSpinBox *credito_ = nullptr;
};

#endif // MAQUINASCARTAODIALOG_H
