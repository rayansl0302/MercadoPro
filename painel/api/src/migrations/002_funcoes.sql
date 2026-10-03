-- Conversão segura de texto para data/hora: o espelho guarda as datas como a loja mandou (texto) e valores
-- inesperados viram NULL em vez de derrubar a consulta inteira.
CREATE OR REPLACE FUNCTION painel.ts_seguro(t text) RETURNS timestamp
LANGUAGE plpgsql IMMUTABLE AS $$
BEGIN
  IF t IS NULL OR btrim(t) = '' THEN
    RETURN NULL;
  END IF;
  RETURN t::timestamp;
EXCEPTION WHEN others THEN
  RETURN NULL;
END;
$$;

CREATE OR REPLACE FUNCTION painel.data_segura(t text) RETURNS date
LANGUAGE plpgsql IMMUTABLE AS $$
BEGIN
  IF t IS NULL OR btrim(t) = '' THEN
    RETURN NULL;
  END IF;
  RETURN left(t, 10)::date;
EXCEPTION WHEN others THEN
  RETURN NULL;
END;
$$;
