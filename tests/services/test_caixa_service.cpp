#include <QtTest>
#include <QMap>
#include <QUuid>
#include <QSqlQuery>
#include <QSqlError>
#include "repository/caixa_repository.h"
#include "repository/operador_repository.h"
#include "repository/sessao_repository.h"
#include "infra/databaseconnection_service.h"
#include "dto/Vendas_dto.h"
#include "dto/Sessao_dto.h"
#include "test_caixa_service.h"
#include "services/caixa_service.h"
#include "services/operador_service.h"
#include "services/sessao_service.h"
#include "../db/test_db_factory.h"

void TestCaixaService::pin_formato()
{
    QVERIFY(Operador_service::pinValido("1234"));
    QVERIFY(Operador_service::pinValido("123456"));
    QVERIFY(!Operador_service::pinValido("123"));
    QVERIFY(!Operador_service::pinValido("1234567"));
    QVERIFY(!Operador_service::pinValido("12a4"));
}

void TestCaixaService::pin_bloqueia_na_terceira_tentativa()
{
    Operador_service os;
    const auto cad = os.cadastrar("Op Bloqueio", "4321");
    QVERIFY(cad.ok);

    QVERIFY(!os.validarPin(cad.id, "0000").ok);
    QVERIFY(!os.validarPin(cad.id, "0000").ok);
    const auto terceira = os.validarPin(cad.id, "0000");
    QVERIFY(!terceira.ok);
    QVERIFY(os.getPorId(cad.id).bloqueado);
    QVERIFY(!os.validarPin(cad.id, "4321").ok);
}

void TestCaixaService::tolerancia_usa_maior_entre_valor_e_percentual()
{
    Caixa_service cs;
    QVERIFY(cs.dentroDaTolerancia(2.0, 100.0));
    QVERIFY(!cs.dentroDaTolerancia(2.01, 100.0));
    QVERIFY(cs.dentroDaTolerancia(5.0, 1000.0));
    QVERIFY(!cs.dentroDaTolerancia(5.01, 1000.0));
}

void TestCaixaService::fechar_acima_da_tolerancia_exige_justificativa()
{
    TestDbFactory::garantirCaixaAberto();
    Caixa_service cs;
    const CaixaDTO caixa = cs.caixaAtual();
    QVERIFY(caixa.aberto());

    QMap<QString, double> informados;
    informados["Dinheiro"] = caixa.trocoInicial + 50.0;
    informados["Crédito"] = 0;
    informados["Débito"] = 0;
    informados["Pix"] = 0;

    const auto semObs = cs.fecharCaixa(caixa.id, "1234", informados, "");
    QVERIFY(!semObs.ok);

    const auto comObs = cs.fecharCaixa(caixa.id, "1234", informados, "Diferença de teste no dinheiro.");
    QVERIFY(comObs.ok);
}

// ─── regras do caixa ─────────────────────────────────────────────────────────

namespace {

const QStringList kFormas = {"Dinheiro", "Crédito", "Débito", "Pix"};

qlonglong novoOperador(const QString &prefixo, const QString &pin = "1234")
{
    Operador_service os;
    const QString nome = prefixo + " " + QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
    const auto r = os.cadastrar(nome, pin);
    return r.ok ? r.id : -1;
}

// informa exatamente o esperado em cada forma (fecha sem diferença)
QMap<QString, double> informadosIguaisAoEsperado(const ResumoCaixaDTO &r)
{
    QMap<QString, double> m;
    for (const QString &f : kFormas)
        m[f] = r.esperadoPorForma.value(f, 0.0);
    return m;
}

void fecharCaixaDoTerminalSeHouver()
{
    Caixa_service cs;
    const CaixaDTO c = cs.caixaAtual();
    if (!c.aberto())
        return;
    const auto r = cs.fecharCaixa(c.id, "1234", informadosIguaisAoEsperado(cs.resumo(c.id)), "");
    if (!r.ok)
        qWarning("Não fechou o caixa #%lld: %s", c.id, qPrintable(r.msg));
}

qlonglong abrirCaixaNovo(double troco = 0)
{
    const qlonglong op = novoOperador("Op Caixa");
    Caixa_service cs;
    const auto r = cs.abrirCaixa(op, "1234", troco, 0);
    return r.ok ? r.id : -1;
}

qlonglong inserirVendaSql(qlonglong idCaixa, const QString &forma, double valor)
{
    DatabaseConnection_service::open();
    QSqlQuery q(DatabaseConnection_service::db());
    q.prepare("INSERT INTO vendas2 (cliente, total, data_hora, forma_pagamento, valor_recebido, troco, taxa, "
              "valor_final, desconto, id_cliente, esta_pago, id_caixa) "
              "VALUES ('Teste', :v, '2026-01-01 10:00:00', :f, :v, 0, 0, :v, 0, 1, :p, :c)");
    q.bindValue(":v", valor);
    q.bindValue(":f", forma);
    q.bindValue(":p", forma != "Prazo");
    q.bindValue(":c", idCaixa);
    if (!q.exec())
        return -1;
    return q.lastInsertId().toLongLong();
}

VendasDTO vendaDto(qlonglong id, qlonglong idCaixa, const QString &forma, double valor, bool paga)
{
    VendasDTO v;
    v.id = id;
    v.idCaixa = idCaixa;
    v.formaPagamento = forma;
    v.valorFinal = valor;
    v.estaPago = paga;
    v.idCliente = 1;
    return v;
}

} // namespace

void TestCaixaService::init()
{
    fecharCaixaDoTerminalSeHouver();
}

void TestCaixaService::cleanupTestCase()
{
    TestDbFactory::garantirCaixaAberto();
}

void TestCaixaService::operador_nome_unico_e_pin_nao_fica_em_texto()
{
    Operador_service os;
    const QString base = "Maria Silva " + QUuid::createUuid().toString(QUuid::WithoutBraces).left(6);
    const auto a = os.cadastrar(base, "4455");
    QVERIFY(a.ok);

    // mesmo nome, só muda caixa alta e espaços
    QVERIFY(!os.cadastrar(base.toUpper().replace(" ", "   "), "4455").ok);
    QVERIFY(!os.cadastrar("   ", "4455").ok);
    QVERIFY(!os.cadastrar("Fulano PIN curto", "12").ok);

    const auto b = os.cadastrar(base + " Junior", "4455");
    QVERIFY(b.ok);

    const OperadorDTO opA = os.getPorId(a.id);
    const OperadorDTO opB = os.getPorId(b.id);
    QVERIFY(opA.pinHash != "4455");
    QCOMPARE(opA.pinHash.size(), 64);          // SHA-256 em hexadecimal
    QCOMPARE(opA.pinSalt.size(), 32);          // 16 bytes de salt
    QVERIFY(opA.pinSalt != opB.pinSalt);
    QVERIFY(opA.pinHash != opB.pinHash);       // mesmo PIN, salts diferentes -> hashes diferentes
}

void TestCaixaService::abrir_exige_pin_correto_e_operador_ativo()
{
    Caixa_service cs;
    Operador_service os;
    const qlonglong op = novoOperador("Op Pin");
    QVERIFY(op > 0);

    QVERIFY(!cs.abrirCaixa(0, "1234", 0, 0).ok);               // sem operador
    QVERIFY(!cs.abrirCaixa(op, "9999", 0, 0).ok);              // PIN errado
    QVERIFY(!cs.abrirCaixa(op, "1234", -5, 0).ok);             // troco negativo
    QVERIFY(!cs.caixaAtual().aberto());

    QVERIFY(os.definirAtivo(op, false).ok);
    const auto inativo = cs.abrirCaixa(op, "1234", 0, 0);
    QVERIFY(!inativo.ok);
    QVERIFY(inativo.msg.contains("desativado"));

    QVERIFY(os.definirAtivo(op, true).ok);
    QVERIFY(cs.abrirCaixa(op, "1234", 10, 5).ok);
    const CaixaDTO aberto = cs.caixaAtual();
    QVERIFY(aberto.aberto());
    QCOMPARE(aberto.trocoInicial, 10.0);
    QCOMPARE(aberto.trocoSugerido, 5.0);         // os dois valores ficam gravados
    QCOMPARE(aberto.terminal, Caixa_service::terminalAtual());
}

void TestCaixaService::abrir_recusa_segundo_caixa_no_terminal()
{
    // o caixa é do operador: o de outro não trava o terminal. Já o mesmo operador não abre dois.
    const qlonglong primeiro = abrirCaixaNovo();
    QVERIFY(primeiro > 0);
    Caixa_service cs;
    const qlonglong outro = novoOperador("Op Segundo");
    const auto r = cs.abrirCaixa(outro, "1234", 0, 0);
    QVERIFY2(r.ok, qPrintable(r.msg));

    const auto repetido = cs.abrirCaixa(outro, "1234", 0, 0);
    QVERIFY(!repetido.ok);

    QVERIFY(cs.fecharCaixa(r.id, "1234", informadosIguaisAoEsperado(cs.resumo(r.id)), "").ok);
}

void TestCaixaService::abrir_recusa_operador_com_caixa_em_outro_terminal()
{
    const qlonglong op = novoOperador("Op Outro PC");
    Caixa_repository repo;
    CaixaDTO outroPc;
    outroPc.idOperador = op;
    outroPc.terminal = "OUTRO-PC-TESTE";
    QString erro;
    const qlonglong idOutro = repo.abrir(outroPc, &erro);
    QVERIFY2(idOutro > 0, qPrintable(erro));

    Caixa_service cs;
    const auto r = cs.abrirCaixa(op, "1234", 0, 0);
    QVERIFY(!r.ok);
    QVERIFY(r.msg.contains("OUTRO-PC-TESTE"));

    // limpeza: fecha o caixa do outro terminal
    QVERIFY(cs.fecharCaixa(idOutro, "1234", informadosIguaisAoEsperado(cs.resumo(idOutro)), "").ok);
}

void TestCaixaService::banco_impede_dois_caixas_abertos()
{
    Caixa_repository repo;
    const qlonglong op1 = novoOperador("Op Unico 1");
    const qlonglong op2 = novoOperador("Op Unico 2");

    CaixaDTO a;
    a.idOperador = op1;
    a.terminal = "TERMINAL-UNICO-TESTE";
    const qlonglong idA = repo.abrir(a);
    QVERIFY(idA > 0);

    CaixaDTO mesmoTerminal;
    mesmoTerminal.idOperador = op2;
    mesmoTerminal.terminal = "TERMINAL-UNICO-TESTE";
    const qlonglong idMesmoTerminal = repo.abrir(mesmoTerminal);
    QVERIFY(idMesmoTerminal > 0);                   // outro operador no mesmo terminal pode
    QVERIFY(repo.fechar(idMesmoTerminal, "", {}));

    CaixaDTO mesmoOperador;
    mesmoOperador.idOperador = op1;
    mesmoOperador.terminal = "OUTRO-TERMINAL-TESTE";
    QCOMPARE(repo.abrir(mesmoOperador), -1LL);      // índice único: um aberto por operador

    QVERIFY(repo.fechar(idA, "", {}));
    // depois de fechado, o terminal e o operador ficam livres
    const qlonglong idB = repo.abrir(mesmoOperador);
    QVERIFY(idB > 0);
    QVERIFY(repo.fechar(idB, "", {}));
}

void TestCaixaService::sangria_e_suprimento_exigem_valor_e_motivo_e_entram_no_esperado()
{
    Caixa_service cs;
    QVERIFY(!cs.registrarSangria(10, "banco").ok);          // sem caixa aberto

    const qlonglong id = abrirCaixaNovo(100);
    QVERIFY(id > 0);

    QVERIFY(!cs.registrarSuprimento(0, "reforço").ok);
    QVERIFY(!cs.registrarSuprimento(-5, "reforço").ok);
    QVERIFY(!cs.registrarSangria(10, "   ").ok);            // motivo obrigatório

    QVERIFY(cs.registrarSuprimento(50, "reforço de troco").ok);
    QVERIFY(cs.registrarSangria(30, "depósito no banco").ok);

    const ResumoCaixaDTO r = cs.resumo(id);
    QCOMPARE(r.totalSuprimentos, 50.0);
    QCOMPARE(r.totalSangrias, 30.0);
    QCOMPARE(r.esperadoPorForma.value("Dinheiro"), 120.0);   // 100 + 50 - 30
    QCOMPARE(r.movimentacoes.size(), 2);
}

void TestCaixaService::sangria_pode_ser_editada_ou_removida_no_caixa_aberto()
{
    Caixa_service cs;
    const qlonglong id = abrirCaixaNovo(100);
    QVERIFY(id > 0);
    QVERIFY(cs.registrarSangria(30, "deposito").ok);
    QVERIFY(cs.registrarSangria(30, "deposito").ok);           // lancada duas vezes sem querer
    QList<MovimentacaoCaixaDTO> lista = cs.sangriasESuprimentosDoCaixa(id);
    QCOMPARE(lista.size(), 2);

    QVERIFY(!cs.removerSangriaSuprimento(lista.at(1).id, "").ok);              // exige motivo
    QVERIFY(cs.removerSangriaSuprimento(lista.at(1).id, "lancada em duplicidade").ok);
    QVERIFY(!cs.editarSangriaSuprimento(lista.at(0).id, 0, "deposito", "valor errado").ok);
    QVERIFY(cs.editarSangriaSuprimento(lista.at(0).id, 45.5, "deposito no banco", "valor errado").ok);

    lista = cs.sangriasESuprimentosDoCaixa(id);
    QCOMPARE(lista.size(), 1);
    QCOMPARE(lista.first().valor, 45.5);
    QCOMPARE(lista.first().motivo, QString("deposito no banco"));
    QCOMPARE(cs.resumo(id).esperadoPorForma.value("Dinheiro"), 54.5);          // 100 - 45,50
}

void TestCaixaService::sangria_desconta_da_forma_escolhida()
{
    Caixa_service cs;
    const qlonglong id = abrirCaixaNovo(100);
    QVERIFY(id > 0);
    QVERIFY(inserirVendaSql(id, "Pix", 80) > 0);
    QVERIFY(!cs.registrarSangria(10, "banco", "Cheque").ok);                 // forma invalida
    QVERIFY(cs.registrarSangria(20, "deposito").ok);                         // padrao: dinheiro
    QVERIFY(cs.registrarSangria(30, "retirada do pix", "Pix").ok);

    ResumoCaixaDTO r = cs.resumo(id);
    QCOMPARE(r.esperadoPorForma.value("Dinheiro"), 80.0);                    // 100 - 20
    QCOMPARE(r.esperadoPorForma.value("Pix"), 50.0);                         // 80 - 30

    const auto lista = cs.sangriasESuprimentosDoCaixa(id);
    QVERIFY(cs.editarSangriaSuprimento(lista.last().id, 30, "retirada do pix", "era dinheiro", "Dinheiro").ok);
    r = cs.resumo(id);
    QCOMPARE(r.esperadoPorForma.value("Dinheiro"), 50.0);                    // 100 - 20 - 30
    QCOMPARE(r.esperadoPorForma.value("Pix"), 80.0);
}

void TestCaixaService::caixa_vencido_nao_vende_e_o_do_dia_nao_vira_o_do_outro()
{
    Caixa_service cs;
    const qlonglong id = abrirCaixaNovo(50);
    QVERIFY(id > 0);
    QVERIFY(cs.exigirCaixaAberto().ok);                                 // recem aberto: vale
    QVERIFY(!cs.caixaVencido(cs.caixaAtual()));

    DatabaseConnection_service::open();
    QSqlQuery q(DatabaseConnection_service::db());
    q.prepare("UPDATE caixas SET aberto_em = :t WHERE id = :id");
    q.bindValue(":t", QDateTime::currentDateTime().addSecs(-16 * 3600));
    q.bindValue(":id", id);
    QVERIFY(q.exec());
    QString motivo;
    QVERIFY(cs.caixaVencido(cs.caixaAtual(), &motivo));                 // 16 h: passou das 15
    QVERIFY(!motivo.isEmpty());
    QVERIFY(!cs.exigirCaixaAberto().ok);

    q.bindValue(":t", QDateTime::currentDateTime().addDays(-1));        // ontem, mesmo com poucas horas a mais
    q.bindValue(":id", id);
    QVERIFY(q.exec());
    QVERIFY(!cs.exigirCaixaAberto().ok);

    q.bindValue(":t", QDateTime::currentDateTime().addSecs(-3600));     // 1 h atras (se ainda for hoje, vale)
    q.bindValue(":id", id);
    QVERIFY(q.exec());
    if (QDateTime::currentDateTime().addSecs(-3600).date() == QDate::currentDate())
        QVERIFY(cs.exigirCaixaAberto().ok);
}

void TestCaixaService::fechamento_leva_o_dinheiro_para_deposito_e_deixa_o_troco()
{
    Caixa_service cs;
    const qlonglong id = abrirCaixaNovo(100);
    QVERIFY(id > 0);
    QVERIFY(inserirVendaSql(id, "Dinheiro", 50) > 0);
    const auto antes = cs.resumo(id);
    QCOMPARE(antes.esperadoPorForma.value("Dinheiro"), 150.0);

    QVERIFY(!cs.fecharCaixa(id, "1234", informadosIguaisAoEsperado(antes), "", 200).ok);   // fica mais do que contou
    QVERIFY(cs.fecharCaixa(id, "1234", informadosIguaisAoEsperado(antes), "", 20).ok);

    const auto depois = cs.resumo(id);
    QCOMPARE(depois.esperadoPorForma.value("Dinheiro"), 150.0);                 // o deposito nao muda o esperado
    double recolhido = 0;
    for (const MovimentacaoCaixaDTO &m : depois.movimentacoes)
        if (m.tipo == "RECOLHIMENTO")
            recolhido += m.valor;
    QCOMPARE(recolhido, 130.0);                                                 // 150 contados - 20 que ficam
    QCOMPARE(cs.sugerirTrocoInicial(), 20.0);                                   // o que ficou vira o troco sugerido
}

void TestCaixaService::esperado_soma_vendas_e_recebimentos_por_forma()
{
    Caixa_service cs;
    const qlonglong id = abrirCaixaNovo(10);
    QVERIFY(id > 0);

    QVERIFY(inserirVendaSql(id, "Dinheiro", 40) > 0);
    QVERIFY(inserirVendaSql(id, "Dinheiro", 10) > 0);
    QVERIFY(inserirVendaSql(id, "Pix", 25) > 0);
    QVERIFY(inserirVendaSql(id, "Crédito", 30) > 0);
    const qlonglong prazo = inserirVendaSql(id, "Prazo", 99);   // não entra no caixa
    QVERIFY(prazo > 0);
    QVERIFY(inserirVendaSql(id, "Não Sei", 7) > 0);             // fora da conferência

    QVERIFY(cs.registrarRecebimento(prazo, 1001, "Dinheiro", 15).ok);
    QVERIFY(cs.registrarRecebimento(prazo, 1002, "Pix", 5).ok);

    const ResumoCaixaDTO r = cs.resumo(id);
    QCOMPARE(r.esperadoPorForma.value("Dinheiro"), 75.0);   // 10 troco + 50 vendas + 15 recebimento
    QCOMPARE(r.esperadoPorForma.value("Pix"), 30.0);        // 25 + 5
    QCOMPARE(r.esperadoPorForma.value("Crédito"), 30.0);
    QCOMPARE(r.esperadoPorForma.value("Débito"), 0.0);
    QCOMPARE(r.totalVendas, 112.0);                         // sem o Prazo
    QCOMPARE(r.quantidadeVendas, 6);
    QCOMPARE(r.totalRecebimentos, 20.0);
    QCOMPARE(r.outrasFormas.value("Não Sei"), 7.0);         // aparece como aviso no fechamento
    QVERIFY(!r.outrasFormas.contains("Prazo"));
}

void TestCaixaService::fechar_dentro_da_tolerancia_sem_justificativa_e_marca_ocorrencia_tecnica()
{
    Caixa_service cs;
    const qlonglong id = abrirCaixaNovo(0);
    QVERIFY(id > 0);
    QVERIFY(inserirVendaSql(id, "Pix", 100) > 0);

    QMap<QString, double> inf;
    inf["Dinheiro"] = 0.50;     // sobra de 50 centavos: dentro da tolerância, sem justificativa
    inf["Crédito"] = 0;
    inf["Débito"] = 0;
    inf["Pix"] = 99.99;         // 1 centavo a menos no Pix: ocorrência técnica
    const auto r = cs.fecharCaixa(id, "1234", inf, "");
    QVERIFY2(r.ok, qPrintable(r.msg));

    const ResumoCaixaDTO fechado = cs.resumo(id);
    QCOMPARE(fechado.caixa.status, QString("FECHADO"));
    QCOMPARE(fechado.fechamento.size(), 4);
    for (const FechamentoFormaDTO &f : fechado.fechamento) {
        if (f.formaPagamento == "Pix") {
            QVERIFY(f.ocorrenciaTecnica);
            QCOMPARE(f.diferenca, -0.01);
        } else if (f.formaPagamento == "Dinheiro") {
            QVERIFY(!f.ocorrenciaTecnica);     // ocorrência técnica só vale para cartão/Pix
            QCOMPARE(f.diferenca, 0.5);
        } else {
            QVERIFY(!f.ocorrenciaTecnica);
        }
    }
}

void TestCaixaService::fechar_valida_formas_pin_e_nao_fecha_duas_vezes()
{
    Caixa_service cs;
    const qlonglong id = abrirCaixaNovo(0);
    QVERIFY(id > 0);

    QMap<QString, double> completo;
    for (const QString &f : kFormas)
        completo[f] = 0;

    QMap<QString, double> semPix = completo;
    semPix.remove("Pix");
    QVERIFY(!cs.fecharCaixa(id, "1234", semPix, "").ok);            // falta uma forma

    QMap<QString, double> negativo = completo;
    negativo["Débito"] = -1;
    QVERIFY(!cs.fecharCaixa(id, "1234", negativo, "").ok);

    QVERIFY(!cs.fecharCaixa(id, "0000", completo, "").ok);          // PIN errado
    QVERIFY(cs.caixaAtual().aberto());                   // continua aberto

    QVERIFY(cs.fecharCaixa(id, "1234", completo, "").ok);
    const auto segunda = cs.fecharCaixa(id, "1234", completo, "");
    QVERIFY(!segunda.ok);
    QVERIFY(segunda.msg.contains("fechado"));
    QVERIFY(!cs.caixaAtual().aberto());
}

void TestCaixaService::troco_sugerido_vem_do_dinheiro_contado_no_ultimo_fechamento()
{
    Caixa_service cs;
    const qlonglong id = abrirCaixaNovo(0);
    QVERIFY(id > 0);
    QVERIFY(cs.registrarSuprimento(200, "reforço").ok);

    QMap<QString, double> inf;
    inf["Dinheiro"] = 180;      // faltam 20: acima da tolerância, exige justificativa
    inf["Crédito"] = 0;
    inf["Débito"] = 0;
    inf["Pix"] = 0;
    QVERIFY(!cs.fecharCaixa(id, "1234", inf, "").ok);
    QVERIFY(!cs.fecharCaixa(id, "1234", inf, "curt").ok);           // menos de 5 caracteres
    QVERIFY(cs.fecharCaixa(id, "1234", inf, "Faltou dinheiro na contagem.").ok);

    QCOMPARE(cs.sugerirTrocoInicial(), 180.0);   // o contado, não o esperado
}

void TestCaixaService::cancelamento_segue_as_regras_do_caixa()
{
    Caixa_service cs;

    // venda anterior ao módulo (sem caixa) cancela como antes
    QVERIFY(cs.validarCancelamento(vendaDto(0, 0, "Dinheiro", 10, true), "").ok);

    const qlonglong id = abrirCaixaNovo(0);
    QVERIFY(id > 0);
    const qlonglong idVenda = inserirVendaSql(id, "Dinheiro", 20);
    const VendasDTO venda = vendaDto(idVenda, id, "Dinheiro", 20, true);

    QVERIFY(!cs.validarCancelamento(venda, "").ok);             // motivo obrigatório
    QVERIFY(!cs.validarCancelamento(venda, "   ").ok);
    QVERIFY(cs.validarCancelamento(venda, "Cliente desistiu").ok);

    QVERIFY(cs.registrarCancelamento(venda, "Cliente desistiu").ok);
    const ResumoCaixaDTO r = cs.resumo(id);
    QCOMPARE(r.cancelamentos.size(), 1);
    QVERIFY(r.cancelamentos.first().estornado);                 // venda já paga: estorno
    QCOMPARE(r.cancelamentos.first().motivo, QString("Cliente desistiu"));

    // depois de fechado o caixa, a venda não cancela mais
    QVERIFY(cs.fecharCaixa(id, "1234", informadosIguaisAoEsperado(cs.resumo(id)), "").ok);
    const auto fechado = cs.validarCancelamento(venda, "Tarde demais");
    QVERIFY(!fechado.ok);
    QVERIFY(fechado.msg.contains("já fechado"));
}

void TestCaixaService::cancelamento_bloqueado_com_recebimento_em_caixa_fechado()
{
    Caixa_service cs;
    const qlonglong caixaA = abrirCaixaNovo(0);
    QVERIFY(caixaA > 0);
    const qlonglong prazo = inserirVendaSql(caixaA, "Prazo", 100);
    QVERIFY(cs.registrarRecebimento(prazo, 2001, "Dinheiro", 40).ok);
    QVERIFY(cs.fecharCaixa(caixaA, "1234", informadosIguaisAoEsperado(cs.resumo(caixaA)), "").ok);

    const qlonglong caixaB = abrirCaixaNovo(0);
    QVERIFY(caixaB > 0);
    // a venda a prazo é do caixa B, mas parte dela foi recebida no caixa A (já conferido)
    const VendasDTO venda = vendaDto(prazo, caixaB, "Prazo", 100, false);
    const auto r = cs.validarCancelamento(venda, "Teste");
    QVERIFY(!r.ok);
    QVERIFY(r.msg.contains("caixa já fechado"));
}

void TestCaixaService::recebimento_de_caixa_fechado_nao_pode_ser_excluido()
{
    Caixa_service cs;
    const qlonglong caixaA = abrirCaixaNovo(0);
    QVERIFY(caixaA > 0);
    const qlonglong prazo = inserirVendaSql(caixaA, "Prazo", 100);

    // no caixa aberto pode excluir; o lançamento sai do caixa
    QVERIFY(cs.registrarRecebimento(prazo, 3001, "Dinheiro", 25).ok);
    QVERIFY(cs.podeRemoverRecebimento(3001).ok);
    QVERIFY(cs.removerRecebimento(3001).ok);
    QCOMPARE(cs.resumo(caixaA).totalRecebimentos, 0.0);

    // depois de fechado, não
    QVERIFY(cs.registrarRecebimento(prazo, 3002, "Dinheiro", 25).ok);
    QVERIFY(cs.fecharCaixa(caixaA, "1234", informadosIguaisAoEsperado(cs.resumo(caixaA)), "").ok);
    const auto r = cs.podeRemoverRecebimento(3002);
    QVERIFY(!r.ok);
    QVERIFY(r.msg.contains("já fechado"));
    QVERIFY(!cs.removerRecebimento(3002).ok);

    // recebimento sem vínculo com caixa (antigo) segue permitido
    QVERIFY(cs.podeRemoverRecebimento(99999).ok);
}

void TestCaixaService::desativar_operador_com_caixa_aberto_e_recusado()
{
    Caixa_service cs;
    Operador_service os;
    const qlonglong op = novoOperador("Op Preso");
    QVERIFY(cs.abrirCaixa(op, "1234", 0, 0).ok);

    const auto r = os.definirAtivo(op, false);
    QVERIFY(!r.ok);
    QVERIFY(r.msg.contains("caixa"));
    QVERIFY(os.getPorId(op).ativo);

    // com o caixa fechado, desativar é permitido
    const CaixaDTO aberto = cs.caixaAtual();
    QVERIFY(cs.fecharCaixa(aberto.id, "1234", informadosIguaisAoEsperado(cs.resumo(aberto.id)), "").ok);
    QVERIFY(os.definirAtivo(op, false).ok);
}

static QString terminalDoTeste()
{
    const QString host = QSysInfo::machineHostName().trimmed();
    return host.isEmpty() ? QStringLiteral("TERMINAL") : host.toUpper();
}

void TestCaixaService::pin_do_gerente()
{
    Operador_service os;
    Operador_repository repo;
    // estado limpo: zera bloqueio e tentativas de execuções anteriores
    repo.setConfigCaixa("gerente_bloqueio_ate_" + terminalDoTeste(), "0");
    repo.setConfigCaixa("gerente_tentativas_" + terminalDoTeste(), "0");

    QVERIFY(!os.definirGerentePin("12").ok);
    QVERIFY(!os.definirGerentePin("abcd").ok);
    QVERIFY(os.definirGerentePin("2468").ok);
    QVERIFY(os.gerentePinDefinido());

    QVERIFY(repo.getConfigCaixa("gerente_pin_hash") != "2468");      // nunca em texto puro
    QCOMPARE(repo.getConfigCaixa("gerente_pin_hash").size(), 64);

    QVERIFY(os.validarGerentePin("2468").ok);
    QVERIFY(!os.validarGerentePin("0000").ok);

    // 5 erros seguidos bloqueiam por alguns minutos, inclusive para o PIN certo
    for (int i = 0; i < 5; ++i)
        os.validarGerentePin("1111");
    const auto bloqueado = os.validarGerentePin("2468");
    QVERIFY(!bloqueado.ok);
    QVERIFY(bloqueado.msg.contains("bloqueado", Qt::CaseInsensitive));

    // limpeza para não afetar o que vier depois
    repo.setConfigCaixa("gerente_bloqueio_ate_" + terminalDoTeste(), "0");
    repo.setConfigCaixa("gerente_tentativas_" + terminalDoTeste(), "0");
    QVERIFY(os.validarGerentePin("2468").ok);
}

void TestCaixaService::gerente_precisa_de_identidade_no_cadastro()
{
    Operador_service os;
    const auto primeiro = os.cadastrar("Op Gerente", "3456");
    QVERIFY(primeiro.ok);

    // ninguém nasce gerente: o acesso de gerente só existe com marcação ou PIN geral
    QVERIFY(!os.getPorId(primeiro.id).gerente);
    QVERIFY(os.marcarGerente(primeiro.id, true).ok);
    QVERIFY(os.getPorId(primeiro.id).gerente);

    // não dá para tirar o acesso do único gerente ativo, senão a loja fica sem gerente
    QVERIFY(!os.marcarGerente(primeiro.id, false).ok);
    QVERIFY(os.getPorId(primeiro.id).gerente);

    // com dois gerentes, um deles pode ser rebaixado
    const auto segundo = os.cadastrar("Op Gerente 2", "3457");
    QVERIFY(segundo.ok);
    QVERIFY(os.marcarGerente(segundo.id, true).ok);
    QVERIFY(os.marcarGerente(segundo.id, false).ok);
    QVERIFY(!os.getPorId(segundo.id).gerente);
    QVERIFY(os.getPorId(primeiro.id).gerente);

    // e a marcação sobrevive ao cadastro: o repository devolve o campo
    QVERIFY(os.listar(true).size() > 0);
}

void TestCaixaService::login_por_pingeral_entra_como_gerente()
{
    Operador_service os;
    Operador_repository repo;
    repo.setConfigCaixa("gerente_bloqueio_ate_" + terminalDoTeste(), "0");
    repo.setConfigCaixa("gerente_tentativas_" + terminalDoTeste(), "0");
    QVERIFY(os.definirGerentePin("2468").ok);

    // PIN errado não abre sessão
    const SessaoDTO recusada = os.validarLogin(kOperadorGerenteId, "0000");
    QVERIFY(recusada.nomeOperador.isEmpty());

    const SessaoDTO gerente = os.validarLogin(kOperadorGerenteId, "2468");
    QVERIFY(gerente.valida());
    QCOMPARE(gerente.idOperador, kOperadorGerenteId);
    QVERIFY(gerente.gerente);
    QVERIFY(gerente.pinGeral);
    QCOMPARE(gerente.nomeOperador, QString::fromLatin1(kOperadorGerenteNome));

    // operador comum entra com o próprio PIN e só é gerente se estiver marcado
    const auto cad = os.cadastrar("Op Login", "1122");
    QVERIFY(cad.ok);
    const SessaoDTO comum = os.validarLogin(cad.id, "1122");
    QVERIFY(comum.valida());
    QCOMPARE(comum.idOperador, cad.id);
    QVERIFY(!comum.gerente);
    QVERIFY(!comum.pinGeral);

    QVERIFY(os.marcarGerente(cad.id, true).ok);
    QVERIFY(os.validarLogin(cad.id, "1122").gerente);
}

void TestCaixaService::sessao_grava_entrada_e_saida()
{
    TestDbFactory::garantirCaixaAberto();
    Sessao_repository repo;

    SessaoDTO sessao;
    sessao.idOperador = kOperadorGerenteId;
    sessao.nomeOperador = QStringLiteral("Gerente de Teste");
    sessao.gerente = true;
    sessao.terminal = QStringLiteral("TERM-TESTE-SESSAO");

    const qlonglong idLog = repo.registrarEntrada(sessao, 0);
    QVERIFY(idLog > 0);

    // enquanto não grava a saída, a sessão aparece como aberta
    QVERIFY(repo.fecharSessoesPendentes(sessao.terminal) >= 1);

    const QList<LogSessaoDTO> logs = repo.listar(10);
    QVERIFY(!logs.isEmpty());
    const LogSessaoDTO encontrado = [&logs, &sessao]() {
        for (const LogSessaoDTO &l : logs)
            if (l.terminal == sessao.terminal)
                return l;
        return LogSessaoDTO();
    }();
    QCOMPARE(encontrado.idOperador, kOperadorGerenteId);
    QVERIFY(encontrado.gerente);
    QVERIFY(!encontrado.saidaEm.isEmpty());
    QCOMPARE(encontrado.motivoSaida, QStringLiteral("SEM_LOGOUT"));
}

void TestCaixaService::venda_registra_o_operador_da_sessao_e_nao_o_dono_do_caixa()
{
    TestDbFactory::garantirCaixaAberto();
    Caixa_service cs;
    const CaixaDTO caixa = cs.caixaAtual();
    QVERIFY(caixa.aberto());

    // o dono do caixa é o operador do teste; quem vende é outro, logado na sessão
    Operador_service os;
    const auto vendedor = os.cadastrar("Op Vendedor", "5678");
    QVERIFY(vendedor.ok);

    QSqlDatabase db = DatabaseConnection_service::db();
    QSqlQuery query(db);
    query.prepare("INSERT INTO vendas2 (cliente, total, data_hora, forma_pagamento, valor_recebido, "
                  "troco, taxa, valor_final, desconto, esta_pago, id_caixa, id_operador_sessao) "
                  "VALUES ('Cliente', 10, '2026-01-01 10:00:00', 'Dinheiro', 10, 0, 0, 10, 0, 1, :caixa, :sessao)");
    query.bindValue(":caixa", caixa.id);
    query.bindValue(":sessao", vendedor.id);
    QVERIFY2(query.exec(), qPrintable(query.lastError().text()));

    // o dono do caixa é a resposta do cabeçalho e nunca entra na lista de "outros"
    const ResumoCaixaDTO resumo = cs.resumo(caixa.id);
    QCOMPARE(resumo.caixa.idOperador, caixa.idOperador);
    bool achouVendedor = false;
    for (const OperadorSessaoCaixaDTO &op : resumo.operadoresDaSessao) {
        QVERIFY(op.id != caixa.idOperador);
        if (op.id == vendedor.id) {
            achouVendedor = true;
            QCOMPARE(op.nome, QStringLiteral("Op Vendedor"));
            QCOMPARE(op.quantidade, 1LL);
        }
    }
    QVERIFY(achouVendedor);
}

namespace {
// cadastra um operador de teste e devolve uma sessão aberta para ele
qlonglong novoOperadorDeTeste(Operador_service &os, const QString &prefixo, const QString &pin)
{
    const auto r = os.cadastrar(prefixo + QUuid::createUuid().toString(QUuid::Id128).left(8), pin);
    return r.ok ? r.id : 0;
}

bool abrirSessaoDe(qlonglong id, Operador_service &os)
{
    SessaoDTO s;
    s.idOperador = id;
    s.nomeOperador = os.getPorId(id).nome;
    s.terminal = QStringLiteral("TERM-TESTE-SESSAO");
    QString erro;
    Sessao_service::instancia()->abrir(s, &erro);
    return Sessao_service::instancia()->ativa() && Sessao_service::instancia()->idOperador() == id;
}
}

void TestCaixaService::sessao_expira_e_novo_login_continua_valido()
{
    Operador_service os;
    const qlonglong a = novoOperadorDeTeste(os, "SessA ", "1357");
    const qlonglong b = novoOperadorDeTeste(os, "SessB ", "2468");
    QVERIFY(a > 0 && b > 0);
    auto *sess = Sessao_service::instancia();

    QVERIFY(abrirSessaoDe(a, os));
    // quem ouve o sinal de expiração faz o novo login na hora (como o MainWindow): a sessão nova
    // não pode ser derrubada pelo fim do encerramento da antiga
    connect(sess, &Sessao_service::sessaoExpirada, this, [&]() { abrirSessaoDe(b, os); },
            Qt::SingleShotConnection);
    sess->iniciarTimeout(true, 1);
    sess->simularInatividadeParaTeste(3600);
    sess->tique();
    QVERIFY(sess->ativa());
    QCOMPARE(sess->idOperador(), b);
    sess->pararTimeout();
    sess->encerrar("LOGOUT");
}

void TestCaixaService::sessao_bloqueia_e_desbloqueia_com_pin_do_operador_ou_do_gerente()
{
    Operador_service os;
    Operador_repository repo;
    repo.setConfigCaixa("gerente_bloqueio_ate_" + terminalDoTeste(), "0");
    repo.setConfigCaixa("gerente_tentativas_" + terminalDoTeste(), "0");
    QVERIFY(os.definirGerentePin("2468").ok);
    const qlonglong a = novoOperadorDeTeste(os, "Bloq ", "1357");
    QVERIFY(a > 0);
    auto *sess = Sessao_service::instancia();
    QVERIFY(abrirSessaoDe(a, os));

    sess->definirVendaEmAndamento(true);
    sess->iniciarTimeout(true, 1);
    sess->simularInatividadeParaTeste(3600);
    sess->tique();
    QVERIFY(sess->ativa());
    QVERIFY(sess->bloqueada());

    QVERIFY(!sess->desbloquear("0000").ok);
    QVERIFY(sess->bloqueada());
    QVERIFY(sess->desbloquear("1357").ok);
    QVERIFY(!sess->bloqueada());

    // bloqueia de novo e desbloqueia com o PIN do gerente (fica na auditoria)
    sess->simularInatividadeParaTeste(3600);
    sess->tique();
    QVERIFY(sess->bloqueada());
    QVERIFY(sess->desbloquear("2468").ok);
    bool achou = false;
    for (const LogAcaoDTO &l : sess->ultimasAcoes(50))
        if (l.acao == "DESBLOQUEIO_POR_GERENTE")
            achou = true;
    QVERIFY(achou);

    sess->definirVendaEmAndamento(false);
    sess->pararTimeout();
    sess->encerrar("LOGOUT");
    repo.setConfigCaixa("gerente_bloqueio_ate_" + terminalDoTeste(), "0");
    repo.setConfigCaixa("gerente_tentativas_" + terminalDoTeste(), "0");
}

void TestCaixaService::sessao_cai_quando_operador_e_desativado()
{
    Operador_service os;
    const qlonglong a = novoOperadorDeTeste(os, "Desat ", "1357");
    QVERIFY(a > 0);
    auto *sess = Sessao_service::instancia();
    QVERIFY(abrirSessaoDe(a, os));

    QVERIFY(os.definirAtivo(a, false).ok);
    sess->revalidar();
    QVERIFY(!sess->ativa());
}

void TestCaixaService::sessao_com_venda_em_andamento_so_bloqueia()
{
    Operador_service os;
    const qlonglong a = novoOperadorDeTeste(os, "Venda ", "1357");
    QVERIFY(a > 0);
    auto *sess = Sessao_service::instancia();
    QVERIFY(abrirSessaoDe(a, os));

    QObject tela;
    bool temItens = true;
    sess->registrarTelaDeVenda(&tela, [&]() { return temItens; });
    QVERIFY(sess->temVendaEmAndamento());
    sess->iniciarTimeout(true, 1);
    sess->simularInatividadeParaTeste(3600);
    sess->tique();
    QVERIFY(sess->ativa());        // bloqueou, não expirou: a venda não se perde
    QVERIFY(sess->bloqueada());

    sess->removerTelaDeVenda(&tela);
    QVERIFY(!sess->temVendaEmAndamento());
    sess->pararTimeout();
    sess->encerrar("LOGOUT");
}

void TestCaixaService::caixa_e_do_operador_logado_e_abre_sem_pin_novo()
{
    Operador_service os;
    Caixa_service cs;
    const qlonglong a = novoOperadorDeTeste(os, "CxA ", "1357");
    const qlonglong b = novoOperadorDeTeste(os, "CxB ", "2468");
    QVERIFY(a > 0 && b > 0);
    auto *sess = Sessao_service::instancia();

    QVERIFY(abrirSessaoDe(a, os));
    const auto ra = cs.abrirCaixa(a, QString(), 0, 0);        // logado: sem PIN
    QVERIFY2(ra.ok, qPrintable(ra.msg));
    QCOMPARE(cs.caixaAtual().id, ra.id);
    sess->encerrar("TROCA_OPERADOR");                         // sair não exige fechar o caixa

    QVERIFY(abrirSessaoDe(b, os));
    QVERIFY(!cs.caixaAtual().aberto());                       // o caixa de A não é de B
    QVERIFY(!cs.abrirCaixa(a, QString(), 0, 0).ok);           // abrir o de outro continua pedindo PIN
    const auto rb = cs.abrirCaixa(b, QString(), 0, 0);
    QVERIFY2(rb.ok, qPrintable(rb.msg));
    QCOMPARE(cs.caixaAtual().id, rb.id);

    QVERIFY(cs.fecharCaixa(rb.id, "2468", informadosIguaisAoEsperado(cs.resumo(rb.id)), "").ok);
    sess->encerrar("LOGOUT");
    QVERIFY(cs.fecharCaixa(ra.id, "1357", informadosIguaisAoEsperado(cs.resumo(ra.id)), "").ok);
}

void TestCaixaService::autorizador_bloqueia_administracao_sem_gerente()
{
    Operador_service os;
    Sessao_service::instancia()->encerrar("LOGOUT");
    bool liberar = false;
    Operador_service::definirAutorizador([&]() { return liberar; });
    const auto negado = os.cadastrar("Sem Permissao " + QUuid::createUuid().toString(QUuid::Id128).left(6), "1357");
    QVERIFY(!negado.ok);
    QVERIFY(negado.msg.contains("gerente", Qt::CaseInsensitive));
    liberar = true;
    const auto ok = os.cadastrar("Com Permissao " + QUuid::createUuid().toString(QUuid::Id128).left(6), "1357");
    QVERIFY(ok.ok);
    Operador_service::definirAutorizador(nullptr);
}
