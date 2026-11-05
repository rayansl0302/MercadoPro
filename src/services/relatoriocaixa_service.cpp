#include "relatoriocaixa_service.h"
#include "../dto/Vendas_dto.h"
#include "escposprinter_service.h"
#include "../util/escposcomandos.h"
#include <QDateTime>
#include <QTextDocument>
#include <QPdfWriter>
#include <QPageSize>
#include <QPageLayout>
#include <QFile>
#include <cmath>

namespace {
constexpr int LarguraCupom = 42;

QByteArray linha(const QString &texto)
{
    return texto.toLocal8Bit() + '\n';
}

QString esquerdaDireita(const QString &esq, const QString &dir)
{
    const int espaco = LarguraCupom - esq.size() - dir.size();
    if (espaco >= 1)
        return esq + QString(espaco, QLatin1Char(' ')) + dir;
    return esq.left(LarguraCupom - dir.size() - 1) + QLatin1Char(' ') + dir;
}

QString centralizar(const QString &texto)
{
    if (texto.size() >= LarguraCupom)
        return texto.left(LarguraCupom);
    const int margem = (LarguraCupom - texto.size()) / 2;
    return QString(margem, QLatin1Char(' ')) + texto;
}

QString separador()
{
    return QString(LarguraCupom, QLatin1Char('-'));
}

QString tipoLegivel(const QString &tipo)
{
    if (tipo == "SANGRIA") return "Sangria";
    if (tipo == "SUPRIMENTO") return "Suprimento";
    if (tipo == "RECEBIMENTO") return "Recebimento a prazo";
    if (tipo == "CANCELAMENTO") return "Cancelamento";
    return tipo;
}
}

RelatorioCaixa_service::RelatorioCaixa_service(QObject *parent)
    : QObject{parent}, pt(QLocale::Portuguese, QLocale::Brazil)
{}

QString RelatorioCaixa_service::formatarDataHora(const QString &iso)
{
    if (iso.isEmpty())
        return "—";
    const QDateTime d = QDateTime::fromString(iso, Qt::ISODate);
    return d.isValid() ? d.toString("dd/MM/yyyy HH:mm") : iso;
}

QString RelatorioCaixa_service::dinheiro(double v) const
{
    return "R$ " + pt.toString(v, 'f', 2);
}

QString RelatorioCaixa_service::situacao(const FechamentoFormaDTO &f) const
{
    if (std::abs(f.diferenca) < 0.005)
        return "Conferido";
    QString s = f.diferenca > 0 ? "Sobra" : "Falta";
    if (f.ocorrenciaTecnica)
        s += " (ocorrência técnica)";
    if (!f.foraDaTolerancia && !f.ocorrenciaTecnica)
        s += " dentro da tolerância";
    return s;
}

QString RelatorioCaixa_service::gerarHtml(const ResumoCaixaDTO &r, const QString &descricaoTolerancia)
{
    const ConfigDTO cfg = confServ.carregarTudo();
    const CaixaDTO &c = r.caixa;
    QString h;
    h += "<html><head><meta charset='utf-8'><style>"
         "body{font-family:'Segoe UI',Arial,sans-serif;font-size:10pt;color:#1e293b;}"
         "h1{font-size:16pt;margin:0 0 2px 0;} h2{font-size:12pt;margin:18px 0 6px 0;color:#0d5ca1;"
         "border-bottom:1px solid #cbd5e1;padding-bottom:2px;}"
         "table{border-collapse:collapse;width:100%;} th,td{padding:4px 8px;border-bottom:1px solid #e2e8f0;}"
         "th{background:#f1f5f9;text-align:left;} td.n,th.n{text-align:right;}"
         ".muted{color:#64748b;} .neg{color:#b91c1c;font-weight:bold;} .pos{color:#15803d;font-weight:bold;}"
         ".box{background:#f8fafc;border:1px solid #e2e8f0;padding:8px 10px;margin-top:6px;}"
         "</style></head><body>";

    h += "<h1>Fechamento de Caixa #" + QString::number(c.id) + "</h1>";
    h += "<div class='muted'>" + cfg.nomeEmpresa.toHtmlEscaped() +
         (cfg.cnpjEmpresa.isEmpty() ? "" : " · CNPJ " + cfg.cnpjEmpresa.toHtmlEscaped()) + "</div>";

    h += "<h2>Turno</h2><table>";
    h += "<tr><td>Operador</td><td><b>" + c.nomeOperador.toHtmlEscaped() + "</b></td>"
         "<td>Terminal</td><td>" + c.terminal.toHtmlEscaped() + "</td></tr>";
    h += "<tr><td>Abertura</td><td>" + formatarDataHora(c.abertoEm) + "</td>"
         "<td>Fechamento</td><td>" + (c.status == "FECHADO" ? formatarDataHora(c.fechadoEm) : "<i>caixa aberto</i>") +
         "</td></tr>";
    h += "<tr><td>Troco inicial</td><td>" + dinheiro(c.trocoInicial) + "</td>"
         "<td class='muted'>Sugerido pelo último fechamento</td><td class='muted'>" + dinheiro(c.trocoSugerido) +
         "</td></tr></table>";

    // Quem mais operou neste caixa: sem esta linha o relatório escondia vendas de outro operador
    if (!r.operadoresDaSessao.isEmpty()) {
        h += "<h2>Operadores da sessão</h2><p class='muted'>"
             "Caixa aberto por " + c.nomeOperador.toHtmlEscaped() + ". "
             "Também registraram vendas ou movimentações:</p><table>"
             "<tr><th>Operador</th><th class='n'>Registros</th></tr>";
        for (const OperadorSessaoCaixaDTO &op : r.operadoresDaSessao)
            h += "<tr><td>" + op.nome.toHtmlEscaped() + "</td><td class='n'>" +
                 QString::number(op.quantidade) + "</td></tr>";
        h += "</table>";
    }

    h += "<h2>Vendas por forma de pagamento</h2><table><tr><th>Forma</th><th class='n'>Qtd</th><th class='n'>Total</th></tr>";
    for (const VendasFormaDTO &v : r.vendasPorForma)
        h += "<tr><td>" + v.formaPagamento.toHtmlEscaped() + (v.formaPagamento == "Prazo"
                                                                   ? " <span class='muted'>(não entra no caixa)</span>"
                                                                   : v.formaPagamento == VendasDTO::FormaMista
                                                                         ? " <span class='muted'>(pago em mais de uma forma; o valor já está nas formas acima)</span>" : "") +
             "</td><td class='n'>" + QString::number(v.quantidade) + "</td><td class='n'>" + dinheiro(v.total) + "</td></tr>";
    if (r.vendasPorForma.isEmpty())
        h += "<tr><td colspan='3' class='muted'>Nenhuma venda neste turno.</td></tr>";
    h += "<tr><th>Total recebido em vendas</th><th class='n'>" + QString::number(r.quantidadeVendas) +
         "</th><th class='n'>" + dinheiro(r.totalVendas) + "</th></tr></table>";

    h += "<h2>Movimentações</h2><table><tr><th>Hora</th><th>Tipo</th><th>Forma</th><th>Motivo</th><th class='n'>Valor</th></tr>";
    for (const MovimentacaoCaixaDTO &m : r.movimentacoes)
        h += "<tr><td>" + formatarDataHora(m.dataHora).mid(11) + "</td><td>" + tipoLegivel(m.tipo) + "</td><td>" +
             m.formaPagamento.toHtmlEscaped() + "</td><td>" + m.motivo.toHtmlEscaped() + "</td><td class='n'>" +
             (m.tipo == "SANGRIA" ? "- " : "+ ") + dinheiro(m.valor) + "</td></tr>";
    if (r.movimentacoes.isEmpty())
        h += "<tr><td colspan='5' class='muted'>Nenhuma sangria, suprimento ou recebimento.</td></tr>";
    h += "<tr><th colspan='4'>Suprimentos</th><th class='n'>" + dinheiro(r.totalSuprimentos) + "</th></tr>";
    h += "<tr><th colspan='4'>Sangrias</th><th class='n'>" + dinheiro(r.totalSangrias) + "</th></tr>";
    h += "<tr><th colspan='4'>Recebimentos de vendas a prazo</th><th class='n'>" + dinheiro(r.totalRecebimentos) +
         "</th></tr></table>";

    h += "<h2>Conferência</h2>";
    h += "<div class='muted'>Dinheiro esperado = troco inicial + vendas em dinheiro + recebimentos em dinheiro + "
         "suprimentos − sangrias. Tolerância: " + descricaoTolerancia.toHtmlEscaped() + "</div>";
    h += "<table><tr><th>Forma</th><th class='n'>Esperado</th><th class='n'>Informado</th><th class='n'>Diferença</th><th>Situação</th></tr>";
    double totalDif = 0;
    if (r.fechamento.isEmpty()) {
        for (auto it = r.esperadoPorForma.constBegin(); it != r.esperadoPorForma.constEnd(); ++it)
            h += "<tr><td>" + it.key() + "</td><td class='n'>" + dinheiro(it.value()) +
                 "</td><td class='n muted'>—</td><td class='n muted'>—</td><td class='muted'>aguardando fechamento</td></tr>";
    } else {
        for (const FechamentoFormaDTO &f : r.fechamento) {
            totalDif += f.diferenca;
            const QString cls = std::abs(f.diferenca) < 0.005 ? "" : (f.diferenca < 0 ? "neg" : "pos");
            h += "<tr><td>" + f.formaPagamento + "</td><td class='n'>" + dinheiro(f.valorEsperado) +
                 "</td><td class='n'>" + dinheiro(f.valorInformado) + "</td><td class='n " + cls + "'>" +
                 dinheiro(f.diferenca) + "</td><td>" + situacao(f) + "</td></tr>";
        }
        h += "<tr><th colspan='3'>Diferença total</th><th class='n'>" + dinheiro(totalDif) + "</th><th></th></tr>";
    }
    h += "</table>";
    if (!r.outrasFormas.isEmpty()) {
        QStringList itens;
        for (auto it = r.outrasFormas.constBegin(); it != r.outrasFormas.constEnd(); ++it)
            itens << it.key().toHtmlEscaped() + ": " + dinheiro(it.value());
        h += "<div class='box neg'>Atenção: há valores em formas de pagamento que não entram na conferência (" +
             itens.join("; ") + ").</div>";
    }

    h += "<h2>Justificativa / observação</h2><div class='box'>" +
         (c.observacaoFechamento.trimmed().isEmpty() ? "<span class='muted'>Sem observações.</span>"
                                                     : c.observacaoFechamento.toHtmlEscaped().replace("\n", "<br>")) +
         "</div>";

    h += "<h2>Vendas canceladas</h2>";
    if (r.cancelamentos.isEmpty()) {
        h += "<div class='muted'>Nenhuma venda cancelada neste turno.</div>";
    } else {
        h += "<table><tr><th>Hora</th><th>Venda</th><th>Forma</th><th class='n'>Valor</th><th>Estorno</th><th>Motivo</th><th>Operador</th></tr>";
        for (const MovimentacaoCaixaDTO &m : r.cancelamentos)
            h += "<tr><td>" + formatarDataHora(m.dataHora).mid(11) + "</td><td>#" + QString::number(m.idVenda) +
                 "</td><td>" + m.formaPagamento.toHtmlEscaped() + "</td><td class='n'>" + dinheiro(m.valor) +
                 "</td><td>" + (m.estornado ? "Sim (venda já paga)" : "Não") + "</td><td>" + m.motivo.toHtmlEscaped() +
                 "</td><td>" + m.nomeOperador.toHtmlEscaped() + "</td></tr>";
        h += "</table>";
    }

    h += "<p class='muted' style='margin-top:24px'>Gerado em " +
         QDateTime::currentDateTime().toString("dd/MM/yyyy HH:mm") + "</p>";
    h += "<p style='margin-top:40px'>____________________________________<br>Assinatura do operador: " +
         c.nomeOperador.toHtmlEscaped() + "</p>";
    h += "</body></html>";
    return h;
}

bool RelatorioCaixa_service::salvarPdf(const QString &html, const QString &caminho, QString *erro)
{
    QPdfWriter writer(caminho);
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setPageMargins(QMarginsF(15, 15, 15, 15), QPageLayout::Millimeter);
    writer.setTitle("Fechamento de caixa");

    QTextDocument doc;
    doc.setHtml(html);
    doc.setPageSize(writer.pageLayout().paintRectPixels(writer.resolution()).size());
    doc.print(&writer);

    if (!QFile::exists(caminho)) {
        if (erro)
            *erro = "Não foi possível gravar o arquivo PDF.";
        return false;
    }
    return true;
}

bool RelatorioCaixa_service::imprimirTermica(const ResumoCaixaDTO &r, QString *erro)
{
    const ConfigDTO cfg = confServ.carregarTudo();
    const QString impressora = cfg.impressoraNomeDispositivo.trimmed();
    if (impressora.isEmpty()) {
        if (erro)
            *erro = "Nenhuma impressora térmica selecionada. Escolha em Configurações.";
        return false;
    }
    const CaixaDTO &c = r.caixa;

    QByteArray d;
    d += EscPosComandos::inicializar();
    d += QByteArray("\x1B\x74\x10", 3);
    d += EscPosComandos::alinharCentro();
    d += EscPosComandos::bold(true);
    d += linha(centralizar("FECHAMENTO DE CAIXA #" + QString::number(c.id)));
    d += EscPosComandos::bold(false);
    if (!cfg.nomeEmpresa.isEmpty())
        d += linha(centralizar(cfg.nomeEmpresa.left(LarguraCupom)));
    d += EscPosComandos::alinharEsquerda();
    d += linha(separador());
    d += linha(esquerdaDireita("Operador:", c.nomeOperador.left(28)));
    d += linha(esquerdaDireita("Terminal:", c.terminal.left(28)));
    d += linha(esquerdaDireita("Abertura:", formatarDataHora(c.abertoEm)));
    d += linha(esquerdaDireita("Fechamento:", c.status == "FECHADO" ? formatarDataHora(c.fechadoEm) : "aberto"));
    d += linha(esquerdaDireita("Troco inicial:", dinheiro(c.trocoInicial)));
    if (!r.operadoresDaSessao.isEmpty()) {
        d += linha(esquerdaDireita("Também operaram:",
                                  QString("%1").arg(r.operadoresDaSessao.size())));
        for (const OperadorSessaoCaixaDTO &op : r.operadoresDaSessao)
            d += linha(QString("  - %1 (%2)").arg(op.nome.left(20)).arg(op.quantidade));
    }
    d += linha(separador());

    d += EscPosComandos::bold(true);
    d += linha("VENDAS POR FORMA");
    d += EscPosComandos::bold(false);
    for (const VendasFormaDTO &v : r.vendasPorForma)
        d += linha(esquerdaDireita(v.quantidade > 0 ? QString("%1 (%2)").arg(v.formaPagamento).arg(v.quantidade)
                                                    : v.formaPagamento, dinheiro(v.total)));
    d += linha(esquerdaDireita("Total em vendas:", dinheiro(r.totalVendas)));
    d += linha(separador());

    d += EscPosComandos::bold(true);
    d += linha("MOVIMENTACOES");
    d += EscPosComandos::bold(false);
    d += linha(esquerdaDireita("Suprimentos:", "+ " + dinheiro(r.totalSuprimentos)));
    d += linha(esquerdaDireita("Sangrias:", "- " + dinheiro(r.totalSangrias)));
    d += linha(esquerdaDireita("Receb. a prazo:", "+ " + dinheiro(r.totalRecebimentos)));
    for (const MovimentacaoCaixaDTO &m : r.movimentacoes) {
        if (m.tipo == "RECEBIMENTO")
            continue;
        d += linha(esquerdaDireita("  " + formatarDataHora(m.dataHora).mid(11) + " " + tipoLegivel(m.tipo).left(11),
                                   dinheiro(m.valor)));
        d += linha(("    " + m.motivo).left(LarguraCupom));
    }
    d += linha(separador());

    d += EscPosComandos::bold(true);
    d += linha("CONFERENCIA");
    d += EscPosComandos::bold(false);
    d += linha(esquerdaDireita("Forma", "Esperado  Informado"));
    double totalDif = 0;
    const QList<FechamentoFormaDTO> &fs = r.fechamento;
    for (const FechamentoFormaDTO &f : fs) {
        totalDif += f.diferenca;
        d += linha(esquerdaDireita(f.formaPagamento,
                                   pt.toString(f.valorEsperado, 'f', 2).rightJustified(9) + " " +
                                       pt.toString(f.valorInformado, 'f', 2).rightJustified(9)));
        d += linha(esquerdaDireita("   dif.: " + situacao(f).left(24), dinheiro(f.diferenca)));
    }
    if (fs.isEmpty()) {
        for (auto it = r.esperadoPorForma.constBegin(); it != r.esperadoPorForma.constEnd(); ++it)
            d += linha(esquerdaDireita(it.key() + " esperado:", dinheiro(it.value())));
    } else {
        d += EscPosComandos::bold(true);
        d += linha(esquerdaDireita("DIFERENCA TOTAL:", dinheiro(totalDif)));
        d += EscPosComandos::bold(false);
    }
    d += linha(separador());

    if (!r.outrasFormas.isEmpty()) {
        d += EscPosComandos::bold(true);
        d += linha("ATENCAO: FORA DA CONFERENCIA");
        d += EscPosComandos::bold(false);
        for (auto it = r.outrasFormas.constBegin(); it != r.outrasFormas.constEnd(); ++it)
            d += linha(esquerdaDireita(it.key().left(24), dinheiro(it.value())));
        d += linha(separador());
    }

    if (!c.observacaoFechamento.trimmed().isEmpty()) {
        d += EscPosComandos::bold(true);
        d += linha("JUSTIFICATIVA");
        d += EscPosComandos::bold(false);
        for (const QString &l : c.observacaoFechamento.split('\n'))
            for (int i = 0; i < l.size(); i += LarguraCupom)
                d += linha(l.mid(i, LarguraCupom));
        d += linha(separador());
    }

    if (!r.cancelamentos.isEmpty()) {
        d += EscPosComandos::bold(true);
        d += linha("VENDAS CANCELADAS");
        d += EscPosComandos::bold(false);
        for (const MovimentacaoCaixaDTO &m : r.cancelamentos) {
            d += linha(esquerdaDireita(QString("#%1 %2%3").arg(m.idVenda).arg(m.formaPagamento,
                                                                                m.estornado ? " (estorno)" : ""),
                                       dinheiro(m.valor)));
            d += linha(("   " + m.motivo).left(LarguraCupom));
        }
        d += linha(separador());
    }

    d += EscPosComandos::feed(2);
    d += EscPosComandos::alinharCentro();
    d += linha("__________________________________");
    d += linha(c.nomeOperador.left(LarguraCupom));
    d += EscPosComandos::feed(4);
    d += EscPosComandos::cortar();

    EscPosPrinter_service printer;
    const auto res = printer.imprimirRaw(impressora, d);
    if (!res.ok && erro)
        *erro = res.msg;
    return res.ok;
}
