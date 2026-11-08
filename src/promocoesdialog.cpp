#include "promocoesdialog.h"

#include "dto/Produto_dto.h"
#include "services/Produto_service.h"
#include "services/config_service.h"
#include "services/precotaxa_service.h"
#include "services/promocao_service.h"
#include "util/etiquetaspreco.h"
#include "util/postpromocao.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QDateEdit>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardPaths>
#include <QTableWidget>
#include <QUrl>
#include <QVBoxLayout>

namespace {
const QLocale ptBR(QLocale::Portuguese, QLocale::Brazil);

QString reais(double v) { return ptBR.toString(v, 'f', 2); }

// "3,99", "R$ 3.99", "1.234,50" -> número; NaN se não for número
double lerPreco(QString t)
{
    t.remove(QStringLiteral("R$")).remove(' ');
    if (t.contains(','))
        t.remove('.').replace(',', '.');
    bool ok = false;
    const double v = t.toDouble(&ok);
    return ok ? v : qQNaN();
}

// Formulário de uma promoção: escolhe o produto (ou vem fixo), o preço "por" da gôndola e até quando vale.
class FormPromocao : public QDialog
{
public:
    FormPromocao(QWidget *parent, const QList<ProdutoDTO> &candidatos, const PromocaoDTO *existente)
        : QDialog(parent), lista(candidatos)
    {
        setWindowTitle(existente ? QStringLiteral("Alterar promoção") : QStringLiteral("Nova promoção"));
        resize(520, existente ? 330 : 560);
        auto *lay = new QVBoxLayout(this);

        if (existente) {
            idFixo = existente->idProduto;
            auto *nome = new QLabel(QStringLiteral("<b>%1</b>").arg(existente->descricao.toHtmlEscaped()), this);
            nome->setWordWrap(true);
            lay->addWidget(nome);
            precoDeBase = existente->precoDe;
        } else {
            busca = new QLineEdit(this);
            busca->setPlaceholderText(QStringLiteral("Buscar produto por nome ou código de barras..."));
            busca->setClearButtonEnabled(true);
            lay->addWidget(busca);
            lw = new QListWidget(this);
            for (int i = 0; i < lista.size(); ++i) {
                auto *it = new QListWidgetItem(QStringLiteral("%1   —   R$ %2").arg(lista.at(i).descricao, reais(PrecoTaxa::comTaxa(lista.at(i).preco))), lw);
                it->setData(Qt::UserRole, i);
                it->setData(Qt::UserRole + 1, (lista.at(i).descricao + ' ' + lista.at(i).codigoBarras).toLower());
            }
            lay->addWidget(lw, 1);
            connect(busca, &QLineEdit::textChanged, this, [this](const QString &t) {
                const QString termo = t.trimmed().toLower();
                for (int i = 0; i < lw->count(); ++i)
                    lw->item(i)->setHidden(!termo.isEmpty() && !lw->item(i)->data(Qt::UserRole + 1).toString().contains(termo));
            });
            connect(lw, &QListWidget::currentRowChanged, this, [this]() { atualizar(); });
        }

        info = new QLabel(this);
        info->setWordWrap(true);
        lay->addWidget(info);

        lay->addWidget(new QLabel(QStringLiteral("Preço da promoção na gôndola (\"por\"):"), this));
        por = new QLineEdit(this);
        por->setPlaceholderText(QStringLiteral("Ex.: 3,99"));
        if (existente)
            por->setText(reais(PrecoTaxa::comTaxa(existente->precoPor)));
        lay->addWidget(por);
        resumo = new QLabel(this);
        resumo->setWordWrap(true);
        lay->addWidget(resumo);

        semData = new QCheckBox(QStringLiteral("Sem data para acabar (encerro eu mesmo)"), this);
        lay->addWidget(semData);
        ate = new QDateEdit(QDate::currentDate().addDays(7), this);
        ate->setCalendarPopup(true);
        ate->setDisplayFormat("dd/MM/yyyy");
        ate->setMinimumDate(QDate::currentDate());
        lay->addWidget(ate);
        connect(semData, &QCheckBox::toggled, ate, &QWidget::setDisabled);
        if (existente) {
            const QDate d = QDate::fromString(existente->ate, Qt::ISODate);
            semData->setChecked(!d.isValid());
            if (d.isValid())
                ate->setDate(qMax(d, QDate::currentDate()));
        }

        auto *botoes = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        botoes->button(QDialogButtonBox::Ok)->setText(QStringLiteral("Salvar promoção"));
        botoes->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("Cancelar"));
        lay->addWidget(botoes);
        connect(botoes, &QDialogButtonBox::rejected, this, &QDialog::reject);
        connect(botoes, &QDialogButtonBox::accepted, this, [this]() { confirmar(); });
        connect(por, &QLineEdit::textChanged, this, [this]() { atualizar(); });
        atualizar();
    }

    qlonglong produtoId = 0;
    double precoPorBase = 0;
    QDate fim;

private:
    void atualizar()
    {
        if (idFixo == 0) {
            const int linha = lw->currentRow();
            if (linha < 0 || !lw->currentItem()) {
                precoDeBase = 0;
                info->setText(QStringLiteral("Escolha o produto na lista."));
            } else {
                const ProdutoDTO &p = lista.at(lw->currentItem()->data(Qt::UserRole).toInt());
                precoDeBase = p.preco;
                info->setText(QStringLiteral("Preço de antes na gôndola (\"de\"): <b>R$ %1</b>").arg(reais(PrecoTaxa::comTaxa(p.preco))));
            }
        } else {
            info->setText(QStringLiteral("Preço de antes na gôndola (\"de\"): <b>R$ %1</b>").arg(reais(PrecoTaxa::comTaxa(precoDeBase))));
        }
        const double gondola = lerPreco(por->text());
        if (precoDeBase <= 0 || qIsNaN(gondola) || gondola <= 0) {
            resumo->setText(QString());
            return;
        }
        const double base = Promocao_service::baseParaGondola(gondola, PrecoTaxa::maiorTaxa());
        const double de = PrecoTaxa::comTaxa(precoDeBase);
        const double real = PrecoTaxa::comTaxa(base);
        const int desc = PostPromocao::descontoPercentual(de, real);
        resumo->setText(real >= de - 0.004
            ? QStringLiteral("<span style='color:#B91C1C'>O preço da promoção precisa ser menor que o de antes.</span>")
            : QStringLiteral("De R$ %1 por <b>R$ %2</b> (%3% de desconto).").arg(reais(de), reais(real)).arg(desc));
    }

    void confirmar()
    {
        qlonglong id = idFixo;
        if (id == 0) {
            if (!lw->currentItem()) {
                QMessageBox::information(this, windowTitle(), QStringLiteral("Escolha o produto."));
                return;
            }
            id = lista.at(lw->currentItem()->data(Qt::UserRole).toInt()).id;
        }
        const double gondola = lerPreco(por->text());
        if (qIsNaN(gondola) || gondola <= 0) {
            QMessageBox::information(this, windowTitle(), QStringLiteral("Informe o preço da promoção."));
            return;
        }
        produtoId = id;
        precoPorBase = Promocao_service::baseParaGondola(gondola, PrecoTaxa::maiorTaxa());
        fim = semData->isChecked() ? QDate() : ate->date();
        accept();
    }

    QList<ProdutoDTO> lista;
    qlonglong idFixo = 0;
    double precoDeBase = 0;
    QLineEdit *busca = nullptr;
    QListWidget *lw = nullptr;
    QLabel *info = nullptr;
    QLineEdit *por = nullptr;
    QLabel *resumo = nullptr;
    QCheckBox *semData = nullptr;
    QDateEdit *ate = nullptr;
};
} // namespace

PromocoesDialog::PromocoesDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Promoções (de/por)"));
    resize(900, 560);
    auto *lay = new QVBoxLayout(this);

    auto *explica = new QLabel(QStringLiteral(
        "Produto em promoção vende pelo preço da promoção no PDV e volta sozinho ao preço de antes quando a data acaba. "
        "Daqui você imprime as etiquetas \"de/por\" da gôndola e gera as imagens para postar no Instagram."), this);
    explica->setWordWrap(true);
    lay->addWidget(explica);

    tabela = new QTableWidget(0, 5, this);
    tabela->setHorizontalHeaderLabels({QStringLiteral("Produto"), QStringLiteral("De"), QStringLiteral("Por"),
                                       QStringLiteral("Desconto"), QStringLiteral("Vale até")});
    tabela->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    tabela->setSelectionBehavior(QAbstractItemView::SelectRows);
    tabela->setSelectionMode(QAbstractItemView::SingleSelection);
    tabela->setEditTriggers(QAbstractItemView::NoEditTriggers);
    lay->addWidget(tabela, 1);

    auto *linha1 = new QHBoxLayout;
    auto *bAdd = new QPushButton(QStringLiteral("Adicionar produto em promoção..."), this);
    auto *bAlt = new QPushButton(QStringLiteral("Alterar preço ou data"), this);
    auto *bEnc = new QPushButton(QStringLiteral("Encerrar promoção"), this);
    linha1->addWidget(bAdd);
    linha1->addWidget(bAlt);
    linha1->addWidget(bEnc);
    linha1->addStretch();
    lay->addLayout(linha1);

    auto *linhaTxt = new QHBoxLayout;
    linhaTxt->addWidget(new QLabel(QStringLiteral("Texto de validade nas imagens:"), this));
    validade = new QLineEdit(this);
    validade->setPlaceholderText(QStringLiteral("Ex.: Válido até 15/11 ou enquanto durarem os estoques"));
    linhaTxt->addWidget(validade, 1);
    lay->addLayout(linhaTxt);

    auto *linha2 = new QHBoxLayout;
    auto *bEtq = new QPushButton(QStringLiteral("Etiquetas de gôndola (PDF A4)..."), this);
    auto *bPost = new QPushButton(QStringLiteral("Posts para o Instagram (imagens)..."), this);
    auto *bLeg = new QPushButton(QStringLiteral("Copiar legenda"), this);
    auto *bFechar = new QPushButton(QStringLiteral("Fechar"), this);
    linha2->addWidget(bEtq);
    linha2->addWidget(bPost);
    linha2->addWidget(bLeg);
    linha2->addStretch();
    linha2->addWidget(bFechar);
    lay->addLayout(linha2);

    connect(bAdd, &QPushButton::clicked, this, &PromocoesDialog::adicionar);
    connect(bAlt, &QPushButton::clicked, this, &PromocoesDialog::alterar);
    connect(bEnc, &QPushButton::clicked, this, &PromocoesDialog::encerrar);
    connect(bEtq, &QPushButton::clicked, this, &PromocoesDialog::etiquetas);
    connect(bPost, &QPushButton::clicked, this, &PromocoesDialog::posts);
    connect(bLeg, &QPushButton::clicked, this, &PromocoesDialog::copiarLegenda);
    connect(bFechar, &QPushButton::clicked, this, &QDialog::accept);
    connect(tabela, &QTableWidget::cellDoubleClicked, this, [this]() { alterar(); });

    Promocao_service().encerrarVencidas();
    recarregar();
}

QString PromocoesDialog::dataCurta(const QString &iso)
{
    const QDate d = QDate::fromString(iso, Qt::ISODate);
    return d.isValid() ? d.toString("dd/MM/yyyy") : QStringLiteral("sem data");
}

void PromocoesDialog::recarregar()
{
    promocoes = Promocao_service().listar();
    tabela->setRowCount(promocoes.size());
    QDate menor;
    bool todasComData = !promocoes.isEmpty();
    for (int i = 0; i < promocoes.size(); ++i) {
        const PromocaoDTO &p = promocoes.at(i);
        const double de = PrecoTaxa::comTaxa(p.precoDe);
        const double por = PrecoTaxa::comTaxa(p.precoPor);
        const QStringList cel = {p.descricao, QStringLiteral("R$ ") + reais(de), QStringLiteral("R$ ") + reais(por),
                                 QStringLiteral("%1%").arg(PostPromocao::descontoPercentual(de, por)), dataCurta(p.ate)};
        for (int c = 0; c < cel.size(); ++c) {
            auto *it = new QTableWidgetItem(cel.at(c));
            if (c > 0)
                it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            tabela->setItem(i, c, it);
        }
        const QDate d = QDate::fromString(p.ate, Qt::ISODate);
        if (!d.isValid())
            todasComData = false;
        else if (!menor.isValid() || d < menor)
            menor = d;
    }
    // sugestão de texto de validade (o usuário pode trocar)
    if (validade->text().isEmpty() && todasComData && menor.isValid())
        validade->setText(QStringLiteral("Válido até %1").arg(menor.toString("dd/MM")));
}

qlonglong PromocoesDialog::idSelecionado() const
{
    const int linha = tabela->currentRow();
    return linha >= 0 && linha < promocoes.size() ? promocoes.at(linha).idProduto : 0;
}

void PromocoesDialog::adicionar()
{
    Produto_Service produtos;
    const QList<ProdutoDTO> candidatos = produtos.listarParaEtiquetas(false);
    FormPromocao f(this, candidatos, nullptr);
    if (f.exec() != QDialog::Accepted)
        return;
    const auto r = Promocao_service().definir(f.produtoId, f.precoPorBase, f.fim);
    if (!r.ok) {
        QMessageBox::warning(this, windowTitle(), r.msg);
        return;
    }
    validade->clear();
    recarregar();
}

void PromocoesDialog::alterar()
{
    const qlonglong id = idSelecionado();
    if (id == 0) {
        QMessageBox::information(this, windowTitle(), QStringLiteral("Selecione um produto da lista."));
        return;
    }
    const PromocaoDTO p = promocoes.at(tabela->currentRow());
    FormPromocao f(this, {}, &p);
    if (f.exec() != QDialog::Accepted)
        return;
    const auto r = Promocao_service().definir(id, f.precoPorBase, f.fim);
    if (!r.ok) {
        QMessageBox::warning(this, windowTitle(), r.msg);
        return;
    }
    validade->clear();
    recarregar();
}

void PromocoesDialog::encerrar()
{
    const qlonglong id = idSelecionado();
    if (id == 0) {
        QMessageBox::information(this, windowTitle(), QStringLiteral("Selecione um produto da lista."));
        return;
    }
    const PromocaoDTO p = promocoes.at(tabela->currentRow());
    if (QMessageBox::question(this, windowTitle(),
            QStringLiteral("Encerrar a promoção de \"%1\"?\nO produto volta para R$ %2.").arg(p.descricao, reais(PrecoTaxa::comTaxa(p.precoDe)))) != QMessageBox::Yes)
        return;
    const auto r = Promocao_service().encerrar(id);
    if (!r.ok)
        QMessageBox::warning(this, windowTitle(), r.msg);
    validade->clear();
    recarregar();
}

QString PromocoesDialog::textoValidade() const
{
    return validade->text().trimmed();
}

void PromocoesDialog::etiquetas()
{
    if (promocoes.isEmpty()) {
        QMessageBox::information(this, windowTitle(), QStringLiteral("Nenhum produto em promoção."));
        return;
    }
    const QString padrao = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/etiquetas-promocao.pdf";
    QString caminho = QFileDialog::getSaveFileName(this, QStringLiteral("Salvar o PDF: etiquetas de promoção"), padrao, "PDF (*.pdf)");
    if (caminho.isEmpty())
        return;
    if (!caminho.endsWith(".pdf", Qt::CaseInsensitive))
        caminho += ".pdf";
    QList<EtiquetasPreco::Item> itens;
    for (const PromocaoDTO &p : promocoes) {
        EtiquetasPreco::Item it;
        it.descricao = p.descricao;
        it.unidade = p.unidade;
        it.preco = PrecoTaxa::comTaxa(p.precoPor);
        it.precoDe = PrecoTaxa::comTaxa(p.precoDe);
        const QDate d = QDate::fromString(p.ate, Qt::ISODate);
        if (d.isValid())
            it.validade = QStringLiteral("válido até %1").arg(d.toString("dd/MM"));
        itens.append(it);
    }
    QString erro;
    if (!EtiquetasPreco::gerarPdf(caminho, Config_service().carregarTudo().nomeEmpresa, QStringLiteral("Etiquetas de promoção (de/por)."), itens, &erro)) {
        QMessageBox::warning(this, windowTitle(), erro);
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(caminho));
}

namespace {
QList<PostPromocao::Oferta> ofertasDe(const QList<PromocaoDTO> &promocoes)
{
    QList<PostPromocao::Oferta> o;
    for (const PromocaoDTO &p : promocoes)
        o.append({p.descricao, p.unidade, PrecoTaxa::comTaxa(p.precoDe), PrecoTaxa::comTaxa(p.precoPor)});
    return o;
}
}

void PromocoesDialog::posts()
{
    if (promocoes.isEmpty()) {
        QMessageBox::information(this, windowTitle(), QStringLiteral("Nenhum produto em promoção."));
        return;
    }
    const QString base = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/promocoes-" + QDate::currentDate().toString("yyyy-MM-dd");
    const QString pasta = QFileDialog::getExistingDirectory(this, QStringLiteral("Em qual pasta salvar as imagens?"),
                                                            QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
    if (pasta.isEmpty())
        return;
    const bool individuais = QMessageBox::question(this, windowTitle(),
        QStringLiteral("Gerar também uma imagem para cada produto (além da imagem com várias ofertas)?")) == QMessageBox::Yes;
    QString erro;
    const QString destino = pasta + "/promocoes-" + QDate::currentDate().toString("yyyy-MM-dd");
    Q_UNUSED(base);
    const QStringList arquivos = PostPromocao::salvar(destino, Config_service().carregarTudo().nomeEmpresa, textoValidade(),
                                                      ofertasDe(promocoes), individuais, &erro);
    if (arquivos.isEmpty() || !erro.isEmpty()) {
        QMessageBox::warning(this, windowTitle(), erro.isEmpty() ? QStringLiteral("Nenhuma imagem foi gerada.") : erro);
        return;
    }
    QApplication::clipboard()->setText(PostPromocao::legenda(Config_service().carregarTudo().nomeEmpresa, textoValidade(), ofertasDe(promocoes)));
    QMessageBox::information(this, windowTitle(),
        QStringLiteral("%1 imagem(ns) salva(s) em:\n%2\n\nA legenda do post já foi copiada: é só colar no Instagram.").arg(arquivos.size()).arg(destino));
    QDesktopServices::openUrl(QUrl::fromLocalFile(destino));
}

void PromocoesDialog::copiarLegenda()
{
    if (promocoes.isEmpty()) {
        QMessageBox::information(this, windowTitle(), QStringLiteral("Nenhum produto em promoção."));
        return;
    }
    QApplication::clipboard()->setText(PostPromocao::legenda(Config_service().carregarTudo().nomeEmpresa, textoValidade(), ofertasDe(promocoes)));
    QMessageBox::information(this, windowTitle(), QStringLiteral("Legenda copiada. Cole no Instagram."));
}
