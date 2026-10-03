import { createHash } from 'node:crypto';
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import path from 'node:path';

// Contrato de dados com a loja: lista de permissão das tabelas/colunas aceitas. A cópia em contrato.json é
// idêntica à do QEstoqueLoja (src/nuvem/contrato.json); um teste confere as duas quando o repositório da loja
// está ao lado.
const arquivo = path.join(path.dirname(fileURLToPath(import.meta.url)), 'contrato.json');

// quebras de linha não podem mudar o hash (Windows x Linux)
export function normalizarTexto(texto) {
  return texto.replace(/\r\n/g, '\n');
}

export function carregarContrato(caminho = arquivo) {
  const bruto = normalizarTexto(readFileSync(caminho, 'utf8'));
  const json = JSON.parse(bruto);
  const tabelas = Object.entries(json.tabelas).map(([nome, t]) => ({
    nome,
    pk: t.pk,
    colunas: t.colunas.map((c) => ({ nome: c.n, tipo: c.t })),
  }));
  return {
    versao: json.versao,
    hash: createHash('sha256').update(bruto, 'utf8').digest('hex'),
    tabelas,
    tabela: (nome) => tabelas.find((t) => t.nome === nome),
  };
}

export const contrato = carregarContrato();

// tipo do contrato -> tipo no PostgreSQL (datas ficam como texto, como a loja manda; as visões convertem com segurança)
export const tiposPg = {
  int: 'bigint',
  dec: 'numeric(14,2)',
  real: 'double precision',
  text: 'text',
  bool: 'boolean',
  ts: 'text',
};

// expressão que converte a coluna de preparação (texto) para o tipo final
export function expressaoConversao(coluna, tipo) {
  const col = `"${coluna}"`;
  switch (tipo) {
    case 'int':
      return `${col}::bigint`;
    case 'dec':
      return `${col}::numeric(14,2)`;
    case 'real':
      return `${col}::double precision`;
    case 'bool':
      return `CASE WHEN ${col} IS NULL THEN NULL ELSE ${col} = '1' END`;
    default:
      return col;
  }
}
