-- gestão de usuários: perfil "admin" (cria/edita usuários) e troca de senha obrigatória no primeiro acesso
ALTER TABLE painel.usuarios ADD COLUMN IF NOT EXISTS trocar_senha boolean NOT NULL DEFAULT false;
ALTER TABLE painel.usuarios ADD COLUMN IF NOT EXISTS senha_trocada_em timestamptz;
COMMENT ON COLUMN painel.usuarios.perfil IS 'consulta | gerente (pede ações) | admin (também gerencia usuários)';

-- quem criou o painel (o primeiro usuário) vira admin, para não depender de acesso ao servidor
UPDATE painel.usuarios SET perfil = 'admin'
 WHERE id = (SELECT min(id) FROM painel.usuarios)
   AND NOT EXISTS (SELECT 1 FROM painel.usuarios WHERE perfil = 'admin');
