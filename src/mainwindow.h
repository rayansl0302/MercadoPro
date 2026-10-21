#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#if defined(Q_OS_WIN)
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#endif


#include <QMainWindow>
#include <QStandardItemModel>
#include <vector>
#include <QSqlDatabase>
#include <QSqlQueryModel>
#include <QLocale>
#include <QSet>
#include <QRandomGenerator>
#include <QKeyEvent>
#include <QPointer>
#include <QLabel>
#include <QToolButton>
#include "configuracao.h"
#include "subclass/customlineedit.h"
#include "nota/acbrmanager.h"
#include "../services/Produto_service.h"
#include "dto/Config_dto.h"
#include "dto/Sessao_dto.h"
#include "services/contingencia_service.h"


#define VERSAO_QE "2.10.0"

QT_BEGIN_NAMESPACE
class MenuInicial;
namespace Ui {
class MainWindow; }
QT_END_NAMESPACE

class venda;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:


    QSqlDatabase db;
    QSqlQueryModel* model = nullptr;
    void atualizarTableview();
    void abrirPdv();
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    // Exibe a tarja "MODO DESENVOLVIMENTO" no topo: só entra ligado pelo autologin de Debug.
    void setModoDesenvolvimento(bool ativo);
    // Chamado depois de abrir a sessão: repõe o rodapé e arma o timeout de inatividade.
    void aplicarSessao();
    // Se houver mais de uma empresa, pergunta com qual vai trabalhar (uma vez por abertura do programa).
    void perguntarEmpresaSeNecessario();
    QLocale portugues;
    QIcon iconAlterarProduto, iconAddProduto, iconBtnVenda, iconDelete, iconPesquisa, iconBtnRelatorios,
        iconImpressora, iconClientes;

public slots:
   void atualizarTableviewComQuery(QString &query);

private slots:

    void on_Btn_Delete_clicked();

    void on_Btn_Pesquisa_clicked();

    void on_Btn_Alterar_clicked();

    void on_Btn_Venda_clicked();

    void on_Btn_PDV_clicked();

    void on_Btn_Relatorios_clicked();

    void on_Btn_Orcamento_clicked();

    void on_actionRealizar_Venda_triggered();

    void on_Btn_AddProd_clicked();

    void on_Tview_Produtos_customContextMenuRequested(const QPoint &pos);

    void imprimirEtiqueta1();
    void imprimirEtiqueta3();

    void imprimirNomePreco1();
    void imprimirNomePreco3();

    void on_actionConfig_triggered();

    void on_Ledit_Pesquisa_textChanged(const QString &arg1);

    void on_Btn_Clientes_clicked();

    void setLocalProd();

    void verProd();


    void on_actionDocumenta_o_triggered();
    void atualizarConfigAcbr();

    void on_Btn_Entradas_clicked();

    void on_actionEnviar_triggered();

    void on_actionSobre_triggered();

    void on_actionEnviar_Notas_Contador_triggered();

    void on_actionMonitor_Fiscal_triggered();

    void on_actionInutilizar_Numera_o_NF_triggered();

    void on_actionEnviar_Carta_de_Corre_o_triggered();

    void on_actionSQLite_triggered();

    void abrirCaixaClicked();
    void fecharCaixaClicked();
    void sangriaClicked();
    void suprimentoClicked();
    void historicoCaixaClicked();
    // tela inicial (menu de botões) x lista de produtos
    void montarMenuInicial();
    void irParaInicio();
    void irParaProdutos();
    bool naListaDeProdutos() const;
    void atualizarSaudacao();
    void atualizarLogoCabecalho();
    // multi-empresa e contas a pagar
    void escolherEmpresaClicked();
    void abrirContasPagar() { contasPagarClicked(); }
    void abrirContasReceber() { contasReceberClicked(); }
    void nuvemClicked();
    void atualizarIndicadorNuvem();
    void contasReceberClicked(qlonglong idCliente = 0);
    void contasPagarClicked(const QString &statusInicial = QString());
    void atualizarIndicadorEmpresa();
    void atualizarAlertaContas();
    void empresaMudou();
    void operadoresClicked();
    void trocarOperadorClicked();
    void sairSessaoClicked();
    void sessaoExpirada();
    void sessaoBloqueada();
    void sessaoInvalidada(const QString &motivo);
    void logAcessoClicked();
    void mostrarAvisoTimeout(int segundosRestantes);

private:
    Ui::MainWindow *ui;
    // ACBrNFe *acbr;
    bool verificarCodigoBarras();
    QSet<QString> generatedNumbers;
    QAction* actionMenuAlterarProd;
    QAction* actionMenuDeletarProd;
    QAction* actionSetLocalProd;
    QAction* actionMenuPrintBarCode1;
    QAction* actionMenuPrintBarCode3;
    QAction* actionMenuPrintNomePreco1;
    QAction* actionMenuPrintNomePreco3;
    QAction* actionVerProduto;
    Produto_Service *produtoService;
    Config_service *confServ = new Config_service(this);
    ConfigDTO configDTO;
    ContingenciaService *contingenciaService = nullptr;
    // QPointer zera sozinho quando a tela de venda é fechada (WA_DeleteOnClose).
    // Fica como QWidget porque venda só é forward-declared aqui; o .cpp faz o
    // static_cast quando precisa dos métodos específicos do PDV.
    QPointer<QWidget> pdvAberto;
public:
    // true quando é seguro reiniciar o programa para atualizar: sem venda aberta e sem diálogo na frente
    bool ociosoParaAtualizar() const;
    // Ajuda > Verificar atualizações: consulta agora e, se houver versão nova, oferece instalar
    void verificarAtualizacoes();
private:


    void setarIconesJanela();
    //QModelIndex selected_index;

    const int ultimaVersaoSchema = 23;

    // operador comum só entra em histórico, cadastro de operadores, configurações e
    // relatórios gerenciais depois de informar o PIN do gerente
    bool exigirGerente(const QString &acao);
    void bloqueioParaTroca(QString *motivo) const;
    void encerrarSessao(bool sairDoPrograma);
    // Abre a tela de login e, se entrar, arma a sessão e o timeout. Devolve false se
    // cancelou (e, nesse caso, fecha o programa quando fecharSeCancelar).
    bool pedirLoginNovamente(bool fecharSeCancelar);
    // abre a sessão do login aceito e arma o timeout; false (e encerra o programa) se não abrir
    bool iniciarSessao(const SessaoDTO &nova);
    // sem sessão aceita o programa não continua: fecha todas as janelas (inclusive o PDV)
    void encerrarPrograma();
    void atualizarIndicadorSessao();
    void mostrarProdutoPorCodigoBarras(const QString &codigo);
    void imprimirNomePreco(int quantidade);
    void iniciarMigration();
    void atualizarConfigDTO();
    void montarMenuCaixa();
    void atualizarIndicadorCaixa();
    bool garantirCaixaAberto();
    QLabel *lblCaixaStatus = nullptr;
    bool saindoDoPrograma = false;
    QToolButton *btnOperador = nullptr;   // chip do operador logado (menu: trocar / sair)
    QToolButton *btnSair = nullptr;       // "Sair da conta", sempre à vista
    QLabel *lblAvisoTimeout = nullptr;
    MenuInicial *menuInicial = nullptr;
    QToolButton *btnEmpresa = nullptr;
    QToolButton *btnNuvem = nullptr;        // estado da sincronização com o painel online     // empresa (CNPJ) em uso, no rodapé
    bool empresaPerguntada = false;
    QAction *actionTrocarOperador = nullptr;
    QAction *actionSairSessao = nullptr;
    bool modoDesenvolvimento = false;
protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    QString getIdProdSelected();
signals:
    void localSetado();





};
#endif // MAINWINDOW_H
