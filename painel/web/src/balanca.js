// Regras da etiqueta de balança (produto vendido a granel, preço por kg). Sem dependências: o que é cálculo
// fica aqui para ser testado; o desenho (canvas) está em etiqueta.js e a tela em balanca.jsx.
//
// Código de barras EAN-13 de balança, prefixo 2:  2 CCCCCC VVVVV D
//   2       prefixo de uso interno/balança
//   CCCCCC  código do produto, 6 dígitos (o id do produto no cadastro da loja)
//   VVVVV   VALOR TOTAL em centavos, 5 dígitos (até R$ 999,99)
//   D       dígito verificador EAN-13
// Valores são sempre inteiros (gramas e centavos) para não acumular erro de ponto flutuante.

export const MAX_CENTAVOS = 99999;
export const MAX_ID_PRODUTO = 999999;

// dígito verificador EAN-13 dos 12 primeiros dígitos
export function digitoEan13(base12) {
  if (!/^\d{12}$/.test(base12)) throw new Error('A base do EAN-13 precisa ter 12 dígitos.');
  let soma = 0;
  for (let i = 0; i < 12; i += 1) soma += Number(base12[i]) * (i % 2 === 0 ? 1 : 3);
  return (10 - (soma % 10)) % 10;
}

export function eanValido(ean13) {
  return /^\d{13}$/.test(ean13) && digitoEan13(ean13.slice(0, 12)) === Number(ean13[12]);
}

// "1,250", "1.25", "0,5" -> gramas (inteiro). null se inválido, zero ou acima de 999,999 kg.
export function lerPesoEmGramas(texto) {
  const t = String(texto ?? '').trim().replace(',', '.');
  if (!/^\d{1,3}(\.\d{1,3})?$/.test(t)) return null;
  const [kg, frac = ''] = t.split('.');
  const gramas = Number(kg) * 1000 + Number(frac.padEnd(3, '0'));
  return gramas > 0 ? gramas : null;
}

export const paraCentavos = (reais) => Math.round(Number(reais) * 100);

// peso (g) x preço por kg (centavos) -> total em centavos, arredondado para o centavo mais próximo
export function totalEmCentavos(gramas, precoKgCentavos) {
  return Math.round((gramas * precoKgCentavos) / 1000);
}

export function eanDeBalanca(idProduto, totalCentavos) {
  if (!Number.isInteger(idProduto) || idProduto < 1 || idProduto > MAX_ID_PRODUTO) {
    throw new Error('O código interno do produto não cabe no código de barras da balança (máximo 6 dígitos).');
  }
  if (!Number.isInteger(totalCentavos) || totalCentavos < 1) throw new Error('O valor total precisa ser maior que zero.');
  if (totalCentavos > MAX_CENTAVOS) throw new Error('O valor total passa de R$ 999,99, o máximo que cabe no código de barras da balança.');
  const base = `2${String(idProduto).padStart(6, '0')}${String(totalCentavos).padStart(5, '0')}`;
  return base + digitoEan13(base);
}

// Código que vai impresso na etiqueta: o MESMO código de barras cadastrado no produto, para o caixa ler e
// identificar o produto. EAN-13 válido sai como EAN-13; qualquer outro código (interno, EAN-8, curto) sai em CODE128.
export function codigoDaEtiqueta(codigoBarras) {
  const codigo = String(codigoBarras ?? '').trim();
  if (!codigo) throw new Error('Este produto está sem código de barras no cadastro.');
  if (!/^[\x20-\x7e]+$/.test(codigo)) throw new Error('O código de barras do produto tem caracteres que a etiqueta não imprime.');
  return { codigo, formato: eanValido(codigo) ? 'EAN13' : 'CODE128' };
}

export const moedaDeCentavos =(c) => (c / 100).toLocaleString('pt-BR', { style: 'currency', currency: 'BRL' });
export const pesoDeGramas = (g) => `${(g / 1000).toLocaleString('pt-BR', { minimumFractionDigits: 3, maximumFractionDigits: 3 })} kg`;

// o produto é vendido por peso? (cadastro com unidade KG)
export const unidadeEhQuilo = (un) => /^(kg|kgs|quilo|quilos)$/i.test(String(un ?? '').trim());
