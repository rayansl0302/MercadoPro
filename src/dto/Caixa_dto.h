#ifndef CAIXA_DTO_H
#define CAIXA_DTO_H
#include <QString>
#include <QList>
#include <QMap>

// Turno de caixa de um operador em um terminal.
struct CaixaDTO {
    qlonglong id = 0;
    qlonglong idOperador = 0;
    QString nomeOperador;
    QString terminal;
    QString status;            // ABERTO | FECHADO
    QString abertoEm;
    QString fechadoEm;
    double trocoInicial = 0;
    double trocoSugerido = 0;
    QString observacaoFechamento;
    qlonglong reabertoPor = 0;
    QString motivoReabertura;
    QString reabertoEm;

    bool aberto() const { return id > 0 && status == "ABERTO"; }
};

// SANGRIA | SUPRIMENTO | RECEBIMENTO (venda a prazo) | CANCELAMENTO (venda cancelada)
struct MovimentacaoCaixaDTO {
    qlonglong id = 0;
    qlonglong idCaixa = 0;
    QString tipo;
    double valor = 0;
    QString formaPagamento;
    QString motivo;
    qlonglong idVenda = 0;
    qlonglong idEntradaVenda = 0;
    qlonglong idPagamentoDivida = 0;     // recebimento de dívida manual (contas a receber)
    qlonglong idOperador = 0;            // dono do caixa (quem abriu)
    // -1 = ainda não registrado (NULL no banco); 0 = entrada pelo PIN geral do gerente
    qlonglong idOperadorSessao = -1;     // quem estava logado quando a movimentação aconteceu
    QString nomeOperador;
    QString nomeOperadorSessao;
    bool estornado = false;
    QString dataHora;
};

// Operador da sessão registrado no caixa. Somente id != id do dono do caixa aparece no relatório.
struct OperadorSessaoCaixaDTO {
    qlonglong id = 0;
    QString nome;
    qlonglong quantidade = 0;
};

// Linha do fechamento para uma forma de pagamento.
struct FechamentoFormaDTO {
    QString formaPagamento;
    double valorEsperado = 0;
    double valorInformado = 0;
    double diferenca = 0;
    bool ocorrenciaTecnica = false;   // divergência em cartão/PIX
    bool foraDaTolerancia = false;
};

struct VendasFormaDTO {
    QString formaPagamento;
    int quantidade = 0;
    double total = 0;
};

// Tudo que o fechamento e o relatório precisam sobre um caixa.
struct ResumoCaixaDTO {
    CaixaDTO caixa;
    QList<VendasFormaDTO> vendasPorForma;
    QMap<QString, double> recebimentosPorForma;
    QMap<QString, double> sangriasPorForma;       // a retirada pode sair de Dinheiro, Crédito, Débito ou Pix
    QMap<QString, double> suprimentosPorForma;
    double totalSangrias = 0;
    double totalSuprimentos = 0;
    double totalRecebimentos = 0;
    double totalVendas = 0;
    int quantidadeVendas = 0;
    QList<MovimentacaoCaixaDTO> movimentacoes;    // sangrias, suprimentos e recebimentos
    QList<MovimentacaoCaixaDTO> cancelamentos;
    QMap<QString, double> esperadoPorForma;       // Dinheiro, Crédito, Débito, Pix
    QMap<QString, double> outrasFormas;           // vendas/recebimentos fora da conferência (ex.: "Não Sei")
    QList<FechamentoFormaDTO> fechamento;         // preenchido após fechar
    // quem vendeu/movimentou sem ser o dono do caixa (vazio quando só ele operou)
    QList<OperadorSessaoCaixaDTO> operadoresDaSessao;
};

#endif // CAIXA_DTO_H
