#ifndef DESCONTOCPF_SERVICE_H
#define DESCONTOCPF_SERVICE_H

#include <QDate>
#include <QString>

// Desconto para cliente identificado (cadastrado com CPF): um percentual que o operador marca no fim da venda.
// Configurável: liga/desliga, percentual, período (campanha sazonal) e teto em R$ por venda.
// Guardado em config_caixa; não mexe em preço cadastrado nem em estoque.
class DescontoCpf_service
{
public:
    struct Config {
        bool ativo = false;
        double percentual = 5.0;
        bool comPeriodo = false;   // false = vale sempre
        QDate de;
        QDate ate;
        double tetoReais = 0;      // 0 = sem teto
    };

    static Config carregar();
    static bool salvar(const Config &c, QString *erro = nullptr);

    // vale hoje? (ligado, percentual > 0 e dentro do período, se houver)
    static bool vigente(const Config &c, const QDate &hoje);

    // Desconto em R$ sobre o total da venda (2 casas); 0 se não vigente.
    static double calcular(const Config &c, double totalVenda, const QDate &hoje);

    // "Desconto cliente cadastrado (5%)"
    static QString rotulo(const Config &c);
};

#endif // DESCONTOCPF_SERVICE_H
