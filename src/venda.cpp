#include "venda.h"
#include "operadores.h"
#include "services/contasreceber_service.h"
#include "ui_venda.h"
#include "customdelegate.h"
#include <QSqlQueryModel>
#include <QSqlQuery>
#include <QStandardItemModel>
#include <QVector>
#include <QMessageBox>
#include <QDoubleValidator>
#include <QTimer>
#include "delegateprecof2.h"
#include "delegateprecovalidate.h"
#include "delegatelockcol.h"
#include "delegatequant.h"
#include <QCompleter>
#include <QStringListModel>
#include <QMenu>
#include <QHeaderView>
#include <QKeyEvent>
#include <QRegularExpression>
#include <QShortcut>
#include <cmath>
#include "delegateacoescarrinho.h"
#include "delegateacoescatalogo.h"
#include "delegatecatalogopdv.h"
#include <QIdentityProxyModel>
#include <QLabel>
#include <QFont>
#include <QMargins>
#include <QResizeEvent>
#include <QSettings>
#include "infra/apppath_service.h"
#include "inserircliente.h"
#include "infojanelaprod.h"
#include "../services/Produto_service.h"
#include "services/config_service.h"
#include "services/sessao_service.h"

namespace {
// Coluna extra "Ações" em cima do QSqlQueryModel do catálogo, sem alterar o SELECT.
class CatalogoAcoesProxy : public QIdentityProxyModel
{
public:
    explicit CatalogoAcoesProxy(QObject *parent = nullptr)
        : QIdentityProxyModel(parent) {}

    int columnCount(const QModelIndex &parent = QModelIndex()) const override
    {
        if (parent.isValid() || !sourceModel())
            return 0;
        return sourceModel()->columnCount() + 1;
    }

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override
    {
        if (parent.isValid() || !sourceModel())
            return {};
        if (column == sourceModel()->columnCount()) {
            if (row < 0 || row >= sourceModel()->rowCount())
                return {};
            return createIndex(row, column);
        }
        return QIdentityProxyModel::index(row, column, parent);
    }

    QModelIndex mapToSource(const QModelIndex &proxyIndex) const override
    {
        if (!proxyIndex.isValid() || !sourceModel())
            return {};
        if (proxyIndex.column() >= sourceModel()->columnCount())
            return {};
        return QIdentityProxyModel::mapToSource(proxyIndex);
    }

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override
    {
        if (!sourceModel() || index.column() >= sourceModel()->columnCount())
            return {};
        return QIdentityProxyModel::data(index, role);
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override
    {
        if (orientation == Qt::Horizontal && sourceModel()
            && section == sourceModel()->columnCount()) {
            if (role == Qt::DisplayRole)
                return QStringLiteral("Ações");
            return {};
        }
        return QIdentityProxyModel::headerData(section, orientation, role);
    }

    Qt::ItemFlags flags(const QModelIndex &index) const override
    {
        if (sourceModel() && index.column() == sourceModel()->columnCount())
            return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
        return QIdentityProxyModel::flags(index);
    }
};

// 1 -> "1", 1.5 -> "1,50" (sem separador de milhar, para o editor aceitar de volta)
QString formatarQuantidade(const QLocale &base, double q)
{
    QLocale l = base;
    l.setNumberOptions(l.numberOptions() | QLocale::OmitGroupSeparator);
    const bool inteira = std::fabs(q - std::round(q)) < 1e-9;
    return l.toString(q, 'f', inteira ? 0 : 2);
}
}

namespace {
// preferências do PDV: seção [pdv] do config.ini (sem migration, com valor padrão)
bool lerPreferenciaPdv(const QString &chave, bool padrao)
{
    QSettings ini(AppPath_service::configPath(), QSettings::IniFormat);
    const QVariant v = ini.value("pdv/" + chave);
    return v.isValid() ? v.toString() == "1" : padrao;
}

void gravarPreferenciaPdv(const QString &chave, bool valor)
{
    QSettings ini(AppPath_service::configPath(), QSettings::IniFormat);
    ini.setValue("pdv/" + chave, valor ? "1" : "0");
}
}

venda::venda(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::venda)
{
    ui->setupUi(this);

    // enquanto houver itens no carrinho a sessão não pode ser trocada nem expirar (só bloquear)
    Sessao_service::instancia()->registrarTelaDeVenda(this, [this]() { return carrinhoComItens(); });

    // quem está vendendo fica visível na tela: a venda é gravada com o id desta sessão
    connect(Sessao_service::instancia(), &Sessao_service::sessaoMudou,
            this, [this]() { atualizarIndicadorOperador(); });
    atualizarIndicadorOperador();

    prodServ.listarProdutos(modeloProdutos);
    modeloProdutos->setHeaderData(0, Qt::Horizontal, tr("ID"));
    modeloProdutos->setHeaderData(1, Qt::Horizontal, tr("Quantidade"));
    modeloProdutos->setHeaderData(2, Qt::Horizontal, tr("Descrição"));
    modeloProdutos->setHeaderData(3, Qt::Horizontal, tr("Preço"));
    modeloProdutos->setHeaderData(4, Qt::Horizontal, tr("Código de Barras"));
    modeloProdutos->setHeaderData(5, Qt::Horizontal, tr("NF"));
    auto *proxyCatalogo = new CatalogoAcoesProxy(this);
    proxyCatalogo->setSourceModel(modeloProdutos);
    ui->Tview_Produtos->setModel(proxyCatalogo);

    // delegates
    // catálogo: borda vermelha no estoque zerado + fundo discreto na linha (só visual)
    ui->Tview_Produtos->setItemDelegateForColumn(1, new DelegateCatalogoPDV(true, this));
    for (int coluna : {2, 3, 4})
        ui->Tview_Produtos->setItemDelegateForColumn(coluna, new DelegateCatalogoPDV(false, this));
    DelegateAcoesCatalogo *delegateAcoesCatalogo = new DelegateAcoesCatalogo(this);
    ui->Tview_Produtos->setItemDelegateForColumn(modeloProdutos->columnCount(), delegateAcoesCatalogo);
    connect(delegateAcoesCatalogo, &DelegateAcoesCatalogo::adicionarClicado, this, [this](int linha) {
        ui->Tview_Produtos->selectRow(linha);
        on_Btn_SelecionarProduto_clicked();
    }, Qt::QueuedConnection);
    connect(delegateAcoesCatalogo, &DelegateAcoesCatalogo::verClicado, this, [this](int linha) {
        ui->Tview_Produtos->selectRow(linha);
        verProd();
    }, Qt::QueuedConnection);
    ui->Tview_Produtos->verticalHeader()->setDefaultSectionSize(34);
    ui->Tview_Produtos->setMouseTracking(true);
    ui->Tview_Produtos->viewport()->setMouseTracking(true);
    ui->Tview_Produtos->installEventFilter(this);
    ui->Ledit_Pesquisa->installEventFilter(this);
    DelegatePrecoValidate *validatePreco = new DelegatePrecoValidate(this);
    ui->Tview_ProdutosSelecionados->setItemDelegateForColumn(3, validatePreco);
    DelegateLockCol *delegateLockCol = new DelegateLockCol(0, this);
    ui->Tview_ProdutosSelecionados->setItemDelegateForColumn(0, delegateLockCol);
    DelegateLockCol *delegateLockCol2 = new DelegateLockCol(2, this);
    ui->Tview_ProdutosSelecionados->setItemDelegateForColumn(2, delegateLockCol2);
    DelegateQuant *delegateQuant = new DelegateQuant(this);
    ui->Tview_ProdutosSelecionados->setItemDelegateForColumn(1, delegateQuant);
    DelegateLockCol *delegateLockCol3 = new DelegateLockCol(4, this);
    ui->Tview_ProdutosSelecionados->setItemDelegateForColumn(4, delegateLockCol3);

    ui->Tview_Produtos->horizontalHeader()->setStyleSheet("background-color: rgb(16, 122, 46)");

    modeloSelecionados->setHorizontalHeaderItem(0, new QStandardItem("ID Produto"));
    modeloSelecionados->setHorizontalHeaderItem(1, new QStandardItem("Quantidade Vendida"));
    modeloSelecionados->setHorizontalHeaderItem(2, new QStandardItem("Descrição"));
    modeloSelecionados->setHorizontalHeaderItem(3, new QStandardItem("Preço Unitário Vendido"));
    modeloSelecionados->setHorizontalHeaderItem(4, new QStandardItem("Total"));
    modeloSelecionados->setHorizontalHeaderItem(5, new QStandardItem("Ações"));
    ui->Tview_ProdutosSelecionados->setModel(modeloSelecionados);

    QItemSelectionModel *selectionModel = ui->Tview_ProdutosSelecionados->selectionModel();
    connect(selectionModel, &QItemSelectionModel::selectionChanged, this, &venda::handleSelectionChange);
    QItemSelectionModel *selectionModelProdutos = ui->Tview_Produtos->selectionModel();
    connect(selectionModelProdutos, &QItemSelectionModel::selectionChanged, this,
            &venda::handleSelectionChangeProdutos);
    selecionarPrimeiraLinhaCatalogo();

    ui->Tview_Produtos->setColumnWidth(2, 260);
    ui->Tview_Produtos->setColumnWidth(1, 148);
    ui->Tview_ProdutosSelecionados->setColumnWidth(0, 100);
    ui->Tview_ProdutosSelecionados->setColumnWidth(1, 170);
    ui->Tview_ProdutosSelecionados->setColumnWidth(2, 300);
    ui->Tview_ProdutosSelecionados->setColumnWidth(3, 210);
    ui->Tview_ProdutosSelecionados->setColumnWidth(4, 130);
    ui->Tview_ProdutosSelecionados->setColumnWidth(5, 150);
    QHeaderView *cabecalhoCarrinho = ui->Tview_ProdutosSelecionados->horizontalHeader();
    cabecalhoCarrinho->setStretchLastSection(false);
    cabecalhoCarrinho->setSectionResizeMode(2, QHeaderView::Stretch);
    cabecalhoCarrinho->setSectionResizeMode(5, QHeaderView::Fixed);
    ui->Tview_ProdutosSelecionados->verticalHeader()->setDefaultSectionSize(38);

    // botões [−] [+] [lixeira] em cada linha do carrinho
    DelegateAcoesCarrinho *delegateAcoes = new DelegateAcoesCarrinho(this);
    ui->Tview_ProdutosSelecionados->setItemDelegateForColumn(5, delegateAcoes);
    connect(delegateAcoes, &DelegateAcoesCarrinho::menosClicado, this,
            [this](int linha) { alterarQuantidade(linha, -1); }, Qt::QueuedConnection);
    connect(delegateAcoes, &DelegateAcoesCarrinho::maisClicado, this,
            [this](int linha) { alterarQuantidade(linha, +1); }, Qt::QueuedConnection);
    connect(delegateAcoes, &DelegateAcoesCarrinho::removerClicado, this,
            [this](int linha) { removerItem(linha); }, Qt::QueuedConnection);
    ui->Tview_ProdutosSelecionados->setMouseTracking(true);
    ui->Tview_ProdutosSelecionados->viewport()->setMouseTracking(true);
    ui->Tview_ProdutosSelecionados->installEventFilter(this);

    // desfazer a última remoção
    desfazerTimer = new QTimer(this);
    desfazerTimer->setSingleShot(true);
    desfazerTimer->setInterval(8000);
    connect(desfazerTimer, &QTimer::timeout, this, [this]() {
        temRemovido = false;
        ui->Btn_DesfazerRemocao->hide();
    });
    connect(ui->Btn_DesfazerRemocao, &QPushButton::clicked, this, &venda::desfazerRemocao);
    QShortcut *atalhoDesfazer = new QShortcut(QKeySequence(Qt::Key_F8), this);
    connect(atalhoDesfazer, &QShortcut::activated, this, &venda::desfazerRemocao);

    // preferências do PDV
    ui->Chk_NovaVenda->setChecked(lerPreferenciaPdv("nova_venda_ao_finalizar", true));
    connect(ui->Chk_NovaVenda, &QCheckBox::toggled, this,
            [](bool marcado) { gravarPreferenciaPdv("nova_venda_ao_finalizar", marcado); });
    ui->CheckImprimirCupomPag->setChecked(true);
    connect(ui->CheckImprimirCupomPag, &QCheckBox::toggled, this, [this](bool marcado) {
        if (!marcado)
            ui->CheckImprimirCupomPag->setChecked(true);
    });

    // Alt+1..4 escolhem a forma de pagamento (só valem na página de pagamento)
    const QList<QPair<Qt::Key, int>> atalhosForma = {
        {Qt::Key_1, 0}, {Qt::Key_2, 3}, {Qt::Key_3, 2}, {Qt::Key_4, 4}};
    for (const auto &atalho : atalhosForma) {
        QShortcut *sc = new QShortcut(QKeySequence(Qt::ALT | atalho.first), this);
        const int indice = atalho.second;
        connect(sc, &QShortcut::activated, this, [this, indice]() { selecionarFormaPagamento(indice); });
    }

    // contagem de itens ao lado do total
    connect(modeloSelecionados, &QStandardItemModel::itemChanged,  this, &venda::atualizarContagemItens);
    connect(modeloSelecionados, &QStandardItemModel::rowsInserted, this, &venda::atualizarContagemItens);
    connect(modeloSelecionados, &QStandardItemModel::rowsRemoved,  this, &venda::atualizarContagemItens);

    ui->DateEdt_Venda->setDateTime(QDateTime::currentDateTime());

    actionMenuDeletarProd = new QAction(this);
    actionMenuDeletarProd->setText("Deletar Produto");
    deletar.addFile(":/QEstoqueLOja/amarok-cart-remove.svg");
    actionMenuDeletarProd->setIcon(deletar);
    connect(actionMenuDeletarProd, SIGNAL(triggered(bool)), this, SLOT(deletarProd()));

    ui->Tview_ProdutosSelecionados->setEditTriggers(
        QAbstractItemView::DoubleClicked | QAbstractItemView::SelectedClicked);

    connect(modeloSelecionados, &QStandardItemModel::itemChanged, this, [=]() {
        ui->Lbl_Total->setText(Total());
        atualizarTotalProduto();
    });

    QCompleter *completer = new QCompleter(this);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchContains);

    atualizarListaCliente();

    definirClientePadrao();

    QStringListModel *model = new QStringListModel(clientesComId, this);
    completer->setModel(model);
    ui->Ledit_Cliente->setCompleter(completer);

    connect(ui->Ledit_Cliente, &QLineEdit::textEdited, this, [=]() { completer->complete(); });
    connect(ui->Ledit_Cliente, &QLineEdit::cursorPositionChanged, this, [=]() { completer->complete(); });
    connect(ui->Ledit_Cliente, &QLineEdit::editingFinished, this, [=]() { validarCliente(true); });

    Config_service *confServ = new Config_service(this);
    configDTO = confServ->carregarTudo();
    bool tipoAmb = configDTO.tpAmbFiscal;
    bool emitirNf = configDTO.emitNfFiscal;

    if (tipoAmb == 1 && emitirNf == 1) {
        ui->Lbl_TpAmb->setText("Ambiente: Produção");
        ui->Lbl_TpAmb->setStyleSheet("color: white; background-color: green; font-weight: bold; padding: 4px; border-radius: 5px;");
    } else if (emitirNf == 1) {
        ui->Lbl_TpAmb->setText("Ambiente: Homologação");
        ui->Lbl_TpAmb->setStyleSheet("color: white; background-color: orange; font-weight: bold; padding: 4px; border-radius: 5px;");
    }

    // duplo clique adiciona ao carrinho; a ficha do produto fica no menu de contexto
    connect(ui->Tview_Produtos, &QTableView::doubleClicked, this, [this](const QModelIndex &idx) {
        if (!idx.isValid() || idx.column() == idx.model()->columnCount() - 1)
            return;
        ui->Tview_Produtos->selectRow(idx.row());
        on_Btn_SelecionarProduto_clicked();
    });
    ui->Tview_Produtos->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->Tview_Produtos, &QWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        const QModelIndex idx = ui->Tview_Produtos->indexAt(pos);
        if (!idx.isValid())
            return;
        ui->Tview_Produtos->selectRow(idx.row());
        QMenu menu(this);
        QAction *acaoAdicionar = menu.addAction("Adicionar ao carrinho");
        QAction *acaoVer = menu.addAction("Ver produto");
        QAction *escolhida = menu.exec(ui->Tview_Produtos->viewport()->mapToGlobal(pos));
        if (escolhida == acaoAdicionar)
            on_Btn_SelecionarProduto_clicked();
        else if (escolhida == acaoVer)
            verProd();
    });

    // payment validators
    QDoubleValidator *validador = new QDoubleValidator(0.0, 9999.99, 2, this);
    ui->Ledit_Taxa->setValidator(validador);
    ui->Ledit_Recebido->setValidator(new QDoubleValidator(0.0, 99999.99, 2, this));
    ui->Ledit_Desconto->setValidator(new QDoubleValidator(0.0, 99999.99, 2, this));
    QIntValidator *validadorInt = new QIntValidator(this);
    ui->Ledit_NNF->setValidator(validadorInt);

    // step 4 só aparece quando NF está habilitado nas configurações
    ui->Lbl_Step4->setVisible(configDTO.emitNfFiscal);
    ui->lbl_sep3->setVisible(configDTO.emitNfFiscal);
    ui->CheckImprimirCupomPag->setVisible(!configDTO.emitNfFiscal);

    rascunhoTimer = new QTimer(this);
    rascunhoTimer->setSingleShot(true);
    rascunhoTimer->setInterval(400);
    connect(rascunhoTimer, &QTimer::timeout, this, &venda::salvarRascunho);

    auto agendarSalvar = [=]() { rascunhoTimer->start(); };

    // produtos
    connect(modeloSelecionados, &QStandardItemModel::itemChanged,  this, agendarSalvar);
    connect(modeloSelecionados, &QStandardItemModel::rowsInserted, this, agendarSalvar);
    connect(modeloSelecionados, &QStandardItemModel::rowsRemoved,  this, agendarSalvar);

    // cliente e data
    connect(ui->Ledit_Cliente,   &QLineEdit::editingFinished,  this, agendarSalvar);
    connect(ui->DateEdt_Venda,   &QDateTimeEdit::dateTimeChanged, this, agendarSalvar);

    // pagamento
    connect(ui->CBox_FormaPagamento, QOverload<int>::of(&QComboBox::currentIndexChanged), this, agendarSalvar);
    connect(ui->Ledit_Desconto,  &QLineEdit::textChanged, this, agendarSalvar);
    connect(ui->Ledit_Taxa,      &QLineEdit::textChanged, this, agendarSalvar);
    connect(ui->Ledit_Recebido,  &QLineEdit::textChanged, this, agendarSalvar);
    connect(ui->CheckPorcentagem, &QCheckBox::stateChanged, this, agendarSalvar);

    // nota fiscal
    connect(ui->CBox_ModeloEmit, QOverload<int>::of(&QComboBox::currentIndexChanged), this, agendarSalvar);
    connect(ui->RadioBtn_EmitNfTodos,  &QRadioButton::toggled, this, agendarSalvar);
    connect(ui->RadioBtn_EmitNfApenas, &QRadioButton::toggled, this, agendarSalvar);

    irParaPagina(0);
    ui->Ledit_Pesquisa->setFocus();

    verificarRascunho();
}

// ─── Navigation ──────────────────────────────────────────────────────────────

void venda::irParaPagina(int pagina)
{
    ui->stack_Pages->setCurrentIndex(pagina);
    if (pagina == 1)
        pulouCliente = false;

    const QString styleAtivo   = "background-color: rgb(23, 150, 58); color: white; border-radius: 14px; font: 700 9pt \"Ubuntu\"; padding: 2px 12px;";
    const QString styleInativo = "background-color: rgba(197, 222, 208,60); color: rgba(197, 222, 208,160); border-radius: 14px; font: 700 9pt \"Ubuntu\"; padding: 2px 12px;";

    ui->Lbl_Step1->setStyleSheet(pagina == 0 ? styleAtivo : styleInativo);
    ui->Lbl_Step2->setStyleSheet(pagina == 1 ? styleAtivo : styleInativo);
    ui->Lbl_Step3->setStyleSheet(pagina == 2 ? styleAtivo : styleInativo);
    ui->Lbl_Step4->setStyleSheet(pagina == 3 ? styleAtivo : styleInativo);

    ui->Btn_Voltar->setVisible(pagina > 0);

    switch (pagina) {
    case 0:
        ui->Btn_Aceitar->setText("Pagamento (F10) →");
        ui->Ledit_Pesquisa->setFocus();
        break;
    case 1:
        ui->Btn_Aceitar->setText("Próximo: Pagamento →");
        ui->DateEdt_Venda->setFocus();
        break;
    case 2:
        ui->Btn_Aceitar->setText(configDTO.emitNfFiscal ? "Próximo: Nota Fiscal →" : "✓ Confirmar Venda (F10)");
        QTimer::singleShot(0, this, [this]() { igualarCartoesPagamento(); });
        if (ui->Ledit_Recebido->isHidden()) {
            ui->Btn_Aceitar->setFocus();
        } else {
            ui->Ledit_Recebido->setFocus();
            QTimer::singleShot(0, ui->Ledit_Recebido, &QLineEdit::selectAll);
        }
        break;
    case 3:
        ui->Btn_Aceitar->setText("Emitir e Confirmar →");
        ui->Ledit_NNF->setFocus();
        break;
    }
}

void venda::on_Btn_Aceitar_clicked()
{
    int pagina = ui->stack_Pages->currentIndex();

    if (pagina == 0) {
        avancarParaPagamento();

    } else if (pagina == 1) {
        idClienteAtual = validarCliente(true);
        if (idClienteAtual < 0)
            return;
        configurarPaginaPagamento();
        irParaPagina(2);

    } else if (pagina == 2) {
        if (configDTO.emitNfFiscal) {
            configurarPaginaNF();
            irParaPagina(3);
        } else {
            terminarPagamento();
        }
    } else if (pagina == 3) {
        terminarPagamento();
    }
}

void venda::avancarParaPagamento()
{
    if (modeloSelecionados->rowCount() == 0) {
        QMessageBox::warning(this, "Carrinho", "Adicione pelo menos um produto ao carrinho.");
        return;
    }
    ui->Lbl_ResumoTotalCliente->setText("R$ " + Total());
    ui->Lbl_ResumoItens->setText(QString::number(modeloSelecionados->rowCount()) + " itens");

    // o cliente padrão já vem preenchido: só passa pela página de cliente se ele não servir
    idClienteAtual = validarCliente(false);
    if (idClienteAtual < 0) {
        irParaPagina(1);
        validarCliente(true);
        return;
    }
    configurarPaginaPagamento();
    irParaPagina(2);
    pulouCliente = true;
}

bool venda::pagamentoValido()
{
    if (ui->CBox_FormaPagamento->currentIndex() == 0) {
        const double recebido = portugues.toDouble(ui->Ledit_Recebido->text());
        const double valorFinal = portugues.toDouble(ui->Lbl_TotalTaxa->text());
        if (recebido + 0.005 < valorFinal) {
            QMessageBox::warning(this, "Pagamento", "O valor recebido é menor que o valor a pagar.");
            ui->Ledit_Recebido->setFocus();
            ui->Ledit_Recebido->selectAll();
            return false;
        }
    }
    return true;
}

// F10: leva ao pagamento e, estando nele, confirma
void venda::finalizarRapido()
{
    const int pagina = ui->stack_Pages->currentIndex();
    if (pagina == 0) {
        avancarParaPagamento();
        return;
    }
    if (pagina == 2 && !pagamentoValido())
        return;
    on_Btn_Aceitar_clicked();
}

void venda::selecionarFormaPagamento(int index)
{
    if (ui->stack_Pages->currentIndex() != 2)
        return;
    ui->CBox_FormaPagamento->setCurrentIndex(index);
    on_CBox_FormaPagamento_activated(index);
    if (index == 0) {
        ui->Ledit_Recebido->setFocus();
        QTimer::singleShot(0, ui->Ledit_Recebido, &QLineEdit::selectAll);
    } else {
        ui->Btn_Aceitar->setFocus();
    }
}

void venda::on_Ledit_Recebido_returnPressed()
{
    if (ui->stack_Pages->currentIndex() == 2 && pagamentoValido())
        on_Btn_Aceitar_clicked();
}

void venda::on_Btn_Voltar_clicked()
{
    int pagina = ui->stack_Pages->currentIndex();
    if (pagina == 2 && pulouCliente)
        irParaPagina(0);
    else if (pagina > 0)
        irParaPagina(pagina - 1);
}

// ─── Payment setup ────────────────────────────────────────────────────────────

void venda::configurarPaginaPagamento()
{
    auto [nome, id] = cliServ.extrairNomeId(ui->Ledit_Cliente->text());
    QString data = portugues.toString(ui->DateEdt_Venda->dateTime(), "dd-MM-yyyy hh:mm:ss");

    CLIENTE = cliServ.getClienteByID(idClienteAtual);

    ui->Lbl_ResumoTotal->setText(Total());
    ui->Lbl_ResumoCliente->setText(nome);
    ui->Lbl_ResumoData->setText(data);

    // defaults
    ui->Ledit_Recebido->setText(Total());
    ui->Lbl_Troco->setText("0");
    ui->Ledit_Desconto->setText("0");
    ui->Ledit_Taxa->setText("0");
    ui->Lbl_TotalTaxa->setText(Total());
    ui->Lbl_Total->setText(Total());

    if (temRascunhoPendente) {
        // aplica forma de pagamento salva e reconfigura visibilidade dos campos
        int idxForma = ui->CBox_FormaPagamento->findText(rascunhoPendente.formaPagamento);
        if (idxForma >= 0)
            ui->CBox_FormaPagamento->setCurrentIndex(idxForma);
        on_CBox_FormaPagamento_activated(ui->CBox_FormaPagamento->currentIndex());

        ui->CheckPorcentagem->setChecked(rascunhoPendente.descontoPorcentagem);
        ui->Ledit_Desconto->setText(rascunhoPendente.desconto);
        ui->Ledit_Taxa->setText(rascunhoPendente.taxa);
        ui->Ledit_Recebido->setText(rascunhoPendente.recebido);
    } else {
        // defaults para nova venda: dinheiro — hide taxa, show troco
        ui->frame_Taxa->hide();
        ui->Ledit_Recebido->show();
        ui->Lbl_Troco->show();
        ui->frame_Recebido->show();
        ui->frame_Troco->show();
    }
    ui->CheckImprimirCupomPag->setChecked(true);
}

void venda::configurarPaginaNF()
{
    if (temRascunhoPendente) {
        ui->CBox_ModeloEmit->setCurrentIndex(rascunhoPendente.modeloNf);
        if (rascunhoPendente.emitirTodos)
            ui->RadioBtn_EmitNfTodos->setChecked(true);
        else
            ui->RadioBtn_EmitNfApenas->setChecked(true);
        temRascunhoPendente = false;
    }

    int idx = ui->CBox_ModeloEmit->currentIndex();
    ModeloNota modelo = (idx == 1) ? ModeloNota::NFe : ModeloNota::NFCe;
    ui->Ledit_NNF->setText(QString::number(notaServ.getProximoNNF(configDTO.tpAmbFiscal, modelo)));

    // na primeira entrada com rascunho usa o CPF salvo; nas demais usa o do cliente
    if (!rascunhoPendente.cpfManual.isEmpty()) {
        ui->Ledit_CpfCnpjCliente->setText(rascunhoPendente.cpfManual);
        rascunhoPendente.cpfManual.clear();
    } else {
        ui->Ledit_CpfCnpjCliente->setText(CLIENTE.cpf);
    }

    bool emite = (idx != 2);
    ui->RadioBtn_EmitNfApenas->setVisible(emite);
    ui->RadioBtn_EmitNfTodos->setVisible(emite);
    ui->Lbl_NNF->setVisible(emite);
    ui->Ledit_NNF->setVisible(emite);
}

// ─── Payment form slots ───────────────────────────────────────────────────────

float venda::obterValorFinal(QString taxa, QString desconto)
{
    return (portugues.toFloat(Total()) - portugues.toFloat(desconto))
           * (1 + portugues.toFloat(taxa) / 100);
}

void venda::descontoTaxa()
{
    QString novaTaxa = ui->Ledit_Taxa->text();
    QString descontoInicial = ui->Ledit_Desconto->text();
    QString desconto;
    if (ui->CheckPorcentagem->isChecked())
        desconto = portugues.toString((portugues.toFloat(descontoInicial) / 100) * portugues.toFloat(Total()));
    else
        desconto = descontoInicial;

    QString valorFinal = portugues.toString(obterValorFinal(novaTaxa, desconto), 'f', 2);
    ui->Lbl_TotalTaxa->setText(valorFinal);
    ui->Lbl_Total->setText(valorFinal);
    ui->Ledit_Recebido->setText(valorFinal);
    ui->Lbl_Troco->setText("0");
}

void venda::on_CBox_FormaPagamento_activated(int index)
{
    QString taxaDebito  = QString::number(configDTO.taxaDebitoFinanceiro);
    QString taxaCredito = QString::number(configDTO.taxaCreditoFinanceiro);

    switch (index) {
    case 0: // dinheiro
        ui->Ledit_Recebido->show(); ui->Lbl_Troco->show();
        ui->frame_Recebido->show(); ui->frame_Troco->show();
        ui->frame_Taxa->hide();
        ui->Ledit_Recebido->setText(Total());
        ui->Lbl_Troco->setText("0");
        ui->Ledit_Desconto->setText("0");
        ui->Ledit_Taxa->setText("0");
        ui->Lbl_TotalTaxa->setText(Total());
        ui->Lbl_Total->setText(Total());
        break;
    case 2: // crédito
        ui->frame_Recebido->hide(); ui->frame_Troco->hide();
        ui->Ledit_Taxa->show(); ui->frame_Taxa->show();
        ui->Ledit_Desconto->setText("0");
        ui->Ledit_Taxa->setText(taxaCredito);
        ui->Lbl_TotalTaxa->setText(portugues.toString(obterValorFinal(taxaCredito, "0"), 'f', 2));
        ui->Lbl_Total->setText(ui->Lbl_TotalTaxa->text());
        break;
    case 3: // débito
        ui->frame_Recebido->hide(); ui->frame_Troco->hide();
        ui->Ledit_Taxa->show(); ui->frame_Taxa->show();
        ui->Ledit_Desconto->setText("0");
        ui->Ledit_Taxa->setText(taxaDebito);
        ui->Lbl_TotalTaxa->setText(portugues.toString(obterValorFinal(taxaDebito, "0"), 'f', 2));
        ui->Lbl_Total->setText(ui->Lbl_TotalTaxa->text());
        break;
    default:
        ui->frame_Recebido->hide(); ui->frame_Troco->hide();
        ui->frame_Taxa->hide();
        ui->Ledit_Desconto->setText("0");
        ui->Ledit_Taxa->setText("0");
        ui->Lbl_TotalTaxa->setText(Total());
        ui->Lbl_Total->setText(Total());
        break;
    }
}

void venda::on_Ledit_Recebido_textChanged(const QString &)
{
    float troco = portugues.toFloat(ui->Ledit_Recebido->text())
                  - portugues.toFloat(ui->Lbl_TotalTaxa->text());
    ui->Lbl_Troco->setText(portugues.toString(troco, 'f', 2));
    // verde quando há troco, vermelho quando falta dinheiro
    ui->Lbl_Troco->setStyleSheet(troco < 0
        ? "font: 700 72pt \"Ubuntu\"; color: rgb(191,61,64);"
        : "font: 700 72pt \"Ubuntu\"; color: rgb(30,140,70);");
}

void venda::on_Ledit_Taxa_textChanged(const QString &) { descontoTaxa(); }
void venda::on_Ledit_Desconto_textChanged(const QString &) { descontoTaxa(); }
void venda::on_CheckPorcentagem_stateChanged(int) { descontoTaxa(); }

void venda::on_CBox_ModeloEmit_currentIndexChanged(int index)
{
    if (index == 0) {
        ui->Ledit_NNF->setText(QString::number(
            notaServ.getProximoNNF(configDTO.tpAmbFiscal, ModeloNota::NFCe)));
        ui->RadioBtn_EmitNfApenas->setVisible(true);
        ui->RadioBtn_EmitNfTodos->setVisible(true);
        ui->Ledit_NNF->setVisible(true);
        ui->Lbl_NNF->setVisible(true);
    } else if (index == 1) {
        ui->Ledit_NNF->setText(QString::number(
            notaServ.getProximoNNF(configDTO.tpAmbFiscal, ModeloNota::NFe)));
        ui->RadioBtn_EmitNfApenas->setVisible(true);
        ui->RadioBtn_EmitNfTodos->setVisible(true);
        ui->Ledit_NNF->setVisible(true);
        ui->Lbl_NNF->setVisible(true);
    } else if (index == 2) {
        ui->RadioBtn_EmitNfApenas->setVisible(false);
        ui->RadioBtn_EmitNfTodos->setVisible(false);
        ui->Ledit_NNF->setVisible(false);
        ui->Lbl_NNF->setVisible(false);
    }
}

// ─── Finish sale ─────────────────────────────────────────────────────────────

void venda::terminarPagamento()
{
    if (vendaFinalizada)
        return;

    // Enquanto a venda não fecha, a sessão não expira e a troca de operador fica travada.
    // Nas falhas a venda continua aberta, então o bloqueio é mantido; só reiniciarVenda()
    // (sucesso, cancelamento ou rascunho descartado) devolve a liberdade.
    Sessao_service::instancia()->definirVendaEmAndamento(true);

    QString troco          = ui->Lbl_Troco->text();
    QString recebido       = ui->Ledit_Recebido->text();
    QString forma          = ui->CBox_FormaPagamento->currentText();
    QString taxa           = ui->Ledit_Taxa->text();
    QString valor_final    = ui->Lbl_TotalTaxa->text();
    QString descontoInicial = ui->Ledit_Desconto->text();
    QString desconto;

    if (ui->CheckPorcentagem->isChecked())
        desconto = portugues.toString(
            (portugues.toFloat(descontoInicial) / 100) * portugues.toFloat(Total()));
    else
        desconto = descontoInicial;

    emitTodosNf = ui->RadioBtn_EmitNfTodos->isChecked();

    QString data = portugues.toString(ui->DateEdt_Venda->dateTime(), "dd-MM-yyyy hh:mm:ss");
    QDateTime dataIngles = portugues.toDateTime(data, "dd-MM-yyyy hh:mm:ss");

    VendasDTO newVenda;
    newVenda.clienteNome    = CLIENTE.nome;
    newVenda.dataHora       = dataIngles.toString("yyyy-MM-dd hh:mm:ss");
    newVenda.desconto       = portugues.toDouble(desconto);
    newVenda.formaPagamento = forma;
    newVenda.estaPago       = (forma != "Prazo");
    newVenda.idCliente      = idClienteAtual;
    newVenda.taxa           = portugues.toDouble(taxa);
    newVenda.total          = portugues.toDouble(Total());
    newVenda.troco          = portugues.toDouble(troco);
    newVenda.valorFinal     = portugues.toDouble(valor_final);
    newVenda.valorRecebido  = portugues.toDouble(recebido);

    // venda a prazo (fiado): cliente identificado, ativo e dentro do limite de crédito
    if (forma == "Prazo") {
        ContasReceber_service receber;
        auto prazo = receber.validarVendaPrazo(idClienteAtual, newVenda.valorFinal);
        if (!prazo.ok && prazo.limiteExcedido) {
            const auto resp = QMessageBox::question(this, "Limite de crédito",
                prazo.msg + "\n\nLiberar esta venda acima do limite? (precisa do PIN do gerente)",
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (resp == QMessageBox::Yes && Operadores::exigirGerente(this, QStringLiteral("Liberar venda acima do limite")))
                prazo = receber.validarVendaPrazo(idClienteAtual, newVenda.valorFinal, true);
            else
                return;
        }
        if (!prazo.ok) {
            QMessageBox::warning(this, "Venda a prazo", prazo.msg);
            return;
        }
    }
    // a venda fica amarrada à sessão do operador logado, não ao dono do caixa
    newVenda.idOperadorSessao = Sessao_service::instancia()->idOperador();

    if(idClienteAtual > 0)
        CLIENTE = cliServ.getClienteByID(idClienteAtual);
    ClienteDTO cli = CLIENTE;
    cli.cpf = ui->Ledit_CpfCnpjCliente->text().trimmed();

    QList<ProdutoVendidoDTO> produtos = obterProdutosSelecionados();

    auto result = vendaServ.inserirVendaRegraDeNegocio(newVenda, produtos);
    if (!result.ok) {
        QMessageBox::warning(this, "Erro", result.msg);
        return;
    }
    newVenda.id = result.idVendaInserida;

    if (ui->CheckImprimirCNF->isChecked() ||
        (!configDTO.emitNfFiscal && ui->CheckImprimirCupomPag->isChecked()))
        Vendas::imprimirReciboVenda(newVenda.id);

    if (configDTO.emitNfFiscal) {
        qlonglong nnf = ui->Ledit_NNF->text().toLongLong();
        int modeloIdx = ui->CBox_ModeloEmit->currentIndex();

        auto handleResultNf = [&](auto result1) {
            if (!result1.ok && result1.erro == FiscalEmitterErro::NCMInvalido) {
                auto resp = QMessageBox::question(this, "Atenção",
                    result1.msg + "\nDeseja continuar mesmo assim?",
                    QMessageBox::Yes | QMessageBox::No);
                if (resp == QMessageBox::No) {
                    auto r1 = vendaServ.deletarVendaRegraNegocio(newVenda.id, false, "Falha ao emitir a nota fiscal.");
                    if (!r1.ok) QMessageBox::warning(this, "Erro", r1.msg);
                    return false;
                }
            }
            if (!waitDialog) waitDialog = new WaitDialog(this);
            waitDialog->setMessage("Aguardando resposta do servidor...");
            waitDialog->show();
            waitDialog->allowClose();
            if (!result1.ok) {
                waitDialog->setMessage(result1.msg);
                auto r1 = vendaServ.deletarVendaRegraNegocio(newVenda.id, false, "Falha ao emitir a nota fiscal.");
                if (!r1.ok) QMessageBox::warning(this, "Erro", r1.msg);
                return false;
            }
            waitDialog->setMessage(result1.msg);
            QTimer::singleShot(1500, waitDialog, &WaitDialog::close);
            return true;
        };

        if (modeloIdx == 0) {
            auto r = fiscalServ.enviarNfcePadrao(newVenda, produtos, nnf, cli, emitTodosNf, false);
            if (!r.ok && r.erro == FiscalEmitterErro::NCMInvalido) {
                auto resp = QMessageBox::question(this, "Atenção",
                    r.msg + "\nDeseja continuar mesmo assim?", QMessageBox::Yes | QMessageBox::No);
                if (resp == QMessageBox::No) {
                    auto r1 = vendaServ.deletarVendaRegraNegocio(newVenda.id, false, "Falha ao emitir a nota fiscal.");
                    if (!r1.ok) QMessageBox::warning(this, "Erro", r1.msg);
                    return;
                }
                r = fiscalServ.enviarNfcePadrao(newVenda, produtos, nnf, cli, emitTodosNf, true);
            }
            if (!waitDialog) waitDialog = new WaitDialog(this);
            waitDialog->setMessage("Aguardando resposta do servidor...");
            waitDialog->show();
            waitDialog->allowClose();
            if (!r.ok) {
                waitDialog->setMessage(r.msg);
                auto r1 = vendaServ.deletarVendaRegraNegocio(newVenda.id, false, "Falha ao emitir a nota fiscal.");
                if (!r1.ok) QMessageBox::warning(this, "Erro", r1.msg);
                return;
            }
            waitDialog->setMessage(r.msg);
            QTimer::singleShot(1500, waitDialog, &WaitDialog::close);

        } else if (modeloIdx == 1) {
            auto r = fiscalServ.enviarNFePadrao(newVenda, produtos, nnf, cli, emitTodosNf, false);
            if (!r.ok && r.erro == FiscalEmitterErro::NCMInvalido) {
                auto resp = QMessageBox::question(this, "Atenção",
                    r.msg + "\nDeseja continuar mesmo assim?", QMessageBox::Yes | QMessageBox::No);
                if (resp == QMessageBox::No) {
                    auto r1 = vendaServ.deletarVendaRegraNegocio(newVenda.id, false, "Falha ao emitir a nota fiscal.");
                    if (!r1.ok) QMessageBox::warning(this, "Erro", r1.msg);
                    return;
                }
                r = fiscalServ.enviarNFePadrao(newVenda, produtos, nnf, cli, emitTodosNf, true);
            }
            if (!waitDialog) waitDialog = new WaitDialog(this);
            waitDialog->setMessage("Aguardando resposta do servidor...");
            waitDialog->show();
            waitDialog->allowClose();
            if (!r.ok) {
                waitDialog->setMessage(r.msg);
                auto r1 = vendaServ.deletarVendaRegraNegocio(newVenda.id, false, "Falha ao emitir a nota fiscal.");
                if (!r1.ok) QMessageBox::warning(this, "Erro", r1.msg);
                return;
            }
            waitDialog->setMessage(r.msg);
            QTimer::singleShot(1500, waitDialog, &WaitDialog::close);
        }
        // index 2 = Não Emitir NF — nada a fazer
    }

    vendaFinalizada = true;
    const QString totalVenda = ui->Lbl_Total->text();
    const QString mensagem = QString("Venda realizada com sucesso. Total: R$ %1").arg(totalVenda);
    const int atrasoAviso = (waitDialog && waitDialog->isVisible()) ? 1600 : 0;

    descartarRascunho();
    emit vendaConcluida();
    reiniciarVenda(false);
    QTimer::singleShot(atrasoAviso, this, [this, mensagem]() { mostrarToast(mensagem); });
}

void venda::mostrarToast(const QString &texto)
{
    if (!toastSucesso) {
        toastSucesso = new QLabel(this);
        toastSucesso->setAlignment(Qt::AlignCenter);
        toastSucesso->setAttribute(Qt::WA_TransparentForMouseEvents);
        toastSucesso->setStyleSheet(
            "background-color: rgb(30,140,70); color: white;"
            "font: 700 12pt \"Segoe UI\"; border-radius: 8px; padding: 10px 18px;");
        toastTimer = new QTimer(this);
        toastTimer->setSingleShot(true);
        connect(toastTimer, &QTimer::timeout, toastSucesso, &QWidget::hide);
    }
    QFont fonte(QStringLiteral("Segoe UI"));
    fonte.setPointSize(12);
    fonte.setBold(true);
    toastSucesso->setFont(fonte);
    toastSucesso->setText(texto);
    posicionarToast();
    toastSucesso->show();
    toastSucesso->raise();
    toastTimer->start(3500);
}

void venda::posicionarToast()
{
    if (!toastSucesso)
        return;
    const int largura = qBound(420, toastSucesso->fontMetrics().horizontalAdvance(toastSucesso->text()) + 72,
                               qMax(420, width() - 24));
    const int altura = 56;
    toastSucesso->setGeometry(qMax(8, (width() - largura) / 2), 74, largura, altura);
}

void venda::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    posicionarToast();
    igualarCartoesPagamento();
}

void venda::igualarCartoesPagamento()
{
    const int espaco = 24;
    if (ui->hl_p2->spacing() != espaco)
        ui->hl_p2->setSpacing(espaco);
    const QMargins margem = ui->hl_p2->contentsMargins();
    const int disponivel = ui->page_Pagamento->height() - margem.top() - margem.bottom() - espaco;
    const int altura = qMax(160, disponivel / 2);
    if (ui->frame_Campos->height() == altura && ui->frame_Pagar->height() == altura)
        return;
    ui->frame_Campos->setFixedHeight(altura);
    ui->frame_Resumo->setFixedHeight(altura);
    ui->frame_Pagar->setFixedHeight(altura);
    ui->frame_Troco->setFixedHeight(altura);
}

void venda::definirClientePadrao()
{
    if (clientesComId.isEmpty())
        return;
    const QString primeiroCliente = clientesComId.first();
    // sem sinais: evita abrir o popup do completer solto na tela
    const QSignalBlocker bloqueio(ui->Ledit_Cliente);
    ui->Ledit_Cliente->setText(primeiroCliente);
    const int posFinalNome = primeiroCliente.indexOf(" (ID:");
    if (posFinalNome != -1)
        ui->Ledit_Cliente->setSelection(0, posFinalNome);
}

// Deixa a tela pronta para a próxima venda, sem fechar.
void venda::reiniciarVenda(bool manterRascunho)
{
    vendaFinalizada = false;
    // a venda terminou (ou foi abandonada): libera sessão e troca de operador
    Sessao_service::instancia()->definirVendaEmAndamento(false);
    modeloSelecionados->removeRows(0, modeloSelecionados->rowCount());
    temRemovido = false;
    desfazerTimer->stop();
    ui->Btn_DesfazerRemocao->hide();

    temRascunhoPendente = false;
    rascunhoPendente = RascunhoVendaDTO();
    idClienteAtual = -1;
    CLIENTE = ClienteDTO();

    atualizarListaCliente();
    definirClientePadrao();
    ui->DateEdt_Venda->setDateTime(QDateTime::currentDateTime());

    // pagamento volta ao padrão (Dinheiro, sem desconto)
    ui->CBox_FormaPagamento->setCurrentIndex(0);
    ui->CheckPorcentagem->setChecked(false);
    ui->Ledit_Desconto->setText("0");
    ui->Ledit_Taxa->setText("0");
    ui->Ledit_Recebido->setText("0");
    ui->Ledit_CpfCnpjCliente->clear();
    if (configDTO.emitNfFiscal) {
        ui->CBox_ModeloEmit->setCurrentIndex(0);
        ui->RadioBtn_EmitNfApenas->setChecked(true);
    }
    ui->Lbl_Total->setText(Total());

    // recarrega o catálogo (estoque atualizado) e volta para a busca
    ui->Ledit_Pesquisa->clear();
    on_Btn_Pesquisa_clicked();
    irParaPagina(0);
    ui->Ledit_Pesquisa->setFocus();

    // as alterações acima agendaram o salvamento do rascunho: a venda nova não herda nada
    rascunhoTimer->stop();
    if (!manterRascunho)
        descartarRascunho();
}

// ─── Products page ────────────────────────────────────────────────────────────

void venda::atualizarTotalProduto()
{
    for (int row = 0; row < modeloSelecionados->rowCount(); ++row) {
        float quantidade = portugues.toFloat(
            modeloSelecionados->data(modeloSelecionados->index(row, 1)).toString());
        double preco = portugues.toDouble(
            modeloSelecionados->data(modeloSelecionados->index(row, 3)).toString());
        modeloSelecionados->setData(
            modeloSelecionados->index(row, 4),
            portugues.toString(quantidade * preco, 'f', 2));
    }
}

void venda::atualizarListaCliente()
{
    clientesComId = cliServ.listarClientesParaCompleter();
    QCompleter *completer = ui->Ledit_Cliente->completer();
    if (completer) {
        QStringListModel *model = qobject_cast<QStringListModel *>(completer->model());
        if (model) model->setStringList(clientesComId);
    }
}

void venda::atualizarIndicadorOperador()
{
    const SessaoDTO sessao = Sessao_service::instancia()->sessao();
    ui->Lbl_Operador->setText(sessao.nomeOperador.isEmpty()
                                  ? QStringLiteral("Operador: —")
                                  : QStringLiteral("Operador: %1").arg(sessao.nomeOperador));
}

venda::~venda() {
    // o PDV pode ser fechado no meio do pagamento: sem isso a sessão ficaria travada
    Sessao_service::instancia()->definirVendaEmAndamento(false);
    Sessao_service::instancia()->removerTelaDeVenda(this);
    delete ui;
}

// ─── Rascunho ────────────────────────────────────────────────────────────────

void venda::salvarRascunho()
{
    if (modeloSelecionados->rowCount() == 0) {
        descartarRascunho();
        return;
    }

    RascunhoVendaSaveDTO dto;

    for (int row = 0; row < modeloSelecionados->rowCount(); ++row) {
        ProdutoVendidoDTO p;
        p.idProduto    = modeloSelecionados->item(row, 0)->text().toLongLong();
        p.quantidade   = portugues.toDouble(modeloSelecionados->item(row, 1)->text());
        p.descricao    = modeloSelecionados->item(row, 2)->text();
        p.precoVendido = portugues.toDouble(modeloSelecionados->item(row, 3)->text());
        dto.produtos.append(p);
    }

    // extrai ID direto do campo — idClienteAtual só é setado ao clicar "Próximo"
    auto [nomeIgnorado, idCli] = cliServ.extrairNomeId(ui->Ledit_Cliente->text());
    dto.idCliente           = idCli > 0 ? idCli : idClienteAtual;
    dto.cpfManual           = ui->Ledit_CpfCnpjCliente->text().trimmed();
    dto.dataHora            = ui->DateEdt_Venda->dateTime().toString("yyyy-MM-dd HH:mm:ss");
    dto.formaPagamento      = ui->CBox_FormaPagamento->currentText();
    dto.desconto            = ui->Ledit_Desconto->text();
    dto.taxa                = ui->Ledit_Taxa->text();
    dto.recebido            = ui->Ledit_Recebido->text();
    dto.descontoPorcentagem = ui->CheckPorcentagem->isChecked();
    dto.modeloNf            = ui->CBox_ModeloEmit->currentIndex();
    dto.emitirTodos         = ui->RadioBtn_EmitNfTodos->isChecked();

    rascunhoServ.salvar(dto);
}

void venda::descartarRascunho()
{
    rascunhoServ.descartar();
}

void venda::verificarRascunho()
{
    if (!rascunhoServ.existe()) return;

    RascunhoVendaDTO rascunho = rascunhoServ.carregar();
    QList<ProdutoVendidoDTO> produtos = rascunhoServ.carregarProdutos(rascunho.produtosJson);
    if (produtos.isEmpty()) { descartarRascunho(); return; }

    auto resp = QMessageBox::question(this, "Rascunho de venda",
        QString("Existe um rascunho com %1 produto(s) não finalizado.\n"
                "Deseja continuar de onde parou?").arg(produtos.size()),
        QMessageBox::Yes | QMessageBox::No);

    if (resp == QMessageBox::No) { descartarRascunho(); return; }

    // restaura produtos
    for (const ProdutoVendidoDTO &p : produtos) {
        QStandardItem *itemPreco = new QStandardItem();
        itemPreco->setData(p.precoVendido, Qt::EditRole);
        itemPreco->setText(portugues.toString(p.precoVendido, 'f', 2));

        QStandardItem *itemTotal = new QStandardItem();
        itemTotal->setData(p.quantidade * p.precoVendido, Qt::EditRole);
        itemTotal->setText(portugues.toString(p.quantidade * p.precoVendido, 'f', 2));

        modeloSelecionados->appendRow({
            new QStandardItem(QString::number(p.idProduto)),
            new QStandardItem(portugues.toString(p.quantidade, 'f', 2)),
            new QStandardItem(p.descricao),
            itemPreco,
            itemTotal
        });
    }

    // restaura cliente e data
    if (rascunho.idCliente > 0) {
        for (const QString &s : clientesComId) {
            if (s.contains(QString("(ID: %1)").arg(rascunho.idCliente))) {
                ui->Ledit_Cliente->setText(s);
                idClienteAtual = rascunho.idCliente;
                break;
            }
        }
    }

    if (!rascunho.dataHora.isEmpty())
        ui->DateEdt_Venda->setDateTime(QDateTime::fromString(rascunho.dataHora, "yyyy-MM-dd HH:mm:ss"));

    // pagamento e NF serão aplicados em configurarPaginaPagamento/configurarPaginaNF
    // para não serem sobrescritos pelos defaults dessas funções
    rascunhoPendente    = rascunho;
    temRascunhoPendente = true;

    ui->Lbl_Total->setText(Total());
}

qlonglong venda::validarCliente(bool mostrarMensagens)
{
    auto resultado = cliServ.validarClienteTexto(ui->Ledit_Cliente->text());
    if (!resultado.ok) {
        if (mostrarMensagens) {
            switch (resultado.erro) {
            case ClienteErro::CampoVazio:
                QMessageBox::warning(this, "Cliente", "Por favor informe o cliente.");
                ui->Ledit_Cliente->setFocus();
                break;
            case ClienteErro::InsercaoInvalida:
                QMessageBox::warning(this, "Cliente", "Cliente não encontrado.");
                ui->Ledit_Cliente->clear();
                ui->Ledit_Cliente->setFocus();
                break;
            case ClienteErro::QuebraDeRegra:
                QMessageBox::warning(this, "Cliente", "Nome não corresponde ao ID.");
                ui->Ledit_Cliente->selectAll();
                ui->Ledit_Cliente->setFocus();
                break;
            default:
                QMessageBox::warning(this, "Erro", resultado.msg);
            }
        }
        return -1;
    }
    if (!resultado.nomeCorrigido.isEmpty())
        ui->Ledit_Cliente->setText(resultado.nomeCorrigido);
    return resultado.clienteId;
}

void venda::on_Btn_SelecionarProduto_clicked()
{
    const QModelIndexList selecionados = ui->Tview_Produtos->selectionModel()->selectedRows();
    if (selecionados.isEmpty())
        return;

    const int row = selecionados.first().row();
    QAbstractItemModel *m = ui->Tview_Produtos->model();
    adicionarAoCarrinho(m->data(m->index(row, 0)).toLongLong(),
                        m->data(m->index(row, 2)).toString(),
                        m->data(m->index(row, 3)).toDouble(), 1);
}

int venda::inserirLinhaCarrinho(int posicao, qlonglong id, const QString &descricao,
                                double preco, double quantidade)
{
    QStandardItem *itemPreco = new QStandardItem();
    itemPreco->setData(preco, Qt::EditRole);
    itemPreco->setText(portugues.toString(preco, 'f', 2));

    QStandardItem *itemTotal = new QStandardItem();
    itemTotal->setData(quantidade * preco, Qt::EditRole);
    itemTotal->setText(portugues.toString(quantidade * preco, 'f', 2));

    posicao = qBound(0, posicao, modeloSelecionados->rowCount());
    modeloSelecionados->insertRow(posicao, {new QStandardItem(QString::number(id)),
                                            new QStandardItem(formatarQuantidade(portugues, quantidade)),
                                            new QStandardItem(descricao), itemPreco, itemTotal,
                                            new QStandardItem()});
    return posicao;
}

int venda::adicionarAoCarrinho(qlonglong id, const QString &descricao, double preco, double quantidade)
{
    int row = -1;
    for (int r = 0; r < modeloSelecionados->rowCount(); ++r) {
        if (modeloSelecionados->item(r, 0)->text().toLongLong() == id) {
            row = r;
            break;
        }
    }

    if (row >= 0) {
        const double atual = portugues.toDouble(modeloSelecionados->item(row, 1)->text());
        modeloSelecionados->item(row, 1)->setText(formatarQuantidade(portugues, atual + quantidade));
    } else {
        row = inserirLinhaCarrinho(modeloSelecionados->rowCount(), id, descricao, preco, quantidade);
    }

    ui->Lbl_Total->setText(Total());
    destacarItem(row);
    ui->Ledit_Pesquisa->clear();
    ui->Ledit_Pesquisa->setFocus();
    return row;
}

void venda::destacarItem(int row)
{
    if (row < 0 || row >= modeloSelecionados->rowCount())
        return;
    const QModelIndex idx = modeloSelecionados->index(row, 2);
    ui->Tview_ProdutosSelecionados->setCurrentIndex(idx);
    ui->Tview_ProdutosSelecionados->scrollTo(idx);
}

void venda::alterarQuantidade(int row, int delta)
{
    if (row < 0 || row >= modeloSelecionados->rowCount())
        return;
    const double atual = portugues.toDouble(modeloSelecionados->item(row, 1)->text());
    const double nova = atual + delta;
    if (nova <= 0) {
        removerItem(row);
        return;
    }
    modeloSelecionados->item(row, 1)->setText(formatarQuantidade(portugues, nova));
    ui->Lbl_Total->setText(Total());
    destacarItem(row);
}

void venda::removerItem(int row)
{
    if (row < 0 || row >= modeloSelecionados->rowCount())
        return;

    ultimoRemovido.id = modeloSelecionados->item(row, 0)->text().toLongLong();
    ultimoRemovido.descricao = modeloSelecionados->item(row, 2)->text();
    ultimoRemovido.quantidade = portugues.toDouble(modeloSelecionados->item(row, 1)->text());
    ultimoRemovido.preco = portugues.toDouble(modeloSelecionados->item(row, 3)->text());
    ultimoRemovido.linha = row;
    temRemovido = true;

    modeloSelecionados->removeRow(row);
    ui->Lbl_Total->setText(Total());
    if (modeloSelecionados->rowCount() > 0)
        destacarItem(qMin(row, modeloSelecionados->rowCount() - 1));

    const QString desc = ultimoRemovido.descricao.left(28);
    ui->Btn_DesfazerRemocao->setText(QString("Desfazer remoção: %1 (F8)").arg(desc));
    ui->Btn_DesfazerRemocao->show();
    desfazerTimer->start();
}

void venda::desfazerRemocao()
{
    if (!temRemovido)
        return;
    temRemovido = false;
    desfazerTimer->stop();
    ui->Btn_DesfazerRemocao->hide();

    const int row = inserirLinhaCarrinho(ultimoRemovido.linha, ultimoRemovido.id,
                                         ultimoRemovido.descricao, ultimoRemovido.preco,
                                         ultimoRemovido.quantidade);
    ui->Lbl_Total->setText(Total());
    destacarItem(row);
}

void venda::atualizarContagemItens()
{
    double unidades = 0;
    for (int r = 0; r < modeloSelecionados->rowCount(); ++r) {
        if (QStandardItem *it = modeloSelecionados->item(r, 1))
            unidades += portugues.toDouble(it->text());
    }
    ui->Lbl_QtdItens->setText(QString("%1 %2").arg(formatarQuantidade(portugues, unidades),
                                                   unidades == 1 ? "item" : "itens"));
}

bool venda::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == ui->Ledit_Pesquisa && event->type() == QEvent::KeyPress) {
        if (static_cast<QKeyEvent *>(event)->key() == Qt::Key_Down) {
            focarCatalogo();
            return true;
        }
    }
    if (obj == ui->Tview_Produtos && event->type() == QEvent::KeyPress) {
        if (static_cast<QKeyEvent *>(event)->key() == Qt::Key_Escape) {
            ui->Ledit_Pesquisa->setFocus();
            ui->Ledit_Pesquisa->selectAll();
            return true;
        }
    }
    if (obj == ui->Tview_ProdutosSelecionados && event->type() == QEvent::KeyPress) {
        const int key = static_cast<QKeyEvent *>(event)->key();
        const int row = ui->Tview_ProdutosSelecionados->currentIndex().row();
        if (key == Qt::Key_Plus) {
            alterarQuantidade(row, +1);
            return true;
        }
        if (key == Qt::Key_Minus) {
            alterarQuantidade(row, -1);
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void venda::atualizarBotaoSelecionar()
{
    ui->Btn_SelecionarProduto->setEnabled(ui->Tview_Produtos->selectionModel()->hasSelection());
}

void venda::configurarColunasCatalogo()
{
    // só Código de barras (4), Descrição (2), Preço (3) e Estoque (1);
    // esconde o resto sem mexer na query (o código lê as colunas por índice)
    QTableView *view = ui->Tview_Produtos;
    const int colunasOrigem = modeloProdutos->columnCount();
    const int colAcoes = view->model() ? view->model()->columnCount() - 1 : -1;
    const bool temAcoes = colAcoes >= colunasOrigem && colunasOrigem > 0;
    for (int col = 0; col < colunasOrigem; ++col)
        view->setColumnHidden(col, !(col == 1 || col == 2 || col == 3 || col == 4));
    if (temAcoes)
        view->setColumnHidden(colAcoes, false);

    QHeaderView *cabecalho = view->horizontalHeader();
    cabecalho->setStretchLastSection(false);
    if (cabecalho->count() > 4) {
        if (cabecalho->visualIndex(4) >= 0)
            cabecalho->moveSection(cabecalho->visualIndex(4), 0);
        if (cabecalho->visualIndex(1) >= 0)
            cabecalho->moveSection(cabecalho->visualIndex(1), cabecalho->count() - 1);
        if (temAcoes && cabecalho->visualIndex(colAcoes) >= 0)
            cabecalho->moveSection(cabecalho->visualIndex(colAcoes), cabecalho->count() - 1);
        cabecalho->setSectionResizeMode(2, QHeaderView::Stretch);
        cabecalho->setSectionResizeMode(1, QHeaderView::Fixed);
        cabecalho->setSectionResizeMode(3, QHeaderView::Fixed);
        cabecalho->setSectionResizeMode(4, QHeaderView::Fixed);
        if (temAcoes && colAcoes < cabecalho->count())
            cabecalho->setSectionResizeMode(colAcoes, QHeaderView::Fixed);
        cabecalho->resizeSection(1, 148);
        cabecalho->resizeSection(3, 100);
        cabecalho->resizeSection(4, 160);
        if (temAcoes && colAcoes < cabecalho->count())
            cabecalho->resizeSection(colAcoes, 88);
    }
}

void venda::focarCatalogo()
{
    if (modeloProdutos->rowCount() == 0)
        return;
    ui->Tview_Produtos->setFocus();
    ui->Tview_Produtos->selectRow(0);
    ui->Tview_Produtos->setCurrentIndex(ui->Tview_Produtos->model()->index(0, 2));
    ui->Tview_Produtos->scrollToTop();
}

void venda::selecionarPrimeiraLinhaCatalogo()
{
    configurarColunasCatalogo();
    if (modeloProdutos->rowCount() > 0)
        ui->Tview_Produtos->selectRow(0);
    atualizarBotaoSelecionar();
}

void venda::handleSelectionChange(const QItemSelection &, const QItemSelection &) {}

void venda::handleSelectionChangeProdutos(const QItemSelection &, const QItemSelection &)
{
    atualizarBotaoSelecionar();
}

void venda::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_F1) {
        irParaPagina(0);
        ui->Ledit_Pesquisa->setFocus();
        ui->Ledit_Pesquisa->selectAll();
    } else if (event->key() == Qt::Key_Escape) {
        // fora da página de produtos, Esc só volta para ela
        if (ui->stack_Pages->currentIndex() > 0)
            irParaPagina(0);
        else if (!ui->Chk_NovaVenda->isChecked() || modeloSelecionados->rowCount() > 0)
            ui->Btn_CancelarVenda->click();  // no modo contínuo, com carrinho vazio, Esc não fecha a tela
    } else if (event->key() == Qt::Key_F10) {
        finalizarRapido();
    } else if (ui->stack_Pages->currentIndex() == 2 && focusWidget() == ui->Btn_Aceitar &&
               (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)) {
        if (pagamentoValido())
            on_Btn_Aceitar_clicked();
    } else if (event->key() == Qt::Key_F4) {
        ui->Tview_Produtos->setFocus();
    } else if (event->key() == Qt::Key_F2) {
        irParaPagina(1);
        ui->Ledit_Cliente->setFocus();
        ui->Ledit_Cliente->selectAll();
    } else if (event->key() == Qt::Key_F3) {
        irParaPagina(1);
        ui->DateEdt_Venda->setFocus();
    } else if (event->key() == Qt::Key_F9) {
        ui->Tview_ProdutosSelecionados->setFocus();
    } else if (ui->Tview_Produtos->hasFocus() &&
               (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)) {
        ui->Btn_SelecionarProduto->click();
    } else if (ui->Tview_ProdutosSelecionados->hasFocus() && event->key() == Qt::Key_Delete) {
        deletarProd();
    }
    QWidget::keyPressEvent(event);
}

void venda::focusInEvent(QFocusEvent *event)
{
    QWidget::focusInEvent(event);
    ui->Ledit_Cliente->setReadOnly(false);
}

void venda::on_Btn_Pesquisa_clicked()
{
    prodServ.pesquisar(ui->Ledit_Pesquisa->text(), modeloProdutos);
    if (!modeloProdutos)
        QMessageBox::warning(this, "Erro", "Erro ao realizar a pesquisa.");
    selecionarPrimeiraLinhaCatalogo();
}

QList<ProdutoVendidoDTO> venda::obterProdutosSelecionados()
{
    QList<ProdutoVendidoDTO> lista;
    QLocale brasil(QLocale::Portuguese, QLocale::Brazil);
    for (int row = 0; row < modeloSelecionados->rowCount(); ++row) {
        ProdutoVendidoDTO prod;
        prod.idProduto   = modeloSelecionados->data(modeloSelecionados->index(row, 0)).toLongLong();
        prod.quantidade  = brasil.toDouble(modeloSelecionados->data(modeloSelecionados->index(row, 1)).toString());
        prod.descricao   = modeloSelecionados->data(modeloSelecionados->index(row, 2)).toString();
        prod.precoVendido = brasil.toDouble(modeloSelecionados->data(modeloSelecionados->index(row, 3)).toString());
        lista.append(prod);
    }
    return lista;
}

QString venda::Total()
{
    double totalValue = 0.0;
    for (int row = 0; row < modeloSelecionados->rowCount(); ++row) {
        float quantidade = portugues.toFloat(
            modeloSelecionados->data(modeloSelecionados->index(row, 1)).toString());
        double preco = portugues.toDouble(
            modeloSelecionados->data(modeloSelecionados->index(row, 3)).toString());
        totalValue += quantidade * preco;
    }
    return portugues.toString(totalValue, 'f', 2);
}

void venda::on_Ledit_Pesquisa_textChanged(const QString &)
{
    ui->Btn_Pesquisa->click();
}

void venda::on_Tview_ProdutosSelecionados_customContextMenuRequested(const QPoint &pos)
{
    if (!ui->Tview_ProdutosSelecionados->currentIndex().isValid()) return;
    QMenu menu(this);
    menu.addAction(actionMenuDeletarProd);
    menu.exec(ui->Tview_ProdutosSelecionados->viewport()->mapToGlobal(pos));
}

void venda::deletarProd()
{
    removerItem(ui->Tview_ProdutosSelecionados->currentIndex().row());
}

void venda::on_Ledit_Pesquisa_returnPressed()
{
    QString barras = ui->Ledit_Pesquisa->text().trimmed();

    // multiplicador: "3*7891234567890" adiciona 3 unidades
    double quantidade = 1;
    static const QRegularExpression multiplicador(QStringLiteral("^(\\d+(?:[.,]\\d+)?)\\s*\\*\\s*(.+)$"));
    const QRegularExpressionMatch m = multiplicador.match(barras);
    if (m.hasMatch()) {
        QString qtdTexto = m.captured(1);
        qtdTexto.replace(',', '.');
        quantidade = qtdTexto.toDouble();
        barras = m.captured(2).trimmed();
        if (quantidade <= 0) {
            QMessageBox::warning(this, "Erro", "A quantidade deve ser maior que zero.");
            return;
        }
    }

    if (barras.isEmpty())
        return;

    if (prodServ.codigoBarrasExiste(barras)) {
        ProdutoDTO prod = prodServ.getProdutoPeloCodBarras(barras);
        adicionarAoCarrinho(prod.id, prod.descricao, prod.preco, quantidade);
        return;
    }

    // só dígitos: é claramente um código de barras que não existe
    static const QRegularExpression soDigitos(QStringLiteral("^\\d+$"));
    if (soDigitos.match(barras).hasMatch()) {
        QMessageBox::warning(this, "Erro", "Esse código de barras não foi registrado ainda.");
        ui->Ledit_Pesquisa->selectAll();
        return;
    }

    // texto: pesquisa por nome; um único resultado é adicionado direto
    prodServ.pesquisar(barras, modeloProdutos);
    selecionarPrimeiraLinhaCatalogo();
    if (modeloProdutos->rowCount() == 1 && !modeloProdutos->canFetchMore()) {
        adicionarAoCarrinho(modeloProdutos->data(modeloProdutos->index(0, 0)).toLongLong(),
                            modeloProdutos->data(modeloProdutos->index(0, 2)).toString(),
                            modeloProdutos->data(modeloProdutos->index(0, 3)).toDouble(), quantidade);
    } else if (modeloProdutos->rowCount() > 0) {
        focarCatalogo();
    } else {
        ui->Ledit_Pesquisa->selectAll();
    }
}

void venda::on_Btn_CancelarVenda_clicked()
{
    const bool continuo = ui->Chk_NovaVenda->isChecked();
    if (modeloSelecionados->rowCount() > 0) {
        auto resp = QMessageBox::question(this, "Cancelar Venda",
            "Deseja salvar um rascunho para continuar esta venda depois?",
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        if (resp == QMessageBox::Cancel) return;
        if (resp == QMessageBox::No) {
            rascunhoTimer->stop();
            descartarRascunho();
        } else if (continuo) {
            rascunhoTimer->stop();
            salvarRascunho();
        }
        if (continuo) {
            reiniciarVenda(resp == QMessageBox::Yes);
            return;
        }
    }
    this->close();
}

void venda::selecionarClienteNovo()
{
    atualizarListaCliente();
    if (!clientesComId.isEmpty()) {
        ui->Ledit_Cliente->setText(clientesComId.last());
        int posFinalNome = clientesComId.last().indexOf(" (ID:");
        if (posFinalNome != -1)
            ui->Ledit_Cliente->setSelection(0, posFinalNome);
    }
}

void venda::on_Btn_NovoCliente_clicked()
{
    InserirCliente *inserirCliente = new InserirCliente;
    inserirCliente->setWindowModality(Qt::ApplicationModal);
    connect(inserirCliente, &InserirCliente::clienteInserido, this, &venda::selecionarClienteNovo);
    inserirCliente->show();
}

QString venda::getIdProdSelected()
{
    QItemSelectionModel *sel = ui->Tview_Produtos->selectionModel();
    QModelIndexList selectedIndexes = sel->selectedIndexes();
    if (!selectedIndexes.isEmpty()) {
        int row = selectedIndexes.first().row();
        return ui->Tview_Produtos->model()->data(
            ui->Tview_Produtos->model()->index(row, 0)).toString();
    }
    return {};
}

void venda::verProd()
{
    QString id = getIdProdSelected();
    InfoJanelaProd *janelaProd = new InfoJanelaProd(this, id);
    janelaProd->show();
}
