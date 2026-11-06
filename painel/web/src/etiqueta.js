// Desenha a etiqueta da balança num <canvas>, na largura da cabeça de impressão da Niimbot B1 (384 px, 203 dpi).
// Preto no branco, sem meio-tom: é o que a térmica imprime bem.
import JsBarcode from 'jsbarcode';
import { moedaDeCentavos, pesoDeGramas } from './balanca.js';

export const LARGURA_PX = 384;
// altura (comprimento da etiqueta) em pixels a 203 dpi: 8 px por mm
export const ALTURAS = [['30 mm', 240], ['40 mm', 320], ['50 mm', 400]];

const FONTE = 'Arial, Helvetica, sans-serif';

// quebra o texto em no máximo `maxLinhas` linhas que caibam em `largura`; o que sobrar vira "…" no fim da última
export function quebrarTexto(ctx, texto, largura, maxLinhas) {
  const cabe = (t) => ctx.measureText(t).width <= largura;
  const cortar = (t, sufixo = '') => { let c = t; while (c.length > 1 && !cabe(c + sufixo)) c = c.slice(0, -1); return c + sufixo; };
  const todas = [];
  let atual = '';
  for (const p of String(texto).trim().split(/[ \t\r\n]+/).filter(Boolean)) {
    const teste = atual ? `${atual} ${p}` : p;
    if (!atual || cabe(teste)) atual = teste;
    else { todas.push(atual); atual = p; }
  }
  if (atual) todas.push(atual);
  const linhas = todas.slice(0, maxLinhas).map((l) => cortar(l));
  if (todas.length > maxLinhas) linhas[maxLinhas - 1] = cortar(linhas[maxLinhas - 1], '…');
  return linhas;
}

// dados: { nome, gramas, precoKgCentavos, totalCentavos, ean }
export function desenharEtiqueta(dados, alturaPx = 240, canvas = document.createElement('canvas')) {
  canvas.width = LARGURA_PX;
  canvas.height = alturaPx;
  const ctx = canvas.getContext('2d');
  ctx.fillStyle = '#fff';
  ctx.fillRect(0, 0, LARGURA_PX, alturaPx);
  ctx.fillStyle = '#000';
  ctx.textBaseline = 'top';
  const margem = 10;
  const util = LARGURA_PX - 2 * margem;

  // código de barras EAN-13 (desenhado antes só para medir o espaço que ele ocupa embaixo)
  const barras = document.createElement('canvas');
  JsBarcode(barras, dados.ean, {
    format: 'EAN13', width: 3, height: 54, displayValue: true, fontSize: 20, textMargin: 2, margin: 0,
    background: '#ffffff', lineColor: '#000000', font: 'Arial',
  });
  const escala = Math.min(1, util / barras.width);
  const bw = Math.round(barras.width * escala);
  const bh = Math.round(barras.height * escala);
  const yBarras = alturaPx - bh - 8;

  // nome do produto (até 2 linhas)
  let y = 8;
  ctx.font = `bold 26px ${FONTE}`;
  for (const linha of quebrarTexto(ctx, dados.nome, util, 2)) {
    ctx.fillText(linha, margem, y);
    y += 29;
  }

  // peso e preço por kg
  y += 2;
  ctx.font = `22px ${FONTE}`;
  ctx.fillText(`${pesoDeGramas(dados.gramas)}  ·  ${moedaDeCentavos(dados.precoKgCentavos)}/kg`, margem, y);
  y += 28;

  // valor total, grande, no espaço entre o texto e o código de barras
  const livre = yBarras - y;
  const tamanho = Math.max(28, Math.min(52, livre - 6));
  ctx.font = `bold ${tamanho}px ${FONTE}`;
  ctx.fillText(moedaDeCentavos(dados.totalCentavos), margem, y + Math.max(0, (livre - tamanho) / 2) - 2);

  ctx.imageSmoothingEnabled = false;
  ctx.drawImage(barras, Math.round((LARGURA_PX - bw) / 2), yBarras, bw, bh);
  return canvas;
}

// Etiqueta com o QR de acesso rápido à tela da balança (o funcionário aponta a câmera e abre a tela certa).
// QR à esquerda, em módulos inteiros (nítido na térmica), e a instrução à direita.
export function desenharEtiquetaQr(modulos, tamanhoModulos, alturaPx = 240, canvas = document.createElement('canvas')) {
  canvas.width = LARGURA_PX;
  canvas.height = alturaPx;
  const ctx = canvas.getContext('2d');
  ctx.fillStyle = '#fff';
  ctx.fillRect(0, 0, LARGURA_PX, alturaPx);
  ctx.fillStyle = '#000';
  const zonaQuieta = 4; // módulos de margem branca em volta do QR
  const total = tamanhoModulos + 2 * zonaQuieta;
  const lado = alturaPx - 16;
  const px = Math.max(1, Math.floor(lado / total)); // pixels por módulo, inteiro
  const ladoQr = px * total;
  const x0 = 8;
  const y0 = Math.round((alturaPx - ladoQr) / 2);
  for (let l = 0; l < tamanhoModulos; l++) {
    for (let c = 0; c < tamanhoModulos; c++) {
      if (modulos[l * tamanhoModulos + c]) ctx.fillRect(x0 + (c + zonaQuieta) * px, y0 + (l + zonaQuieta) * px, px, px);
    }
  }
  const xTexto = x0 + ladoQr + 6;
  const largura = LARGURA_PX - xTexto - 8;
  ctx.textBaseline = 'top';
  ctx.font = `bold 30px ${FONTE}`;
  let y = Math.max(10, Math.round(alturaPx / 2) - 70);
  for (const linha of quebrarTexto(ctx, 'ETIQUETA DE BALANÇA', largura, 3)) { ctx.fillText(linha, xTexto, y); y += 34; }
  ctx.font = `20px ${FONTE}`;
  y += 6;
  for (const linha of quebrarTexto(ctx, 'Aponte a câmera do celular', largura, 3)) { ctx.fillText(linha, xTexto, y); y += 24; }
  return canvas;
}
