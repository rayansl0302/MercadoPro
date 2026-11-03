#ifndef SELETORMAQUINACARTAO_H
#define SELETORMAQUINACARTAO_H

#include <QList>
#include <QObject>
#include "dto/MaquinaCartao_dto.h"

class QComboBox;
class QWidget;

// Escolha da maquininha na hora de receber no cartão. Cuida do combo (máquinas ativas), de lembrar a última
// usada neste computador e de dizer qual taxa vale. Sem nenhuma máquina cadastrada o combo fica escondido e
// vale a taxa padrão das Configurações. A taxa nunca é somada ao valor: o cliente paga o preço de gôndola.
class SeletorMaquinaCartao : public QObject
{
    Q_OBJECT
public:
    // `rotulo` (opcional) é escondido junto com o combo
    SeletorMaquinaCartao(QComboBox *combo, QWidget *rotulo, QObject *parent = nullptr);

    bool temMaquinas() const { return !maquinas_.isEmpty(); }
    // mostra o combo só quando é cartão E há máquinas cadastradas
    void mostrar(bool cartao);
    // taxa (%) a gravar: a da máquina escolhida; sem máquinas, `taxaPadrao`
    double taxaDaForma(const QString &forma, double taxaPadrao) const;
    // 0 = sem máquina (nenhuma cadastrada)
    qlonglong idMaquina() const;

signals:
    void escolhaMudou();

private:
    QComboBox *combo_;
    QWidget *rotulo_;
    QList<MaquinaCartaoDTO> maquinas_;
};

#endif // SELETORMAQUINACARTAO_H
