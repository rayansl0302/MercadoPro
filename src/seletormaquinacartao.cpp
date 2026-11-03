#include "seletormaquinacartao.h"
#include "services/maquinacartao_service.h"

#include <QComboBox>
#include <QLocale>
#include <QSettings>
#include <QWidget>

namespace {
const char *kChaveUltima = "pagamento/ultima_maquina_cartao";
}

SeletorMaquinaCartao::SeletorMaquinaCartao(QComboBox *combo, QWidget *rotulo, QObject *parent)
    : QObject(parent)
    , combo_(combo)
    , rotulo_(rotulo)
{
    MaquinaCartao_service servico;
    maquinas_ = servico.listar(false);

    const QLocale ptBR(QLocale::Portuguese, QLocale::Brazil);
    for (const MaquinaCartaoDTO &m : maquinas_) {
        combo_->addItem(QStringLiteral("%1 — déb. %2% · créd. %3%")
                            .arg(m.nome, ptBR.toString(m.taxaDebito, 'f', 2), ptBR.toString(m.taxaCredito, 'f', 2)),
                        m.id);
    }
    // última máquina usada neste computador (a loja costuma usar sempre a mesma por caixa)
    const qlonglong ultima = QSettings().value(kChaveUltima, 0).toLongLong();
    const int indice = combo_->findData(ultima);
    if (indice >= 0)
        combo_->setCurrentIndex(indice);

    connect(combo_, &QComboBox::currentIndexChanged, this, [this]() {
        QSettings().setValue(kChaveUltima, idMaquina());
        emit escolhaMudou();
    });
    mostrar(false);
}

void SeletorMaquinaCartao::mostrar(bool cartao)
{
    const bool visivel = cartao && temMaquinas();
    combo_->setVisible(visivel);
    if (rotulo_)
        rotulo_->setVisible(visivel);
}

qlonglong SeletorMaquinaCartao::idMaquina() const
{
    if (!temMaquinas() || combo_->currentIndex() < 0)
        return 0;
    return combo_->currentData().toLongLong();
}

double SeletorMaquinaCartao::taxaDaForma(const QString &forma, double taxaPadrao) const
{
    const qlonglong id = idMaquina();
    if (id <= 0)
        return taxaPadrao;
    for (const MaquinaCartaoDTO &m : maquinas_) {
        if (m.id == id)
            return m.taxaDaForma(forma);
    }
    return taxaPadrao;
}
