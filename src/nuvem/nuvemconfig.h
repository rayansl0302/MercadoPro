#ifndef NUVEMCONFIG_H
#define NUVEMCONFIG_H

#include <QString>

// Configuração do módulo Nuvem (sincronização com o painel online). Fica no config.ini deste computador
// ("nuvem/..."): cada computador decide se sincroniza. A Nuvem nasce DESLIGADA: com ela desligada o programa
// se comporta exatamente como antes.
struct NuvemConfig {
    bool ativa = false;
    QString url;                      // https://... (http só para localhost, em desenvolvimento)
    QString token;                    // chave da loja (segredo): identifica a loja na API
    int intervaloMinutos = 60;        // intervalo do envio completo dos dados (mínimo 15)
    bool sincronizaNesteComputador = false;

    // erro de validação em português ("" = ok)
    QString validar() const;
    bool pronta() const { return ativa && sincronizaNesteComputador && validar().isEmpty(); }

    static NuvemConfig carregar();
    static bool salvar(const NuvemConfig &cfg);

    // http só vale para o próprio computador/rede de desenvolvimento; a internet exige https
    static bool urlPermitida(const QString &url, QString *motivo = nullptr);
};

#endif // NUVEMCONFIG_H
