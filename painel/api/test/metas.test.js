// Ponto de equilíbrio (painel/api/src/metas.js): conta pura, sem banco.
import { describe, expect, it } from 'vitest';
import { pontoDeEquilibrio } from '../src/metas.js';

describe('ponto de equilíbrio', () => {
  it('8 mil de despesa fixa a 30% de margem: precisa vender 26.666,67 no mês', () => {
    const r = pontoDeEquilibrio(8000, 30, 30);
    expect(r.faturamentoNecessario).toBe(26666.67);
    expect(r.porDia).toBe(888.89);
  });
  it('margem maior exige menos venda; dias de funcionamento mudam só o valor por dia', () => {
    expect(pontoDeEquilibrio(8000, 40, 26).faturamentoNecessario).toBe(20000);
    expect(pontoDeEquilibrio(8000, 40, 26).porDia).toBe(769.23);
  });
  it('margem zero ou inválida não explode', () => {
    expect(pontoDeEquilibrio(8000, 0, 30)).toEqual({ faturamentoNecessario: 0, porDia: 0 });
    expect(pontoDeEquilibrio(0, 30, 30).faturamentoNecessario).toBe(0);
  });
});
