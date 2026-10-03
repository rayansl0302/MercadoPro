#ifndef CONTASPAGAR_SERVICE_H
#define CONTASPAGAR_SERVICE_H

#include <QObject>
#include <QList>
#include <functional>
#include "../dto/ContasPagar_dto.h"
#include "../repository/contaspagar_repository.h"

// Contas a pagar: lançamento (com parcelas), baixa, estorno e cancelamento. Cada conta pertence a uma
// empresa (CNPJ); o lançamento usa a empresa ativa.
class ContasPagar_service : public QObject
{
    Q_OBJECT
public:
    struct Resultado {
        bool ok = false;
        QString msg;
        qlonglong id = 0;          // primeira conta criada (lançamento) ou a conta afetada
        int quantidade = 0;        // parcelas criadas
    };

    enum class Periodicidade { Mensal, Quinzenal, Semanal };

    explicit ContasPagar_service(QObject *parent = nullptr);

    // Estornar baixa e cancelar são só do gerente. Quem registra o autorizador é o main.cpp
    // (o mesmo critério do cadastro de operadores); sem autorizador, tudo passa (testes).
    static void definirAutorizador(std::function<bool()> autorizador);

    static QStringList formasDePagamento();
    static QStringList categoriasSugeridas();

    // Lança uma conta; com parcelas > 1 divide o valor total (a diferença de centavos vai para a última
    // parcela) e gera os vencimentos a partir do primeiro, na periodicidade escolhida.
    // origem (opcional): quem pediu quando não foi um operador logado, ex.: "Painel: Maria". Vai para o log de acesso.
    Resultado lancar(const ContaPagarDTO &base, int parcelas = 1,
                     Periodicidade periodicidade = Periodicidade::Mensal, const QString &origem = QString());

    Resultado alterar(const ContaPagarDTO &conta);
    Resultado baixar(qlonglong id, double valorPago, const QString &forma, const QDateTime &quando = QDateTime(),
                     const QString &origem = QString());
    Resultado estornarBaixa(qlonglong id);
    Resultado cancelar(qlonglong id, const QString &motivo);

    QList<ContaPagarDTO> listar(const FiltroContasPagarDTO &filtro);
    ContaPagarDTO getConta(qlonglong id);
    ResumoContasPagarDTO resumo(qlonglong idEmpresa);

    // Divisão em parcelas, exposta para teste: soma exata do total.
    static QList<double> dividirValor(double total, int parcelas);

private:
    ContasPagar_repository repo;
    static std::function<bool()> autorizador;
    static bool autorizado();
};

#endif // CONTASPAGAR_SERVICE_H
