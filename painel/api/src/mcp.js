import { timingSafeEqual } from 'node:crypto';

// Servidor MCP (somente leitura) para a IA de design (Canva, Figma...) consultar nome e preço dos produtos
// e produzir artes/encartes sem digitar. Protocolo: MCP "Streamable HTTP", sem sessão (cada POST é independente).
// Acesso: a chave vai na própria URL (/mcp/<chave>), porque os conectores de IA geralmente só pedem uma URL.
// Só sai nome, preço, unidade e código de barras. Nunca custo, margem, estoque ou vendas.
// Ligado só se MCP_DESIGN_CHAVE estiver definida (mínimo 24 caracteres); sem ela a rota nem existe.

const VERSAO_PROTOCOLO = '2025-03-26';
const moeda = (v) => 'R$ ' + Number(v ?? 0).toFixed(2).replace('.', ',');

const FERRAMENTAS = [
  {
    name: 'buscar_produtos',
    description: 'Busca produtos da loja pelo nome (qualquer parte, sem diferenciar acento ou maiúscula) ou pelo código de barras. '
      + 'Devolve nome, preço de venda, unidade e código de barras. Use para pegar os preços certos ao montar encartes, cartazes e posts.',
    inputSchema: {
      type: 'object',
      properties: {
        termo: { type: 'string', description: 'Parte do nome do produto (ex.: "pão", "coca 2l") ou código de barras.' },
        limite: { type: 'integer', minimum: 1, maximum: 50, description: 'Quantos resultados (padrão 20).' },
      },
      required: ['termo'],
    },
  },
  {
    name: 'produtos_atualizados_recentemente',
    description: 'Lista os produtos cadastrados ou que tiveram o preço/cadastro alterado nos últimos dias (bom para saber o que entrou ou mudou de preço).',
    inputSchema: {
      type: 'object',
      properties: {
        dias: { type: 'integer', minimum: 1, maximum: 90, description: 'Janela em dias (padrão 7).' },
        limite: { type: 'integer', minimum: 1, maximum: 100, description: 'Quantos resultados (padrão 30).' },
      },
    },
  },
];

function chaveConfere(informada, esperada) {
  const a = Buffer.from(String(informada ?? ''));
  const b = Buffer.from(String(esperada ?? ''));
  return a.length === b.length && timingSafeEqual(a, b);
}

const texto = (t, erro = false) => ({ content: [{ type: 'text', text: t }], ...(erro ? { isError: true } : {}) });

function formatar(linhas) {
  if (!linhas.length) return 'Nenhum produto encontrado.';
  const itens = linhas.map((x) => ({
    nome: x.descricao,
    preco: Number(Number(x.preco ?? 0).toFixed(2)),
    precoFormatado: moeda(x.preco) + (x.balanca || /^kg$/i.test(x.un_comercial ?? '') ? ' /kg' : ''),
    unidade: x.un_comercial,
    codigoBarras: x.codigo_barras,
  }));
  return JSON.stringify(itens, null, 2);
}

async function chamar(pool, lojaId, nome, args) {
  const colunas = 'descricao, preco, un_comercial, codigo_barras, coalesce(balanca, false) AS balanca';
  if (nome === 'buscar_produtos') {
    const termo = String(args?.termo ?? '').trim().slice(0, 100);
    if (!termo) return texto('Informe o termo de busca.', true);
    const limite = Math.min(Math.max(Number.parseInt(args?.limite ?? 20, 10) || 20, 1), 50);
    const r = await pool.query(
      `SELECT ${colunas} FROM espelho.produtos
       WHERE loja_id = $1 AND NOT coalesce(uso_loja, false)
         AND (codigo_barras = $2 OR painel.sem_acento(descricao) ILIKE '%' || painel.sem_acento($2) || '%')
       ORDER BY descricao LIMIT $3`,
      [lojaId, termo, limite],
    );
    return texto(formatar(r.rows));
  }
  if (nome === 'produtos_atualizados_recentemente') {
    const dias = Math.min(Math.max(Number.parseInt(args?.dias ?? 7, 10) || 7, 1), 90);
    const limite = Math.min(Math.max(Number.parseInt(args?.limite ?? 30, 10) || 30, 1), 100);
    const r = await pool.query(
      `SELECT ${colunas} FROM espelho.produtos
       WHERE loja_id = $1 AND NOT coalesce(uso_loja, false)
         AND greatest(painel.ts_seguro(atualizado_em), painel.ts_seguro(adicionado_em)) > now() - make_interval(days => $2)
       ORDER BY greatest(painel.ts_seguro(atualizado_em), painel.ts_seguro(adicionado_em)) DESC LIMIT $3`,
      [lojaId, dias, limite],
    );
    return texto(formatar(r.rows));
  }
  return null;
}

export async function tratarMcp({ pool, mensagem }) {
  const { id, method, params } = mensagem ?? {};
  const ok = (result) => ({ jsonrpc: '2.0', id, result });
  const erro = (code, message) => ({ jsonrpc: '2.0', id: id ?? null, error: { code, message } });
  if (id === undefined) return null;   // notificação: não tem resposta
  switch (method) {
    case 'initialize':
      return ok({ protocolVersion: VERSAO_PROTOCOLO, capabilities: { tools: {} }, serverInfo: { name: 'mercadopro-produtos', version: '1.0.0' },
        instructions: 'Consulta de produtos e preços da loja (somente leitura) para produzir artes e encartes.' });
    case 'ping':
      return ok({});
    case 'tools/list':
      return ok({ tools: FERRAMENTAS });
    case 'tools/call': {
      const l = await pool.query('SELECT id FROM painel.lojas WHERE ativa ORDER BY id LIMIT 1');
      if (!l.rowCount) return ok(texto('Nenhuma loja cadastrada.', true));
      const r = await chamar(pool, l.rows[0].id, params?.name, params?.arguments);
      return r ? ok(r) : erro(-32602, 'Ferramenta desconhecida.');
    }
    default:
      return erro(-32601, 'Método não suportado.');
  }
}

export function registrarMcp(app, { pool, config }) {
  if (!config.mcpDesignChave || config.mcpDesignChave.length < 24) return;
  const tentativas = new Map();
  app.get('/mcp/:chave', async (req, resp) => resp.code(405).send({ mensagem: 'Use POST.' }));
  app.post('/mcp/:chave', async (req, resp) => {
    const agora = Date.now();
    const t = tentativas.get(req.ip);
    if (!t || agora - t.inicio > 60000) tentativas.set(req.ip, { inicio: agora, n: 1 });
    else if (++t.n > 120) return resp.code(429).send({ mensagem: 'Muitas consultas seguidas.' });
    if (tentativas.size > 5000) for (const [ip, v] of tentativas) if (agora - v.inicio > 60000) tentativas.delete(ip);
    if (!chaveConfere(req.params.chave, config.mcpDesignChave)) return resp.code(404).send({ mensagem: 'Rota não encontrada.' });
    const corpo = req.body;
    if (Array.isArray(corpo)) {
      const respostas = (await Promise.all(corpo.map((m) => tratarMcp({ pool, mensagem: m })))).filter(Boolean);
      return respostas.length ? respostas : resp.code(202).send();
    }
    const r = await tratarMcp({ pool, mensagem: corpo });
    return r ?? resp.code(202).send();
  });
}
