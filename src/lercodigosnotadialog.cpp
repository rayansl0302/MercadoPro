#include "lercodigosnotadialog.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include "services/estoquenota_service.h"
#include "util/lanches.h"

namespace {
const QString kSemGtin = QStringLiteral("SEM GTIN");

bool temCodigo(const ProdutoNotaDTO &p)
{
    return EstoqueNota_service::ehGtinValido(p.codigoBarras);
}
}

LerCodigosNotaDialog::LerCodigosNotaDialog(qlonglong idNota, QWidget *parent)
    : QDialog(parent), idNf(idNota)
{
    setWindowTitle("Ler códigos de barras dos itens");
    setModal(true);
    resize(900, 640);

    auto *titulo = new QLabel("Ler os códigos de barras da embalagem");
    titulo->setStyleSheet("font-size: 18pt; font-weight: 700; color: #142D45;");
    auto *explica = new QLabel("A nota do fornecedor costuma vir sem o código de barras (\"SEM GTIN\"). Aponte o leitor para a "
                               "embalagem de cada produto: o código é gravado no item e o próximo item sem código aparece sozinho. "
                               "Itens sem código de barras (a granel) podem ser pulados: o sistema gera um código interno depois.");
    explica->setWordWrap(true);
    explica->setStyleSheet("color: #5B6B7F;");

    lblProgresso = new QLabel;
    lblProgresso->setStyleSheet("color: #107A2E; font-weight: 600;");
    lblDescricao = new QLabel;
    lblDescricao->setWordWrap(true);
    lblDescricao->setStyleSheet("font-size: 16pt; font-weight: 700; color: #142D45;");
    lblDetalhe = new QLabel;
    lblDetalhe->setStyleSheet("color: #5B6B7F;");
    lblAviso = new QLabel;
    lblAviso->setWordWrap(true);
    lblAviso->setTextFormat(Qt::RichText);

    edCodigo = new QLineEdit;
    edCodigo->setPlaceholderText("Aponte o leitor para o código de barras da embalagem e pressione Enter");
    edCodigo->setStyleSheet("font-size: 14pt; padding: 8px;");
    connect(edCodigo, &QLineEdit::returnPressed, this, &LerCodigosNotaDialog::salvarEAvancar);

    btnAnterior = new QPushButton("← Anterior");
    btnPular = new QPushButton("Pular →");
    btnLimpar = new QPushButton("Remover o código lido");
    auto *btnSalvar = new QPushButton("Gravar e próximo (Enter)");
    auto *btnConcluir = new QPushButton("Concluir");
    const QString estiloClaro = "QPushButton { background: white; color: #142D45; border: 1px solid #C9D3DF; }"
                                "QPushButton:hover { background: #EAF6EC; }";
    for (QPushButton *b : {btnAnterior, btnPular, btnLimpar, btnConcluir})
        b->setStyleSheet(estiloClaro);
    connect(btnAnterior, &QPushButton::clicked, this, [this]() { irPara(qMax(0, atual - 1)); });
    connect(btnPular, &QPushButton::clicked, this, [this]() { irPara(qMin(int(itens.size()) - 1, atual + 1)); });
    connect(btnLimpar, &QPushButton::clicked, this, [this]() {
        if (atual < 0)
            return;
        if (prodNotaServ.atualizarCodigoBarras(itens[atual].id, kSemGtin)) {
            itens[atual].codigoBarras = kSemGtin;
            preencherLinha(atual);
            atualizarProgresso();
            irPara(atual);
        }
    });
    connect(btnSalvar, &QPushButton::clicked, this, &LerCodigosNotaDialog::salvarEAvancar);
    connect(btnConcluir, &QPushButton::clicked, this, &QDialog::accept);

    tabela = new QTableWidget(0, 4);
    tabela->setHorizontalHeaderLabels({"#", "Produto", "Qtd", "Código de barras"});
    tabela->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tabela->setSelectionBehavior(QAbstractItemView::SelectRows);
    tabela->setSelectionMode(QAbstractItemView::SingleSelection);
    tabela->verticalHeader()->setVisible(false);
    tabela->setAlternatingRowColors(true);
    tabela->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    tabela->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    tabela->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    tabela->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    connect(tabela, &QTableWidget::cellClicked, this, [this](int linha, int) { irPara(linha); });

    auto *navegacao = new QHBoxLayout;
    navegacao->addWidget(btnAnterior);
    navegacao->addWidget(btnPular);
    navegacao->addWidget(btnLimpar);
    navegacao->addStretch(1);
    navegacao->addWidget(btnSalvar);

    auto *rodape = new QHBoxLayout;
    rodape->addStretch(1);
    rodape->addWidget(btnConcluir);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 16, 20, 16);
    layout->setSpacing(10);
    layout->addWidget(titulo);
    layout->addWidget(explica);
    layout->addWidget(lblProgresso);
    layout->addWidget(lblDescricao);
    layout->addWidget(lblDetalhe);
    layout->addWidget(edCodigo);
    layout->addWidget(lblAviso);
    layout->addLayout(navegacao);
    layout->addWidget(tabela, 1);
    layout->addLayout(rodape);

    carregar();
}

void LerCodigosNotaDialog::carregar()
{
    itens.clear();
    for (const ProdutoNotaDTO &p : prodNotaServ.listarDtoPorNota(idNf))
        if (!p.adicionado && p.status != "DEVOLVIDO" && !p.descricao.trimmed().isEmpty())
            itens.append(p);

    tabela->setRowCount(itens.size());
    for (int i = 0; i < itens.size(); ++i)
        preencherLinha(i);
    atualizarProgresso();

    if (itens.isEmpty()) {
        lblDescricao->setText("Nenhum item pendente nesta nota.");
        edCodigo->setEnabled(false);
        return;
    }
    const int primeiro = proximoSemCodigo(0);
    irPara(primeiro >= 0 ? primeiro : 0);
}

void LerCodigosNotaDialog::preencherLinha(int i)
{
    const ProdutoNotaDTO &p = itens.at(i);
    tabela->setItem(i, 0, new QTableWidgetItem(QString::number(p.nitem)));
    tabela->setItem(i, 1, new QTableWidgetItem(p.descricao));
    auto *qtd = new QTableWidgetItem(QString::number(p.quantidade, 'f', 2) + " " + p.uCom);
    qtd->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    tabela->setItem(i, 2, qtd);
    const bool ok = temCodigo(p);
    auto *cod = new QTableWidgetItem(ok ? QStringLiteral("✓ ") + p.codigoBarras.trimmed() : QStringLiteral("— sem código"));
    cod->setForeground(ok ? QColor("#107A2E") : QColor("#B45309"));
    tabela->setItem(i, 3, cod);
}

void LerCodigosNotaDialog::atualizarProgresso()
{
    int sem = 0;
    for (const ProdutoNotaDTO &p : itens)
        if (!temCodigo(p))
            ++sem;
    lblProgresso->setText(QStringLiteral("%1 de %2 itens já têm código de barras · faltam %3")
                              .arg(int(itens.size()) - sem).arg(itens.size()).arg(sem));
}

int LerCodigosNotaDialog::proximoSemCodigo(int aPartirDe) const
{
    for (int i = qMax(0, aPartirDe); i < itens.size(); ++i)
        if (!temCodigo(itens.at(i)))
            return i;
    return -1;
}

void LerCodigosNotaDialog::irPara(int indice)
{
    if (indice < 0 || indice >= itens.size())
        return;
    atual = indice;
    const ProdutoNotaDTO &p = itens.at(indice);
    lblDescricao->setText(p.descricao);
    lblDetalhe->setText(QStringLiteral("Item %1 da nota · %2 %3 · R$ %4 a unidade")
                            .arg(p.nitem).arg(QString::number(p.quantidade, 'f', 2), p.uCom, QString::number(p.preco, 'f', 2)));
    edCodigo->setText(temCodigo(p) ? p.codigoBarras.trimmed() : QString());
    lblAviso->clear();
    // se o código já existe no cadastro, o estoque será somado ao produto existente
    if (temCodigo(p) && prodServ.codigoBarrasExiste(p.codigoBarras.trimmed())) {
        const ProdutoDTO existente = prodServ.getProdutoPeloCodBarras(p.codigoBarras.trimmed());
        lblAviso->setText(QStringLiteral("<span style='color:#107A2E'>Já cadastrado como <b>%1</b> (estoque %2): ao lançar a nota, o estoque dele é somado.</span>")
                              .arg(existente.descricao.toHtmlEscaped()).arg(existente.quantidade));
    }
    btnAnterior->setEnabled(indice > 0);
    btnPular->setEnabled(indice < itens.size() - 1);
    btnLimpar->setEnabled(temCodigo(p));
    tabela->selectRow(indice);
    edCodigo->setFocus();
    edCodigo->selectAll();
}

bool LerCodigosNotaDialog::confirmar(const QString &titulo, const QString &texto)
{
    return QMessageBox::question(this, titulo, texto, QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes;
}

void LerCodigosNotaDialog::salvarEAvancar()
{
    if (atual < 0 || atual >= itens.size())
        return;
    const QString codigo = edCodigo->text().trimmed();
    if (codigo.isEmpty()) {            // Enter sem ler nada: só passa para o próximo
        irPara(qMin(int(itens.size()) - 1, atual + 1));
        return;
    }
    if (codigo == itens[atual].codigoBarras.trimmed()) { // já estava gravado
        const int prox = proximoSemCodigo(atual + 1);
        irPara(prox >= 0 ? prox : qMin(int(itens.size()) - 1, atual + 1));
        return;
    }

    // 1) parece um código de barras de produto (EAN/GTIN com dígito verificador certo)?
    if (!EstoqueNota_service::ehGtinValido(codigo)) {
        if (!confirmar("Código estranho",
                       QStringLiteral("\"%1\" não parece um código de barras de produto (8, 12, 13 ou 14 números).\n\nUsar mesmo assim?").arg(codigo)))
            return;
    } else if (!EstoqueNota_service::digitoVerificadorGtinConfere(codigo)) {
        if (!confirmar("Dígito verificador não confere",
                       QStringLiteral("O código %1 não passa na conferência do dígito verificador. Pode ter sido lido errado.\n\nUsar mesmo assim?").arg(codigo)))
            return;
    }
    // 2) faixa dos lanches (200 a 500): o estoque deles não é controlado
    if (Lanches::codigoEhLanche(codigo, Lanches::faixaConfigurada())) {
        if (!confirmar("Código da faixa de lanches",
                       QStringLiteral("O código %1 está na faixa dos lanches. Produtos com esse código ficam sem controle de estoque.\n\nÉ mesmo um lanche?").arg(codigo)))
            return;
    }
    // 3) o mesmo código já foi lido para outro item desta nota?
    for (int i = 0; i < itens.size(); ++i) {
        if (i != atual && itens.at(i).codigoBarras.trimmed() == codigo) {
            if (!confirmar("Código repetido na nota",
                           QStringLiteral("Esse código já foi lido para \"%1\".\n\nUsar o mesmo código neste item também?").arg(itens.at(i).descricao)))
                return;
            break;
        }
    }

    if (!prodNotaServ.atualizarCodigoBarras(itens[atual].id, codigo)) {
        QMessageBox::warning(this, "Código de barras", "Não foi possível gravar o código neste item.");
        return;
    }
    itens[atual].codigoBarras = codigo;
    ++gravados;
    preencherLinha(atual);
    atualizarProgresso();

    const int prox = proximoSemCodigo(atual + 1);
    if (prox >= 0) {
        irPara(prox);
    } else if (proximoSemCodigo(0) >= 0) {
        irPara(proximoSemCodigo(0));   // ainda faltam itens anteriores
    } else {
        lblAviso->setText("<span style='color:#107A2E'><b>Todos os itens têm código de barras.</b> Clique em Concluir.</span>");
        edCodigo->clear();
    }
}
