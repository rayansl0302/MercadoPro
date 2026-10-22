-- itens de lanche (códigos de barras de 200 a 500 por padrão) não são estoque: têm quantidade fixa (ex.: 9999)
CREATE OR REPLACE FUNCTION painel.eh_lanche(cod text, minimo int, maximo int) RETURNS boolean LANGUAGE sql IMMUTABLE AS $$
  SELECT CASE WHEN btrim(coalesce(cod, '')) ~ '^[1-9][0-9]{0,3}$' THEN btrim(cod)::int BETWEEN minimo AND maximo ELSE false END
$$;
