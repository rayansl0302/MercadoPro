#include "descontocpfdialog.h"
#include "services/descontocpf_service.h"
#include <QCheckBox>
#include <QDateEdit>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

DescontoCpfDialog::DescontoCpfDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Desconto para cliente cadastrado (CPF)"));
    setMinimumWidth(460);
    const DescontoCpf_service::Config c = DescontoCpf_service::carregar();

    auto *lay = new QVBoxLayout(this);
    auto *info = new QLabel(QStringLiteral(
        "No pagamento, quando a venda é de um cliente cadastrado (não o cliente padrão), o operador pode marcar este "
        "desconto. Use o período para campanhas sazonais e o teto para limitar o desconto por venda."), this);
    info->setWordWrap(true);
    lay->addWidget(info);

    chkAtivo = new QCheckBox(QStringLiteral("Desconto ligado"), this);
    chkAtivo->setChecked(c.ativo);
    lay->addWidget(chkAtivo);

    auto *form = new QFormLayout;
    spnPercentual = new QDoubleSpinBox(this);
    spnPercentual->setRange(0, 50);
    spnPercentual->setDecimals(1);
    spnPercentual->setSuffix(QStringLiteral(" %"));
    spnPercentual->setValue(c.percentual);
    form->addRow(QStringLiteral("Percentual:"), spnPercentual);

    spnTeto = new QDoubleSpinBox(this);
    spnTeto->setRange(0, 100000);
    spnTeto->setDecimals(2);
    spnTeto->setPrefix(QStringLiteral("R$ "));
    spnTeto->setSpecialValueText(QStringLiteral("sem teto"));
    spnTeto->setValue(c.tetoReais);
    form->addRow(QStringLiteral("Teto por venda:"), spnTeto);

    chkPeriodo = new QCheckBox(QStringLiteral("Valer só neste período (campanha)"), this);
    chkPeriodo->setChecked(c.comPeriodo);
    form->addRow(chkPeriodo);
    dtDe = new QDateEdit(c.de.isValid() ? c.de : QDate::currentDate(), this);
    dtAte = new QDateEdit(c.ate.isValid() ? c.ate : QDate::currentDate().addMonths(1), this);
    for (QDateEdit *d : {dtDe, dtAte}) {
        d->setCalendarPopup(true);
        d->setDisplayFormat(QStringLiteral("dd/MM/yyyy"));
        d->setEnabled(c.comPeriodo);
    }
    form->addRow(QStringLiteral("De:"), dtDe);
    form->addRow(QStringLiteral("Até:"), dtAte);
    connect(chkPeriodo, &QCheckBox::toggled, dtDe, &QWidget::setEnabled);
    connect(chkPeriodo, &QCheckBox::toggled, dtAte, &QWidget::setEnabled);
    lay->addLayout(form);

    auto *botoes = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    botoes->button(QDialogButtonBox::Save)->setText(QStringLiteral("Salvar"));
    botoes->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("Cancelar"));
    connect(botoes, &QDialogButtonBox::accepted, this, &DescontoCpfDialog::salvar);
    connect(botoes, &QDialogButtonBox::rejected, this, &QDialog::reject);
    lay->addWidget(botoes);
}

void DescontoCpfDialog::salvar()
{
    DescontoCpf_service::Config c;
    c.ativo = chkAtivo->isChecked();
    c.percentual = spnPercentual->value();
    c.tetoReais = spnTeto->value();
    c.comPeriodo = chkPeriodo->isChecked();
    c.de = dtDe->date();
    c.ate = dtAte->date();
    QString erro;
    if (!DescontoCpf_service::salvar(c, &erro)) {
        QMessageBox::warning(this, windowTitle(), erro);
        return;
    }
    accept();
}
