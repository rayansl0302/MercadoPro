-- Despesa que se repete todo dia, inclusive domingo (ex.: transporte de mercadoria), usada no lucro líquido do mês.
ALTER TABLE painel.metas ADD COLUMN IF NOT EXISTS despesa_diaria numeric(12,2) NOT NULL DEFAULT 100 CHECK (despesa_diaria >= 0);
