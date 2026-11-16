#include <QtTest>
#include <QUuid>
#include "test_empresa_contas.h"
#include "services/empresa_service.h"
#include "services/config_service.h"
#include "services/contaspagar_service.h"
#include "services/sessao_service.h"
#include "repository/vendas_repository.h"
#include "infra/empresaativa.h"
#include "repository/relatorios_repository.h"
#include <QSqlQueryModel>
#include <QSqlQuery>
#include <QSqlError>
#include "services/Produto_service.h"
#include "infra/databaseconnection_service.h"

namespace {
QString cnpjDeTeste(int n)
{
    // 12 primeiros dígitos distintos por empresa + dígitos verificadores calculados
    QString base = QStringLiteral("%1").arg(10000000 + n, 8, 10, QLatin1Char('0')) + "0001";
    const int p1[12] = {5, 4, 3, 2, 9, 8, 7, 6, 5, 4, 3, 2};
    const int p2[13] = {6, 5, 4, 3, 2, 9, 8, 7, 6, 5, 4, 3, 2};
    auto dv = [&](const QString &s, const int *p, int tam) {
        int soma = 0;
        for (int i = 0; i < tam; ++i)
            soma += s.at(i).digitValue() * p[i];
        const int r = soma % 11;
        return r < 2 ? 0 : 11 - r;
    };
    base += QString::number(dv(base, p1, 12));
    base += QString::number(dv(base, p2, 13));
    return base;
}
const QDate kHoje = QDate::currentDate();
}

void TestEmpresaContas::initTestCase()
{
    Empresa_service::instancia()->carregarEstadoInicial();
    EmpresaAtiva::definir(1);
}

void TestEmpresaContas::cleanupTestCase()
{
    EmpresaAtiva::definir(1);
    Sessao_service::instancia()->definirVerificadorVenda(nullptr);
    ContasPagar_service::definirAutorizador(nullptr);
}

void TestEmpresaContas::cnpj_valida_digitos_verificadores()
{
    QVERIFY(Empresa_service::cnpjValido("11.222.333/0001-81"));
    QVERIFY(Empresa_service::cnpjValido("11222333000181"));
    QVERIFY(!Empresa_service::cnpjValido("11222333000182"));
    QVERIFY(!Empresa_service::cnpjValido("11111111111111"));
    QVERIFY(!Empresa_service::cnpjValido("123"));
    QVERIFY(Empresa_service::cnpjValido(cnpjDeTeste(1)));
}

void TestEmpresaContas::cadastrar_empresa_valida_nome_cnpj_e_duplicidade()
{
    auto *es = Empresa_service::instancia();
    const QString sufixo = QUuid::createUuid().toString(QUuid::Id128).left(6);
    const QString cnpj = cnpjDeTeste(QRandomGenerator::global()->bounded(1000, 80000000));

    QVERIFY(!es->cadastrar("A", cnpj).ok);                               // nome curto
    QVERIFY(!es->cadastrar("Loja " + sufixo, "12345678000100").ok);     // CNPJ inválido
    const auto ok = es->cadastrar("Loja " + sufixo, cnpj);
    QVERIFY2(ok.ok, qPrintable(ok.msg));
    QVERIFY(ok.id > 1);
    QVERIFY(!es->cadastrar("Outra " + sufixo, cnpj).ok);                 // CNPJ repetido
    QVERIFY(!es->cadastrar("loja " + sufixo.toUpper(), cnpjDeTeste(7)).ok);   // nome repetido (sem diferenciar maiúsculas)
    QCOMPARE(es->getPorId(ok.id).cnpj, cnpj);

    // desativar a empresa ativa não pode; outra pode e some da lista de ativas
    QVERIFY(!es->definirAtivaNoCadastro(es->idAtiva(), false).ok);
    QVERIFY(es->definirAtivaNoCadastro(ok.id, false).ok);
    bool aparece = false;
    for (const EmpresaDTO &e : es->listar(true))
        aparece = aparece || e.id == ok.id;
    QVERIFY(!aparece);
    QVERIFY(!es->trocarPara(ok.id).ok);                                  // desativada: não vira ativa
    QVERIFY(es->definirAtivaNoCadastro(ok.id, true).ok);
}

void TestEmpresaContas::troca_de_empresa_recusa_com_venda_em_andamento()
{
    auto *es = Empresa_service::instancia();
    const auto nova = es->cadastrar("Troca " + QUuid::createUuid().toString(QUuid::Id128).left(6),
                                    cnpjDeTeste(QRandomGenerator::global()->bounded(1000, 80000000)));
    QVERIFY(nova.ok);

    bool carrinho = true;
    Sessao_service::instancia()->definirVerificadorVenda([&]() { return carrinho; });
    Sessao_service::instancia()->definirVendaEmAndamento(true);
    const auto recusada = es->trocarPara(nova.id);
    QVERIFY(!recusada.ok);
    QCOMPARE(es->idAtiva(), 1LL);

    Sessao_service::instancia()->definirVendaEmAndamento(false);
    Sessao_service::instancia()->definirVerificadorVenda(nullptr);
    QSignalSpy spy(es, &Empresa_service::empresaMudou);
    const auto ok = es->trocarPara(nova.id);
    QVERIFY2(ok.ok, qPrintable(ok.msg));
    QCOMPARE(es->idAtiva(), nova.id);
    QCOMPARE(spy.count(), 1);
    QVERIFY(es->trocarPara(1).ok);
}

void TestEmpresaContas::configuracao_fiscal_e_separada_por_empresa()
{
    auto *es = Empresa_service::instancia();
    Config_service cs;
    const ConfigDTO original = cs.carregarTudo();

    const auto nova = es->cadastrar("Fiscal " + QUuid::createUuid().toString(QUuid::Id128).left(6),
                                    cnpjDeTeste(QRandomGenerator::global()->bounded(1000, 80000000)));
    QVERIFY(nova.ok);
    QVERIFY(es->trocarPara(nova.id).ok);

    // a empresa nova começa sem dados fiscais e sem emissão ligada
    ConfigDTO daNova = cs.carregarTudo();
    QVERIFY(daNova.nomeEmpresa.isEmpty());
    QVERIFY(!daNova.emitNfFiscal);
    QVERIFY(daNova.certificadoPathFiscal.isEmpty());

    daNova.nomeEmpresa = "Empresa Nova LTDA";
    daNova.cnpjEmpresa = cnpjDeTeste(55);
    daNova.nnfProdFiscal = 777;
    QString erro;
    QVERIFY2(cs.salvarTudo(daNova, erro), qPrintable(erro));

    // volta para a original: nada mudou nela
    QVERIFY(es->trocarPara(1).ok);
    const ConfigDTO voltou = cs.carregarTudo();
    QCOMPARE(voltou.nomeEmpresa, original.nomeEmpresa);
    QCOMPARE(voltou.nnfProdFiscal, original.nnfProdFiscal);

    // e a da nova continua guardada
    QVERIFY(es->trocarPara(nova.id).ok);
    QCOMPARE(cs.carregarTudo().nnfProdFiscal, 777);
    QCOMPARE(es->getPorId(nova.id).razaoSocial, QStringLiteral("Empresa Nova LTDA"));
    QVERIFY(es->trocarPara(1).ok);
}

void TestEmpresaContas::venda_grava_a_empresa_ativa()
{
    auto *es = Empresa_service::instancia();
    const auto nova = es->cadastrar("Vendas " + QUuid::createUuid().toString(QUuid::Id128).left(6),
                                    cnpjDeTeste(QRandomGenerator::global()->bounded(1000, 80000000)));
    QVERIFY(nova.ok);
    Vendas_repository repo;

    auto novaVenda = []() {
        VendasDTO v;
        v.clienteNome = "Cliente";
        v.dataHora = "2026-01-01 10:00:00";
        v.total = 10;
        v.valorFinal = 10;
        v.formaPagamento = "Dinheiro";
        v.estaPago = true;
        v.idCliente = 1;
        return v;
    };

    const qlonglong naOriginal = repo.inserir(novaVenda());
    QVERIFY(naOriginal > 0);
    QCOMPARE(repo.getVenda(naOriginal).idEmpresa, 1LL);

    QVERIFY(es->trocarPara(nova.id).ok);
    const qlonglong naNova = repo.inserir(novaVenda());
    QVERIFY(naNova > 0);
    QCOMPARE(repo.getVenda(naNova).idEmpresa, nova.id);
    // a venda antiga continua na empresa dela, mesmo depois da troca
    QCOMPARE(repo.getVenda(naOriginal).idEmpresa, 1LL);
    QVERIFY(es->trocarPara(1).ok);
}

void TestEmpresaContas::relatorios_e_lista_de_vendas_filtram_pela_empresa()
{
    auto *es = Empresa_service::instancia();
    const auto nova = es->cadastrar("Relat " + QUuid::createUuid().toString(QUuid::Id128).left(6),
                                    cnpjDeTeste(QRandomGenerator::global()->bounded(1000, 80000000)));
    QVERIFY(nova.ok);
    Vendas_repository vendas;
    Relatorios_repository rel;
    const QString dia = QStringLiteral("2031-03-15");

    auto venda = [&](double valor, qlonglong empresa) {
        VendasDTO v;
        v.clienteNome = "Cliente";
        v.dataHora = dia + " 10:00:00";
        v.total = valor;
        v.valorFinal = valor;
        v.formaPagamento = "Dinheiro";
        v.estaPago = true;
        v.idCliente = 1;
        v.idEmpresa = empresa;
        return vendas.inserir(v);
    };
    QVERIFY(venda(10, 1) > 0);
    QVERIFY(venda(70, nova.id) > 0);
    QVERIFY(venda(30, nova.id) > 0);

    const QDate de(2031, 3, 1), ate(2031, 3, 31);

    EmpresaAtiva::definir(1);
    QCOMPARE(rel.buscarQuantVendasPeriodo(de, ate, Agrupamento::Dia).value(dia), 1);
    QCOMPARE(rel.buscarValorVendasPeriodo(de, ate, Agrupamento::Dia).value(dia).first, 10.0);
    QCOMPARE(rel.buscarFormasPagamentoPeriodo(de, ate, Agrupamento::Dia).value("Dinheiro").value(dia), 1);

    EmpresaAtiva::definir(nova.id);
    QCOMPARE(rel.buscarQuantVendasPeriodo(de, ate, Agrupamento::Dia).value(dia), 2);
    QCOMPARE(rel.buscarValorVendasPeriodo(de, ate, Agrupamento::Dia).value(dia).first, 100.0);

    // as demais consultas só precisam executar sem erro de SQL para a empresa em uso
    rel.buscarTopProdutosVendidosPeriodo(de, ate);
    rel.buscarValoresNfPeriodo(de, ate, Agrupamento::Dia, 1);
    rel.produtosMaisLucrativosPeriodo(de, ate);
    rel.buscarLucroPeriodo(de, ate, Agrupamento::Dia);
    rel.buscarClientesInadimplentes();

    // lista de vendas: só as da empresa em uso
    QSqlQueryModel modelo;
    vendas.listarVendasDeAteFormaPagamento(&modelo, dia + " 00:00:00", dia + " 23:59:59",
                                           VendasUtil::VendasFormaPagamento::Nenhuma, 0);
    QCOMPARE(modelo.rowCount(), 2);
    const ResumoVendasDTO resumoNova = vendas.calcularResumo(dia + " 00:00:00", dia + " 23:59:59", false, 0);
    QCOMPARE(resumoNova.quantidade, 2);
    QCOMPARE(resumoNova.total, 100.0);

    EmpresaAtiva::definir(1);
    vendas.listarVendasDeAteFormaPagamento(&modelo, dia + " 00:00:00", dia + " 23:59:59",
                                           VendasUtil::VendasFormaPagamento::Nenhuma, 0);
    QCOMPARE(modelo.rowCount(), 1);
}

void TestEmpresaContas::editar_empresa_corrige_nome_cnpj_e_valida_duplicidade()
{
    auto *es = Empresa_service::instancia();
    const QString sufixo = QUuid::createUuid().toString(QUuid::Id128).left(6);
    const QString cnpjA = cnpjDeTeste(QRandomGenerator::global()->bounded(1000, 40000000));
    const QString cnpjB = cnpjDeTeste(QRandomGenerator::global()->bounded(40000001, 80000000));
    const auto a = es->cadastrar("Edita A " + sufixo, cnpjA);
    const auto b = es->cadastrar("Edita B " + sufixo, cnpjB);
    QVERIFY(a.ok && b.ok);

    QVERIFY(!es->atualizar(a.id, "X", cnpjA, "").ok);                       // nome curto
    QVERIFY(!es->atualizar(a.id, "Edita A " + sufixo, "123", "").ok);       // CNPJ inválido
    QVERIFY(!es->atualizar(a.id, "Edita A " + sufixo, "", "").ok);          // não apaga o CNPJ
    QVERIFY(!es->atualizar(a.id, "Edita A " + sufixo, cnpjB, "").ok);       // CNPJ de outra empresa
    QVERIFY(!es->atualizar(a.id, "edita b " + sufixo, cnpjA, "").ok);       // nome de outra empresa
    QVERIFY(!es->atualizar(99999999, "Qualquer", cnpjA, "").ok);

    // corrigir o próprio cadastro (mesmo nome/CNPJ dela não conta como duplicado)
    const QString novoCnpj = cnpjDeTeste(QRandomGenerator::global()->bounded(1000, 40000000) + 5);
    QSignalSpy spy(es, &Empresa_service::empresaMudou);
    const auto ok = es->atualizar(a.id, "Edita A Certo " + sufixo, novoCnpj, "Edita A LTDA");
    QVERIFY2(ok.ok, qPrintable(ok.msg));
    const EmpresaDTO depois = es->getPorId(a.id);
    QCOMPARE(depois.apelido, "Edita A Certo " + sufixo);
    QCOMPARE(depois.cnpj, novoCnpj);
    QCOMPARE(depois.razaoSocial, QStringLiteral("Edita A LTDA"));
    QCOMPARE(spy.count(), 0);                      // não é a empresa em uso: nada a recarregar

    // a configuração fiscal da empresa acompanha o nome/CNPJ corrigidos
    QVERIFY(es->trocarPara(a.id).ok);
    Config_service cs;
    const ConfigDTO cfg = cs.carregarTudo();
    QCOMPARE(cfg.cnpjEmpresa, novoCnpj);
    QCOMPARE(cfg.nomeEmpresa, QStringLiteral("Edita A LTDA"));
    QVERIFY(es->trocarPara(1).ok);

    // reativar empresa desativada
    QVERIFY(es->definirAtivaNoCadastro(b.id, false).ok);
    QVERIFY(es->definirAtivaNoCadastro(b.id, true).ok);
    QVERIFY(es->getPorId(b.id).ativa);
    QCOMPARE(es->quantidadeDeVendas(b.id), 0);
}

void TestEmpresaContas::produto_vendido_nao_e_apagado_e_lista_as_vendas()
{
    QSqlDatabase db = DatabaseConnection_service::db();
    const QString desc = "PRODUTO VENDIDO " + QUuid::createUuid().toString(QUuid::Id128).left(6);
    auto novoProduto = [&](const QString &d) {
        QSqlQuery q(db);
        q.prepare("INSERT INTO produtos (quantidade, descricao, preco, codigo_barras, nf, un_comercial, "
                  "preco_fornecedor, porcent_lucro, ncm, cest, aliquota_imposto, csosn, pis, adicionado_em, atualizado_em) "
                  "VALUES (5, :d, 10, :cb, 0, 'UN', 5, 0, '', '', 0, '', '', '2025-01-01 00:00:00', '2025-01-01 00:00:00')");
        q.bindValue(":d", d);
        q.bindValue(":cb", QUuid::createUuid().toString(QUuid::Id128).left(12));
        [&]() { QVERIFY2(q.exec(), qPrintable(q.lastError().text())); }();
        return q.lastInsertId().toLongLong();
    };
    const qlonglong idProd = novoProduto(desc);
    const qlonglong idLivre = novoProduto(desc + " LIVRE");
    QVERIFY(idProd > 0 && idLivre > 0);

    // uma venda na empresa original
    Vendas_repository vendas;
    VendasDTO v;
    v.clienteNome = "Cliente Teste";
    v.dataHora = "2031-05-10 09:30:00";
    v.total = 20;
    v.valorFinal = 20;
    v.formaPagamento = "Pix";
    v.estaPago = true;
    v.idCliente = 1;
    const qlonglong idVenda = vendas.inserir(v);
    QVERIFY(idVenda > 0);
    QSqlQuery pv(db);
    pv.prepare("INSERT INTO produtos_vendidos (id_produto, id_venda, quantidade, preco_vendido, adicionado_em, "
               "atualizado_em, emitido_nf) VALUES (:p, :v, 2, 10, '2031-05-10 09:30:00', '2031-05-10 09:30:00', 0)");
    pv.bindValue(":p", idProd);
    pv.bindValue(":v", idVenda);
    QVERIFY2(pv.exec(), qPrintable(pv.lastError().text()));

    Produto_Service ps;
    const QList<ProdutoVendaRefDTO> refs = ps.vendasDoProduto(QString::number(idProd));
    QCOMPARE(refs.size(), 1);
    QCOMPARE(refs.first().idVenda, idVenda);
    QCOMPARE(refs.first().cliente, QStringLiteral("Cliente Teste"));
    QCOMPARE(refs.first().quantidade, 2.0);
    QCOMPARE(refs.first().valorFinalVenda, 20.0);
    QCOMPARE(refs.first().formaPagamento, QStringLiteral("Pix"));
    QCOMPARE(refs.first().idEmpresa, 1LL);

    const auto negado = ps.deletar(QString::number(idProd));
    QVERIFY(!negado.ok);
    QCOMPARE(negado.erro, ProdutoErro::EmUso);
    QVERIFY(negado.msg.contains("1 venda"));

    // produto sem venda continua podendo ser apagado
    QVERIFY(ps.vendasDoProduto(QString::number(idLivre)).isEmpty());
    QVERIFY(ps.deletar(QString::number(idLivre)).ok);

    // cancelando a venda (some o item), o produto pode ser apagado
    QSqlQuery del(db);
    QVERIFY(del.exec(QString("DELETE FROM produtos_vendidos WHERE id_venda = %1").arg(idVenda)));
    QVERIFY(ps.deletar(QString::number(idProd)).ok);
}

void TestEmpresaContas::dividir_valor_soma_exata()
{
    const QList<double> v = ContasPagar_service::dividirValor(100.0, 3);
    QCOMPARE(v.size(), 3);
    QCOMPARE(v.at(0), 33.33);
    QCOMPARE(v.at(1), 33.33);
    QCOMPARE(v.at(2), 33.34);
    double soma = 0;
    for (double x : ContasPagar_service::dividirValor(1000.01, 7))
        soma += x;
    QCOMPARE(qRound64(soma * 100), 100001LL);
    QCOMPARE(ContasPagar_service::dividirValor(50, 1).value(0), 50.0);
}

void TestEmpresaContas::lancar_parcelado_gera_vencimentos_mensais_sem_deriva()
{
    ContasPagar_service cs;
    ContaPagarDTO c;
    c.descricao = "Compra parcelada " + QUuid::createUuid().toString(QUuid::Id128).left(6);
    c.fornecedor = "Fornecedor X";
    c.valor = 100;
    c.vencimento = "2026-01-31";
    const auto r = cs.lancar(c, 3);
    QVERIFY2(r.ok, qPrintable(r.msg));
    QCOMPARE(r.quantidade, 3);

    FiltroContasPagarDTO f;
    f.texto = c.descricao;
    const QList<ContaPagarDTO> lista = cs.listar(f);
    QCOMPARE(lista.size(), 3);
    QCOMPARE(lista.at(0).vencimento, QStringLiteral("2026-01-31"));
    QCOMPARE(lista.at(1).vencimento, QStringLiteral("2026-02-28"));   // fevereiro recua...
    QCOMPARE(lista.at(2).vencimento, QStringLiteral("2026-03-31"));   // ...e março volta ao dia 31 (sem deriva)
    QCOMPARE(lista.at(2).valor, 33.34);
    QCOMPARE(lista.at(0).parcela, 1);
    QCOMPARE(lista.at(2).totalParcelas, 3);
    QVERIFY(!lista.at(0).grupo.isEmpty());
    QCOMPARE(lista.at(0).grupo, lista.at(2).grupo);
    QCOMPARE(lista.at(0).idEmpresa, EmpresaAtiva::id());

    // semanal e quinzenal
    ContaPagarDTO s = c;
    s.descricao = "Semanal " + QUuid::createUuid().toString(QUuid::Id128).left(6);
    s.vencimento = "2026-05-01";
    QVERIFY(cs.lancar(s, 3, ContasPagar_service::Periodicidade::Semanal).ok);
    FiltroContasPagarDTO f2;
    f2.texto = s.descricao;
    const auto sem = cs.listar(f2);
    QCOMPARE(sem.at(1).vencimento, QStringLiteral("2026-05-08"));
    QCOMPARE(sem.at(2).vencimento, QStringLiteral("2026-05-15"));
}

void TestEmpresaContas::lancar_valida_campos()
{
    ContasPagar_service cs;
    ContaPagarDTO c;
    c.descricao = "Luz";
    c.valor = 50;
    c.vencimento = kHoje.toString(Qt::ISODate);

    ContaPagarDTO semDesc = c; semDesc.descricao = " ";
    QVERIFY(!cs.lancar(semDesc).ok);
    ContaPagarDTO semValor = c; semValor.valor = 0;
    QVERIFY(!cs.lancar(semValor).ok);
    ContaPagarDTO semData = c; semData.vencimento = "31/12/2026";
    QVERIFY(!cs.lancar(semData).ok);
    QVERIFY(!cs.lancar(c, 0).ok);
    QVERIFY(!cs.lancar(c, 121).ok);
    ContaPagarDTO centavo = c; centavo.valor = 0.05;
    QVERIFY(!cs.lancar(centavo, 12).ok);       // não dá para dividir 5 centavos em 12
    QVERIFY(cs.lancar(c).ok);
}

void TestEmpresaContas::baixar_estornar_e_cancelar_seguem_as_regras()
{
    ContasPagar_service cs;
    ContaPagarDTO c;
    c.descricao = "Baixa " + QUuid::createUuid().toString(QUuid::Id128).left(6);
    c.valor = 200;
    c.vencimento = kHoje.toString(Qt::ISODate);
    const auto lanc = cs.lancar(c);
    QVERIFY(lanc.ok);

    QVERIFY(!cs.baixar(lanc.id, 0, "Pix").ok);
    QVERIFY(!cs.baixar(lanc.id, 200, " ").ok);
    QVERIFY(cs.baixar(lanc.id, 205.5, "Pix").ok);          // pagou com juros
    ContaPagarDTO paga = cs.getConta(lanc.id);
    QCOMPARE(paga.status, QStringLiteral("PAGA"));
    QCOMPARE(paga.valorPago, 205.5);
    QCOMPARE(paga.formaPagamento, QStringLiteral("Pix"));
    QVERIFY(!paga.pagoEm.isEmpty());
    QVERIFY(!cs.baixar(lanc.id, 200, "Pix").ok);           // não baixa duas vezes
    QVERIFY(!cs.cancelar(lanc.id, "motivo qualquer").ok);  // paga não cancela
    // conta paga: corrige só os textos; valor e vencimento ficam como estão
    ContaPagarDTO corrigida = paga;
    corrigida.descricao = c.descricao + " (corrigida)";
    corrigida.fornecedor = "Fornecedor certo";
    corrigida.valor = 1;
    corrigida.vencimento = "2020-01-01";
    QVERIFY(cs.alterar(corrigida).ok);
    const ContaPagarDTO depois = cs.getConta(lanc.id);
    QCOMPARE(depois.descricao, c.descricao + " (corrigida)");
    QCOMPARE(depois.fornecedor, QStringLiteral("Fornecedor certo"));
    QCOMPARE(depois.valor, 200.0);
    QCOMPARE(depois.vencimento, c.vencimento);
    paga = depois;

    // só gerente estorna
    bool gerente = false;
    ContasPagar_service::definirAutorizador([&]() { return gerente; });
    QVERIFY(!cs.estornarBaixa(lanc.id).ok);
    gerente = true;
    QVERIFY(cs.estornarBaixa(lanc.id).ok);
    ContaPagarDTO aberta = cs.getConta(lanc.id);
    QCOMPARE(aberta.status, QStringLiteral("ABERTA"));
    QCOMPARE(aberta.valorPago, 0.0);
    QVERIFY(aberta.pagoEm.isEmpty());

    // cancelamento (exclusão): qualquer operador, mas com motivo
    gerente = false;
    QVERIFY(!cs.cancelar(lanc.id, "ab").ok);
    QVERIFY(cs.cancelar(lanc.id, "lançada em duplicidade").ok);
    QCOMPARE(cs.getConta(lanc.id).status, QStringLiteral("CANCELADA"));
    QVERIFY(!cs.baixar(lanc.id, 200, "Pix").ok);           // cancelada não baixa
    QVERIFY(!cs.alterar(cs.getConta(lanc.id)).ok);         // nem altera

    // alterar conta em aberto
    const auto outra = cs.lancar(c);
    ContaPagarDTO alvo = cs.getConta(outra.id);
    alvo.valor = 250;
    alvo.vencimento = kHoje.addDays(5).toString(Qt::ISODate);
    QVERIFY(cs.alterar(alvo).ok);
    QCOMPARE(cs.getConta(outra.id).valor, 250.0);
    ContasPagar_service::definirAutorizador(nullptr);
}

void TestEmpresaContas::listagem_e_resumo_por_empresa_e_vencimento()
{
    auto *es = Empresa_service::instancia();
    const auto nova = es->cadastrar("Contas " + QUuid::createUuid().toString(QUuid::Id128).left(6),
                                    cnpjDeTeste(QRandomGenerator::global()->bounded(1000, 80000000)));
    QVERIFY(nova.ok);
    ContasPagar_service cs;
    const QString marca = QUuid::createUuid().toString(QUuid::Id128).left(8);

    auto lanc = [&](const QString &nome, double valor, int diasAteVencer) {
        ContaPagarDTO c;
        c.descricao = marca + " " + nome;
        c.valor = valor;
        c.vencimento = kHoje.addDays(diasAteVencer).toString(Qt::ISODate);
        return cs.lancar(c);
    };

    // empresa original
    QVERIFY(lanc("vencida", 100, -3).ok);
    QVERIFY(lanc("hoje", 50, 0).ok);
    QVERIFY(lanc("semana", 30, 4).ok);
    QVERIFY(lanc("longe", 20, 40).ok);
    // outra empresa
    QVERIFY(es->trocarPara(nova.id).ok);
    QVERIFY(lanc("outra empresa", 999, -1).ok);
    QVERIFY(es->trocarPara(1).ok);

    FiltroContasPagarDTO f;
    f.texto = marca;
    QCOMPARE(cs.listar(f).size(), 5);                       // todas as empresas
    f.idEmpresa = 1;
    QCOMPARE(cs.listar(f).size(), 4);
    f.idEmpresa = nova.id;
    QCOMPARE(cs.listar(f).size(), 1);

    f.idEmpresa = 1;
    f.status = "VENCIDA";
    const auto vencidas = cs.listar(f);
    QCOMPARE(vencidas.size(), 1);
    QVERIFY(vencidas.first().vencida());
    QCOMPARE(vencidas.first().valor, 100.0);

    f.status.clear();
    f.vencimentoDe = kHoje.toString(Qt::ISODate);
    f.vencimentoAte = kHoje.addDays(7).toString(Qt::ISODate);
    QCOMPARE(cs.listar(f).size(), 2);                       // hoje + semana

    // o resumo é por empresa (outras contas abertas do banco entram também: compara a diferença)
    const ResumoContasPagarDTO daNova = cs.resumo(nova.id);
    QVERIFY(daNova.qtdVencidas >= 1);
    QVERIFY(daNova.valorVencidas >= 999.0);
    const ResumoContasPagarDTO daOriginal = cs.resumo(1);
    QVERIFY(daOriginal.qtdVencidas >= 1);
    QVERIFY(daOriginal.qtdHoje >= 1);
    QVERIFY(daOriginal.qtdProximos7Dias >= 1);
    QVERIFY(daOriginal.qtdAbertas >= 4);
    QVERIFY(daOriginal.valorAbertas >= 200.0);
}
