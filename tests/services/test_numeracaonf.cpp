#include <QtTest>
#include <QSqlQuery>
#include <QRandomGenerator>
#include "test_numeracaonf.h"
#include "repository/notafiscal_repository.h"
#include "services/notafiscal_service.h"
#include "infra/databaseconnection_service.h"
#include "dto/NotaFiscal_dto.h"

namespace {
// série única por teste: não mistura com as notas dos outros testes
int serieNova() { return QRandomGenerator::global()->bounded(1000, 900000); }

void gravarNota(notafiscal_repository &repo, int serie, qlonglong nnf, const QString &cnpj, const QString &modelo = "65")
{
    NotaFiscalDTO n;
    n.cstat = "100";
    n.nnf = nnf;
    n.serie = serie;
    n.modelo = modelo;
    n.tpAmb = 0;
    n.finalidade = "NORMAL";
    n.cnpjEmit = cnpj;
    n.chNfe = QString("CH%1_%2").arg(serie).arg(nnf);
    n.saida = true;
    n.valorTotal = 10;
    n.dhEmi = "2031-01-01 10:00:00";
    QVERIFY(repo.inserir(n));
}
}

void TestNumeracaoNF::sem_notas_comeca_em_1_ou_no_numero_configurado()
{
    notafiscal_repository repo;
    const QString serie = QString::number(serieNova());
    QCOMPARE(repo.getProximoNNF65(serie, false, 0, "11222333000181", true), 1LL);
    QCOMPARE(repo.getProximoNNF65(serie, false, 40, "11222333000181", true), 41LL);   // configurado como ponto de partida
}

void TestNumeracaoNF::proximo_considera_as_notas_salvas_do_mesmo_cnpj()
{
    notafiscal_repository repo;
    const int serie = serieNova();
    const QString cnpj = "11222333000181";
    gravarNota(repo, serie, 5, cnpj);
    gravarNota(repo, serie, 6, cnpj);
    QCOMPARE(repo.getProximoNNF65(QString::number(serie), false, 0, cnpj, false), 7LL);
    // outro modelo e outro ambiente têm a numeração própria
    QCOMPARE(repo.getProximoNNF55(QString::number(serie), false, 0, cnpj, false), 1LL);
    QCOMPARE(repo.getProximoNNF65(QString::number(serie), true, 0, cnpj, false), 1LL);
}

void TestNumeracaoNF::reserva_da_chave_unica_impede_numero_repetido()
{
    notafiscal_repository repo;
    const QString serie = QString::number(serieNova());
    const QString cnpj = "11222333000181";
    QVERIFY(repo.reservarNNF(cnpj, "65", serie, false, 1, "CAIXA-A"));
    QString erro;
    // o segundo caixa tenta o mesmo número ao mesmo tempo: a chave única recusa
    QVERIFY(!repo.reservarNNF(cnpj, "65", serie, false, 1, "CAIXA-B", &erro));
    QVERIFY(!erro.isEmpty());
    QVERIFY(repo.reservarNNF(cnpj, "65", serie, false, 2, "CAIXA-B"));
    // quem consulta o próximo já enxerga as reservas em andamento
    QCOMPARE(repo.getProximoNNF65(serie, false, 0, cnpj, false), 3LL);
}

void TestNumeracaoNF::servico_reserva_numeros_distintos_e_troca_o_numero_ja_usado()
{
    NotaFiscal_service serv;
    const QString serie = QString::number(serieNova());

    const auto a = serv.reservarNNF(serie, false, ModeloNota::NFCe, 0);
    const auto b = serv.reservarNNF(serie, false, ModeloNota::NFCe, 0);
    QVERIFY(a.ok && b.ok);
    QCOMPARE(b.nnf, a.nnf + 1);
    QVERIFY(!a.alterado);

    // a tela mostrou o número "a" ao usuário, mas outro caixa já o levou: o sistema usa o próximo livre
    const auto c = serv.reservarNNF(serie, false, ModeloNota::NFCe, a.nnf);
    QVERIFY(c.ok);
    QVERIFY(c.alterado);
    QCOMPARE(c.nnf, b.nnf + 1);

    // número escolhido à mão, ainda livre e à frente: é respeitado
    const auto d = serv.reservarNNF(serie, false, ModeloNota::NFCe, c.nnf + 5);
    QVERIFY(d.ok);
    QVERIFY(!d.alterado);
    QCOMPARE(d.nnf, c.nnf + 5);
}

void TestNumeracaoNF::numero_liberado_volta_para_a_fila_e_o_confirmado_nao()
{
    NotaFiscal_service serv;
    const QString serie = QString::number(serieNova());

    const auto a = serv.reservarNNF(serie, false, ModeloNota::NFCe, 0);
    serv.liberarNNF(serie, false, ModeloNota::NFCe, a.nnf);                 // rejeitada antes de autorizar
    const auto de_novo = serv.reservarNNF(serie, false, ModeloNota::NFCe, 0);
    QCOMPARE(de_novo.nnf, a.nnf);                                           // o mesmo número é reaproveitado

    serv.confirmarNNF(serie, false, ModeloNota::NFCe, de_novo.nnf);         // autorizada: consumido
    serv.liberarNNF(serie, false, ModeloNota::NFCe, de_novo.nnf);           // liberar depois de confirmar não faz nada
    const auto seguinte = serv.reservarNNF(serie, false, ModeloNota::NFCe, 0);
    QCOMPARE(seguinte.nnf, de_novo.nnf + 1);
}

void TestNumeracaoNF::reserva_vencida_nao_trava_a_numeracao()
{
    notafiscal_repository repo;
    const QString serie = QString::number(serieNova());
    const QString cnpj = "11222333000181";
    QVERIFY(repo.reservarNNF(cnpj, "65", serie, false, 1, "CAIXA-QUE-TRAVOU"));
    QCOMPARE(repo.getProximoNNF65(serie, false, 0, cnpj, false), 2LL);

    // o caixa travou no meio da emissão: depois de 15 minutos a reserva deixa de valer
    QSqlQuery q(DatabaseConnection_service::db());
    q.prepare("UPDATE nnf_reservas SET reservado_em = :velho WHERE serie = :s AND cnpj_emit = :c");
    q.bindValue(":velho", QDateTime::currentDateTime().addSecs(-3600).toString("yyyy-MM-dd HH:mm:ss"));
    q.bindValue(":s", serie);
    q.bindValue(":c", cnpj);
    QVERIFY(q.exec());
    QCOMPARE(repo.getProximoNNF65(serie, false, 0, cnpj, false), 1LL);
    QVERIFY(repo.reservarNNF(cnpj, "65", serie, false, 1, "CAIXA-B"));      // a reserva velha foi limpa
}

void TestNumeracaoNF::cada_cnpj_tem_a_sua_numeracao_e_nota_antiga_sem_cnpj_e_da_empresa_original()
{
    notafiscal_repository repo;
    const int serie = serieNova();
    const QString s = QString::number(serie);
    const QString empresaA = "11222333000181";
    const QString empresaB = "69133888000137";
    gravarNota(repo, serie, 10, empresaA);
    gravarNota(repo, serie, 3, "69.133.888/0001-37");            // CNPJ gravado com pontuação
    gravarNota(repo, serie, 8, "");                              // nota antiga, sem CNPJ gravado

    QCOMPARE(repo.getProximoNNF65(s, false, 0, empresaA, false), 11LL);
    QCOMPARE(repo.getProximoNNF65(s, false, 0, empresaB, false), 4LL);     // só enxerga as notas dele
    // a empresa original (legado) também conta as notas sem CNPJ; as demais não
    QCOMPARE(repo.getProximoNNF65(s, false, 0, empresaA, true), 11LL);
    QCOMPARE(repo.getProximoNNF65(s, false, 0, empresaB, true), 9LL);
    // reservas também são por CNPJ
    QVERIFY(repo.reservarNNF(empresaB, "65", s, false, 4, "CAIXA-A"));
    QVERIFY(repo.reservarNNF(empresaA, "65", s, false, 4, "CAIXA-A"));     // mesmo número em outro CNPJ pode
    QCOMPARE(repo.getProximoNNF65(s, false, 0, empresaB, false), 5LL);
}
