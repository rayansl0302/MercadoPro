// Regras da etiqueta de balança (painel/web/src/balanca.js): funções puras, sem banco.
import { describe, expect, it } from 'vitest';
import {
  codigoDaEtiqueta, digitoEan13, eanDeBalanca, eanValido, lerPesoEmGramas, paraCentavos, totalEmCentavos, unidadeEhQuilo,
} from '../../web/src/balanca.js';

describe('EAN-13', () => {
  it('calcula o dígito verificador (exemplos conhecidos)', () => {
    expect(digitoEan13('400638133393')).toBe(1);   // 4006381333931
    expect(digitoEan13('789000000001')).toBe(digitoEan13('789000000001'));
    expect(eanValido('4006381333931')).toBe(true);
    expect(eanValido('4006381333932')).toBe(false);
    expect(eanValido('123')).toBe(false);
  });

  it('monta o código de balança: 2 + produto(6) + valor em centavos(5) + dígito', () => {
    const ean = eanDeBalanca(123, 1613);
    expect(ean).toHaveLength(13);
    expect(ean.slice(0, 1)).toBe('2');
    expect(ean.slice(1, 7)).toBe('000123');
    expect(ean.slice(7, 12)).toBe('01613');
    expect(eanValido(ean)).toBe(true);
  });

  it('recusa o que não cabe no código', () => {
    expect(() => eanDeBalanca(1000000, 100)).toThrow(/6 dígitos/);
    expect(() => eanDeBalanca(0, 100)).toThrow();
    expect(() => eanDeBalanca(5, 0)).toThrow(/maior que zero/);
    expect(() => eanDeBalanca(5, 100000)).toThrow(/999,99/);
    expect(eanValido(eanDeBalanca(999999, 99999))).toBe(true);   // limites aceitos
  });
});

describe('peso e valor', () => {
  it('lê o peso em kg com vírgula ou ponto e devolve gramas', () => {
    expect(lerPesoEmGramas('1,250')).toBe(1250);
    expect(lerPesoEmGramas('1.25')).toBe(1250);
    expect(lerPesoEmGramas('0,5')).toBe(500);
    expect(lerPesoEmGramas(' 2 ')).toBe(2000);
    expect(lerPesoEmGramas('0,001')).toBe(1);
  });

  it('recusa peso inválido, zero ou com mais de 3 casas', () => {
    for (const ruim of ['', 'abc', '0', '0,000', '-1', '1,2345', '1,2,3', '1000', null, undefined]) expect(lerPesoEmGramas(ruim)).toBeNull();
  });

  it('total = peso x preço/kg, arredondado ao centavo e sem erro de ponto flutuante', () => {
    expect(totalEmCentavos(1250, paraCentavos(12.9))).toBe(1613);     // 1,250 x 12,90 = 16,125 -> 16,13
    expect(totalEmCentavos(1000, paraCentavos(39.9))).toBe(3990);
    expect(totalEmCentavos(333, paraCentavos(19.99))).toBe(666);      // 6,65667 -> 6,66
    expect(totalEmCentavos(1, paraCentavos(10))).toBe(1);             // 1 g a R$ 10/kg = 1 centavo
    expect(paraCentavos(0.1 + 0.2)).toBe(30);
    expect(paraCentavos(19.99)).toBe(1999);
  });
});

describe('unidade', () => {
  it('reconhece quilo', () => {
    for (const ok of ['KG', 'kg', ' Kg ', 'KGS', 'quilo']) expect(unidadeEhQuilo(ok)).toBe(true);
    for (const nao of ['UN', 'CX', 'G', '', null]) expect(unidadeEhQuilo(nao)).toBe(false);
  });
});

describe('código impresso na etiqueta', () => {
  it('usa o código de barras cadastrado no produto, sem montar outro', () => {
    expect(codigoDaEtiqueta('4006381333931')).toEqual({ codigo: '4006381333931', formato: 'EAN13' });   // EAN-13 válido
    expect(codigoDaEtiqueta(' 356227099 ')).toEqual({ codigo: '356227099', formato: 'CODE128' });        // código interno curto
    expect(codigoDaEtiqueta('4006381333932').formato).toBe('CODE128');                                    // 13 dígitos com dígito errado: não finge ser EAN
  });
  it('recusa produto sem código ou com caractere que não imprime', () => {
    expect(() => codigoDaEtiqueta('')).toThrow(/sem código/);
    expect(() => codigoDaEtiqueta(null)).toThrow(/sem código/);
    expect(() => codigoDaEtiqueta('12ç4')).toThrow(/caracteres/);
  });
});
