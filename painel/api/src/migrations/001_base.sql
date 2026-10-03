-- Estrutura base do painel. As tabelas do espelho (cópia dos dados da loja) e as de preparação (stg) são criadas
-- a partir do contrato em src/espelho.js, para nunca divergirem dele.
CREATE SCHEMA IF NOT EXISTS painel;
CREATE SCHEMA IF NOT EXISTS espelho;
CREATE SCHEMA IF NOT EXISTS stg;
CREATE SCHEMA IF NOT EXISTS visoes;

CREATE TABLE IF NOT EXISTS painel.lojas (
  id               serial PRIMARY KEY,
  nome             text NOT NULL,
  token_hash       text NOT NULL UNIQUE,
  ativa            boolean NOT NULL DEFAULT true,
  criada_em        timestamptz NOT NULL DEFAULT now(),
  ultimo_sinal     timestamptz,
  terminal         text,
  versao_app       text,
  empresa_ativa    integer,
  contrato_versao  integer,
  fuso             text NOT NULL DEFAULT 'America/Sao_Paulo'
);

CREATE TABLE IF NOT EXISTS painel.ciclos (
  id               uuid PRIMARY KEY DEFAULT gen_random_uuid(),
  loja_id          integer NOT NULL REFERENCES painel.lojas(id),
  status           text NOT NULL DEFAULT 'ABERTO',      -- ABERTO | CONCLUIDO | CANCELADO | FALHOU | EXPIRADO
  iniciado_em      timestamptz NOT NULL DEFAULT now(),
  concluido_em     timestamptz,
  terminal         text,
  contrato_versao  integer,
  gerado_em        text,
  fuso             text,
  linhas           bigint,
  erro             text,
  declarado        jsonb NOT NULL DEFAULT '{}'::jsonb
);
CREATE INDEX IF NOT EXISTS idx_ciclos_loja ON painel.ciclos (loja_id, iniciado_em DESC);

-- último lote recebido por tabela: reenviar o mesmo lote (a internet caiu na resposta) não duplica linhas
CREATE TABLE IF NOT EXISTS painel.ciclo_lotes (
  ciclo_id      uuid NOT NULL REFERENCES painel.ciclos(id) ON DELETE CASCADE,
  tabela        text NOT NULL,
  ultimo_lote   integer NOT NULL,
  PRIMARY KEY (ciclo_id, tabela)
);

CREATE TABLE IF NOT EXISTS painel.usuarios (
  id               serial PRIMARY KEY,
  nome             text NOT NULL,
  email            text NOT NULL UNIQUE,
  senha_hash       text NOT NULL,
  perfil           text NOT NULL DEFAULT 'consulta',     -- consulta | gerente (pode pedir ações)
  totp_segredo     text,
  totp_ativo       boolean NOT NULL DEFAULT false,
  ativo            boolean NOT NULL DEFAULT true,
  falhas_login     integer NOT NULL DEFAULT 0,
  bloqueado_ate    timestamptz,
  criado_em        timestamptz NOT NULL DEFAULT now()
);

CREATE TABLE IF NOT EXISTS painel.sessoes (
  id_hash      text PRIMARY KEY,
  usuario_id   integer NOT NULL REFERENCES painel.usuarios(id) ON DELETE CASCADE,
  criada_em    timestamptz NOT NULL DEFAULT now(),
  expira_em    timestamptz NOT NULL,
  ip           text
);

CREATE TABLE IF NOT EXISTS painel.pedidos (
  id                   uuid PRIMARY KEY DEFAULT gen_random_uuid(),
  loja_id              integer NOT NULL REFERENCES painel.lojas(id),
  tipo                 text NOT NULL,
  payload              jsonb NOT NULL,
  chave                text,                              -- idempotência: o mesmo pedido enviado duas vezes vale uma só
  status               text NOT NULL DEFAULT 'PENDENTE',  -- PENDENTE | ENTREGUE | APLICADO | RECUSADO | CONFLITO | EXPIRADO | CANCELADO
  solicitante_id       integer REFERENCES painel.usuarios(id),
  solicitante_nome     text NOT NULL,
  estado_visto         jsonb,                             -- o que o usuário viu na tela ao pedir
  criado_em            timestamptz NOT NULL DEFAULT now(),
  expira_em            timestamptz NOT NULL,
  entregue_em          timestamptz,
  concluido_em         timestamptz,
  resultado_mensagem   text,
  resultado_detalhe    jsonb
);
CREATE UNIQUE INDEX IF NOT EXISTS ux_pedidos_chave ON painel.pedidos (loja_id, chave) WHERE chave IS NOT NULL;
CREATE INDEX IF NOT EXISTS idx_pedidos_status ON painel.pedidos (loja_id, status);

CREATE TABLE IF NOT EXISTS painel.auditoria (
  id          bigserial PRIMARY KEY,
  quando      timestamptz NOT NULL DEFAULT now(),
  usuario_id  integer,
  usuario     text,
  acao        text NOT NULL,
  detalhe     text,
  ip          text
);

CREATE TABLE IF NOT EXISTS painel.migracoes (
  nome        text PRIMARY KEY,
  aplicada_em timestamptz NOT NULL DEFAULT now()
);
