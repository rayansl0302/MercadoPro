#ifndef TEST_PRECOTAXA_H
#define TEST_PRECOTAXA_H

#include <QObject>

// Preço de venda com a taxa da maquininha mais cara + folha A4 de etiquetas de preço.
class TestPrecoTaxa : public QObject
{
    Q_OBJECT
private slots:
    void conta_por_dentro_devolve_o_preco_base_depois_de_tirar_a_taxa();
    void taxa_invalida_ou_preco_zero_nao_altera_o_preco();
    void usa_a_maior_taxa_das_maquininhas_ativas_e_acompanha_o_cadastro();
    void texto_do_cadastro_mostra_taxa_e_preco_final();
    void etiquetas_a4_geram_pdf_e_contam_as_folhas();
};

#endif // TEST_PRECOTAXA_H
