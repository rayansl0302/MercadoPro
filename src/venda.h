#ifndef VENDA_H
#define VENDA_H

#include <QVector>
#include <QSqlDatabase>
#include <QStandardItemModel>
#include <QTimer>
#include "services/rascunhovenda_service.h"
#include "vendas.h"
#include <QItemSelection>
#include "mainwindow.h"
#include <QLocale>
#include "dto/Config_dto.h"
#include "dto/Cliente_dto.h"
#include "services/Produto_service.h"
#include "services/cliente_service.h"
#include "services/vendas_service.h"
#include "services/notafiscal_service.h"
#include "subclass/waitdialog.h"

class QLabel;
class QResizeEvent;

namespace Ui {
class venda;
}

class venda : public QWidget
{
    Q_OBJECT

public:
    QSqlDatabase db = QSqlDatabase::database();
    explicit venda(QWidget *parent = nullptr);
    ~venda();
    QString Total();
    QLocale portugues;
    QIcon deletar;

    // Carrinho com itens: a sessão não pode expirar enquanto o operador monta a venda.
    // (As páginas de cliente e pagamento só são alcançáveis com o carrinho cheio.)
    bool carrinhoComItens() const { return modeloSelecionados->rowCount() > 0; }

protected:
    void handleSelectionChangeProdutos(const QItemSelection &selected, const QItemSelection &deselected);
    void keyPressEvent(QKeyEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    qlonglong validarCliente(bool mostrarMensagens);
    void atualizarListaCliente();
    void atualizarTotalProduto();
    void selecionarClienteNovo();
    int adicionarAoCarrinho(qlonglong id, const QString &descricao, double preco, double quantidade);
    void atualizarBotaoSelecionar();
    void selecionarPrimeiraLinhaCatalogo();
    void configurarColunasCatalogo();
    void focarCatalogo();
    int inserirLinhaCarrinho(int posicao, qlonglong id, const QString &descricao,
                             double preco, double quantidade);
    void destacarItem(int row);
    void alterarQuantidade(int row, int delta);
    void removerItem(int row);
    void desfazerRemocao();
    void atualizarContagemItens();
    void definirClientePadrao();
    void reiniciarVenda(bool manterRascunho);
    void avancarParaPagamento();
    void finalizarRapido();
    bool pagamentoValido();
    void selecionarFormaPagamento(int index);
    bool eventFilter(QObject *obj, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void igualarCartoesPagamento();

private slots:
    void on_Btn_SelecionarProduto_clicked();
    void handleSelectionChange(const QItemSelection &selected, const QItemSelection &deselected);
    void on_Btn_Pesquisa_clicked();
    void on_Btn_Aceitar_clicked();
    void on_Btn_Voltar_clicked();
    void on_Ledit_Pesquisa_textChanged(const QString &arg1);
    void on_Tview_ProdutosSelecionados_customContextMenuRequested(const QPoint &pos);
    void deletarProd();
    void on_Ledit_Pesquisa_returnPressed();
    void on_Btn_CancelarVenda_clicked();
    void on_Btn_NovoCliente_clicked();
    // payment slots
    void on_CBox_FormaPagamento_activated(int index);
    void on_Ledit_Taxa_textChanged(const QString &arg1);
    void on_Ledit_Desconto_textChanged(const QString &arg1);
    void on_Ledit_Recebido_textChanged(const QString &arg1);
    void on_Ledit_Recebido_returnPressed();
    void on_CheckPorcentagem_stateChanged(int arg1);
    void on_CBox_ModeloEmit_currentIndexChanged(int index);

private:
    // maquininha de cartão: o cliente paga o preço de gôndola, a taxa da máquina escolhida é custo da loja
    class SeletorMaquinaCartao *seletorMaquina = nullptr;
    void mostrarTaxaMaquina(const QString &forma, double taxaPadrao);
    QSqlQueryModel *modeloProdutos = new QSqlQueryModel;
    QStandardItemModel *modeloSelecionados = new QStandardItemModel;
    Ui::venda *ui;
    QAction *actionMenuDeletarProd;
    QStringList clientesComId;
    ConfigDTO configDTO;
    // payment
    Vendas_service vendaServ;
    NotaFiscal_service notaServ;
    FiscalEmitter_service fiscalServ;
    WaitDialog *waitDialog = nullptr;
    ClienteDTO CLIENTE;
    qlonglong idClienteAtual = -1;
    bool emitTodosNf = false;

    QString getIdProdSelected();
    void verProd();
    void atualizarIndicadorOperador();
    void irParaPagina(int pagina);
    void configurarPaginaPagamento();
    void configurarPaginaNF();
    float obterValorFinal(QString taxa, QString desconto);
    void descontoTaxa();
    void terminarPagamento();
    void mostrarToast(const QString &texto);
    void posicionarToast();
    void salvarRascunho();
    void descartarRascunho();
    void verificarRascunho();
    Produto_Service prodServ;
    Cliente_service cliServ;
    QList<ProdutoVendidoDTO> obterProdutosSelecionados();
    QTimer *rascunhoTimer = nullptr;
    RascunhoVenda_service rascunhoServ;
    RascunhoVendaDTO rascunhoPendente;
    bool temRascunhoPendente = false;

    struct ItemRemovido {
        qlonglong id = 0;
        QString descricao;
        double quantidade = 0;
        double preco = 0;
        int linha = 0;
    };
    ItemRemovido ultimoRemovido;
    bool temRemovido = false;
    bool pulouCliente = false;
    bool vendaFinalizada = false;
    QTimer *desfazerTimer = nullptr;
    QLabel *toastSucesso = nullptr;
    QTimer *toastTimer = nullptr;

signals:
    void vendaConcluida();
};

#endif // VENDA_H
