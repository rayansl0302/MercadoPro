#include "caixa_service.h"
#include "sessao_service.h"
#include <QSysInfo>
#include <QLocale>
#include <QDateTime>
#include <cmath>

const QStringList Caixa_service::FormasCaixa = {"Dinheiro", "Crédito", "Débito", "Pix"};

namespace {
double arredondar2(double v)
{
    return std::round(v * 100.0) / 100.0;
}
}

Caixa_service::Caixa_service(QObject *parent)
    : QObject{parent}
{
    cfg = confServ.carregarTudo();
}

QString Caixa_service::terminalAtual()
{
    const QString host = QSysInfo::machineHostName().trimmed();
    return host.isEmpty() ? QStringLiteral("TERMINAL") : host.toUpper();
}

CaixaDTO Caixa_service::caixaAtual()
{
    // o caixa é de quem está logado: o de outro operador (aberto no mesmo computador) não atrapalha
    Sessao_service *sessao = Sessao_service::instancia();
    if (sessao->ativa() && sessao->idOperador() > 0) {
        const CaixaDTO c = repo.getAbertoDoOperador(sessao->idOperador());
        return (c.aberto() && c.terminal == terminalAtual()) ? c : CaixaDTO();
    }
    // sem sessão (ou PIN geral, que não é um operador): o caixa aberto mais recente deste terminal
    return repo.getAbertoNoTerminal(terminalAtual());
}

CaixaDTO Caixa_service::getCaixa(qlonglong id)
{
    return repo.getPorId(id);
}

double Caixa_service::sugerirTrocoInicial()
{
    return repo.getDinheiroInformadoUltimoFechamento(terminalAtual());
}

Caixa_service::Resultado Caixa_service::abrirCaixa(qlonglong idOperador, const QString &pin,
                                                   double trocoInicial, double trocoSugerido)
{
    if (idOperador <= 0)
        return {false, "Selecione o operador."};
    if (trocoInicial < 0)
        return {false, "O troco inicial não pode ser negativo."};

    // quem já entrou no sistema abre o próprio caixa sem digitar o PIN de novo; abrir o caixa de
    // outro operador (ou sem sessão) continua pedindo o PIN dele
    Sessao_service *sessao = Sessao_service::instancia();
    const bool daSessao = sessao->ativa() && !sessao->pinGeral() && !sessao->bloqueada() &&
                          sessao->idOperador() == idOperador;
    if (daSessao) {
        const OperadorDTO op = operadorServ.getPorId(idOperador);
        if (op.id <= 0 || !op.ativo || op.bloqueado)
            return {false, "Operador inválido, desativado ou bloqueado."};
    } else {
        const auto pinOk = operadorServ.validarPin(idOperador, pin);
        if (!pinOk.ok)
            return {false, pinOk.msg};
    }

    const CaixaDTO doOperador = repo.getAbertoDoOperador(idOperador);
    if (doOperador.aberto())
        return {false, QString("Este operador já tem um caixa aberto no terminal %1. "
                               "Feche-o antes de abrir outro.").arg(doOperador.terminal)};

    CaixaDTO novo;
    novo.idOperador = idOperador;
    novo.terminal = terminalAtual();
    novo.trocoInicial = arredondar2(trocoInicial);
    novo.trocoSugerido = arredondar2(trocoSugerido);

    QString erro;
    const qlonglong id = repo.abrir(novo, &erro);
    if (id <= 0) {
        // o banco impede dois caixas abertos do mesmo operador, caso dois computadores abram juntos
        if (erro.contains("unique", Qt::CaseInsensitive) || erro.contains("duplicate", Qt::CaseInsensitive))
            return {false, "Já existe um caixa aberto para este operador "
                           "(aberto agora em outro computador). Atualize e tente de novo."};
        return {false, "Não foi possível abrir o caixa: " + erro};
    }
    return {true, "Caixa aberto.", id};
}

Caixa_service::Resultado Caixa_service::exigirCaixaAberto()
{
    const CaixaDTO caixa = caixaAtual();
    if (!caixa.aberto())
        return {false, "Você não tem caixa aberto. Abra o caixa em Caixa > Abrir caixa antes de vender."};
    return {true, QString(), caixa.id};
}

Caixa_service::Resultado Caixa_service::registrarMovimentacaoSimples(const QString &tipo, double valor,
                                                                     const QString &motivo)
{
    const CaixaDTO caixa = caixaAtual();
    if (!caixa.aberto())
        return {false, "Você não tem caixa aberto."};
    if (valor <= 0)
        return {false, "Informe um valor maior que zero."};
    if (motivo.trimmed().isEmpty())
        return {false, "Informe o motivo."};

    MovimentacaoCaixaDTO mov;
    mov.idCaixa = caixa.id;
    mov.tipo = tipo;
    mov.valor = arredondar2(valor);
    mov.formaPagamento = "Dinheiro";
    mov.motivo = motivo.trimmed();
    mov.idOperador = caixa.idOperador;
    mov.idOperadorSessao = Sessao_service::instancia()->idOperador();

    QString erro;
    const qlonglong id = repo.inserirMovimentacao(mov, &erro);
    if (id <= 0)
        return {false, "Não foi possível registrar: " + erro};
    return {true, tipo == "SANGRIA" ? "Sangria registrada." : "Suprimento registrado.", id};
}

Caixa_service::Resultado Caixa_service::registrarSangria(double valor, const QString &motivo)
{
    return registrarMovimentacaoSimples("SANGRIA", valor, motivo);
}

Caixa_service::Resultado Caixa_service::registrarSuprimento(double valor, const QString &motivo)
{
    return registrarMovimentacaoSimples("SUPRIMENTO", valor, motivo);
}

QList<MovimentacaoCaixaDTO> Caixa_service::sangriasESuprimentosDoCaixa(qlonglong idCaixa)
{
    QList<MovimentacaoCaixaDTO> lista;
    for (const MovimentacaoCaixaDTO &m : repo.listarMovimentacoes(idCaixa))
        if (m.tipo == "SANGRIA" || m.tipo == "SUPRIMENTO")
            lista << m;
    return lista;
}

namespace {
QString dinheiroBr(double v) { return "R$ " + QLocale(QLocale::Portuguese, QLocale::Brazil).toString(v, 'f', 2); }
}

Caixa_service::Resultado Caixa_service::editarSangriaSuprimento(qlonglong idMovimentacao, double novoValor,
                                                                const QString &novoMotivo,
                                                                const QString &motivoDaCorrecao)
{
    const MovimentacaoCaixaDTO mov = repo.getMovimentacao(idMovimentacao);
    if (mov.id <= 0 || (mov.tipo != "SANGRIA" && mov.tipo != "SUPRIMENTO"))
        return {false, "Lançamento não encontrado."};
    if (!repo.getPorId(mov.idCaixa).aberto())
        return {false, QString("Este lançamento é do caixa #%1, já fechado, e não pode ser alterado.").arg(mov.idCaixa)};
    if (novoValor <= 0)
        return {false, "Informe um valor maior que zero."};
    if (novoMotivo.trimmed().isEmpty())
        return {false, "Informe o motivo."};
    if (motivoDaCorrecao.trimmed().size() < 3)
        return {false, "Informe por que está corrigindo."};
    QString erro;
    if (!repo.atualizarMovimentacao(idMovimentacao, arredondar2(novoValor), novoMotivo.trimmed(), &erro))
        return {false, "Não foi possível alterar: " + erro};
    Sessao_service::instancia()->registrarAcao(
        mov.tipo == "SANGRIA" ? QStringLiteral("SANGRIA_EDITADA") : QStringLiteral("SUPRIMENTO_EDITADO"),
        QStringLiteral("Caixa #%1, %2 #%3: de %4 (%5) para %6 (%7). Motivo da correção: %8")
            .arg(mov.idCaixa).arg(mov.tipo.toLower()).arg(mov.id)
            .arg(dinheiroBr(mov.valor), mov.motivo, dinheiroBr(arredondar2(novoValor)), novoMotivo.trimmed(),
                 motivoDaCorrecao.trimmed()));
    return {true, "Lançamento alterado.", idMovimentacao};
}

Caixa_service::Resultado Caixa_service::removerSangriaSuprimento(qlonglong idMovimentacao,
                                                                 const QString &motivoDaRemocao)
{
    const MovimentacaoCaixaDTO mov = repo.getMovimentacao(idMovimentacao);
    if (mov.id <= 0 || (mov.tipo != "SANGRIA" && mov.tipo != "SUPRIMENTO"))
        return {false, "Lançamento não encontrado."};
    if (!repo.getPorId(mov.idCaixa).aberto())
        return {false, QString("Este lançamento é do caixa #%1, já fechado, e não pode ser removido.").arg(mov.idCaixa)};
    if (motivoDaRemocao.trimmed().size() < 3)
        return {false, "Informe por que está removendo."};
    QString erro;
    if (!repo.deletarMovimentacao(idMovimentacao, &erro))
        return {false, "Não foi possível remover: " + erro};
    Sessao_service::instancia()->registrarAcao(
        mov.tipo == "SANGRIA" ? QStringLiteral("SANGRIA_REMOVIDA") : QStringLiteral("SUPRIMENTO_REMOVIDO"),
        QStringLiteral("Caixa #%1, %2 #%3 de %4 (%5) removido. Motivo: %6")
            .arg(mov.idCaixa).arg(mov.tipo.toLower()).arg(mov.id)
            .arg(dinheiroBr(mov.valor), mov.motivo, motivoDaRemocao.trimmed()));
    return {true, "Lançamento removido.", idMovimentacao};
}

Caixa_service::Resultado Caixa_service::registrarRecebimento(qlonglong idVenda, qlonglong idEntradaVenda,
                                                             const QString &forma, double valor)
{
    const CaixaDTO caixa = caixaAtual();
    if (!caixa.aberto())
        return {false, "Você não tem caixa aberto. Abra o caixa antes de receber."};

    MovimentacaoCaixaDTO mov;
    mov.idCaixa = caixa.id;
    mov.tipo = "RECEBIMENTO";
    mov.valor = arredondar2(valor);
    mov.formaPagamento = forma;
    mov.motivo = QString("Recebimento da venda a prazo #%1").arg(idVenda);
    mov.idVenda = idVenda;
    mov.idEntradaVenda = idEntradaVenda;
    mov.idOperador = caixa.idOperador;
    mov.idOperadorSessao = Sessao_service::instancia()->idOperador();

    QString erro;
    const qlonglong id = repo.inserirMovimentacao(mov, &erro);
    if (id <= 0)
        return {false, "Não foi possível lançar o recebimento no caixa: " + erro};
    return {true, "Recebimento lançado no caixa.", caixa.id};
}

Caixa_service::Resultado Caixa_service::registrarRecebimentoDivida(qlonglong idDivida, qlonglong idPagamento,
                                                                   const QString &forma, double valor,
                                                                   const QString &descricao)
{
    const CaixaDTO caixa = caixaAtual();
    if (!caixa.aberto())
        return {false, "Você não tem caixa aberto. Abra o caixa antes de receber."};

    MovimentacaoCaixaDTO mov;
    mov.idCaixa = caixa.id;
    mov.tipo = "RECEBIMENTO";
    mov.valor = arredondar2(valor);
    mov.formaPagamento = forma;
    mov.motivo = descricao.isEmpty() ? QString("Recebimento da dívida #%1").arg(idDivida) : descricao;
    mov.idPagamentoDivida = idPagamento;
    mov.idOperador = caixa.idOperador;
    mov.idOperadorSessao = Sessao_service::instancia()->idOperador();

    QString erro;
    const qlonglong id = repo.inserirMovimentacao(mov, &erro);
    if (id <= 0)
        return {false, "Não foi possível lançar o recebimento no caixa: " + erro};
    return {true, "Recebimento lançado no caixa.", caixa.id};
}

Caixa_service::Resultado Caixa_service::podeEstornarRecebimentoDivida(qlonglong idPagamento)
{
    const MovimentacaoCaixaDTO mov = repo.getMovimentacaoPorPagamentoDivida(idPagamento);
    if (mov.id <= 0)
        return {true, "Recebimento sem vínculo com caixa."};
    const CaixaDTO caixa = repo.getPorId(mov.idCaixa);
    if (!caixa.aberto())
        return {false, QString("Este recebimento pertence ao caixa #%1, já fechado, e não pode ser estornado.")
                           .arg(caixa.id)};
    return {true, QString(), caixa.id};
}

Caixa_service::Resultado Caixa_service::estornarRecebimentoDivida(qlonglong idPagamento)
{
    const auto pode = podeEstornarRecebimentoDivida(idPagamento);
    if (!pode.ok)
        return pode;
    const MovimentacaoCaixaDTO mov = repo.getMovimentacaoPorPagamentoDivida(idPagamento);
    if (mov.id <= 0)
        return {true, "Recebimento sem vínculo com caixa."};
    QString erro;
    if (!repo.deletarMovimentacao(mov.id, &erro))
        return {false, "Não foi possível remover o recebimento do caixa: " + erro};
    return {true, "Recebimento removido do caixa."};
}

Caixa_service::Resultado Caixa_service::podeRemoverRecebimento(qlonglong idEntradaVenda)
{
    const MovimentacaoCaixaDTO mov = repo.getMovimentacaoPorEntradaVenda(idEntradaVenda);
    if (mov.id <= 0)
        return {true, "Recebimento sem vínculo com caixa."};
    const CaixaDTO caixa = repo.getPorId(mov.idCaixa);
    if (!caixa.aberto())
        return {false, QString("Este recebimento pertence ao caixa #%1, já fechado, e não pode ser excluído.")
                           .arg(caixa.id)};
    return {true, QString(), caixa.id};
}

Caixa_service::Resultado Caixa_service::removerRecebimento(qlonglong idEntradaVenda)
{
    const MovimentacaoCaixaDTO mov = repo.getMovimentacaoPorEntradaVenda(idEntradaVenda);
    if (mov.id <= 0)
        return {true, "Recebimento sem vínculo com caixa."};
    const CaixaDTO caixa = repo.getPorId(mov.idCaixa);
    if (!caixa.aberto())
        return {false, QString("Este recebimento pertence ao caixa #%1, já fechado, e não pode ser excluído.")
                           .arg(caixa.id)};
    QString erro;
    if (!repo.deletarMovimentacao(mov.id, &erro))
        return {false, "Não foi possível remover o recebimento do caixa: " + erro};
    return {true, "Recebimento removido do caixa."};
}

Caixa_service::Resultado Caixa_service::removerRecebimentosDaVenda(qlonglong idVenda)
{
    if (idVenda <= 0)
        return {true, QString()};
    QString erro;
    if (!repo.deletarRecebimentosPorVenda(idVenda, &erro))
        return {false, "Não foi possível estornar os recebimentos do caixa: " + erro};
    return {true, QString()};
}

Caixa_service::Resultado Caixa_service::validarCancelamento(const VendasDTO &venda, const QString &motivo)
{
    // venda a prazo com dinheiro já recebido em caixa fechado: cancelar apagaria esse recebimento
    // do relatório de um caixa que já foi conferido
    if (venda.id > 0 && repo.contarRecebimentosEmCaixaFechado(venda.id) > 0)
        return {false, "Esta venda tem recebimentos lançados em caixa já fechado e não pode ser cancelada."};
    if (venda.idCaixa <= 0)
        return {true, "Venda anterior ao controle de caixa."};
    const CaixaDTO caixa = repo.getPorId(venda.idCaixa);
    if (!caixa.aberto())
        return {false, QString("Esta venda pertence ao caixa #%1, já fechado, e não pode ser cancelada.")
                           .arg(caixa.id)};
    if (motivo.trimmed().isEmpty())
        return {false, "Informe o motivo do cancelamento."};
    return {true, QString(), caixa.id};
}

Caixa_service::Resultado Caixa_service::registrarCancelamento(const VendasDTO &venda, const QString &motivo)
{
    const auto v = validarCancelamento(venda, motivo);
    if (!v.ok)
        return v;
    if (venda.idCaixa <= 0)
        return {true, "Sem caixa para registrar."};

    const CaixaDTO caixa = repo.getPorId(venda.idCaixa);
    const CaixaDTO atual = caixaAtual();

    MovimentacaoCaixaDTO mov;
    mov.idCaixa = venda.idCaixa;
    mov.tipo = "CANCELAMENTO";
    mov.valor = arredondar2(venda.valorFinal);
    mov.formaPagamento = venda.formaPagamento;
    mov.motivo = motivo.trimmed();
    mov.idVenda = venda.id;
    mov.idOperador = atual.aberto() ? atual.idOperador : caixa.idOperador;
    mov.idOperadorSessao = Sessao_service::instancia()->idOperador();
    mov.estornado = venda.estaPago;

    QString erro;
    const qlonglong id = repo.inserirMovimentacao(mov, &erro);
    if (id <= 0)
        return {false, "Não foi possível registrar o cancelamento no caixa: " + erro};
    return {true, "Cancelamento registrado no caixa.", id};
}

ResumoCaixaDTO Caixa_service::resumo(qlonglong idCaixa)
{
    cfg = confServ.carregarTudo();
    ResumoCaixaDTO r;
    r.caixa = repo.getPorId(idCaixa);
    if (r.caixa.id <= 0)
        return r;

    r.vendasPorForma = repo.vendasPorForma(idCaixa);
    for (const VendasFormaDTO &v : r.vendasPorForma) {
        r.quantidadeVendas += v.quantidade;
        if (v.formaPagamento != "Prazo")
            r.totalVendas += v.total;
    }

    for (const MovimentacaoCaixaDTO &m : repo.listarMovimentacoes(idCaixa)) {
        if (m.tipo == "CANCELAMENTO") {
            r.cancelamentos << m;
            continue;
        }
        r.movimentacoes << m;
        if (m.tipo == "SANGRIA")
            r.totalSangrias += m.valor;
        else if (m.tipo == "SUPRIMENTO")
            r.totalSuprimentos += m.valor;
        else if (m.tipo == "RECEBIMENTO") {
            r.totalRecebimentos += m.valor;
            r.recebimentosPorForma[m.formaPagamento] += m.valor;
        }
    }

    for (const QString &forma : FormasCaixa) {
        double esperado = 0;
        for (const VendasFormaDTO &v : r.vendasPorForma)
            if (v.formaPagamento == forma)
                esperado += v.total;
        esperado += r.recebimentosPorForma.value(forma, 0.0);
        if (forma == "Dinheiro")
            esperado += r.caixa.trocoInicial + r.totalSuprimentos - r.totalSangrias;
        r.esperadoPorForma[forma] = arredondar2(esperado);
    }

    // formas que não entram na conferência (ex.: "Não Sei"): ficam avisadas no fechamento e no relatório
    for (const VendasFormaDTO &v : r.vendasPorForma)
        if (v.formaPagamento != "Prazo" && v.formaPagamento != VendasDTO::FormaMista && !FormasCaixa.contains(v.formaPagamento))
            r.outrasFormas[v.formaPagamento] += v.total;
    for (auto it = r.recebimentosPorForma.constBegin(); it != r.recebimentosPorForma.constEnd(); ++it)
        if (!FormasCaixa.contains(it.key()))
            r.outrasFormas[it.key()] += it.value();

    if (r.caixa.status == "FECHADO") {
        r.fechamento = repo.getFechamento(idCaixa);
        for (FechamentoFormaDTO &f : r.fechamento)
            f.foraDaTolerancia = !dentroDaTolerancia(f.diferenca, f.valorEsperado);
    }

    // Quem trabalhou no caixa sem ser o dono: o dono é a resposta 1 e não entra na lista
    const QList<OperadorSessaoCaixaDTO> todos = repo.operadoresDaSessao(idCaixa);
    for (const OperadorSessaoCaixaDTO &op : todos) {
        if (op.id == r.caixa.idOperador)
            continue;
        r.operadoresDaSessao << op;
    }
    return r;
}

bool Caixa_service::dentroDaTolerancia(double diferenca, double esperado) const
{
    const double limite = std::max(cfg.caixaToleranciaValor,
                                   std::abs(esperado) * cfg.caixaToleranciaPercent / 100.0);
    return std::abs(diferenca) <= limite + 0.005;
}

QString Caixa_service::descricaoTolerancia() const
{
    QLocale pt(QLocale::Portuguese, QLocale::Brazil);
    return QString("R$ %1 ou %2%% do esperado (o que for maior)")
        .arg(pt.toString(cfg.caixaToleranciaValor, 'f', 2), pt.toString(cfg.caixaToleranciaPercent, 'f', 2));
}

Caixa_service::Resultado Caixa_service::fecharCaixa(qlonglong idCaixa, const QString &pin,
                                                    const QMap<QString, double> &informados,
                                                    const QString &observacao)
{
    cfg = confServ.carregarTudo();
    const ResumoCaixaDTO r = resumo(idCaixa);
    if (r.caixa.id <= 0)
        return {false, "Caixa não encontrado."};
    if (!r.caixa.aberto())
        return {false, "Este caixa já está fechado."};

    const auto pinOk = operadorServ.validarPin(r.caixa.idOperador, pin);
    if (!pinOk.ok)
        return {false, pinOk.msg};

    QList<FechamentoFormaDTO> formas;
    bool exigeJustificativa = false;
    for (const QString &forma : FormasCaixa) {
        if (!informados.contains(forma))
            return {false, QString("Informe o valor contado para %1.").arg(forma)};
        const double informado = informados.value(forma);
        if (informado < 0)
            return {false, QString("O valor contado para %1 não pode ser negativo.").arg(forma)};

        FechamentoFormaDTO f;
        f.formaPagamento = forma;
        f.valorEsperado = r.esperadoPorForma.value(forma, 0.0);
        f.valorInformado = arredondar2(informado);
        f.diferenca = arredondar2(f.valorInformado - f.valorEsperado);
        f.foraDaTolerancia = !dentroDaTolerancia(f.diferenca, f.valorEsperado);
        f.ocorrenciaTecnica = forma != "Dinheiro" && std::abs(f.diferenca) >= 0.005;
        if (f.foraDaTolerancia)
            exigeJustificativa = true;
        formas << f;
    }

    if (exigeJustificativa && observacao.trimmed().length() < 5)
        return {false, "Há diferença acima da tolerância (" + descricaoTolerancia() +
                           "). Descreva a justificativa na observação."};

    QString erro;
    // quantidades usadas nas contas: se mudarem até o fim do fechamento, o repositório desfaz
    if (!repo.fechar(idCaixa, observacao.trimmed(), formas, &erro, r.quantidadeVendas,
                     int(r.movimentacoes.size() + r.cancelamentos.size())))
        return {false, "Não foi possível fechar o caixa: " + erro};
    return {true, "Caixa fechado.", idCaixa};
}

void Caixa_service::listarHistorico(QSqlQueryModel *model, const QString &de, const QString &ate)
{
    repo.listarHistorico(model, de, ate);
}
