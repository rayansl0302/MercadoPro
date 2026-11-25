-- Diária paga por fora só aos domingos (dia de trabalho extra), somada ao transporte diário no lucro líquido.
ALTER TABLE painel.metas ADD COLUMN IF NOT EXISTS diaria_domingo numeric(12,2) NOT NULL DEFAULT 100 CHECK (diaria_domingo >= 0);
