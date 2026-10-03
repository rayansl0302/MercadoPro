#include <QBoxLayout>
#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QSqlQueryModel>
#include "customdelegate.h"
#include "alterarproduto.h"
#include "QItemSelectionModel"
#include <qsqltablemodel.h>
#include <QPrintDialog>
#include <QtPrintSupport/QPrinter>
#include "vendas.h"
#include <QDoubleValidator>
#include "relatorios.h"
#include "janelaorcamento.h"
#include "venda.h"
#include <QModelIndex>
#include <QMenu>
#include <QFontDatabase>
#include "delegateprecof2.h"
#include "util/pdfexporter.h"
#include "clientes.h"
#include "pagamentovenda.h"
#include <QSqlError>
#include <QSqlRecord>
#include <QSql>
#include  "inserirproduto.h"
#include "subclass/leditdialog.h"
#include "infojanelaprod.h"
#include <QStandardPaths>
#include "util/helppage.h"
#include "util/consultacnpjmanager.h"
#include "entradas.h"
#include "util/manifestadordfe.h"
#include "util/mailmanager.h"
#include "janelaemailcontador.h"
#include "sobre.h"
#include "monitorfiscal.h"
#include "util/printutil.h"
#include "services/barcode_service.h"
#include "services/acbr_service.h"
#include "services/schemamigration_service.h"
#include "infra/databaseconnection_service.h"
#include "services/schemamigration_service.h"
#include "services/config_service.h"
#include "services/dfe_service.h"
#include "inutilizacaodialog.h"
#include "cartacorrecaojanela.h"
#include "services/export_service.h"
#include "services/escposprinter_service.h"
#include "services/caixa_service.h"
#include "aberturacaixa.h"
#include "fechamentocaixa.h"
#include "movimentacaocaixa.h"
#include "historicocaixas.h"
#include "operadores.h"
#include "loginoperador.h"
#include "logacessodialog.h"
#include "menuinicial.h"
#include "util/icones.h"
#include "empresadialog.h"
#include "produtoemvendasdialog.h"
#include "contaspagarjanela.h"
#include "contasreceberjanela.h"
#include "nuvemdialog.h"
#include "nuvem/nuvem_service.h"
#include "nuvem/nuvemconfig.h"
#include "services/empresa_service.h"
#include "services/contaspagar_service.h"
#include <QStackedWidget>
#include <QStandardPaths>
#include <QFileInfo>
#include <QPixmap>
#include <QCloseEvent>
#include "services/sessao_service.h"
#include <QInputDialog>
#include <QLineEdit>
#include <QDebug>
#include <QTimer>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    QCoreApplication::setApplicationVersion(VERSAO_QE);

    model = new QSqlQueryModel(this);

    //carrega as configurações no DTO
    configDTO = confServ->carregarTudo();
    qDebug() << "driver: " << configDTO.driverDB;

    DatabaseConnection_service::changeDatabase(configDTO);

    iniciarMigration();

    // a empresa em uso (CNPJ) é lida do banco já migrado; dados da empresa e fiscais são por empresa
    Empresa_service::instancia()->carregarEstadoInicial();
    configDTO = confServ->carregarTudo();

    db = DatabaseConnection_service::db();

    produtoService = new Produto_Service();


    // configuracao do modelo e view produtos
    ui->Tview_Produtos->setModel(model);


    if (configDTO.emitNfFiscal) {
        contingenciaService = new ContingenciaService(this);
        contingenciaService->iniciar();
    }

    // mostrar na tabela da aplicaçao a tabela do banco de dados.
    // header estilizado via stylesheet global
    ui->Ledit_Pesquisa->installEventFilter(this);
    atualizarTableview();
    model->setHeaderData(0, Qt::Horizontal, tr("ID"));
    model->setHeaderData(1, Qt::Horizontal, tr("Quantidade"));
    model->setHeaderData(2, Qt::Horizontal, tr("Descrição"));
    model->setHeaderData(3, Qt::Horizontal, tr("Preço"));
    model->setHeaderData(4, Qt::Horizontal, tr("Código de Barras"));
    model->setHeaderData(5, Qt::Horizontal, tr("NF"));
    //
    // Selecionar a primeira linha da tabela
    QModelIndex firstIndex = model->index(0, 0);
    ui->Tview_Produtos->selectionModel()->select(firstIndex, QItemSelectionModel::Select);

    // ajustar tamanho colunas
    // coluna descricao
    ui->Tview_Produtos->setColumnWidth(2, 750);
    //preco
    ui->Tview_Produtos->setColumnWidth(3, 100);

    // coluna quantidade (o cabeçalho em negrito precisa de folga) e código de barras
    ui->Tview_Produtos->setColumnWidth(0, 70);
    ui->Tview_Produtos->setColumnWidth(1, 115);
    ui->Tview_Produtos->setColumnWidth(4, 160);

    // ações para menu de contexto tabela produtos
    actionMenuAlterarProd = new QAction(this);
    actionMenuDeletarProd = new QAction(this);
    actionSetLocalProd = new QAction(this);
    actionVerProduto = new QAction(this);
    actionSetLocalProd->setText("Adicionar Local Produto");
    actionVerProduto->setText("Ver Produto");
    connect(actionSetLocalProd,SIGNAL(triggered(bool)),this,SLOT(setLocalProd()));
    connect(actionVerProduto, SIGNAL(triggered(bool)),this,SLOT(verProd()));

    actionMenuDeletarProd->setText("Deletar Produto");
    actionMenuDeletarProd->setIcon(iconDelete);
    connect(actionMenuDeletarProd,SIGNAL(triggered(bool)),this,SLOT(on_Btn_Delete_clicked()));


    actionMenuAlterarProd->setText("Alterar Produto");
    actionMenuAlterarProd->setIcon(iconAlterarProduto);
    connect(actionMenuAlterarProd,SIGNAL(triggered(bool)),this,SLOT(on_Btn_Alterar_clicked()));

    actionMenuPrintBarCode1 = new QAction(this);
    actionMenuPrintBarCode1->setText("1 Etiqueta");
    connect(actionMenuPrintBarCode1,SIGNAL(triggered(bool)),this, SLOT(imprimirEtiqueta1()));

    actionMenuPrintBarCode3 = new QAction(this);
    actionMenuPrintBarCode3->setText("3 Etiquetas");
    connect(actionMenuPrintBarCode3,SIGNAL(triggered(bool)),this, SLOT(imprimirEtiqueta3()));

    actionMenuPrintNomePreco1 = new QAction(this);
    actionMenuPrintNomePreco1->setText("1 Etiqueta");
    connect(actionMenuPrintNomePreco1,SIGNAL(triggered(bool)),this, SLOT(imprimirNomePreco1()));

    actionMenuPrintNomePreco3 = new QAction(this);
    actionMenuPrintNomePreco3->setText("3 Etiquetas");
    connect(actionMenuPrintNomePreco3,SIGNAL(triggered(bool)),this, SLOT(imprimirNomePreco3()));
    // -- delegates --
    DelegatePrecoF2 *delegatePreco = new DelegatePrecoF2(this);
    ui->Tview_Produtos->setItemDelegateForColumn(3,delegatePreco);
    CustomDelegate *delegateVermelho = new CustomDelegate(this);
    ui->Tview_Produtos->setItemDelegateForColumn(1,delegateVermelho);

    setarIconesJanela();

    connect(ui->Tview_Produtos, &QTableView::doubleClicked,
            this, &MainWindow::verProd);

    montarMenuCaixa();
    atualizarIndicadorCaixa();
    montarMenuInicial();
    atualizarLogoCabecalho();
    atualizarIndicadorEmpresa();
    atualizarAlertaContas();
    connect(Empresa_service::instancia(), &Empresa_service::empresaMudou, this, &MainWindow::empresaMudou);

    Sessao_service *sessao = Sessao_service::instancia();
    connect(sessao, &Sessao_service::sessaoExpirada, this, &MainWindow::sessaoExpirada);
    // aviso não-modal no rodapé: não interrompe a venda em andamento
    connect(sessao, &Sessao_service::avisoTimeout, this, &MainWindow::mostrarAvisoTimeout);
    // sessão bloqueada (venda aberta + inatividade), sessão invalidada (operador desativado...)
    // e rodapé sempre em dia
    connect(sessao, &Sessao_service::sessaoBloqueada, this, &MainWindow::sessaoBloqueada);
    connect(sessao, &Sessao_service::sessaoInvalidada, this, &MainWindow::sessaoInvalidada);
    connect(sessao, &Sessao_service::sessaoMudou, this, &MainWindow::atualizarIndicadorSessao);
    connect(sessao, &Sessao_service::sessaoMudou, this, &MainWindow::atualizarIndicadorCaixa);
    // Cada tela de venda (PDV, nova venda na lista de Vendas) se registra em Sessao_service e
    // informa se tem itens no carrinho: é isso que trava o logout e bloqueia o timeout.
    // A sessão só existe depois que main.cpp rodar o login, então o timeout
    // é armado em aplicarSessao(), não aqui.

    // ManifestadorDFe *manifestdfe = new ManifestadorDFe();
    // manifestdfe->consultarSePossivel();

}

MainWindow::~MainWindow()
{
    // grava a saída antes de derrubar a conexão do banco
    if (Sessao_service::instancia()->ativa())
        Sessao_service::instancia()->encerrarNoFechamento();
    delete ui;
}

void MainWindow::iniciarMigration(){

    SchemaMigration_service *schema = new SchemaMigration_service(this, ultimaVersaoSchema);
    //conexões de ações para update de versao schema
    connect(schema, &SchemaMigration_service::dbVersao6, this,
            &MainWindow::atualizarConfigAcbr);
    connect(schema, &SchemaMigration_service::dbVersao9, this,
            &MainWindow::atualizarConfigAcbr);
    connect(schema, &SchemaMigration_service::dbVersao13, confServ,
            &Config_service::salvarMudancasMigration13);
    connect(schema, &SchemaMigration_service::dbVersao13, this,
            &MainWindow::atualizarConfigAcbr);

    // schema->update();
    auto result = schema->update();
    if(!result.ok){
        QMessageBox::warning(this, "Erro migração", "Erro ao migrar banco de dados");
    }

}

void MainWindow::mostrarProdutoPorCodigoBarras(const QString &codigo)
{
    model = produtoService->getProdutoPeloCodigo(codigo);
    ui->Tview_Produtos->setModel(model);

}

void MainWindow::setarIconesJanela(){
    iconAlterarProduto.addFile(":/QEstoqueLOja/light-icons/story-editor.svg");
    iconAddProduto.addFile(":/QEstoqueLOja/light-icons/amarok_cart_add.svg");
    iconBtnVenda.addFile(":/QEstoqueLOja/light-icons/amarok_cart_view.svg");
    iconDelete.addFile(":/QEstoqueLOja/light-icons/amarok_cart_remove.svg");
    iconPesquisa.addFile(":/QEstoqueLOja/edit-find.svg");
    iconBtnRelatorios.addFile(":/QEstoqueLOja/light-icons/view-financial-account-investment-security.svg");
    iconImpressora.addFile(":/QEstoqueLOja/document-print.svg");
    iconClientes.addFile(":/QEstoqueLOja/light-icons/user-others.svg");
    QIcon orcamentoIcon;
    orcamentoIcon.addFile(":/QEstoqueLOja/light-icons/view-task.svg");
    QIcon comprasIcon;
    comprasIcon.addFile(":/QEstoqueLOja/light-icons/view-financial-list.svg");

    ui->Btn_AddProd->setIcon(iconAddProduto);
    ui->Btn_Venda->setIcon(iconBtnVenda);
    ui->Btn_PDV->setIcon(iconAddProduto);
    ui->Btn_Alterar->setIcon(iconAlterarProduto);
    ui->Btn_Delete->setIcon(iconDelete);
    ui->Btn_Pesquisa->setIcon(iconPesquisa);
    ui->Btn_Relatorios->setIcon(iconBtnRelatorios);
    ui->Btn_Clientes->setIcon(iconClientes);
    ui->Btn_Orcamento->setIcon(orcamentoIcon);
    ui->Btn_Entradas->setIcon(comprasIcon);
}

void MainWindow::atualizarTableview()
{
    produtoService->listarProdutos(model);
}


void MainWindow::on_Btn_Delete_clicked()
{
    if (!naListaDeProdutos()) {
        irParaProdutos();       // precisa da lista para saber qual produto
        return;
    }
    if(ui->Tview_Produtos->selectionModel()->isSelected(ui->Tview_Produtos->currentIndex())){
    // obter id selecionado
    QItemSelectionModel *selectionModel = ui->Tview_Produtos->selectionModel();
    QModelIndex selectedIndex = selectionModel->selectedIndexes().first();
    QVariant idVariant = ui->Tview_Produtos->model()->data(ui->Tview_Produtos->model()->index(selectedIndex.row(), 0));
    QVariant descVariant = ui->Tview_Produtos->model()->data(ui->Tview_Produtos->model()->index(selectedIndex.row(), 2));
    QString productId = idVariant.toString();
    QString productDesc = descVariant.toString();

    // produto que já saiu em venda não se apaga: mostra em quais vendas ele está
    const QList<ProdutoVendaRefDTO> vendasDoProduto = produtoService->vendasDoProduto(productId);
    if(!vendasDoProduto.isEmpty()){
        ProdutoEmVendasDialog dlg(productDesc, vendasDoProduto, [this]() { ui->Btn_Venda->click(); }, this);
        dlg.exec();
        return;
    }

    // Cria uma mensagem de confirmação
    QMessageBox::StandardButton resposta;
    resposta = QMessageBox::question(
        nullptr,
        "Confirmação",
        "Tem certeza que deseja excluir o produto:\n\n"
        "id: " + productId + "\n"
        "Descrição: " + productDesc,
        QMessageBox::Yes | QMessageBox::No
    );
    // Verifica a resposta do usuário
    if (resposta == QMessageBox::Yes) {
        auto resultado = produtoService->deletar(productId);
        if(!resultado.ok){
            QMessageBox::warning(this, "Erro",
                                 resultado.msg.isEmpty() ? QStringLiteral("Ocorreu um erro ao deletar o produto")
                                                         : "Não foi possível apagar o produto:\n" + resultado.msg);
            return;
        }
        atualizarTableview();
    }
    else {
        // O usuário escolheu não deletar o produto
        qDebug() << "A exclusão do produto foi cancelada.";
    }
    }else{
        QMessageBox::warning(this,"Erro","Selecione um produto antes de tentar deletar!");
    }
}

void MainWindow::on_Btn_Pesquisa_clicked()
{
    QString inputText = ui->Ledit_Pesquisa->text();

    produtoService->pesquisar(inputText, model);

    if (!model) {
        QMessageBox::warning(this, "Erro", "Erro ao realizar a pesquisa.");
        return;
    }

}

void MainWindow::on_Btn_Alterar_clicked()
{
    if (!naListaDeProdutos()) {
        irParaProdutos();
        return;
    }


    if(ui->Tview_Produtos->selectionModel()->isSelected(ui->Tview_Produtos->currentIndex())){
    QItemSelectionModel *selectionModel = ui->Tview_Produtos->selectionModel();
    QModelIndex selectedIndex = selectionModel->selectedIndexes().first();
    auto cell = [&](int col) {
        return ui->Tview_Produtos->model()->data(
            ui->Tview_Produtos->model()->index(selectedIndex.row(), col));
    };

    ProdutoDTO produto;
    produto.id           = cell(0).toLongLong();
    produto.quantidade   = cell(1).toDouble();
    produto.descricao    = cell(2).toString();
    produto.preco        = cell(3).toDouble();
    produto.codigoBarras = cell(4).toString();
    produto.nf           = cell(5).toBool();
    produto.uCom         = cell(6).toString();
    produto.precoFornecedor = cell(7).toString().trimmed().isEmpty() ? 0.0 : cell(7).toDouble();
    produto.percentLucro = cell(8).toDouble();
    produto.ncm          = cell(9).toString();
    produto.cest         = cell(10).toString();
    produto.aliquotaIcms = cell(11).toDouble();
    produto.csosn        = cell(12).toString();
    produto.pis          = cell(13).toString();

    // criar janela
    AlterarProduto *alterar = new AlterarProduto;
    alterar->janelaPrincipal = this;
    alterar->idAlt = QString::number(produto.id);
    alterar->TrazerInfo(produto);
    // alterar->setWindowModality(Qt::ApplicationModal);
    alterar->show();
    connect(alterar, &AlterarProduto::produtoAlterado, this,
            &MainWindow::on_Btn_Pesquisa_clicked);
    }else{
        QMessageBox::warning(this,"Erro","Selecione um produto antes de alterar!");
    }

}


void MainWindow::on_Btn_Venda_clicked()
{
    Vendas *vendas = new Vendas;
    //vendas->setWindowModality(Qt::ApplicationModal);
    connect(vendas, &Vendas::vendaConcluidaVendas, this, &MainWindow::atualizarTableview);

    vendas->show();
}

void MainWindow::on_Btn_Relatorios_clicked()
    {
     if (!exigirGerente(QStringLiteral("Os relatórios")))
         return;
     relatorios *relatorios1 = new relatorios;
    relatorios1->setWindowModality(Qt::ApplicationModal);
    relatorios1->show();
}

void MainWindow::on_Btn_Orcamento_clicked()
{
    JanelaOrcamento *janelaOrcamento = new JanelaOrcamento;
    janelaOrcamento->setWindowModality(Qt::ApplicationModal);
    janelaOrcamento->show();
}

void MainWindow::imprimirEtiqueta1(){
    QItemSelectionModel *selectionModel = ui->Tview_Produtos->selectionModel();
    QModelIndex selectedIndex = selectionModel->selectedIndexes().first();
    QVariant barrasVariant = ui->Tview_Produtos->model()->data(ui->Tview_Produtos->model()->index(selectedIndex.row(), 4));
    QVariant descVariant = ui->Tview_Produtos->model()->data(ui->Tview_Produtos->model()->index(selectedIndex.row(), 2));
    QVariant precoVariant = ui->Tview_Produtos->model()->data(ui->Tview_Produtos->model()->index(selectedIndex.row(), 3));

    QString erro;

    QImage barcode = Barcode_service::gerarCodigoBarras(barrasVariant.toString(), &erro);
    if (barcode.isNull()) {
        QMessageBox::warning(this, "Erro", erro);
        return;
    }


    EscPosPrinter_service printer;
    auto r1 = printer.imprimirEtiquetas(configDTO.impressoraNomeDispositivo, 1, barcode, descVariant.toString(),precoVariant.toDouble());
    if(!r1.ok){
        QMessageBox::warning(this, "Erro", r1.msg);
        return;
    }
}

void MainWindow::imprimirEtiqueta3(){
    QItemSelectionModel *selectionModel = ui->Tview_Produtos->selectionModel();
    QModelIndex selectedIndex = selectionModel->selectedIndexes().first();
    QVariant barrasVariant = ui->Tview_Produtos->model()->data(ui->Tview_Produtos->model()->index(selectedIndex.row(), 4));
    QVariant descVariant = ui->Tview_Produtos->model()->data(ui->Tview_Produtos->model()->index(selectedIndex.row(), 2));
    QVariant precoVariant = ui->Tview_Produtos->model()->data(ui->Tview_Produtos->model()->index(selectedIndex.row(), 3));

    QString erro;

    QImage barcode = Barcode_service::gerarCodigoBarras(barrasVariant.toString(), &erro);
    if (barcode.isNull()) {
        QMessageBox::warning(this, "Erro", erro);
        return;
    }


    EscPosPrinter_service printer;
    auto r1 = printer.imprimirEtiquetas(configDTO.impressoraNomeDispositivo, 3, barcode, descVariant.toString(),precoVariant.toDouble());
    if(!r1.ok){
        QMessageBox::warning(this, "Erro", r1.msg);
        return;
    }
}

void MainWindow::imprimirNomePreco1(){
    imprimirNomePreco(1);
}

void MainWindow::imprimirNomePreco3(){
    imprimirNomePreco(3);
}

void MainWindow::imprimirNomePreco(int quantidade){
    QItemSelectionModel *selectionModel = ui->Tview_Produtos->selectionModel();
    QModelIndex selectedIndex = selectionModel->selectedIndexes().first();
    QVariant descVariant = ui->Tview_Produtos->model()->data(ui->Tview_Produtos->model()->index(selectedIndex.row(), 2));
    QVariant precoVariant = ui->Tview_Produtos->model()->data(ui->Tview_Produtos->model()->index(selectedIndex.row(), 3));

    EscPosPrinter_service printer;
    auto r1 = printer.imprimirNomePreco(configDTO.impressoraNomeDispositivo, quantidade,
                                        descVariant.toString(), precoVariant.toDouble());
    if(!r1.ok){
        QMessageBox::warning(this, "Erro", r1.msg);
        return;
    }
}


void MainWindow::on_actionRealizar_Venda_triggered()
{
    abrirPdv();
}

void MainWindow::on_Btn_PDV_clicked()
{
    abrirPdv();
}

bool MainWindow::naListaDeProdutos() const
{
    return ui->pilhaPrincipal->currentWidget() == ui->paginaProdutos;
}

void MainWindow::irParaInicio()
{
    ui->pilhaPrincipal->setCurrentWidget(ui->paginaMenu);
    atualizarAlertaContas();
}

void MainWindow::irParaProdutos()
{
    ui->pilhaPrincipal->setCurrentWidget(ui->paginaProdutos);
    ui->Ledit_Pesquisa->setFocus();
}

void MainWindow::atualizarLogoCabecalho()
{
    // a logo cadastrada em Configurações > Empresa; sem ela (ou sem o arquivo) fica a do programa
    const QString arquivo = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
                            "/imagens/" + QFileInfo(configDTO.logoPathEmpresa).fileName();
    QPixmap logo;
    if (!configDTO.logoPathEmpresa.trimmed().isEmpty() && QFile::exists(arquivo))
        logo.load(arquivo);
    // a logo do programa é clara (feita para o fundo escuro); a da empresa vai sobre um fundo branco
    const bool logoDaEmpresa = !logo.isNull();
    if (logo.isNull())
        logo.load(QStringLiteral(":/QEstoqueLOja/logo-completo-claro.png"));
    if (logo.isNull())
        return;

    ui->label_6->setStyleSheet(logoDaEmpresa
        ? QStringLiteral("background: white; border-radius: 6px; border: 2px solid rgba(43, 132, 191, 180);")
        : QStringLiteral("border-right: 2px solid rgba(43, 132, 191, 180);"));
    ui->label_6->setScaledContents(false);
    ui->label_6->setAlignment(Qt::AlignCenter);
    ui->label_6->setMargin(3);
    ui->label_6->setCursor(Qt::PointingHandCursor);
    ui->label_6->setToolTip(QStringLiteral("Voltar ao menu inicial"));
    ui->label_6->installEventFilter(this);
    ui->label_6->setPixmap(logo.scaled(ui->label_6->maximumSize() - QSize(8, 8), Qt::KeepAspectRatio,
                                       Qt::SmoothTransformation));
}

void MainWindow::atualizarSaudacao()
{
    if (!menuInicial)
        return;
    const QString nome = Sessao_service::instancia()->nomeOperador();
    const QString empresa = configDTO.nomeFantasiaEmpresa.trimmed().isEmpty()
                                ? configDTO.nomeEmpresa.trimmed() : configDTO.nomeFantasiaEmpresa.trimmed();
    const QString data = QLocale(QLocale::Portuguese, QLocale::Brazil)
                             .toString(QDate::currentDate(), QStringLiteral("dddd, d 'de' MMMM 'de' yyyy"));
    menuInicial->definirSaudacao(nome.isEmpty() ? QStringLiteral("Bem-vindo")
                                                : QStringLiteral("Olá, %1").arg(nome),
                                 empresa.isEmpty() ? data : QStringLiteral("%1  ·  %2").arg(empresa, data));
}

void MainWindow::montarMenuInicial()
{
    menuInicial = new MenuInicial(ui->paginaMenu);
    auto *layout = new QVBoxLayout(ui->paginaMenu);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(menuInicial);

    auto *m = menuInicial;
    m->adicionarGrupo("Atendimento");
    m->adicionarBotao("shopping-cart", "Vender (PDV)", "Abre a tela de venda (F5)", [this]() { abrirPdv(); }, true);
    m->adicionarBotao("receipt", "Vendas", "Consultar e gerenciar vendas", [this]() { ui->Btn_Venda->click(); });
    m->adicionarBotao("file-text", "Orçamentos", "Criar e consultar orçamentos", [this]() { ui->Btn_Orcamento->click(); });

    m->adicionarGrupo("Cadastros");
    m->adicionarBotao("package", "Produtos", "Lista de produtos e estoque", [this]() { irParaProdutos(); });
    m->adicionarBotao("package-plus", "Cadastrar produto", "Adicionar um novo produto", [this]() { ui->Btn_AddProd->click(); });
    m->adicionarBotao("users", "Clientes", "Cadastro de clientes", [this]() { ui->Btn_Clientes->click(); });
    m->adicionarBotao("truck", "Compras", "Entrada de mercadorias", [this]() { ui->Btn_Entradas->click(); });

    m->adicionarGrupo("Caixa");
    m->adicionarBotao("wallet", "Abrir caixa", "Abrir o seu caixa", [this]() { abrirCaixaClicked(); });
    m->adicionarBotao("lock", "Fechar caixa", "Conferir e fechar o seu caixa", [this]() { fecharCaixaClicked(); });
    m->adicionarBotao("circle-arrow-down", "Sangria", "Retirar dinheiro do caixa", [this]() { sangriaClicked(); });
    m->adicionarBotao("circle-arrow-up", "Suprimento", "Colocar dinheiro no caixa", [this]() { suprimentoClicked(); });
    m->adicionarBotao("history", "Histórico de caixas", "Caixas abertos e fechados", [this]() { historicoCaixaClicked(); });

    m->adicionarGrupo("Financeiro");
    m->adicionarBotao("banknote", "Contas a pagar", "Contas, vencimentos e baixas", [this]() { contasPagarClicked(); });
    m->adicionarBotao("plus", "Nova conta a pagar", "Lançar uma conta ou compra parcelada",
                      [this]() { contasPagarClicked(QStringLiteral("NOVA")); });
    m->adicionarBotao("wallet", "Contas a receber", "Fiado e caderneta: o que os clientes devem",
                      [this]() { contasReceberClicked(); });
    m->adicionarBotao("building-2", "Trocar empresa", "Escolher o CNPJ das vendas e notas", [this]() { escolherEmpresaClicked(); });

    m->adicionarGrupo("Gestão e fiscal");
    m->adicionarBotao("chart-column", "Relatórios", "Relatórios gerenciais", [this]() { ui->Btn_Relatorios->click(); });
    m->adicionarBotao("user-cog", "Operadores", "Cadastro de operadores de caixa", [this]() { operadoresClicked(); });
    m->adicionarBotao("shield-check", "Log de acesso", "Quem entrou e o que fez", [this]() { logAcessoClicked(); });
    m->adicionarBotao("calculator", "Monitor fiscal", "Notas fiscais e contingência", [this]() { ui->actionMonitor_Fiscal->trigger(); });
    m->adicionarBotao("mail", "Enviar ao contador", "Enviar as notas ao contador", [this]() { ui->actionEnviar_Notas_Contador->trigger(); });

    m->adicionarGrupo("Sistema");
    m->adicionarBotao("repeat", "Nuvem (painel)", "Sincronização com o painel de gestão online", [this]() { nuvemClicked(); });
    m->adicionarBotao("settings", "Configurações", "Empresa, fiscal, e-mail e outros", [this]() { ui->actionConfig->trigger(); });
    m->adicionarBotao("book-open", "Ajuda", "Documentação do sistema", [this]() { ui->actionDocumenta_o->trigger(); });
    m->adicionarBotao("log-out", "Sair da conta", "Encerrar a sessão do operador", [this]() { sairSessaoClicked(); });

    // na lista de produtos: botão para voltar ao menu
    auto *btnInicio = new QPushButton(QStringLiteral("  Início"), ui->paginaProdutos);
    btnInicio->setIcon(Icones::icone("arrow-left", QColor("#1E3A5F"), 18));
    btnInicio->setCursor(Qt::PointingHandCursor);
    btnInicio->setToolTip("Voltar ao menu inicial");
    btnInicio->setStyleSheet("QPushButton { background: white; color: #1E3A5F; border: 1px solid #C9D3DF;"
                             " border-radius: 8px; padding: 6px 14px; font-weight: 700; }"
                             "QPushButton:hover { background: #EAF3FB; border-color: #2B84BF; }");
    connect(btnInicio, &QPushButton::clicked, this, &MainWindow::irParaInicio);
    ui->horizontalLayout_6->insertWidget(0, btnInicio);
    ui->horizontalLayout_6->insertSpacing(1, 8);

    connect(Sessao_service::instancia(), &Sessao_service::sessaoMudou, this, &MainWindow::atualizarSaudacao);
    atualizarSaudacao();
    irParaInicio();
}

namespace { QString estiloChip(const QString &cor); }

void MainWindow::atualizarIndicadorEmpresa()
{
    if (!btnEmpresa)
        return;
    const EmpresaDTO e = Empresa_service::instancia()->ativa();
    const QString nome = e.valida() ? e.apelido : QStringLiteral("Empresa");
    btnEmpresa->setIcon(Icones::icone("building-2", QColor("#0F766E"), 20));
    btnEmpresa->setText(QStringLiteral(" %1").arg(nome));
    btnEmpresa->setStyleSheet(estiloChip("rgb(15, 118, 110)"));
    btnEmpresa->setToolTip(QStringLiteral("Empresa em uso: %1\nCNPJ %2\nClique para trocar — as vendas e notas "
                                          "vão para o CNPJ escolhido.")
                               .arg(nome, e.cnpj.isEmpty() ? QStringLiteral("não informado") : e.cnpjFormatado()));
}

void MainWindow::escolherEmpresaClicked()
{
    EmpresaDialog dlg([this](const QString &acao) { return exigirGerente(acao); }, QString(), this);
    dlg.exec();
}

void MainWindow::perguntarEmpresaSeNecessario()
{
    if (empresaPerguntada)
        return;
    empresaPerguntada = true;
    if (Empresa_service::instancia()->listar(true).size() < 2)
        return;
    EmpresaDialog dlg([this](const QString &acao) { return exigirGerente(acao); },
                      QStringLiteral("Em qual empresa vai trabalhar hoje?"), this);
    dlg.exec();
}

void MainWindow::empresaMudou()
{
    // cada empresa tem seus dados, certificado e numeração: recarrega tudo que depende da configuração
    configDTO = confServ->carregarTudo();
    atualizarConfigAcbr();
    atualizarLogoCabecalho();
    atualizarSaudacao();
    atualizarIndicadorEmpresa();
    atualizarAlertaContas();
}

void MainWindow::contasPagarClicked(const QString &statusInicial)
{
    const bool nova = statusInicial == QLatin1String("NOVA");
    ContasPagarJanela janela([this](const QString &acao) { return exigirGerente(acao); },
                             nova ? QString() : statusInicial, this);
    if (nova)
        QTimer::singleShot(0, &janela, [&janela]() { janela.novaConta(); });
    janela.exec();
    atualizarAlertaContas();
}

void MainWindow::nuvemClicked()
{
    if (!exigirGerente(QStringLiteral("A configuração da nuvem")))
        return;
    NuvemDialog dlg(this);
    dlg.exec();
    atualizarIndicadorNuvem();
}

void MainWindow::atualizarIndicadorNuvem()
{
    if (!btnNuvem)
        return;
    const auto s = Nuvem_service::instancia()->status();
    if (!s.ativa) {                                   // desligada: nada no rodapé
        btnNuvem->setVisible(false);
        return;
    }
    btnNuvem->setVisible(true);
    QString texto;
    QString cor;
    if (s.emExecucao) {
        texto = QStringLiteral(" Nuvem: sincronizando...");
        cor = "rgb(30, 64, 175)";
    } else if (!s.ultimoErro.isEmpty()) {
        texto = QStringLiteral(" Nuvem: com erro");
        cor = "rgb(185, 28, 28)";
    } else {
        const QDateTime ok = QDateTime::fromString(s.ultimoCicloOk, "yyyy-MM-dd HH:mm:ss");
        texto = ok.isValid() ? QStringLiteral(" Nuvem: %1").arg(ok.toString("HH:mm")) : QStringLiteral(" Nuvem: aguardando");
        cor = "rgb(15, 118, 110)";
    }
    btnNuvem->setIcon(Icones::icone("repeat", QColor(cor.contains("185") ? "#B91C1C" : "#0F766E"), 20));
    btnNuvem->setText(texto);
    btnNuvem->setStyleSheet(estiloChip(cor));
    btnNuvem->setToolTip(s.ultimoErro.isEmpty() ? QStringLiteral("Sincronização com o painel online. Último ciclo: %1").arg(s.ultimoCicloOk.isEmpty() ? "ainda não" : s.ultimoCicloOk)
                                                : s.ultimoErro);
}

void MainWindow::contasReceberClicked(qlonglong idCliente)
{
    ContasReceberJanela janela([this](const QString &acao) { return exigirGerente(acao); }, idCliente, this);
    janela.exec();
}

void MainWindow::atualizarAlertaContas()
{
    if (!menuInicial)
        return;
    ContasPagar_service servico;
    const ResumoContasPagarDTO r = servico.resumo(Empresa_service::instancia()->idAtiva());
    QLocale ptBR(QLocale::Portuguese, QLocale::Brazil);
    QStringList partes;
    if (r.qtdVencidas > 0)
        partes << QStringLiteral("%1 conta(s) vencida(s) (%2)").arg(r.qtdVencidas).arg(ptBR.toCurrencyString(r.valorVencidas, "R$ "));
    if (r.qtdHoje > 0)
        partes << QStringLiteral("%1 vence(m) hoje (%2)").arg(r.qtdHoje).arg(ptBR.toCurrencyString(r.valorHoje, "R$ "));
    if (partes.isEmpty()) {
        menuInicial->definirAlerta(QString());
        return;
    }
    menuInicial->definirAlerta(QStringLiteral("Contas a pagar: %1 — clique para ver").arg(partes.join("  ·  ")),
                               [this, vencidas = r.qtdVencidas]() {
                                   contasPagarClicked(vencidas > 0 ? QStringLiteral("VENCIDA") : QString(kContaAberta));
                               },
                               r.qtdVencidas > 0);
}

void MainWindow::montarMenuCaixa()
{
    QMenu *menuCaixa = new QMenu("Caixa", this);
    menuCaixa->addAction("Abrir caixa...", this, &MainWindow::abrirCaixaClicked);
    menuCaixa->addAction("Fechar caixa...", this, &MainWindow::fecharCaixaClicked);
    menuCaixa->addSeparator();
    menuCaixa->addAction("Sangria...", this, &MainWindow::sangriaClicked);
    menuCaixa->addAction("Suprimento...", this, &MainWindow::suprimentoClicked);
    menuCaixa->addSeparator();
    menuCaixa->addAction("Histórico...", this, &MainWindow::historicoCaixaClicked);
    menuCaixa->addAction("Operadores...", this, &MainWindow::operadoresClicked);
    // alterarPinGerente já confirma com o PIN atual antes de trocar
    menuCaixa->addAction("PIN do gerente...", this, [this]() { Operadores::alterarPinGerente(this); });
    ui->menuBar->insertMenu(ui->menuAjuda->menuAction(), menuCaixa);

    QMenu *menuFinanceiro = new QMenu("Financeiro", this);
    menuFinanceiro->addAction("Contas a pagar...", this, [this]() { contasPagarClicked(); });
    menuFinanceiro->addAction("Nova conta a pagar...", this, [this]() { contasPagarClicked(QStringLiteral("NOVA")); });
    menuFinanceiro->addSeparator();
    menuFinanceiro->addAction("Contas a receber (fiado)...", this, [this]() { contasReceberClicked(); });
    menuFinanceiro->addSeparator();
    menuFinanceiro->addAction("Trocar empresa...", this, &MainWindow::escolherEmpresaClicked);
    ui->menuBar->insertMenu(ui->menuAjuda->menuAction(), menuFinanceiro);

    // menu da sessão: troca de operador e encerramento do turno
    QMenu *menuSessao = new QMenu("Sessão", this);
    actionTrocarOperador = menuSessao->addAction("Trocar operador...", this, &MainWindow::trocarOperadorClicked);
    actionSairSessao = menuSessao->addAction("Sair da conta", this, &MainWindow::sairSessaoClicked);
    menuSessao->addSeparator();
    menuSessao->addAction("Log de acesso...", this, &MainWindow::logAcessoClicked);
    menuSessao->addAction("Nuvem (painel online)...", this, &MainWindow::nuvemClicked);
    ui->menuBar->insertMenu(ui->menuAjuda->menuAction(), menuSessao);

    // Rodapé: quem está logado (chip grande, com menu) + botão de sair sempre à vista.
    ui->statusbar->setMinimumHeight(44);
    QFont fonteRodape = font();
    fonteRodape.setPointSize(qMax(font().pointSize(), 9) + 2);
    fonteRodape.setBold(true);

    btnNuvem = new QToolButton(this);
    btnNuvem->setFont(fonteRodape);
    btnNuvem->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    btnNuvem->setIconSize(QSize(20, 20));
    btnNuvem->setCursor(Qt::PointingHandCursor);
    btnNuvem->setVisible(false);
    connect(btnNuvem, &QToolButton::clicked, this, &MainWindow::nuvemClicked);
    connect(Nuvem_service::instancia(), &Nuvem_service::statusMudou, this, &MainWindow::atualizarIndicadorNuvem);
    ui->statusbar->addPermanentWidget(btnNuvem);

    btnEmpresa = new QToolButton(this);
    btnEmpresa->setFont(fonteRodape);
    btnEmpresa->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    btnEmpresa->setIconSize(QSize(20, 20));
    btnEmpresa->setCursor(Qt::PointingHandCursor);
    btnEmpresa->setToolTip("Empresa (CNPJ) das vendas e notas — clique para trocar");
    connect(btnEmpresa, &QToolButton::clicked, this, &MainWindow::escolherEmpresaClicked);
    ui->statusbar->addPermanentWidget(btnEmpresa);

    btnOperador = new QToolButton(this);
    btnOperador->setFont(fonteRodape);
    btnOperador->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    btnOperador->setIconSize(QSize(20, 20));
    btnOperador->setPopupMode(QToolButton::InstantPopup);
    btnOperador->setCursor(Qt::PointingHandCursor);
    btnOperador->setToolTip("Operador logado — clique para trocar de operador ou sair da conta");
    QMenu *menuOperador = new QMenu(btnOperador);
    menuOperador->addAction(Icones::icone("repeat", QColor("#1E3A5F"), 18), QStringLiteral("Trocar operador..."),
                            this, &MainWindow::trocarOperadorClicked);
    menuOperador->addAction(Icones::icone("log-out", QColor("#B91C1C"), 18), QStringLiteral("Sair da conta"),
                            this, &MainWindow::sairSessaoClicked);
    btnOperador->setMenu(menuOperador);
    ui->statusbar->addPermanentWidget(btnOperador);

    btnSair = new QToolButton(this);
    btnSair->setFont(fonteRodape);
    btnSair->setText(QStringLiteral(" Sair da conta"));
    btnSair->setIcon(Icones::icone("log-out", QColor(Qt::white), 20));
    btnSair->setIconSize(QSize(20, 20));
    btnSair->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    btnSair->setCursor(Qt::PointingHandCursor);
    btnSair->setToolTip("Encerrar a sessão do operador atual");
    btnSair->setStyleSheet(
        "QToolButton { background: rgb(185, 28, 28); color: white; border: 1px solid rgb(127, 29, 29);"
        " border-radius: 6px; padding: 5px 14px; }"
        "QToolButton:hover { background: rgb(220, 38, 38); }"
        "QToolButton:pressed { background: rgb(127, 29, 29); }");
    connect(btnSair, &QToolButton::clicked, this, &MainWindow::sairSessaoClicked);
    ui->statusbar->addPermanentWidget(btnSair);
    atualizarIndicadorSessao();

    lblAvisoTimeout = new QLabel(this);
    lblAvisoTimeout->setStyleSheet("color: rgb(180, 83, 9); font-weight: 600;");
    lblAvisoTimeout->hide();
    ui->statusbar->addWidget(lblAvisoTimeout);

    lblCaixaStatus = new QLabel(this);
    lblCaixaStatus->setFont(fonteRodape);
    ui->statusbar->addPermanentWidget(lblCaixaStatus);

    // o caixa pode ser aberto/fechado por outra tela ou por outro computador: mantém o rodapé em dia
    QTimer *timerCaixa = new QTimer(this);
    connect(timerCaixa, &QTimer::timeout, this, &MainWindow::atualizarIndicadorCaixa);
    timerCaixa->start(15000);
}

namespace {
// chip branco com borda colorida: legível sobre qualquer cor do tema
QString estiloChip(const QString &cor)
{
    return QStringLiteral("QToolButton { background: white; color: %1; border: 2px solid %1;"
                          " border-radius: 8px; padding: 4px 14px; }"
                          "QToolButton::menu-indicator { image: none; }"
                          "QToolButton:hover { background: rgb(243, 244, 246); }").arg(cor);
}
}

void MainWindow::atualizarIndicadorSessao()
{
    if (!btnOperador)
        return;
    const SessaoDTO sessao = Sessao_service::instancia()->sessao();
    const bool logado = !sessao.nomeOperador.isEmpty();
    if (btnSair)
        btnSair->setVisible(logado);
    if (!logado) {
        btnOperador->setIcon(Icones::icone("user", QColor("#B91C1C"), 20));
        btnOperador->setText(QStringLiteral(" Sem operador"));
        btnOperador->setStyleSheet(estiloChip("rgb(185, 28, 28)"));
        return;
    }
    if (Sessao_service::instancia()->bloqueada()) {
        btnOperador->setIcon(Icones::icone("lock", QColor("#B45309"), 20));
        btnOperador->setText(QStringLiteral(" %1 — sessão bloqueada").arg(sessao.nomeOperador));
        btnOperador->setStyleSheet(estiloChip("rgb(180, 83, 9)"));
        return;
    }
    btnOperador->setIcon(Icones::icone("user", QColor("#1E40AF"), 20));
    btnOperador->setText(QStringLiteral(" %1%2  ▾")
                             .arg(sessao.nomeOperador,
                                  sessao.gerente ? QStringLiteral("  ·  Gerente") : QString()));
    btnOperador->setStyleSheet(estiloChip("rgb(30, 64, 175)"));
}

void MainWindow::aplicarSessao()
{
    atualizarIndicadorSessao();
    Sessao_service::instancia()->iniciarTimeout(configDTO.caixaTimeoutAtivo, configDTO.caixaTimeoutMinutos);
}

void MainWindow::setModoDesenvolvimento(bool ativo)
{
    modoDesenvolvimento = ativo;
    if (!ativo)
        return;
    QLabel *tarja = new QLabel(
        QStringLiteral(" MODO DESENVOLVIMENTO — login sem PIN "),
        this);
    tarja->setObjectName(QStringLiteral("Lbl_ModoDesenvolvimento"));
    tarja->setAlignment(Qt::AlignCenter);
    tarja->setToolTip(QStringLiteral("Login automático ativo, sem validação de PIN. Nunca use em produção."));
    tarja->setStyleSheet(
        "background: rgb(180, 83, 9); color: white; font-weight: 700; padding: 4px;");
    // fica no rodapé: o layout da tela principal é em grade e a tarja não deve empurrar nada
    ui->statusbar->insertWidget(0, tarja);
}

bool MainWindow::exigirGerente(const QString &acao)
{
    Sessao_service *sessao = Sessao_service::instancia();
    // a permissão pode ter mudado desde o login (rebaixado, desativado, PIN trocado)
    sessao->revalidar();
    if (!sessao->ativa())
        return false;

    // gerente com identidade no cadastro entra direto, sem digitar nada
    if (sessao->sessao().gerente)
        return true;

    Operador_service serv;
    if (!serv.gerentePinDefinido()) {
        // com operadores já cadastrados, definir o PIN do gerente aqui daria a qualquer
        // operador o controle da loja. O PIN só nasce junto com o primeiro cadastro.
        if (!serv.listar(false).isEmpty()) {
            QMessageBox::warning(this, "Acesso restrito",
                acao + QStringLiteral(" é restrito a gerentes.\n"
                                      "O PIN do gerente ainda não foi definido nesta instalação.\n"
                                      "Peça a um gerente para definí-lo."));
            return false;
        }
        QMessageBox::information(this, "Acesso restrito",
            acao + QStringLiteral(" é restrito a gerentes.\n"
                                  "Defina o PIN do gerente para liberar o acesso."));
        return Operadores::autenticarGerente(this);
    }

    bool ok = false;
    const QString pin = QInputDialog::getText(this, "PIN do gerente",
        acao + QStringLiteral(" é restrito a gerentes.\nInforme o PIN do gerente:"),
        QLineEdit::Password, QString(), &ok);
    if (!ok)
        return false;
    const auto r = serv.validarGerentePin(pin);
    if (!r.ok) {
        QMessageBox::warning(this, "Acesso restrito", r.msg);
        return false;
    }
    // fica registrado quem usou o PIN do gerente, quando e para quê
    sessao->concederElevacao(acao);
    return true;
}

void MainWindow::bloqueioParaTroca(QString *motivo) const
{
    // qualquer tela de venda com itens no carrinho (PDV, nova venda na lista de Vendas...)
    if (Sessao_service::instancia()->temVendaEmAndamento()) {
        *motivo = QStringLiteral("Há uma venda em andamento.\nFinalize ou cancele a venda antes de sair da conta ou trocar de operador.");
        return;
    }
    motivo->clear();
}

void MainWindow::encerrarSessao(bool sairDoPrograma)
{
    QString motivo;
    bloqueioParaTroca(&motivo);
    if (!motivo.isEmpty()) {
        QMessageBox::warning(this, "Sessão", motivo);
        return;
    }

    Sessao_service *sessao = Sessao_service::instancia();
    if (sairDoPrograma) {
        // sair da conta: a sessão acaba e o login volta (cancelar o login fecha o programa)
        sessao->encerrar(QStringLiteral("LOGOUT"));
        pedirLoginNovamente(true);
        return;
    }

    // trocar de operador: a sessão de quem está logado só acaba se o novo login for aceito.
    // Cancelar mantém tudo como estava (antes a sessão já tinha acabado e o programa ficava
    // aberto, sem ninguém identificado).
    bool cancelou = false;
    const SessaoDTO nova = LoginOperador::executar(&cancelou, this);
    if (cancelou || nova.nomeOperador.isEmpty())
        return;
    sessao->encerrar(QStringLiteral("TROCA_OPERADOR"));
    iniciarSessao(nova);
}

bool MainWindow::pedirLoginNovamente(bool fecharSeCancelar)
{
    bool cancelou = false;
    const SessaoDTO nova = LoginOperador::executar(&cancelou, this);
    if (cancelou || nova.nomeOperador.isEmpty()) {
        // sem sessão aceita o PDV não pode continuar: encerra o programa (e o PDV junto)
        if (fecharSeCancelar)
            encerrarPrograma();
        return false;
    }
    return iniciarSessao(nova);
}

bool MainWindow::iniciarSessao(const SessaoDTO &nova)
{
    Sessao_service *sessao = Sessao_service::instancia();
    QString erro;
    sessao->abrir(nova, &erro);
    if (!erro.isEmpty() || !sessao->ativa()) {
        QMessageBox::critical(this, "Sessão do operador", erro.isEmpty() ? QStringLiteral("Não foi possível abrir a sessão.") : erro);
        encerrarPrograma();
        return false;
    }
    sessao->iniciarTimeout(configDTO.caixaTimeoutAtivo, configDTO.caixaTimeoutMinutos);
    atualizarIndicadorSessao();
    return true;
}

void MainWindow::encerrarPrograma()
{
    // O PDV é uma janela independente: fechar só esta janela deixaria o PDV aberto e utilizável
    // sem ninguém identificado. O rascunho da venda em andamento já está salvo.
    saindoDoPrograma = true;
    QApplication::quit();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!saindoDoPrograma && Sessao_service::instancia()->temVendaEmAndamento()) {
        const auto resp = QMessageBox::question(this, "Sair",
            QStringLiteral("Há uma venda em andamento.\n"
                           "O rascunho fica salvo e será oferecido na próxima vez. Sair mesmo assim?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (resp != QMessageBox::Yes) {
            event->ignore();
            return;
        }
    }
    saindoDoPrograma = true;
    // para a sincronização com a nuvem e solta a trava (outro computador assume)
    Nuvem_service::instancia()->parar();
    event->accept();
    QApplication::quit();
}

void MainWindow::mostrarAvisoTimeout(int segundosRestantes)
{
    if (!lblAvisoTimeout)
        return;
    if (segundosRestantes <= 0) {
        lblAvisoTimeout->hide();
        return;
    }
    const int minutos = qMax(1, segundosRestantes / 60);
    lblAvisoTimeout->setText(QStringLiteral(" Sessão expira em %1 min — mova o mouse ou pressione uma tecla para renovar ")
                                 .arg(minutos));
    lblAvisoTimeout->show();
}

void MainWindow::trocarOperadorClicked()
{
    encerrarSessao(false);
}

void MainWindow::sairSessaoClicked()
{
    const auto resp = QMessageBox::question(this, "Sair da conta",
        QStringLiteral("Sair da conta de %1?\nO login será pedido de novo.%2")
            .arg(Sessao_service::instancia()->nomeOperador(),
                 Caixa_service().caixaAtual().aberto()
                     ? QStringLiteral("\n\nO seu caixa continua aberto e pode ser fechado depois.")
                     : QString()),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (resp != QMessageBox::Yes)
        return;
    encerrarSessao(true);
}

void MainWindow::sessaoExpirada()
{
    // O serviço já encerrou a sessão (TIMEOUT) antes de avisar: abrir o novo login aqui dentro é
    // seguro. Não vale o bloqueio de caixa aberto: quem ficou parado pode ter deixado o caixa
    // aberto, e recusar o login deixaria o turno sem ninguém para fechá-lo.
    atualizarIndicadorSessao();
    if (lblAvisoTimeout)
        lblAvisoTimeout->hide();

    QMessageBox::information(this, "Sessão encerrada",
        QStringLiteral("A sessão expirou por falta de movimento.\nEntre novamente para continuar."));

    pedirLoginNovamente(true);
}

void MainWindow::sessaoBloqueada()
{
    // Venda em andamento + inatividade: a sessão não é encerrada (a venda fica intacta), mas só
    // volta com o PIN. Os diálogos abaixo são modais para a aplicação inteira, então o PDV
    // não pode ser usado por trás.
    Sessao_service *sessao = Sessao_service::instancia();
    atualizarIndicadorSessao();
    const QString nome = sessao->nomeOperador();

    while (sessao->ativa() && sessao->bloqueada()) {
        bool ok = false;
        const QString pin = QInputDialog::getText(this, "Sessão bloqueada",
            QStringLiteral("A sessão de %1 foi bloqueada por inatividade, com uma venda em andamento.\n"
                           "Informe o PIN de %1 (ou o PIN do gerente) para continuar:").arg(nome),
            QLineEdit::Password, QString(), &ok);
        if (!ok) {
            const auto resp = QMessageBox::question(this, "Sessão bloqueada",
                QStringLiteral("Sair do programa?\nA venda em andamento fica salva como rascunho."),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (resp == QMessageBox::Yes) {
                encerrarPrograma();
                return;
            }
            continue;
        }
        const auto r = sessao->desbloquear(pin);
        if (!r.ok)
            QMessageBox::warning(this, "Sessão bloqueada", r.msg);
    }
    atualizarIndicadorSessao();
}

void MainWindow::sessaoInvalidada(const QString &motivo)
{
    // a sessão já foi encerrada pelo serviço (operador desativado/bloqueado, PIN geral trocado)
    atualizarIndicadorSessao();
    QMessageBox::warning(this, "Sessão encerrada",
        motivo + QStringLiteral("\nEntre novamente para continuar."));
    pedirLoginNovamente(true);
}

void MainWindow::logAcessoClicked()
{
    if (!exigirGerente(QStringLiteral("O log de acesso")))
        return;
    LogAcessoDialog dlg(this);
    dlg.exec();
}

void MainWindow::atualizarIndicadorCaixa()
{
    if (!lblCaixaStatus)
        return;
    Caixa_service caixaServ;
    const CaixaDTO caixa = caixaServ.caixaAtual();
    if (caixa.aberto()) {
        lblCaixaStatus->setTextFormat(Qt::RichText);
        lblCaixaStatus->setText(QString("  <span style='color:#16A34A'>&#9679;</span> Seu caixa: aberto · #%1  ").arg(caixa.id));
        lblCaixaStatus->setStyleSheet("background: white; color: rgb(21, 128, 61); border: 2px solid rgb(21, 128, 61);"
                                      " border-radius: 8px; padding: 4px 8px;");
    } else {
        lblCaixaStatus->setTextFormat(Qt::RichText);
        lblCaixaStatus->setText("  <span style='color:#DC2626'>&#9679;</span> Seu caixa: fechado  ");
        lblCaixaStatus->setStyleSheet("background: white; color: rgb(185, 28, 28); border: 2px solid rgb(185, 28, 28);"
                                      " border-radius: 8px; padding: 4px 8px;");
    }
}

bool MainWindow::garantirCaixaAberto()
{
    const bool ok = AberturaCaixa::garantirCaixaAberto(this);
    atualizarIndicadorCaixa();
    return ok;
}

void MainWindow::abrirCaixaClicked()
{
    Caixa_service caixaServ;
    if (caixaServ.caixaAtual().aberto()) {
        QMessageBox::information(this, "Caixa", "Você já tem um caixa aberto.");
        return;
    }
    AberturaCaixa dlg(this);
    if (dlg.exec() == QDialog::Accepted)
        atualizarIndicadorCaixa();
}

void MainWindow::fecharCaixaClicked()
{
    Caixa_service caixaServ;
    if (!caixaServ.caixaAtual().aberto()) {
        QMessageBox::information(this, "Caixa", "Você não tem caixa aberto.");
        return;
    }
    FechamentoCaixa dlg(this);
    dlg.exec();
    atualizarIndicadorCaixa();
}

void MainWindow::sangriaClicked()
{
    if (!garantirCaixaAberto())
        return;
    MovimentacaoCaixa dlg(MovimentacaoCaixa::Tipo::Sangria, this);
    dlg.exec();
}

void MainWindow::suprimentoClicked()
{
    if (!garantirCaixaAberto())
        return;
    MovimentacaoCaixa dlg(MovimentacaoCaixa::Tipo::Suprimento, this);
    dlg.exec();
}

void MainWindow::historicoCaixaClicked()
{
    if (!exigirGerente(QStringLiteral("O histórico de caixas")))
        return;
    HistoricoCaixas dlg(this);
    dlg.exec();
}

void MainWindow::operadoresClicked()
{
    if (!exigirGerente(QStringLiteral("O cadastro de operadores")))
        return;
    Operadores dlg(this);
    dlg.exec();
}

// Abre direto a tela de venda (maximizada). Se já estiver aberta, apenas a traz para frente.
void MainWindow::abrirPdv()
{
    if (!garantirCaixaAberto())
        return;

    if (pdvAberto) {
        pdvAberto->showMaximized();
        pdvAberto->raise();
        pdvAberto->activateWindow();
        return;
    }

    venda *pdv = new venda;
    pdv->setAttribute(Qt::WA_DeleteOnClose);
    connect(pdv, &venda::vendaConcluida, this, &MainWindow::atualizarTableview);
    pdvAberto = pdv;
    pdv->showMaximized();
}


void MainWindow::on_Btn_AddProd_clicked()
{

    InserirProduto *addProdJanela = new InserirProduto;
    addProdJanela->show();
    connect(addProdJanela, &InserirProduto::codigoBarrasExistenteSignal,
            this, &MainWindow::mostrarProdutoPorCodigoBarras);
    connect(addProdJanela, &InserirProduto::produtoInserido, this,
            &MainWindow::atualizarTableview);
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    // clicar na logo volta para o menu inicial
    if (obj == ui->label_6 && event->type() == QEvent::MouseButtonRelease) {
        irParaInicio();
        return true;
    }

    // Verificar se o evento é uma tecla pressionada no lineEdit
    if (obj == ui->Ledit_Pesquisa && event->type() == QEvent::KeyPress)
    {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter)
        {
            ui->Btn_Pesquisa->click(); // Simula um clique no botão
            return true; // Evento tratado
        }
    }

    // Processar o evento padrão
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::on_Tview_Produtos_customContextMenuRequested(const QPoint &pos)
{

    if(!ui->Tview_Produtos->currentIndex().isValid())
        return;

    QMenu menu(this);
    QMenu *imprimirMenu = new QMenu("Imprimir Etiqueta Código de Barra", this);
    QMenu *gondolaMenu = new QMenu("Imprimir Nome e Preço (Gondola)", this);

    menu.addAction(actionMenuAlterarProd);
    menu.addAction(actionMenuDeletarProd);
    menu.addAction(actionSetLocalProd);
    menu.addAction(actionVerProduto);
    imprimirMenu->setIcon(iconImpressora);
    imprimirMenu->addAction(actionMenuPrintBarCode1);
    imprimirMenu->addAction(actionMenuPrintBarCode3);
    menu.addMenu(imprimirMenu);
    gondolaMenu->setIcon(iconImpressora);
    gondolaMenu->addAction(actionMenuPrintNomePreco1);
    gondolaMenu->addAction(actionMenuPrintNomePreco3);
    menu.addMenu(gondolaMenu);

    menu.exec(ui->Tview_Produtos->viewport()->mapToGlobal(pos));
}

void MainWindow::on_actionConfig_triggered()
    {
     if (!exigirGerente(QStringLiteral("As configurações")))
         return;
     Configuracao *configuracao = new Configuracao();
    configuracao->show();
    connect(configuracao, &Configuracao::alterouConfig, this,
            &MainWindow::atualizarConfigAcbr);
    connect(configuracao, &Configuracao::alterouConfig, this,
            &MainWindow::atualizarConfigDTO);
    // o timeout é local desta máquina: vale para a sessão atual, sem esperar novo login
    connect(configuracao, &Configuracao::alterouConfig, this, [this]() {
        configDTO = confServ->carregarTudo();
        aplicarSessao();
    });
}
void MainWindow::atualizarConfigDTO(){
    configDTO = confServ->carregarTudo();
    atualizarLogoCabecalho();
    atualizarSaudacao();
}

void MainWindow::on_Ledit_Pesquisa_textChanged(const QString &arg1)
{
    ui->Btn_Pesquisa->click();
}


void MainWindow::on_Btn_Clientes_clicked()
{


    Clientes *clientes = new Clientes;
    // clientes->setWindowModality(Qt::ApplicationModal);
    clientes->show();
}

void MainWindow::atualizarTableviewComQuery(QString &query){
    model->setQuery(query);
}

void MainWindow::setLocalProd()
{
    auto *selectionModel = ui->Tview_Produtos->selectionModel();
    QModelIndexList selected = selectionModel->selectedIndexes();

    if (selected.isEmpty())
        return;

    int row = selected.first().row();

    QModelIndex idIndex    = ui->Tview_Produtos->model()->index(row, 0);
    QModelIndex localIndex = ui->Tview_Produtos->model()->index(row, 14);

    int id = ui->Tview_Produtos->model()->data(idIndex).toInt();
    QString localAtual = ui->Tview_Produtos->model()->data(localIndex).toString();

    // service
    auto sugestoes = produtoService->obterSugestoesLocal();

    LeditDialog dialog(this);
    dialog.setWindowTitle("Inserir Texto");
    dialog.setLabelText("Informe o local do produto:");
    dialog.setLineEditText(localAtual);
    dialog.Ledit_info->setMaxLength(60);
    dialog.setCompleterSuggestions(sugestoes);

    if (dialog.exec() == QDialog::Accepted) {
        QString novoLocal = dialog.getLineEditText();

        auto res = produtoService->atualizarLocalProduto(id, novoLocal);

        if (!res.ok) {
            QMessageBox::warning(this, "Erro", res.msg);
            return;
        }

        atualizarTableview();
        emit localSetado();
    }
}

QString MainWindow::getIdProdSelected(){
    QItemSelectionModel *selectionModel = ui->Tview_Produtos->selectionModel();
    QModelIndexList selectedIndexes = selectionModel->selectedIndexes();

    if (!selectedIndexes.isEmpty()) {
        int selectedRow = selectedIndexes.first().row();
        QModelIndex idIndex = ui->Tview_Produtos->model()->index(selectedRow, 0);

        QString id = ui->Tview_Produtos->model()->data(idIndex).toString();
        return id;
    }
}

void MainWindow::verProd(){
    QString id = getIdProdSelected();
    InfoJanelaProd *janelaProd = new InfoJanelaProd(this, id);
    janelaProd->show();
}

void MainWindow::on_actionDocumenta_o_triggered()
{
    HelpPage *page = new HelpPage(this, "");
    page->show();
}

void MainWindow::atualizarConfigAcbr(){
    qDebug() << " tentou atualizar acbr config";
        Acbr_service acbrService;
        auto res = acbrService.configurar(VERSAO_QE);
        if(!res.ok){
            if(res.erro != AcbrErro::NaoEmitindoNf){
                QMessageBox::critical(this, "Erro", res.msg);
                return;
            }

        }else{
            qDebug() << res.msg;
        }

        if (!contingenciaService && configDTO.emitNfFiscal) {
            contingenciaService = new ContingenciaService(this);
            contingenciaService->iniciar();
        }
}


void MainWindow::on_Btn_Entradas_clicked()
{
    // A tela abre sempre: importar XML funciona sem certificado e a própria tela explica
    // o que falta para buscar pela chave de acesso (certificado A1 e ambiente de Produção).
    Entradas *entradas = new Entradas();
    entradas->show();
    connect(entradas, &Entradas::produtoAdicionado, this,
            &MainWindow::atualizarTableview);

}


void MainWindow::on_actionEnviar_triggered()
{

}


void MainWindow::on_actionSobre_triggered()
{
    Sobre *sobre = new Sobre();
    sobre->show();
}

void MainWindow::on_actionEnviar_Notas_Contador_triggered()
{
    JanelaEmailContador *janelaEmail = new JanelaEmailContador();
    janelaEmail->show();
}


void MainWindow::on_actionMonitor_Fiscal_triggered()
{
    MonitorFiscal *monitor = new MonitorFiscal();
    monitor->show();
}


void MainWindow::on_actionInutilizar_Numera_o_NF_triggered()
{
    InutilizacaoDialog *inu = new InutilizacaoDialog();
    inu->show();
}


void MainWindow::on_actionEnviar_Carta_de_Corre_o_triggered()
{
    CartaCorrecaoJanela *janela = new CartaCorrecaoJanela();
    janela->show();
}


void MainWindow::on_actionSQLite_triggered()
{
    QString pastaDestino = QFileDialog::getExistingDirectory(
        this,
        "Escolha a pasta para salvar o banco"
        );
    Export_service expServ;
    auto r1 = expServ.exportarSqliteDB(pastaDestino);
    if(r1.ok){
        QMessageBox::information(this, "Sucesso", r1.msg);
    }else{
        QMessageBox::warning(this, "Erro ao exportar banco", r1.msg);
    }

}

