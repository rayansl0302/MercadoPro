# Guia: usar os preços da loja na IA de design

Este guia ensina a IA do designer (Claude, com Canva e/ou Figma) a buscar nome e preço dos produtos direto do sistema da loja, para montar encartes, cartazes e posts sem digitar preço.

## 1. O que ele faz

Um conector (MCP) de **somente leitura**. A IA consulta:

| Ferramenta | Para quê |
|---|---|
| `buscar_produtos` | Acha produto por parte do nome (ex.: "pão", "coca 2l") ou por código de barras. Devolve nome, preço, unidade e código. |
| `produtos_atualizados_recentemente` | Lista o que entrou na loja ou mudou de preço nos últimos dias. |

Não mostra custo, margem, estoque nem vendas. Produto de peso vem com "/kg" (ex.: R$ 12,90 /kg).

Limitação atual: mostra só o **preço de venda atual**. O preço "de/por" de promoção ainda não aparece; informe o "de" e o "por" no pedido quando for promoção.

## 2. Conectar (uma vez só)

Você recebe da loja um endereço assim (a chave é secreta, não compartilhe):

```
https://mercadopro.atlantica-e.com.br/mcp/<CHAVE>
```

1. Abra a IA (Claude) e vá em **Configurações > Conectores** (ou "Integrações").
2. Escolha **Adicionar conector personalizado** (ou "Add custom connector").
3. Nome: `Produtos da loja`. URL: o endereço acima, **inteiro, com a chave no final** (termina em `/mcp/<CHAVE>`, não só `/mcp/`).
   Em **Autenticação**, marque **Sem login**. Não use "Entrar agora" nem "Fazer login quando necessário" (o servidor não tem OAuth e a conexão falha). Deixe **Cabeçalhos de requisição** vazio. A chave já está no endereço.
4. Salve. Em uma conversa nova, ative o conector `Produtos da loja`, junto com os de **Canva** e/ou **Figma**.

Teste: peça "busque o produto pão no conector da loja". Se aparecer nome e preço, está funcionando.

Se o conector der erro ao salvar, confira se o endereço foi colado inteiro, sem espaço, e avise a loja (a chave pode ter sido trocada).

## 3. Regras para a IA seguir

Cole isto no início da conversa, ou nas instruções do projeto, para a IA seguir sempre:

> Você tem o conector "Produtos da loja". Regras:
> 1. Nunca invente preço nem nome. Todo produto que aparecer na arte deve ser buscado antes com `buscar_produtos`.
> 2. Use o preço exatamente como vem em `precoFormatado` (ex.: "R$ 12,90 /kg"). Não arredonde nem mude.
> 3. Se a busca trouxer mais de um produto parecido (ex.: várias marcas), liste as opções e pergunte qual usar. Não escolha sozinha.
> 4. Se não achar o produto, avise e peça para eu conferir o nome. Não coloque preço de cabeça.
> 5. Para promoção, o conector só traz o preço atual. Use o valor que eu informar como "de" e "por".
> 6. Antes de finalizar a arte, mostre uma tabela com produto, preço e código de barras usados, para eu conferir.
> 7. O preço pode mudar. Se a arte ficar parada por dias, busque de novo antes de imprimir ou publicar.

## 4. Como pedir (exemplos)

**Encarte com vários produtos**
> Monte um encarte A4 de ofertas da semana no Canva com: pão francês, leite integral 1L, coca-cola 2L e arroz 5kg. Busque os preços no conector da loja. Cores: verde e amarelo. Mostre a tabela de preços usados antes de finalizar.

**Cartaz de um produto**
> Faça um cartaz vertical para o produto "queijo muçarela" com o preço da loja, letras grandes, preço em destaque.

**Promoção com de/por**
> Cartaz de promoção do pão francês. Preço de: R$ 14,90, por: R$ 12,90 (esse "por" confira se bate com o preço da loja no conector e me avise se estiver diferente).

**Descobrir novidades**
> Veja no conector os produtos atualizados nos últimos 7 dias e sugira três para um post de "chegou na loja".

**Figma**
> No arquivo do Figma, nos cartões da página "Preços", troque o nome e o preço de cada produto pelos valores do conector da loja. Liste o que mudou.

## 5. Dicas

- Escreva o nome como o cliente fala; a busca ignora acento e maiúscula, e acha qualquer parte do nome.
- Para ter certeza do item certo, use o código de barras na busca.
- Produto vendido por peso aparece com "/kg"; escreva isso na arte.
- Antes de imprimir ou postar, confira a tabela final com a loja.

## 6. Segurança (para a loja)

- A chave fica no `.env` do servidor (`MCP_DESIGN_CHAVE`). Quem tem o endereço lê os preços, e só isso.
- Para cortar o acesso (fim de campanha ou suspeita de vazamento), troque a chave no `.env` e reinicie; o endereço antigo para na hora. Depois envie o novo endereço ao designer.
