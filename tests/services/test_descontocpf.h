#ifndef TEST_DESCONTOCPF_H
#define TEST_DESCONTOCPF_H

#include <QObject>

// Desconto para cliente cadastrado (CPF): percentual, período sazonal e teto por venda.
class TestDescontoCpf : public QObject
{
    Q_OBJECT
private slots:
    void desconto_e_o_percentual_do_total_em_centavos();
    void desligado_ou_percentual_zero_nao_da_desconto();
    void campanha_so_vale_dentro_do_periodo();
    void teto_limita_o_desconto_por_venda();
    void configuracao_salva_e_volta_igual();
    void configuracao_invalida_e_recusada();
};

#endif // TEST_DESCONTOCPF_H
