// Configuração lida do ambiente. Nada de segredo no código: tudo vem de variáveis de ambiente.
function inteiro(valor, padrao) {
  const n = Number.parseInt(valor ?? '', 10);
  return Number.isFinite(n) ? n : padrao;
}

export function carregarConfig(env = process.env) {
  return {
    porta: inteiro(env.PORT, 3000),
    // conexão do banco PostgreSQL (na Railway: a URL interna, sem porta pública)
    databaseUrl: env.DATABASE_URL || 'postgres://painel:painel@localhost:5432/painel',
    // cookie "Secure" só funciona em HTTPS; em desenvolvimento local (http) fica desligado
    cookieSeguro: (env.COOKIE_SEGURO ?? 'true') === 'true',
    // verificação em duas etapas obrigatória para entrar no painel
    exigir2fa: (env.EXIGIR_2FA ?? 'true') === 'true',
    sessaoHoras: inteiro(env.SESSAO_HORAS, 12),
    pedidoValidadeHoras: inteiro(env.PEDIDO_VALIDADE_HORAS, 24),
    cicloValidadeMinutos: inteiro(env.CICLO_VALIDADE_MINUTOS, 30),
    // loja considerada offline se não deu sinal há mais que isso
    lojaOfflineMinutos: inteiro(env.LOJA_OFFLINE_MINUTOS, 3),
    loginMaxTentativas: inteiro(env.LOGIN_MAX_TENTATIVAS, 5),
    loginBloqueioMinutos: inteiro(env.LOGIN_BLOQUEIO_MINUTOS, 15),
    pastaWeb: env.PASTA_WEB || '',
    ambiente: env.NODE_ENV || 'development',
  };
}
