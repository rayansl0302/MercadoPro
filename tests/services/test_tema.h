#ifndef TEST_TEMA_H
#define TEST_TEMA_H

#include <QObject>

// Aumento do tamanho da letra (0/5/10/20%): a folha de estilo é escalada sem estragar o resto.
class TestTema : public QObject
{
    Q_OBJECT
private slots:
    void multiplica_font_size_em_pt_e_px();
    void fator_1_e_texto_sem_font_size_nao_mudam();
    void escala_varias_ocorrencias_e_preserva_o_resto();
    void os_quatro_niveis_de_zoom_dao_tamanhos_crescentes();
    void forma_abreviada_font_tambem_e_escalada();
};

#endif // TEST_TEMA_H
