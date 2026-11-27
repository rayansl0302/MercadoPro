#ifndef PEDIDOENTREGAJANELA_H
#define PEDIDOENTREGAJANELA_H

#include <QWidget>
#include <QList>
#include <QLocale>
#include "dto/Entrega_dto.h"
#include "services/entrega_service.h"
#include "services/cliente_service.h"

class QLineEdit;
class QComboBox;
class QTableWidget;
class QDoubleSpinBox;
class QLabel;
class QCheckBox;
class QPushButton;

// Pedido de entrega: lista os itens com o preço de entrega, o operador digita as quantidades
// (ou o valor em reais) e a venda é lançada de uma vez, marcada como entrega.
class PedidoEntregaJanela : public QWidget
{
    Q_OBJECT

public:
    explicit PedidoEntregaJanela(QWidget *parent = nullptr);

signals:
    void pedidoLancado();

private slots:
    void lancarPedido();
    void limpar();

private:
    enum Coluna { ColItem = 0, ColUnidade, ColPreco, ColQuantidade, ColValor, ColTotal, TotalColunas };

    void montarTela();
    void carregarItens();
    void atualizarLinha(int row);
    void atualizarTotal();
    double quantidadeDaLinha(int row) const;
    QList<EntregaLinhaDTO> linhasDoPedido() const;
    bool validarCliente(qlonglong *idCliente, QString *nome);

    QLocale portugues;
    Entrega_service entregaServ;
    Cliente_service clienteServ;

    QList<EntregaItemDTO> itens;
    bool atualizando = false;

    QLineEdit *leCliente = nullptr;
    QLineEdit *leEndereco = nullptr;
    QLineEdit *leObservacao = nullptr;
    QComboBox *cbForma = nullptr;
    QTableWidget *tabela = nullptr;
    QLabel *lblTotal = nullptr;
    QCheckBox *chkRecibo = nullptr;
    QPushButton *btnLancar = nullptr;
};

#endif // PEDIDOENTREGAJANELA_H
