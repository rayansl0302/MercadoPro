#include "entradas.h"
#include "lercodigosnotadialog.h"
#include <QPushButton>
#include "ui_entradas.h"
#include "nota/acbrmanager.h"
#include "configuracao.h"
#include <QSqlQuery>
#include <QSqlQueryModel>
#include <QMessageBox>
#include <QFile>
#include <QDomDocument>
#include <QRegularExpression>
#include <QDateTime>
#include "util/manifestadordfe.h"
#include "subclass/leditdialog.h"
#include <qmenu.h>
#include "inserirproduto.h"
#include "configuracao.h"
#include <QFile>
#include <QDomDocument>
#include <QDebug>
#include "util/nfxmlutil.h"
#include "util/dbutil.h"
#include "mergeprodutos.h"
#include "delegatepago.h"
#include "util/mailmanager.h"
#include <QDir>
#include <QSqlError>
#include <QDebug>
#include "delegatehora.h"
#include "util/ibptutil.h"
#include "infra/apppath_service.h"
#include "util/chaveacessoutil.h"
#include "services/acbr_service.h"
#include <QFileDialog>
#include <QFileInfo>
#include <QTimer>
#include <QApplication>
#include "nota/DanfeUtil.h"

Entradas::Entradas(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Entradas)
{
    ui->setupUi(this);
    // filtros colados no topo (sem isso os rótulos se espalham pela altura da janela)
    ui->vl_filtros->setAlignment(Qt::AlignTop);
    Config_service *confServ = new Config_service(this);
    configDTO = confServ->carregarTudo();

    carregarTabela();
    connect(ui->Tview_Entradas->selectionModel(),
            &QItemSelectionModel::currentRowChanged,
            this,
            &Entradas::on_EntradaSelecionada);

    //delegate SIM VERDE NAO VERMELHO para coluna 'adicionado' da tabela produtos
    DelegatePago *delegateSimNao = new DelegatePago(this);
    ui->Tview_ProdutosNota->setItemDelegateForColumn(5, delegateSimNao);
    DelegateHora *delegateData = new DelegateHora(this);
    ui->Tview_Entradas->setItemDelegateForColumn(2, delegateData);

    QDate hoje = QDate::currentDate();
    QDate primeiroDia = QDate(hoje.year(), hoje.month(), 1).addMonths(-1);
    QDate ultimoDia = QDate(hoje.year(), hoje.month(), 1).addMonths(1).addDays(-1);

    ui->DateEdt_De->setDate(primeiroDia);
    ui->DateEdt_Ate->setDate(ultimoDia);

    connect(ui->Tview_ProdutosNota->selectionModel(),
            &QItemSelectionModel::selectionChanged,
            this,
            [this]() { atualizarBotoesAcao(); });
    // botão do assistente de leitura dos códigos de barras (as notas costumam vir "SEM GTIN")
    btnLerCodigos = new QPushButton("Ler códigos de barras...", this);
    btnLerCodigos->setToolTip("Leia com o leitor o código da embalagem de cada item da nota");
    ui->hl_itens_acoes->insertWidget(0, btnLerCodigos);
    connect(btnLerCodigos, &QPushButton::clicked, this, &Entradas::lerCodigosDosItens);

    connect(modelEntradas, &QSqlQueryModel::modelReset, this, [this]() {
        ui->Lbl_QtdNotas->setText(modelEntradas->rowCount() == 0
                                      ? "Nenhuma nota no período"
                                      : QString("%1 nota(s)").arg(modelEntradas->rowCount()));
    });

    ui->Tview_Entradas->selectRow(0);

    atualizarStatusBusca();
    atualizarResumoItens();
    atualizarBotoesAcao();
}

Entradas::~Entradas()
{
    delete ui;
}

void Entradas::on_Btn_ConsultarDF_clicked()
{
    // esta tela abre mesmo sem "Emitir Notas Fiscais": confere e prepara a ACBr para o DF-e
    auto pode = entradaNfeServ.verificarPodeBuscarPorChave();
    if (!pode.ok) {
        QMessageBox::warning(this, "Consulta DF-e", pode.msg);
        return;
    }
    Acbr_service acbrServ;
    auto cfg = acbrServ.configurarParaDFE();
    if (!cfg.ok) {
        QMessageBox::warning(this, "Consulta DF-e", cfg.msg);
        return;
    }

    ManifestadorDFe *manifestdfe = new ManifestadorDFe(this);
    if(dfeServ.possoConsultar()){

        manifestdfe->consultaAlternada();
        QMessageBox::information(this, "Resposta", "Consulta realizada com sucesso.");
        atualizarTabela();
    }else{
        QMessageBox::warning(this, "Aviso", "Não faz uma hora que a última consulta "
                                               "foi realizada, por favor espere.");
    }
    ui->Tview_Entradas->selectRow(0);
}

void Entradas::atualizarTabela(const QString &de, const QString &ate)
{
    notaServ.listarEntradas(modelEntradas, de, ate);
    ui->Tview_Entradas->resizeColumnsToContents();
    ui->Tview_Entradas->setColumnWidth(5, 200);
    ui->Tview_Entradas->selectRow(0);
}

void Entradas::carregarTabela()
{
    modelEntradas = new QSqlQueryModel(this);
    modelProdutosNota = new QSqlQueryModel(this);
    atualizarTabela("", "");

    modelEntradas->setHeaderData(0, Qt::Horizontal, "Emitente");
    modelEntradas->setHeaderData(1, Qt::Horizontal, "Valor NF");
    modelEntradas->setHeaderData(2, Qt::Horizontal, "Emissão");
    modelEntradas->setHeaderData(3, Qt::Horizontal, "CNPJ");
    modelEntradas->setHeaderData(4, Qt::Horizontal, "Modelo");
    modelEntradas->setHeaderData(5, Qt::Horizontal, "Chave");
    modelEntradas->setHeaderData(6, Qt::Horizontal, "CStat");

    ui->Tview_Entradas->setModel(modelEntradas);
    ui->Tview_Entradas->resizeColumnsToContents();
    ui->Tview_Entradas->horizontalHeader()->setStretchLastSection(false);
    ui->Tview_Entradas->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->Tview_Entradas->setColumnHidden(7, true); // Oculta id_nf

    ui->Tview_ProdutosNota->setModel(modelProdutosNota);
    ui->Tview_ProdutosNota->horizontalHeader()->setStretchLastSection(true);
    ui->Tview_ProdutosNota->setColumnHidden(0, true); // Oculta id
}

void Entradas::on_EntradaSelecionada(const QModelIndex &current, const QModelIndex &previous)
{
    if (!current.isValid())
        return;

    // Coluna onde está o id_nf (a 7ª do SELECT)
    int row = current.row();
    QModelIndex idIndex = current.model()->index(row, 7);

    id_nf_selec = current.model()->data(idIndex).toLongLong();

    carregarProdutosDaNota(id_nf_selec);
}

void Entradas::carregarProdutosDaNota(qlonglong id_nf)
{
    prodNotaServ.listarPorNota(modelProdutosNota, id_nf);
    ui->Tview_ProdutosNota->setColumnHidden(0, true);
    ui->Tview_ProdutosNota->resizeColumnsToContents();
    ui->Tview_ProdutosNota->horizontalHeader()->setStretchLastSection(false);
    ui->Tview_ProdutosNota->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    atualizarResumoItens();
    atualizarBotoesAcao();
}

QList<ProdutoNotaDTO> Entradas::produtosNotaSelecionados(bool *algumDevolvido)
{
    QList<ProdutoNotaDTO> prodSelecionados;
    if (algumDevolvido)
        *algumDevolvido = false;

    const QModelIndexList selecionadas = ui->Tview_ProdutosNota->selectionModel()->selectedRows();
    for (const QModelIndex &linha : selecionadas) {
        qlonglong id = ui->Tview_ProdutosNota->model()
        ->data(ui->Tview_ProdutosNota->model()->index(linha.row(), 0))
            .toLongLong();

        ProdutoNotaDTO prod = prodNotaServ.getProdutoNota(id);
        if(!prod.descricao.isEmpty()){
            prodSelecionados.append(prod);
        }
        if(algumDevolvido && prod.status == "DEVOLVIDO"){
            *algumDevolvido = true;
        }
    }
    return prodSelecionados;
}

void Entradas::atualizarResumoItens()
{
    if (id_nf_selec <= 0 || modelProdutosNota->rowCount() == 0) {
        ui->Lbl_ResumoItens->clear();
        return;
    }
    const ResumoEstoqueNota pend = estoqueNotaServ.contarPendentes(id_nf_selec);
    const int total = modelProdutosNota->rowCount();
    if (pend.pendentes == 0) {
        ui->Lbl_ResumoItens->setText(QString("· todos os %1 itens já estão no estoque").arg(total));
        return;
    }
    ui->Lbl_ResumoItens->setText(QString("· %1 de %2 itens ainda não estão no estoque")
                                     .arg(pend.pendentes).arg(total));
}

void Entradas::atualizarBotoesAcao()
{
    const bool temNota = id_nf_selec > 0 && modelProdutosNota->rowCount() > 0;
    const QModelIndexList selecionadas = ui->Tview_ProdutosNota->selectionModel()->selectedRows();
    const bool temSelecao = temNota && !selecionadas.isEmpty();

    ui->Btn_AdicionarEstoque->setEnabled(temSelecao && selecionadas.size() == 1);
    ui->Btn_AdicionarTodos->setEnabled(temNota && estoqueNotaServ.contarPendentes(id_nf_selec).pendentes > 0);
    ui->Btn_Devolucao->setEnabled(temSelecao);
    ui->Btn_VerDanfe->setEnabled(id_nf_selec > 0);
    if (btnLerCodigos)
        btnLerCodigos->setEnabled(temNota);
}

void Entradas::adicionarSelecionadoAoEstoque()
{
    const QList<ProdutoNotaDTO> prodSelecionados = produtosNotaSelecionados();
    if (prodSelecionados.isEmpty()) {
        QMessageBox::information(this, "Adicionar ao estoque", "Selecione um item da nota.");
        return;
    }
    const ProdutoNotaDTO &item = prodSelecionados.first();
    if (item.adicionado) {
        QMessageBox::information(this, "Adicionar ao estoque",
                                 "Este item já foi lançado no estoque.");
        return;
    }

    LeditDialog *barcodePage = new LeditDialog(this);
    barcodePage->setLabelText("Digite ou escaneie o código do produto selecionado:");
    if (EstoqueNota_service::ehGtinValido(item.codigoBarras))
        barcodePage->setLineEditText(item.codigoBarras.trimmed());
    barcodePage->show();

    if (barcodePage->exec() != QDialog::Accepted)
        return;

    QString codigoEscaneado = barcodePage->getLineEditText();

    if (prodServ.codigoBarrasExiste(codigoEscaneado) && !codigoEscaneado.isEmpty()) {
        QMessageBox::warning(this, "Aviso", "Já existe um produto com esse código cadastrado.");

        addProdComCodBarras(QString::number(item.id), codigoEscaneado);
    } else {
        addProdSemCodBarras(QString::number(item.id), codigoEscaneado);
    }
}

void Entradas::devolverSelecionados()
{
    bool jaDevolvido = false;
    QList<ProdutoNotaDTO> prodSelecionados = produtosNotaSelecionados(&jaDevolvido);
    if (prodSelecionados.isEmpty()) {
        QMessageBox::information(this, "Emitir devolução", "Selecione os itens que serão devolvidos.");
        return;
    }
    if (jaDevolvido) {
        QMessageBox::information(this, "Emitir devolução",
                                 "Há item selecionado que já foi devolvido.");
        return;
    }

    QMessageBox::StandardButton resposta = QMessageBox::question(
        this,
        "Confirmação",
        QString("Tem certeza que deseja emitir uma nota de devolução de "
                "%1 produto(s) selecionado(s)?")
            .arg(prodSelecionados.size()),
        QMessageBox::Yes | QMessageBox::No
        );
    if(resposta == QMessageBox::Yes){
        devolverProdutos(prodSelecionados);
    }
}

void Entradas::on_Btn_AdicionarEstoque_clicked()
{
    adicionarSelecionadoAoEstoque();
}

void Entradas::on_Btn_Devolucao_clicked()
{
    devolverSelecionados();
}

void Entradas::on_Btn_AdicionarTodos_clicked()
{
    if (id_nf_selec <= 0)
        return;

    const ResumoEstoqueNota pend = estoqueNotaServ.contarPendentes(id_nf_selec);
    if (pend.pendentes == 0) {
        QMessageBox::information(this, "Adicionar todos", "Todos os itens desta nota já estão no estoque.");
        return;
    }

    QString pergunta = QString("Lançar %1 item(ns) desta nota no estoque agora?\n\n"
                               "• %2 com código de barras: soma ao produto existente ou cadastra um novo.\n"
                               "• %3 sem código de barras: soma ao produto de mesma descrição ou cadastra "
                               "com um código interno gerado pelo sistema.\n\n"
                               "Preço de venda = custo da nota + %4% de lucro (Configurações).")
                           .arg(pend.pendentes).arg(pend.comCodigo).arg(pend.semCodigo)
                           .arg(configDTO.porcentLucroFinanceiro);
    if (QMessageBox::question(this, "Adicionar todos pendentes", pergunta,
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) != QMessageBox::Yes)
        return;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    const ResumoEstoqueNota r = estoqueNotaServ.adicionarPendentes(id_nf_selec);
    QApplication::restoreOverrideCursor();

    carregarProdutosDaNota(id_nf_selec);
    if (r.cadastrados + r.atualizados > 0)
        emit produtoAdicionado();

    QString resumo = QString("Produtos novos cadastrados: %1\nProdutos existentes atualizados: %2")
                         .arg(r.cadastrados).arg(r.atualizados);
    if (r.comCodigoInterno > 0)
        resumo += QString("\nCadastrados com código interno (sem GTIN na nota): %1").arg(r.comCodigoInterno);
    if (r.semNf > 0)
        resumo += QString("\nCadastrados sem NF (NCM inválido na nota; ajuste em Produtos): %1").arg(r.semNf);
    if (!r.falhas.isEmpty())
        resumo += QString("\n\nNão lançados (%1):\n").arg(r.falhas.size()) + r.falhas.join("\n");

    if (r.falhas.isEmpty())
        QMessageBox::information(this, "Adicionar todos pendentes", resumo);
    else
        QMessageBox::warning(this, "Adicionar todos pendentes", resumo);
}

void Entradas::on_Btn_VerDanfe_clicked()
{
    if (id_nf_selec <= 0)
        return;
    const NotaFiscalDTO nota = notaServ.getNotaById(id_nf_selec);
    const QString xmlPath = AppPath_service::resolverXmlPath(nota.xmlPath);
    try {
        DanfeUtil danfe(this);
        if (!danfe.abrirDanfePorXml(xmlPath))
            QMessageBox::warning(this, "Ver DANFE", "O XML desta nota não foi encontrado:\n" + xmlPath);
    } catch (const std::exception &e) {
        QMessageBox::warning(this, "Ver DANFE", QString("Não foi possível gerar o DANFE:\n%1").arg(e.what()));
    }
}

void Entradas::on_Tview_ProdutosNota_customContextMenuRequested(const QPoint &pos)
{
    QModelIndex index = ui->Tview_ProdutosNota->indexAt(pos);
    if (!index.isValid())
        return;

    bool jaDevolvido = false;
    const QList<ProdutoNotaDTO> prodSelecionados = produtosNotaSelecionados(&jaDevolvido);
    if (prodSelecionados.isEmpty())
        return;

    QMenu menu(this);
    QAction *adicionar = menu.addAction("Adicionar ao Estoque");
    QAction *devolucao = menu.addAction("Emitir Devolução");

    if (jaDevolvido) {
        devolucao->setEnabled(false);
    }

    QAction *selecionada = menu.exec(ui->Tview_ProdutosNota->viewport()->mapToGlobal(pos));
    if (!selecionada)
        return;

    if (selecionada == adicionar) {
        adicionarSelecionadoAoEstoque();
    } else if (selecionada == devolucao) {
        devolverSelecionados();
    }
}

void Entradas::devolverProdutos(QList<ProdutoNotaDTO> &produtosNota){
    NotaFiscalDTO notaRef = notaServ.getNotaById(id_nf_selec);
    if(notaRef.chNfe.isEmpty()){
        QMessageBox::warning(this, "Erro", "Nota fiscal de referência não encontrada.");
        return;
    }

    ClienteDTO cliente = clienteServ.getClienteByID(notaRef.idEmissorCliente);

    auto resultado = fiscalEmitterServ.enviarNfeDevolucaoEntrada(id_nf_selec, produtosNota, cliente);

    QMessageBox::information(this, "Aviso", resultado.msg);

    if(resultado.ok){
        carregarProdutosDaNota(id_nf_selec);
    }
}

void Entradas::enviarEmailNFe(QString nomeCliente, QString emailCliente,
                                    QString xmlPath, std::string pdfDanfe, QString cnpj){

    try {

        QDateTime data = QDateTime::currentDateTime();

        auto mail = MailManager::instance().mail();
        QByteArray pdfBytes = QByteArray::fromBase64(
            QByteArray::fromStdString(pdfDanfe)
            );
        QString pdfPath =
            QDir::tempPath() + "/DANFE_" +
            QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") +
            ".pdf";

        QFile pdfFile(pdfPath);
        if (!pdfFile.open(QIODevice::WriteOnly)) {
            qDebug() << "Erro ao criar PDF DANFE";
            return;
        }
        pdfFile.write(pdfBytes);
        pdfFile.close();

        QString corpo;
        QString nomeEmpresa = configDTO.nomeEmpresa;
        QString dataFormatada = portugues.toString(
            data,
            "dddd, dd 'de' MMMM 'de' yyyy 'às' HH:mm"
            );

        corpo = "Olá " + nomeCliente + "\n\n"
            "Foi emitido uma Nota Fiscal de Devolução de " + nomeEmpresa + " para o CNPJ:" +
               cnpj +
                "\n\nem anexo, você encontrará os arquivos referentes à "
                                "Nota Fiscal de " +
                dataFormatada + ".\n\n"
                                "Cordialmente,\n\n" +
                nomeEmpresa;
        mail->Limpar();
        mail->LimparAnexos();
        mail->AddCorpoAlternativo(corpo.toStdString());
        mail->SetAssunto("Nota Fiscal Eletrônica de " + configDTO.nomeEmpresa.toStdString());
        mail->AddDestinatario(emailCliente.toStdString());
        mail->AddAnexo(xmlPath.toStdString(), "XML NFe", 0);
        mail->AddAnexo(pdfPath.toStdString(), "DANFE (PDF)", 0);

        mail->Enviar();
        qDebug() << "email enviado NFE";
    }
    catch (const std::exception& e) {
        qDebug() << "email não enviado NFE";
    }
}

// Abre o assistente que lê, com o leitor, o código de barras da embalagem de cada item da nota
void Entradas::lerCodigosDosItens()
{
    if (id_nf_selec <= 0 || modelProdutosNota->rowCount() == 0) {
        QMessageBox::information(this, "Ler códigos de barras", "Selecione uma nota que tenha itens.");
        return;
    }
    LerCodigosNotaDialog dlg(id_nf_selec, this);
    dlg.exec();
    carregarProdutosDaNota(id_nf_selec);   // a lista mostra os códigos novos e o resumo é recalculado
}

void Entradas::addProdSemCodBarras(QString idProd, QString codBarras){
    qlonglong id = idProd.toLongLong();

    ProdutoNotaDTO prod = prodNotaServ.getProdutoNota(id);
    QString xml_path = AppPath_service::resolverXmlPath(prodNotaServ.getXmlPathPorId(id));

    NfXmlUtil *nfutil = new NfXmlUtil(this);
    CustoItem custoxml = nfutil->calcularCustoItemSN(xml_path, prod.nitem);

    qDebug() << "custo fornecedor taxas incluidas:" << custoxml.custoUnitario;
    qDebug() << "Preço fornecedor " << custoxml.precoUnitarioNota;

    ProdutoDTO novoProd;
    novoProd.quantidade      = prod.quantidade;
    novoProd.descricao       = prod.descricao;
    novoProd.codigoBarras    = codBarras;
    novoProd.nf              = true;
    novoProd.uCom            = prod.uCom;
    novoProd.precoFornecedor = custoxml.custoUnitario;
    novoProd.percentLucro    = configDTO.porcentLucroFinanceiro;
    novoProd.ncm             = prod.ncm;
    novoProd.csosn           = configDTO.csosnPadraoProduto;
    novoProd.pis             = configDTO.pisPadraoProduto;


    InserirProduto *addProd = new InserirProduto();
    addProd->preencherCamposProduto(novoProd);
    addProd->show();

    connect(addProd, &InserirProduto::produtoInserido, this,
            &Entradas::produtoAdicionado);

    connect(addProd, &InserirProduto::produtoInserido, this,
            [=]() {
                atualizarProdutoNotaAdicionado(idProd);
            });

    connect(addProd, &InserirProduto::produtoInserido, this,
            [=]() {
                carregarProdutosDaNota(id_nf_selec);
            });
}

void Entradas::atualizarProdutoNotaAdicionado(QString idProd){
    if(!prodNotaServ.marcarComoAdicionado(idProd.toLongLong())){
        qDebug() << "atualizarProdutoNotaAdicionado falhou para id:" << idProd;
        return;
    }
    qDebug() << "query update produto nota adicionado = 1 ok";
}


void Entradas::addProdComCodBarras(QString idProd, QString codBarras){
    qDebug() << idProd << codBarras;

    QVariantMap produto = prodServ.getProdutoPorCodBarrasMap(codBarras);
    if (produto.isEmpty()) {
        qDebug() << "Produto não encontrado";
        return;
    }

    QVariantMap produtoNota = prodNotaServ.getProdutoNotaComXmlPath(idProd.toLongLong());
    if (produtoNota.isEmpty()) {
        qDebug() << "ProdutoNota não encontrado";
        return;
    }

    NfXmlUtil *nfutil = new NfXmlUtil(this);
    CustoItem custoxml = nfutil->calcularCustoItemSN(AppPath_service::resolverXmlPath(produtoNota["xml_path"].toString()), produtoNota["nitem"].toInt());

    produtoNota["preco"] = custoxml.custoUnitario;

    // comparar os campos do produtoNota e produto e pegar a diferença
    QVariantMap resultado;

    resultado["quantidade"] = produto["quantidade"].toDouble() + produtoNota["quantidade"].toFloat();

    if (produto["descricao"].toString() != produtoNota["descricao"].toString()) {
        if (produto["descricao"].toString() == "") {
            resultado["descricao"] = produtoNota["descricao"].toString();
        }
        else {
            resultado["descricao"] = produto["descricao"].toString();
        }
    }

    if (produto["preco_fornecedor"].toDouble() != produtoNota["preco"].toDouble()) {
        double porcent_lucro = configDTO.porcentLucroFinanceiro;
        resultado["preco"] = produtoNota["preco"].toDouble() * (porcent_lucro/100 + 1);
        resultado["porcent_lucro"] = porcent_lucro;
        resultado["preco_fornecedor"] = produtoNota["preco"].toDouble();
    }

    if (produto["un_comercial"].toString() != produtoNota["un_comercial"].toString()) {
        if (produto["un_comercial"].toString() == "") {
            resultado["un_comercial"] = produtoNota["un_comercial"].toString();
        }
        else {
            resultado["un_comercial"] = produto["un_comercial"].toString();
        }
    }

    resultado["ncm"] =
        produto["ncm"].toString().isEmpty() || produto["ncm"] == "00000000"
            ? produtoNota["ncm"].toString()
            : produto["ncm"].toString();

    qDebug() << "teste antes: " << resultado["ncm"];

    if (resultado.contains("ncm")) {
        IbptUtil *util = new IbptUtil(this);
        resultado["aliquota_imposto"] = util->get_Aliquota_From_Csv(resultado["ncm"].toString());
    }

    qDebug() << "produto: " << produto;

    qDebug() << "produtoNota: " << produtoNota;

    qDebug() << "resultado: " << resultado;

    qDebug() << "teste: " << (produto["ncm"].toString() != produtoNota["ncm"].toString());

    MergeProdutos *janelaMerge = new MergeProdutos(produto, produtoNota, resultado);
    janelaMerge->show();

    connect(janelaMerge, &MergeProdutos::produtoAtualizado, this,
            [=]() {
                atualizarProdutoNotaAdicionado(idProd);
            });
    connect(janelaMerge, &MergeProdutos::produtoAtualizado, this,
            &Entradas::produtoAdicionado);
    connect(janelaMerge, &MergeProdutos::produtoAtualizado, this,
            [=]() {
                carregarProdutosDaNota(id_nf_selec);
            });

}

void Entradas::on_DateEdt_De_userDateChanged(const QDate &date)
{
    QString de = date.toString("yyyy-MM-dd");
    QString ate = ui->DateEdt_Ate->date().addDays(1).toString("yyyy-MM-dd");
    atualizarTabela(de, ate);
}


void Entradas::on_DateEdt_Ate_userDateChanged(const QDate &date)
{
    QString de = ui->DateEdt_De->date().toString("yyyy-MM-dd");
    QString ate = date.addDays(1).toString("yyyy-MM-dd");
    atualizarTabela(de, ate);
}

// ─── Entrada pela nota do fornecedor ─────────────────────────────────────────

void Entradas::atualizarStatusBusca()
{
    // avisa, já ao abrir, o que falta para buscar pela chave (importar XML sempre funciona)
    auto pode = entradaNfeServ.verificarPodeBuscarPorChave();
    ui->Lbl_StatusChave->setVisible(!pode.ok);
    ui->Lbl_StatusChave->setText(pode.msg);
}

void Entradas::definirBuscando(bool buscando)
{
    emBusca = buscando;
    ui->Ledit_ChaveAcesso->setEnabled(!buscando);
    ui->Btn_BuscarChave->setEnabled(!buscando);
    ui->Btn_ImportarXml->setEnabled(!buscando);
    ui->Btn_ConsultarDF->setEnabled(!buscando);
    if (buscando)
        QApplication::setOverrideCursor(Qt::WaitCursor);
    else
        QApplication::restoreOverrideCursor();
}

void Entradas::selecionarNotaPorId(qlonglong idNota)
{
    if (idNota <= 0)
        return;
    while (modelEntradas->canFetchMore())
        modelEntradas->fetchMore();
    for (int row = 0; row < modelEntradas->rowCount(); ++row) {
        if (modelEntradas->data(modelEntradas->index(row, 7)).toLongLong() == idNota) {
            ui->Tview_Entradas->selectRow(row);
            ui->Tview_Entradas->scrollTo(modelEntradas->index(row, 0));
            return;
        }
    }
}

void Entradas::on_Ledit_ChaveAcesso_textChanged(const QString &texto)
{
    // o leitor de código de barras digita os 44 dígitos de uma vez
    if (!emBusca && ChaveAcessoUtil::somenteDigitos(texto).size() == 44)
        QTimer::singleShot(0, this, &Entradas::buscarChave);
}

void Entradas::on_Ledit_ChaveAcesso_returnPressed()
{
    buscarChave();
}

void Entradas::on_Btn_BuscarChave_clicked()
{
    buscarChave();
}

void Entradas::buscarChave()
{
    if (emBusca)
        return;
    const QString texto = ui->Ledit_ChaveAcesso->text();
    if (texto.trimmed().isEmpty())
        return;

    const ChaveAcessoInfo info = ChaveAcessoUtil::analisar(texto);
    if (!info.valida) {
        QMessageBox::warning(this, "Chave de acesso", info.erro);
        ui->Ledit_ChaveAcesso->setFocus();
        ui->Ledit_ChaveAcesso->selectAll();
        return;
    }

    definirBuscando(true);
    QCoreApplication::processEvents();
    const auto r = entradaNfeServ.buscarPorChave(info.chave);
    definirBuscando(false);

    if (r.ok || r.erro == EntradaNfeErro::Duplicada) {
        atualizarTabela();
        selecionarNotaPorId(r.idNota);
        ui->Ledit_ChaveAcesso->clear();
        ui->Ledit_ChaveAcesso->setFocus();
        if (r.ok) {
            QMessageBox::information(this, "NF-e encontrada",
                "A nota foi buscada na SEFAZ e está selecionada na lista.\n"
                "Use \"Adicionar todos pendentes\" ou selecione um item e clique em \"Adicionar ao estoque\".");
        } else {
            QMessageBox::information(this, "NF-e já lançada",
                "Esta NF-e já estava em Compras. Ela foi selecionada na lista, sem duplicar a entrada.");
        }
        return;
    }

    if (r.erro == EntradaNfeErro::AguardandoXml)
        QMessageBox::information(this, "Aguardando a SEFAZ", r.msg);
    else
        QMessageBox::warning(this, "Buscar NF-e pela chave", r.msg);
    ui->Ledit_ChaveAcesso->setFocus();
    ui->Ledit_ChaveAcesso->selectAll();
    atualizarStatusBusca();
}

void Entradas::on_Btn_ImportarXml_clicked()
{
    const QStringList arquivos = QFileDialog::getOpenFileNames(
        this, "Importar XML de NF-e", ultimaPastaXml, "XML de NF-e (*.xml);;Todos os arquivos (*)");
    if (arquivos.isEmpty())
        return;
    ultimaPastaXml = QFileInfo(arquivos.first()).absolutePath();

    int importadas = 0, jaLancadas = 0, comProblema = 0;
    QStringList detalhes;
    qlonglong ultimoId = 0;

    for (const QString &arquivo : arquivos) {
        const QString nome = QFileInfo(arquivo).fileName();
        auto r = entradaNfeServ.importarXml(arquivo);

        if (!r.ok && r.erro == EntradaNfeErro::DestinatarioDiferente) {
            auto resp = QMessageBox::question(this, "Destinatário diferente",
                nome + "\n\n" + r.msg + "\n\nImportar mesmo assim?",
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (resp == QMessageBox::Yes)
                r = entradaNfeServ.importarXml(arquivo, /*ignorarDestinatario=*/true);
        }

        if (r.ok) {
            ++importadas;
            ultimoId = r.idNota;
        } else if (r.erro == EntradaNfeErro::Duplicada) {
            ++jaLancadas;
            ultimoId = r.idNota;
            detalhes << nome + ": já estava lançada";
        } else {
            ++comProblema;
            detalhes << nome + ": " + r.msg;
        }
    }

    atualizarTabela();
    selecionarNotaPorId(ultimoId);

    QString resumo = QString("Importadas: %1\nJá lançadas (ignoradas): %2\nNão importadas: %3")
                         .arg(importadas).arg(jaLancadas).arg(comProblema);
    if (!detalhes.isEmpty())
        resumo += "\n\n" + detalhes.join("\n");
    if (comProblema > 0)
        QMessageBox::warning(this, "Importar XML", resumo);
    else
        QMessageBox::information(this, "Importar XML", resumo);
}
