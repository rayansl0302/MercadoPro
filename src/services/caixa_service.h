#ifndef CAIXA_SERVICE_H
#define CAIXA_SERVICE_H

#include <QObject>
#include <QSqlQueryModel>
#include <QMap>
#include <QStringList>
#include "../repository/caixa_repository.h"
#include "../dto/Vendas_dto.h"
#include "operador_service.h"
#include "config_service.h"

// Regras do turno de caixa: abertura com PIN, movimentações, vínculo das vendas,
// cálculo do valor esperado por forma de pagamento e fechamento com tolerância.
class Caixa_service : public QObject
{
    Q_OBJECT
public:
    struct Resultado {
        bool ok;
        QString msg;
        qlonglong id = 0;
    };

    static const QStringList FormasCaixa;   // Dinheiro, Crédito, Débito, Pix
    static QString terminalAtual();

    explicit Caixa_service(QObject *parent = nullptr);

    // caixa aberto de quem está logado (neste terminal)
    CaixaDTO caixaAtual();
    CaixaDTO getCaixa(qlonglong id);
    double sugerirTrocoInicial();

    Resultado abrirCaixa(qlonglong idOperador, const QString &pin, double trocoInicial, double trocoSugerido);
    // forma: de onde saiu (sangria) ou onde entrou (suprimento): Dinheiro, Crédito, Débito ou Pix
    Resultado registrarSangria(double valor, const QString &motivo, const QString &forma = "Dinheiro");
    Resultado registrarSuprimento(double valor, const QString &motivo, const QString &forma = "Dinheiro");
    // Corrigir ou desfazer uma sangria/suprimento lançado por engano (lançou duas vezes, valor errado).
    // Só no caixa ainda aberto; o motivo da correção é obrigatório e fica no log de acesso.
    QList<MovimentacaoCaixaDTO> sangriasESuprimentosDoCaixa(qlonglong idCaixa);
    Resultado editarSangriaSuprimento(qlonglong idMovimentacao, double novoValor, const QString &novoMotivo,
                                      const QString &motivoDaCorrecao, const QString &novaForma = QString());
    Resultado removerSangriaSuprimento(qlonglong idMovimentacao, const QString &motivoDaRemocao);

    // Chamados pelos fluxos de venda
    Resultado exigirCaixaAberto();
    // recebimento de dívida lançada à mão (contas a receber): entra no caixa aberto de quem está logado
    Resultado registrarRecebimentoDivida(qlonglong idDivida, qlonglong idPagamento, const QString &forma,
                                         double valor, const QString &descricao);
    Resultado podeEstornarRecebimentoDivida(qlonglong idPagamento);
    Resultado estornarRecebimentoDivida(qlonglong idPagamento);
    Resultado registrarRecebimento(qlonglong idVenda, qlonglong idEntradaVenda,
                                   const QString &forma, double valor);
    // Confere (sem apagar) se o recebimento ainda pode ser excluído: o caixa dele precisa estar aberto.
    Resultado podeRemoverRecebimento(qlonglong idEntradaVenda);
    Resultado removerRecebimento(qlonglong idEntradaVenda);
    Resultado removerRecebimentosDaVenda(qlonglong idVenda);
    Resultado validarCancelamento(const VendasDTO &venda, const QString &motivo);
    Resultado registrarCancelamento(const VendasDTO &venda, const QString &motivo);

    // Fechamento
    ResumoCaixaDTO resumo(qlonglong idCaixa);
    bool dentroDaTolerancia(double diferenca, double esperado) const;
    QString descricaoTolerancia() const;
    Resultado fecharCaixa(qlonglong idCaixa, const QString &pin, const QMap<QString, double> &informados,
                          const QString &observacao);

    QList<MovimentacaoCaixaDTO> sangrias(const QDate &de, const QDate &ate) { return repo.listarSangrias(de, ate); }
    void listarHistorico(QSqlQueryModel *model, const QString &de = QString(), const QString &ate = QString());

private:
    Caixa_repository repo;
    Operador_service operadorServ;
    Config_service confServ;
    ConfigDTO cfg;

    Resultado registrarMovimentacaoSimples(const QString &tipo, double valor, const QString &motivo,
                                          const QString &forma = "Dinheiro");
};

#endif // CAIXA_SERVICE_H
