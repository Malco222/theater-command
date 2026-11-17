// TRIAL: the first day's books of several countries, line by line, as the interface shows them (x100).
//   node tools/trial/build-books.mjs && node dist/trial/books.mjs SAF IRN SAU
import { ledger, start, type Game } from '@pt/lib';
const g: Game = start(null, 1, 7);
const r1 = (x: number): number => Math.round(x * 10) / 10;
for (const n of process.argv.slice(2)) {
  const L = ledger(g, n);
  const line = (xs: { label: string; amount: number; count?: number }[]): string => xs.filter((x) => x.amount).map((x) => `${x.label}${x.count ? ` (${x.count})` : ''} ${r1(x.amount * L.scale)}`).join('; ');
  console.log(`${n}: income ${r1(L.gross * L.scale)} a month. From: ${line(L.income)}. Changes: ${line(L.changes) || 'none'}. Upkeep ${r1(L.upkeepTotal * L.scale)}.`);
}
