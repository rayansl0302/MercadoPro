-- Transporte público: 4 passagens por dia (2 funcionários, ida e volta) a R$ 5,90 = R$ 23,60 por dia.
ALTER TABLE painel.metas ALTER COLUMN despesa_diaria SET DEFAULT 23.60;
UPDATE painel.metas SET despesa_diaria = 23.60 WHERE despesa_diaria = 100;
