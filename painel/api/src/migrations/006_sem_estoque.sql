-- produto que não entra nos números de estoque: marcado "sem controle de estoque" na loja (ovos, a granel...)
-- ou lanche (código de barras na faixa 200-500)
CREATE OR REPLACE FUNCTION painel.sem_estoque(cod text, marcado boolean, minimo int, maximo int) RETURNS boolean LANGUAGE sql IMMUTABLE AS $$
  SELECT coalesce(marcado, false) OR painel.eh_lanche(cod, minimo, maximo)
$$;
