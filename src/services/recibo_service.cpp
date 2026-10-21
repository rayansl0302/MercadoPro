#include "recibo_service.h"
#include <QSettings>
#include "../infra/apppath_service.h"
#include "../infra/databaseconnection_service.h"
#include "config_service.h"
#include "escposprinter_service.h"
#include "../util/escposcomandos.h"
#include "../util/comandalanche.h"
#include <QSqlQuery>
#include <QDate>

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
    return esq + QLatin1Char(' ') + dir;
}

QString centralizar(const QString &texto)
{
    if (texto.size() >= LarguraCupom)
        return texto.left(LarguraCupom);
    const int margem = (LarguraCupom - texto.size()) / 2;
    return QString(margem, QLatin1Char(' ')) + texto;
}

QString dinheiro(const QLocale &locale, double valor)
{
    return locale.toString(valor, 'f', 2);
}

QString formatarData(const QString &dataHora)
{
    QDateTime data;
    if (dataHora.contains('T'))
        data = QDateTime::fromString(dataHora, Qt::ISODateWithMs);
    else
        data = QDateTime::fromString(dataHora, "yyyy-MM-dd HH:mm:ss");
    if (!data.isValid())
        data = QDateTime::fromString(dataHora, "yyyy-MM-dd hh:mm:ss");
    return data.isValid() ? data.toString("dd/MM/yyyy HH:mm") : dataHora;
}
}

Recibo_service::Recibo_service(QObject *parent)
    : QObject{parent}
{
    db = DatabaseConnection_service::db();
    portugues = QLocale(QLocale::Portuguese, QLocale::Brazil);
}

Lanches::TipoVenda Recibo_service::tipoDaVenda(qlonglong idvenda, QList<ProdutoVendidoDTO> *itensLanche)
{
    const Lanches::Faixa faixa = Lanches::faixaConfigurada();
    const auto produtos = prodVendaServ.getProdutosVendidos(idvenda);
    QStringList codigos;
    for (const auto &item : produtos) {
        // o código de barras vem do cadastro do produto (a venda guarda só o id)
        const QString codigo = prodServ.getProduto(item.idProduto).codigoBarras;
        codigos << codigo;
        if (itensLanche && Lanches::codigoEhLanche(codigo, faixa))
            itensLanche->append(item);
    }
    return Lanches::classificar(codigos, faixa);
}

int Recibo_service::senhaDoDia(qlonglong idvenda, const QString &dataHora)
{
    // intervalo [00:00 do dia, 00:00 do dia seguinte): funciona com data guardada como texto (SQLite) ou timestamp (PostgreSQL)
    const QDate dia = QDate::fromString(dataHora.left(10), "yyyy-MM-dd");
    if (!dia.isValid())
        return 1;
    QSqlQuery q(db);
    q.prepare("SELECT pv.id_venda, p.codigo_barras FROM produtos_vendidos pv "
              "JOIN vendas2 v ON v.id = pv.id_venda JOIN produtos p ON p.id = pv.id_produto "
              "WHERE v.data_hora >= :ini AND v.data_hora < :fim AND v.id <= :id");
    q.bindValue(":ini", dia.toString("yyyy-MM-dd") + " 00:00:00");
    q.bindValue(":fim", dia.addDays(1).toString("yyyy-MM-dd") + " 00:00:00");
    q.bindValue(":id", idvenda);
    QList<QPair<qlonglong, QString>> itensDoDia;
    if (q.exec())
        while (q.next())
            itensDoDia.append({q.value(0).toLongLong(), q.value(1).toString()});
    return qMax(1, Lanches::senhaDoDia(itensDoDia, idvenda, Lanches::faixaConfigurada()));
}

bool Recibo_service::imprimirComanda(qlonglong idvenda, const QList<ProdutoVendidoDTO> &itens, QString *erro)
{
    const ConfigDTO configs = confServ.carregarTudo();
    // impressora da cozinha (opcional, em [pdv] impressora_cozinha); sem ela, usa a do caixa
    QSettings ini(AppPath_service::configPath(), QSettings::IniFormat);
    QString impressora = ini.value("pdv/impressora_cozinha").toString().trimmed();
    if (impressora.isEmpty())
        impressora = configs.impressoraNomeDispositivo.trimmed();
    if (impressora.isEmpty()) {
        if (erro)
            *erro = "Nenhuma impressora térmica selecionada. Escolha em Configurações.";
        return false;
    }

    const VendasDTO venda = vendaServ.getVenda(idvenda);
    ComandaLanche::Dados dados;
    dados.empresa = configs.nomeEmpresa;
    const QString setor = ini.value("pdv/lanche_setor").toString().trimmed();
    if (!setor.isEmpty())
        dados.setor = setor;
    dados.senha = senhaDoDia(idvenda, venda.dataHora);
    dados.quando = QDateTime::currentDateTime();
    for (const auto &item : itens) {
        ComandaLanche::Item i;
        i.quantidade = item.quantidade;
        i.descricao = item.descricao;
        dados.itens.append(i);
    }

    EscPosPrinter_service impressoraServ;
    const auto resultado = impressoraServ.imprimirRaw(impressora, ComandaLanche::montar(dados));
    if (!resultado.ok) {
        if (erro)
            *erro = resultado.msg.isEmpty() ? "Não foi possível imprimir a comanda de lanches." : resultado.msg;
        return false;
    }
    return true;
}

bool Recibo_service::imprimirAposVenda(qlonglong idvenda, bool querCupom, QString *erro)
{
    QList<ProdutoVendidoDTO> lanches;
    const Lanches::TipoVenda tipo = tipoDaVenda(idvenda, &lanches);
    bool ok = true;
    QString erroLocal;
    switch (tipo) {
    case Lanches::TipoVenda::Normal:
        if (querCupom)
            ok = imprimirCupomPadrao(idvenda, &erroLocal);
        break;
    case Lanches::TipoVenda::Lanche:
        // layout de lanche no lugar do cupom: a cozinha recebe a comanda mesmo sem o cupom marcado
        ok = imprimirComanda(idvenda, lanches, &erroLocal);
        break;
    case Lanches::TipoVenda::Misto:
        ok = imprimirComanda(idvenda, lanches, &erroLocal);
        if (querCupom) {
            QString erroCupom;
            if (!imprimirCupomPadrao(idvenda, &erroCupom) && ok) {
                ok = false;
                erroLocal = erroCupom;
            }
        }
        break;
    }
    if (!ok && erro)
        *erro = erroLocal;
    return ok;
}

bool Recibo_service::imprimirReciboVenda(qlonglong idvenda, QString *erro)
{
    return imprimirAposVenda(idvenda, true, erro);
}

bool Recibo_service::imprimirCupomPadrao(qlonglong idvenda, QString *erro)
{
    const ConfigDTO configs = confServ.carregarTudo();
    const QString impressora = configs.impressoraNomeDispositivo.trimmed();
    if (impressora.isEmpty()) {
        if (erro)
            *erro = "Nenhuma impressora térmica selecionada. Escolha em Configurações.";
        return false;
    }

    const VendasDTO venda = vendaServ.getVenda(idvenda);
    if (venda.dataHora.isEmpty() && venda.clienteNome.isEmpty()) {
        if (erro)
            *erro = "Não foi possível carregar a venda para impressão.";
        return false;
    }

    QByteArray dados;
    dados += EscPosComandos::inicializar();
    dados += QByteArray("\x1B\x74\x10", 3);
    dados += EscPosComandos::alinharCentro();
    dados += EscPosComandos::bold(true);
    dados += linha(centralizar("CUPOM DE COMPRA"));
    dados += EscPosComandos::bold(false);
    dados += EscPosComandos::alinharEsquerda();

    if (!configs.nomeEmpresa.trimmed().isEmpty())
        dados += linha(configs.nomeEmpresa.trimmed());
    if (!configs.enderecoEmpresa.trimmed().isEmpty())
        dados += linha(configs.enderecoEmpresa.trimmed());
    if (!configs.cnpjEmpresa.trimmed().isEmpty())
        dados += linha(configs.cnpjEmpresa.trimmed());
    if (!configs.telefoneEmpresa.trimmed().isEmpty())
        dados += linha(configs.telefoneEmpresa.trimmed());

    dados += linha("Data/Hora: " + formatarData(venda.dataHora));
    dados += linha("Cliente: " + venda.clienteNome);
    dados += linha(QString(LarguraCupom, QLatin1Char('=')));
    dados += EscPosComandos::bold(true);
    dados += linha(esquerdaDireita("Qtd  Produto", "Valor"));
    dados += EscPosComandos::bold(false);

    const auto produtos = prodVendaServ.getProdutosVendidos(idvenda);
    for (const auto &produto : produtos) {
        const QString quantidade = portugues.toString(produto.quantidade, 'f', 2);
        QString descricao = produto.descricao.simplified();
        const int maxDesc = LarguraCupom - quantidade.size() - 3;
        if (descricao.size() > maxDesc && maxDesc > 3)
            descricao = descricao.left(maxDesc - 3) + "...";
        const QString valor = dinheiro(portugues, produto.precoVendido * produto.quantidade);
        dados += linha(quantidade + " x " + descricao);
        dados += linha(esquerdaDireita("", valor));
    }

    dados += linha(QString(LarguraCupom, QLatin1Char('=')));
    dados += linha(esquerdaDireita("Desconto (R$)", dinheiro(portugues, venda.desconto)));
    dados += linha("Forma: " + venda.formaPagamento);
    dados += linha(esquerdaDireita("Total produtos (R$)", dinheiro(portugues, venda.total)));

    if (venda.formaPagamento == "Dinheiro") {
        dados += linha(esquerdaDireita("Valor final (R$)", dinheiro(portugues, venda.valorFinal)));
        dados += linha(esquerdaDireita("Recebido (R$)", dinheiro(portugues, venda.valorRecebido)));
        dados += linha(esquerdaDireita("Troco (R$)", dinheiro(portugues, venda.troco)));
    } else if (venda.formaPagamento == "Crédito" || venda.formaPagamento == "Débito") {
        dados += linha(esquerdaDireita("Taxa (%)", dinheiro(portugues, venda.taxa)));
        dados += linha(esquerdaDireita("Valor final (R$)", dinheiro(portugues, venda.valorFinal)));
    } else if (venda.formaPagamento == "Prazo") {
        dados += linha(esquerdaDireita("Valor final (R$)", dinheiro(portugues, venda.valorFinal)));
        const QList<EntradaVendaDTO> entradas = entradaServ.getEntradasFromVenda(idvenda);
        double devendo = venda.valorFinal;
        int parcela = 1;
        for (const auto &entrada : entradas) {
            dados += linha(QString(LarguraCupom, QLatin1Char('-')));
            dados += EscPosComandos::bold(true);
            dados += linha("Parcela " + QString::number(parcela));
            dados += EscPosComandos::bold(false);
            dados += linha("Data: " + formatarData(entrada.dataHora));
            dados += linha("Forma: " + entrada.formaPagamento);
            dados += linha(esquerdaDireita("Descontado (R$)", dinheiro(portugues, entrada.total)));
            if (entrada.formaPagamento == "Dinheiro") {
                dados += linha(esquerdaDireita("Recebido (R$)", dinheiro(portugues, entrada.valorRecebido)));
                dados += linha(esquerdaDireita("Troco (R$)", dinheiro(portugues, entrada.troco)));
            } else if (entrada.formaPagamento == "Crédito" || entrada.formaPagamento == "Débito") {
                dados += linha(esquerdaDireita("Taxa (%)", dinheiro(portugues, entrada.taxa)));
                dados += linha(esquerdaDireita("Valor final (R$)", dinheiro(portugues, entrada.valorFinal)));
            }
            devendo -= entrada.total;
            ++parcela;
        }
        dados += EscPosComandos::bold(true);
        dados += linha(esquerdaDireita("Devendo (R$)", dinheiro(portugues, devendo)));
        dados += EscPosComandos::bold(false);
    } else {
        dados += linha(esquerdaDireita("Valor final (R$)", dinheiro(portugues, venda.valorFinal)));
    }

    dados += linha("");
    dados += linha("Assinatura:");
    dados += linha("");
    dados += EscPosComandos::alinharCentro();
    dados += linha("Obrigado pela compra");
    dados += linha("Volte sempre!");
    dados += EscPosComandos::feed(4);
    dados += EscPosComandos::cortar();

    EscPosPrinter_service impressoraServ;
    const auto resultado = impressoraServ.imprimirRaw(impressora, dados);
    if (!resultado.ok) {
        if (erro)
            *erro = resultado.msg.isEmpty() ? "Não foi possível imprimir o cupom." : resultado.msg;
        return false;
    }
    return true;
}
