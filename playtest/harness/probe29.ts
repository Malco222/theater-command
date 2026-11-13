// Version 58: is the attack card honest? Every field attack the scripted player orders, with the chance the card
// showed just before, set against what happened; split by whether the line was prepared (dug in and in supply).
// usage: probe29 ME FROM SEED UNTIL PLAN-JSON
import { campaign, date } from './lib';
const [me, fromS, seedS, untilS, planS] = process.argv.slice(2);
const plan = JSON.parse(planS || '{"minOdds":1.5}');
type Row = { n: number; won: number; chance: number; attLost: number; defLost: number; odds: number[] };
const tab: Record<string, Row> = {};
const band = (c: number): string => (c >= 0.995 ? '5 sure (100%)' : c >= 0.8 ? '4 80 to 99%' : c >= 0.6 ? '3 60 to 79%' : c >= 0.4 ? '2 40 to 59%' : '1 under 40%');
let shownEven = 0, nPrep = 0;
const failed: string[] = [];
const run = campaign(me, Number(fromS), Number(seedS), { ...plan, onAttack: (g: any, out: any, rep: any, target: any) => {
  if (rep.yielded || rep.town) return;
  const prep = (rep.prepared ?? 0) > 0, won = !!(rep.taken || rep.siege || rep.gates);
  for (const k of [`${prep ? 'prepared line' : 'open'} | all`, `${prep ? 'prepared line' : 'open'} | card said ${band(out.chance)}`]) {
    const t = (tab[k] ??= { n: 0, won: 0, chance: 0, attLost: 0, defLost: 0, odds: [] }); t.n++; if (won) t.won++; t.chance += out.chance; t.attLost += rep.attLost; t.defLost += rep.defLost; t.odds.push(out.odds);
  }
  if (prep) { nPrep++; shownEven += out.even; if (!won && failed.length < 6) failed.push(`${date(g.turn)} ${target.name}: odds ${out.odds.toFixed(2)}, card ${Math.round(out.chance * 100)}%, even chance at ${out.even.toFixed(1)} to 1, lost ${rep.attLost} against ${rep.defLost}`); }
} }, { until: Number(untilS), quiet: true });
for (const e of run.errors) console.log('ERROR', String(e).slice(0, 600));
const med = (a: number[]): number => { const b = [...a].sort((x, y) => x - y); return b.length ? b[Math.floor(b.length / 2)] : 0; };
for (const k of Object.keys(tab).sort()) {
  const t = tab[k];
  console.log(`CARD ${k.padEnd(40)} ${String(t.n).padStart(4)} attacks | card's mean chance ${String(Math.round((100 * t.chance) / t.n)).padStart(3)}% | won ${String(t.won).padStart(4)} (${Math.round((100 * t.won) / t.n)}%) | median odds ${med(t.odds).toFixed(2)} | lost a battle: mine ${Math.round(t.attLost / t.n)}, theirs ${Math.round(t.defLost / t.n)}`);
}
for (const f of failed) console.log(`   thrown back: ${f}`);
console.log(`TOTAL ${me} seed ${seedS}: attacks on prepared lines ${nPrep}; the card's even-chance mark there averaged ${nPrep ? (shownEven / nPrep).toFixed(2) : '-'} to 1`);
