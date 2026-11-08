#ifndef CAIXA_REPOSITORY_H
#define CAIXA_REPOSITORY_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQueryModel>
#include "../dto/Caixa_dto.h"

class Caixa_repository : public QObject
{
    Q_OBJECT
public:
    explicit Caixa_repository(QObject *parent = nullptr);

    // caixas
    qlonglong abrir(const CaixaDTO &caixa, QString *erro = nullptr);
    CaixaDTO getPorId(qlonglong id);
    CaixaDTO getAbertoNoTerminal(const QString &terminal);
    CaixaDTO getAbertoDoOperador(qlonglong idOperador);
    double getDinheiroInformadoUltimoFechamento(const QString &terminal);
    // vendasConferidas/movimentacoesConferidas (>= 0): quantidades que o fechamento usou nas contas.
    // Se na hora de fechar o banco tiver outras (alguém vendeu/lançou no meio), o fechamento é desfeito
    // e o operador refaz a conferência. -1 = não confere.
    bool fechar(qlonglong idCaixa, const QString &observacao, const QList<FechamentoFormaDTO> &formas,
                QString *erro = nullptr, int vendasConferidas = -1, int movimentacoesConferidas = -1);
    void listarHistorico(QSqlQueryModel *model, const QString &de, const QString &ate);

    // movimentacoes
    qlonglong inserirMovimentacao(const MovimentacaoCaixaDTO &mov, QString *erro = nullptr);
    MovimentacaoCaixaDTO getMovimentacaoPorEntradaVenda(qlonglong idEntradaVenda);
    MovimentacaoCaixaDTO getMovimentacaoPorPagamentoDivida(qlonglong idPagamentoDivida);
    bool deletarMovimentacao(qlonglong id, QString *erro = nullptr);
    MovimentacaoCaixaDTO getMovimentacao(qlonglong id);
    bool atualizarMovimentacao(qlonglong id, double valor, const QString &motivo, QString *erro = nullptr);
    bool deletarRecebimentosPorVenda(qlonglong idVenda, QString *erro = nullptr);
    // recebimentos desta venda lançados em caixas que não estão mais abertos
    int contarRecebimentosEmCaixaFechado(qlonglong idVenda);
    QList<MovimentacaoCaixaDTO> listarMovimentacoes(qlonglong idCaixa);
    // sangrias (não estornadas) feitas entre as datas (inclusive), de todos os caixas
    QList<MovimentacaoCaixaDTO> listarSangrias(const QDate &de, const QDate &ate);

    // totais
    QList<VendasFormaDTO> vendasPorForma(qlonglong idCaixa);
    QList<FechamentoFormaDTO> getFechamento(qlonglong idCaixa);

    // Quem praticou venda ou movimentação no caixa, por nome e quantidade de registros.
    // O id 0 aparece como "Gerente (PIN geral)", pois não existe linha em operadores.
    QList<OperadorSessaoCaixaDTO> operadoresDaSessao(qlonglong idCaixa);

private:
    QSqlDatabase db;
    CaixaDTO lerCaixa(class QSqlQuery &query);
    MovimentacaoCaixaDTO lerMovimentacao(class QSqlQuery &query);
};

#endif // CAIXA_REPOSITORY_H
