-- Despesas fixas mensais da loja (aluguel, funcionários, energia...) e a margem usada no ponto de equilíbrio.
-- Fica no painel (não vem da loja): quem edita é o gerente/administrador pelo próprio painel.
CREATE TABLE IF NOT EXISTS painel.despesas_fixas (
  id          serial PRIMARY KEY,
  loja_id     int NOT NULL REFERENCES painel.lojas(id) ON DELETE CASCADE,
  nome        text NOT NULL,
  valor       numeric(12,2) NOT NULL CHECK (valor >= 0),
  criado_em   timestamptz NOT NULL DEFAULT now()
);
CREATE INDEX IF NOT EXISTS despesas_fixas_loja ON painel.despesas_fixas (loja_id);

CREATE TABLE IF NOT EXISTS painel.metas (
  loja_id            int PRIMARY KEY REFERENCES painel.lojas(id) ON DELETE CASCADE,
  margem_percentual  numeric(5,2) NOT NULL DEFAULT 30 CHECK (margem_percentual > 0 AND margem_percentual <= 100),
  dias_funcionamento int NOT NULL DEFAULT 30 CHECK (dias_funcionamento BETWEEN 1 AND 31)
);
