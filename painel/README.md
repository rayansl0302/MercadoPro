# QEstoqueLoja — Painel (API Node + site React)

A loja (programa QEstoqueLoja) fala com esta API por HTTPS: envia um retrato completo e conferido dos dados a cada
hora e busca pedidos do painel a cada minuto. O banco da nuvem nunca é acessado direto pela loja.

## Rodar no seu computador (Docker)

```bash
# 1) compilar o site (uma vez, ou depois de mudar web/src)
docker run --rm -v "$PWD/web:/w" -w /w node:22-alpine sh -c "npm install && npx vite build"
# 2) subir banco + API + site em http://localhost:3000
docker compose up -d --build
# 3) criar a loja (mostra o token UMA vez) e os usuários
docker compose exec api node src/cli.js criar-loja "Minha loja"
docker compose exec api node src/cli.js criar-usuario "Nome" email@exemplo.com gerente
```

No QEstoqueLoja: Nuvem → endereço da API, token da loja, marcar "sincronizar neste computador".

## Testes

```bash
docker compose --profile test run --rm testes
```

## Segurança (resumo)
- Token da loja guardado só como hash; senhas com scrypt; sessão em cookie httpOnly/SameSite=Strict, guardada com hash.
- 2FA (TOTP) obrigatório em produção (`EXIGIR_2FA=true`); bloqueio após tentativas erradas.
- Perfil `consulta` só vê; `gerente` também envia pedidos (baixar/lançar conta a pagar).
- Colunas sensíveis (PIN, certificados, caminhos) nunca saem da loja (lista fechada em `contrato.json`).
- Na Railway: banco privado (sem proxy público), `COOKIE_SEGURO=true`, `EXIGIR_2FA=true`.
