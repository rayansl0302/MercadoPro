// Lista da balança em PDF: nome e código de todos os produtos vendidos por quilo.
import { unidadeEhQuilo } from './balanca.js';

export function produtosDeBalanca(produtos) {
  return produtos
    .filter((p) => unidadeEhQuilo(p.unidade))
    .sort((a, b) => String(a.descricao).localeCompare(String(b.descricao), 'pt-BR'));
}

export async function baixarListaBalanca(produtos, nomeLoja = '') {
  const lista = produtosDeBalanca(produtos);
  const { jsPDF } = await import('jspdf');
  const doc = new jsPDF({ unit: 'mm', format: 'a4' });
  const larg = doc.internal.pageSize.getWidth();
  const alt = doc.internal.pageSize.getHeight();
  const margem = 15;
  const hoje = new Date().toLocaleDateString('pt-BR');
  let y = 0;

  const cabecalho = () => {
    doc.setFont('helvetica', 'bold'); doc.setFontSize(15);
    doc.text('Lista da balança', margem, 18);
    doc.setFont('helvetica', 'normal'); doc.setFontSize(9);
    doc.text(`${nomeLoja ? nomeLoja + ' · ' : ''}${lista.length} produto(s) vendidos por kg · ${hoje}`, margem, 24);
    doc.setFont('helvetica', 'bold'); doc.setFontSize(10);
    doc.text('Código', margem, 33);
    doc.text('Produto', margem + 45, 33);
    doc.setDrawColor(120); doc.line(margem, 35, larg - margem, 35);
    doc.setFont('helvetica', 'normal');
    y = 41;
  };

  cabecalho();
  doc.setFontSize(10);
  for (const p of lista) {
    const linhas = doc.splitTextToSize(String(p.descricao ?? ''), larg - margem * 2 - 45);
    const h = linhas.length * 5 + 2;
    if (y + h > alt - 15) { doc.addPage(); cabecalho(); doc.setFontSize(10); }
    doc.text(String(p.codigoBarras ?? '—'), margem, y);
    doc.text(linhas, margem + 45, y);
    y += h;
  }
  if (!lista.length) doc.text('Nenhum produto cadastrado em kg.', margem, y);

  const paginas = doc.getNumberOfPages();
  for (let i = 1; i <= paginas; i++) {
    doc.setPage(i); doc.setFontSize(8);
    doc.text(`Página ${i} de ${paginas}`, larg - margem, alt - 8, { align: 'right' });
  }
  doc.save(`lista-balanca-${new Date().toISOString().slice(0, 10)}.pdf`);
  return lista.length;
}
