#include <QtTest>
#include "test_tema.h"
#include "util/tema.h"

void TestTema::multiplica_font_size_em_pt_e_px()
{
    QCOMPARE(Tema::escalarFolha("font-size: 18pt;", 1.2), QString("font-size: 21.6pt;"));
    QCOMPARE(Tema::escalarFolha("font-size:10px", 1.1), QString("font-size: 11px"));
    QCOMPARE(Tema::escalarFolha("font-size : 10.5pt", 1.1), QString("font-size: 11.6pt"));
}

void TestTema::fator_1_e_texto_sem_font_size_nao_mudam()
{
    const QString css = "QPushButton { background: white; border: 1px solid #ccc; }";
    QCOMPARE(Tema::escalarFolha(css, 1.2), css);
    QCOMPARE(Tema::escalarFolha("color: red;", 1.2), QString("color: red;"));
    QCOMPARE(Tema::escalarFolha("", 1.2), QString());
    // fator 1: o valor é preservado (apenas reescrito no mesmo formato)
    QCOMPARE(Tema::escalarFolha("font-size: 12pt;", 1.0), QString("font-size: 12.0pt;"));
}

void TestTema::escala_varias_ocorrencias_e_preserva_o_resto()
{
    const QString css = "QLabel { color: #142D45; font-size: 10pt; } QPushButton { font-weight: 700; font-size: 14px; border-radius: 12px; }";
    const QString esperado = "QLabel { color: #142D45; font-size: 12.0pt; } QPushButton { font-weight: 700; font-size: 17px; border-radius: 12px; }";
    QCOMPARE(Tema::escalarFolha(css, 1.2), esperado);   // border-radius e font-weight ficam como estavam
}

void TestTema::os_quatro_niveis_de_zoom_dao_tamanhos_crescentes()
{
    double anterior = 0;
    for (double fator : {1.0, 1.05, 1.10, 1.20}) {
        const QString r = Tema::escalarFolha("font-size: 20pt", fator);
        const double valor = r.mid(QString("font-size: ").size()).chopped(2).toDouble();
        QVERIFY2(valor > anterior, qPrintable(r));
        anterior = valor;
    }
    QCOMPARE(Tema::escalarFolha("font-size: 20pt", 1.20), QString("font-size: 24.0pt"));
}

void TestTema::forma_abreviada_font_tambem_e_escalada()
{
    QCOMPARE(Tema::escalarFolha("font: 700 9pt \"Segoe UI\";", 1.2), QString("font: 700 10.8pt \"Segoe UI\";"));
    QCOMPARE(Tema::escalarFolha("font: bold 10px Arial;", 1.1), QString("font: bold 11px Arial;"));
    // as duas formas juntas, como na barra do topo; "font-weight" não é confundido com "font:"
    QCOMPARE(Tema::escalarFolha("font: 700 9pt \"Segoe UI\"; font-size: 15px; font-weight: 700;", 1.2),
             QString("font: 700 10.8pt \"Segoe UI\"; font-size: 18px; font-weight: 700;"));
}
