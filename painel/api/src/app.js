import Fastify from 'fastify';
import cookie from '@fastify/cookie';
import helmet from '@fastify/helmet';
import fastifyStatic from '@fastify/static';
import { existsSync } from 'node:fs';
import { z } from 'zod';
import { entrar, lojaDoToken, sair, usuarioDaSessao } from './auth.js';
import { cancelarCiclo, concluirCiclo, ErroHttp, iniciarCiclo, receberLote } from './ciclo.js';
import { cancelarPedido, criarPedido, entregarPedidos, listarPedidos, registrarResultado, FORMAS_PAGAMENTO } from './pedidos.js';
import { registrarRotasConsulta } from './consultas.js';

const COOKIE = 'sid';

export function criarApp({ pool, config }) {
  const app = Fastify({ logger: config.ambiente !== 'test', bodyLimit: 5 * 1024 * 1024, trustProxy: true });

  app.register(helmet, { contentSecurityPolicy: { directives: { defaultSrc: ["'self'"], styleSrc: ["'self'", "'unsafe-inline'"], imgSrc: ["'self'", 'data:'] } } });
  app.register(cookie);

  // erros viram JSON com mensagem em português; erro inesperado nunca vaza detalhe interno
  app.setErrorHandler((erro, req, resp) => {
    if (erro instanceof ErroHttp) return resp.code(erro.status).send({ mensagem: erro.message, detalhe: erro.detalhe });
    if (erro instanceof z.ZodError) {
      const msg = erro.issues.map((i) => `${i.path.join('.') || 'requisição'}: ${i.message}`).join('; ');
      return resp.code(422).send({ mensagem: `Dados inválidos — ${msg}` });
    }
    if (erro.validation || erro.statusCode === 400) return resp.code(400).send({ mensagem: 'Requisição inválida.' });
    if (erro.statusCode === 413) return resp.code(413).send({ mensagem: 'Requisição grande demais.' });
    req.log.error(erro);
    return resp.code(500).send({ mensagem: 'Erro interno. Tente novamente em instantes.' });
  });

  // ------------------------------------------------------------------ rotas da loja (token da loja)
  app.register(async (loja) => {
    loja.addHook('preHandler', async (req) => {
      const auth = req.headers.authorization ?? '';
      const l = await lojaDoToken(pool, auth.startsWith('Bearer ') ? auth.slice(7) : '');
      if (!l) throw new ErroHttp(401, 'Chave da loja inválida.');
      req.loja = l;
    });

    loja.get('/api/loja/ping', async (req) => ({ ok: true, loja: req.loja.nome }));

    loja.post('/api/loja/heartbeat', async (req) => {
      const b = req.body ?? {};
      await pool.query(
        `UPDATE painel.lojas SET ultimo_sinal = now(), terminal = $2, versao_app = $3, contrato_versao = $4, empresa_ativa = $5 WHERE id = $1`,
        [req.loja.id, String(b.terminal ?? '').slice(0, 100), String(b.versaoApp ?? '').slice(0, 50), Number.isInteger(b.contratoVersao) ? b.contratoVersao : null,
         Number.isInteger(b.empresaAtiva) ? b.empresaAtiva : null],
      );
      return { ok: true };
    });

    loja.get('/api/loja/pedidos', async (req) => ({ pedidos: await entregarPedidos(pool, req.loja) }));
    loja.post('/api/loja/pedidos/:id/resultado', async (req) => registrarResultado(pool, req.loja, req.params.id, req.body));

    loja.post('/api/loja/ciclo/iniciar', async (req) => ({ cicloId: await iniciarCiclo(pool, req.loja, req.body ?? {}) }));
    loja.put('/api/loja/ciclo/:id/tabela/:tabela', async (req) => receberLote(pool, req.loja, req.params.id, req.params.tabela, req.body ?? {}));
    loja.post('/api/loja/ciclo/:id/concluir', async (req) => ({ ok: true, ...(await concluirCiclo(pool, req.loja, req.params.id)) }));
    loja.post('/api/loja/ciclo/:id/cancelar', async (req) => cancelarCiclo(pool, req.loja, req.params.id));
  });

  // ------------------------------------------------------------------ rotas do painel (sessão por cookie)
  app.register(async (painel) => {
    // O painel só aceita requisições que mudam dados vindas da própria página (cookie SameSite + este cabeçalho)
    painel.addHook('preHandler', async (req) => {
      if (req.method !== 'GET' && req.headers['x-requested-with'] !== 'painel') throw new ErroHttp(403, 'Requisição não permitida.');
    });
    const exigirSessao = async (req) => {
      const u = await usuarioDaSessao(pool, req.cookies[COOKIE]);
      if (!u) throw new ErroHttp(401, 'Entre para continuar.');
      req.usuario = u;
    };
    const exigirGerente = async (req) => {
      await exigirSessao(req);
      if (req.usuario.perfil !== 'gerente') throw new ErroHttp(403, 'Seu perfil só permite consultar.');
    };
    const lojaUnica = async (req) => {
      const r = await pool.query('SELECT id, nome, fuso FROM painel.lojas WHERE ativa ORDER BY id LIMIT 1');
      if (!r.rowCount) throw new ErroHttp(404, 'Nenhuma loja cadastrada.');
      req.loja = r.rows[0];
    };

    painel.post('/api/painel/login', async (req, resp) => {
      const corpo = z.object({ email: z.string().max(200), senha: z.string().max(200), codigo: z.string().max(10).optional() }).parse(req.body ?? {});
      const r = await entrar(pool, config, { ...corpo, ip: req.ip });
      resp.setCookie(COOKIE, r.token, { httpOnly: true, sameSite: 'strict', secure: config.cookieSeguro, path: '/', maxAge: config.sessaoHoras * 3600 });
      return { usuario: r.usuario };
    });
    painel.post('/api/painel/logout', async (req, resp) => {
      await sair(pool, req.cookies[COOKIE]);
      resp.clearCookie(COOKIE, { path: '/' });
      return { ok: true };
    });
    painel.get('/api/painel/eu', async (req) => {
      await exigirSessao(req);
      return { usuario: req.usuario, formasPagamento: FORMAS_PAGAMENTO };
    });

    painel.get('/api/painel/loja', async (req) => {
      await exigirSessao(req);
      await lojaUnica(req);
      const l = await pool.query('SELECT nome, ultimo_sinal, terminal, versao_app, fuso FROM painel.lojas WHERE id = $1', [req.loja.id]);
      const c = await pool.query("SELECT concluido_em, linhas FROM painel.ciclos WHERE loja_id = $1 AND status = 'CONCLUIDO' ORDER BY concluido_em DESC LIMIT 1", [req.loja.id]);
      const p = await pool.query("SELECT count(*)::int AS n FROM painel.pedidos WHERE loja_id = $1 AND status IN ('PENDENTE','ENTREGUE')", [req.loja.id]);
      const sinal = l.rows[0].ultimo_sinal;
      return {
        nome: l.rows[0].nome,
        online: Boolean(sinal) && Date.now() - sinal.getTime() < config.lojaOfflineMinutos * 60 * 1000,
        ultimoSinal: sinal,
        terminal: l.rows[0].terminal,
        versaoApp: l.rows[0].versao_app,
        atualizadoEm: c.rows[0]?.concluido_em ?? null,
        linhasNoEspelho: c.rows[0]?.linhas ?? null,
        pedidosPendentes: p.rows[0].n,
      };
    });

    painel.get('/api/painel/pedidos', async (req) => {
      await exigirSessao(req);
      await lojaUnica(req);
      return { pedidos: await listarPedidos(pool, req.loja) };
    });
    painel.post('/api/painel/pedidos', async (req) => {
      await exigirGerente(req);
      await lojaUnica(req);
      const corpo = z.object({ tipo: z.string(), payload: z.record(z.any()), chave: z.string().max(100).optional() }).parse(req.body ?? {});
      const r = await criarPedido(pool, config, { loja: req.loja, usuario: req.usuario, ...corpo });
      return { pedido: r.pedido, repetido: r.repetido };
    });
    painel.post('/api/painel/pedidos/:id/cancelar', async (req) => {
      await exigirGerente(req);
      return cancelarPedido(pool, req.usuario, req.params.id);
    });

    registrarRotasConsulta(painel, { pool, config, exigirSessao, lojaUnica });
  });

  // ------------------------------------------------------------------ site (React compilado), se existir
  if (config.pastaWeb && existsSync(config.pastaWeb)) {
    app.register(fastifyStatic, { root: config.pastaWeb, wildcard: false });
    app.setNotFoundHandler((req, resp) => {
      if (req.url.startsWith('/api/')) return resp.code(404).send({ mensagem: 'Rota não encontrada.' });
      return resp.sendFile('index.html');
    });
  }

  app.get('/saude', async () => ({ ok: true }));
  return app;
}
