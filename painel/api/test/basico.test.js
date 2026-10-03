import { existsSync, readFileSync } from 'node:fs';
import { describe, expect, it } from 'vitest';
import { carregarContrato, normalizarTexto } from '../src/contrato.js';
import { checksum } from '../src/ciclo.js';
import { base32Decodificar, codigoTotp, conferirSenha, conferirTotp, gerarSegredoTotp, hashSenha, senhaForte } from '../src/seguranca.js';
import { contrato } from '../src/contrato.js';
import { createHash } from 'node:crypto';

const vetores = JSON.parse(readFileSync(new URL('./checksum_vetores.json', import.meta.url), 'utf8'));

describe('checksum compartilhado com a loja', () => {
  for (const caso of vetores.casos) {
    it(`bate com o vetor: ${caso.nome}`, () => {
      expect(checksum(caso.linhas)).toBe(caso.checksum);
    });
  }
  it('nulo e texto vazio são diferentes', () => {
    expect(checksum([[null]])).not.toBe(checksum([['']]));
  });
});

describe('contrato de dados', () => {
  it('é igual ao do QEstoqueLoja quando o repositório da loja está disponível', () => {
    const caminhoLoja = process.env.CONTRATO_LOJA;
    if (!caminhoLoja || !existsSync(caminhoLoja)) return; // sem o repositório da loja ao lado: nada a comparar
    const daLoja = carregarContrato(caminhoLoja);
    expect(daLoja.versao).toBe(contrato.versao);
    expect(daLoja.hash).toBe(contrato.hash);
  });

  it('o hash não depende de quebra de linha (Windows x Linux)', () => {
    const bruto = readFileSync(new URL('../src/contrato.json', import.meta.url), 'utf8');
    const comCrlf = bruto.replace(/\n/g, '\r\n');
    const h = (t) => createHash('sha256').update(normalizarTexto(t), 'utf8').digest('hex');
    expect(h(comCrlf)).toBe(h(bruto));
    expect(h(bruto)).toBe(contrato.hash);
  });

  it('não lista nenhuma coluna que pareça segredo', () => {
    const proibidas = ['pin', 'hash', 'salt', 'senha', 'password', 'token', 'cpf', 'email', 'endereco', 'xml_path', 'caminho', 'certificado', 'tentativas'];
    for (const t of contrato.tabelas)
      for (const c of t.colunas) for (const p of proibidas) expect(c.nome.toLowerCase().includes(p), `${t.nome}.${c.nome}`).toBe(false);
    for (const t of ['config_caixa', 'sync_trava', 'rascunho_venda']) expect(contrato.tabela(t)).toBeUndefined();
  });
});

describe('senhas e verificação em duas etapas', () => {
  it('guarda a senha com hash e confere', async () => {
    const h = await hashSenha('Senha-Forte-123');
    expect(h.startsWith('scrypt$')).toBe(true);
    expect(h).not.toContain('Senha-Forte-123');
    expect(await conferirSenha('Senha-Forte-123', h)).toBe(true);
    expect(await conferirSenha('outra-senha', h)).toBe(false);
    expect(await conferirSenha('x', 'lixo')).toBe(false);
  });

  it('exige senha com tamanho e variedade mínimos', () => {
    expect(senhaForte('curta1')).toBe(false);
    expect(senhaForte('somenteletrasaqui')).toBe(false);
    expect(senhaForte('Senha-Forte-123')).toBe(true);
  });

  it('TOTP bate com o vetor da RFC 6238 (SHA-1, 6 dígitos)', () => {
    const segredo = 'GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ'; // "12345678901234567890"
    expect(base32Decodificar(segredo).toString()).toBe('12345678901234567890');
    expect(codigoTotp(segredo, 59 * 1000)).toBe('287082');
    expect(codigoTotp(segredo, 1111111109 * 1000)).toBe('081804');
  });

  it('aceita o código do passo atual e vizinhos, e recusa outros', () => {
    const segredo = gerarSegredoTotp();
    const agora = Date.now();
    expect(conferirTotp(segredo, codigoTotp(segredo, agora), agora)).toBe(true);
    expect(conferirTotp(segredo, codigoTotp(segredo, agora - 30000), agora)).toBe(true);
    expect(conferirTotp(segredo, codigoTotp(segredo, agora + 120000), agora)).toBe(false);
    expect(conferirTotp(segredo, '12345', agora)).toBe(false);
    expect(conferirTotp(segredo, 'abcdef', agora)).toBe(false);
  });
});
