// Version 58: prepared defences from the player's seat.
// usage: probe28 ME FROM SEED UNTIL PLAN-JSON [TOWN]
// Every field battle the player's corps took part in, split by who attacked and whether the defence was prepared
// (dug in and in supply): how many, how many the attacker won, by odds, and the losses on each side.
// The odds are the attack card's (the two sides' strength before the day's luck: aPower over dPower); the report's own
// `odds` is the figure after the luck, which is always 1.2 or better for a win. Both are printed.
// With ROWS=1 in the environment every battle is also printed as a row: month, A or B (the player attacks or is attacked), attacker,
// defender, the prepared share, the card's odds, the odds on the day, won, men lost on each side, corps on each side, artillery corps in the attack, province.
// Each January: how many of the player's field corps are dug in. Who holds TOWN (Berlin unless named) in December 1944
// and in the last month. The longest run of months with no battle of the player's at all.
import { campaign, date } from './lib';
const [me, fromS, seedS, untilS, planS, town = 'Berlin'] = process.argv.slice(2);
const plan = JSON.parse(planS || '{"minOdds":1.5}');
const seen = new Set<number>();
type Row = { n: number; won: number; attLost: number; defLost: number; odds: number; day: number };
const tab: Record<string, Row> = {};
const won = (r: any): boolean => !!(r.taken || r.siege || r.gates);
const band = (o: number): string => (o < 1 ? '0 under 1' : o < 1.5 ? '1 1 to 1.5' : o < 2 ? '2 1.5 to 2' : o < 3 ? '3 2 to 3' : '4 3 or more');
const card = (r: any): number => (r.dPower > 0 ? Math.min(99, r.aPower / r.dPower) : 99);
const add = (k: string, r: any): void => { const t = (tab[k] ??= { n: 0, won: 0, attLost: 0, defLost: 0, odds: 0, day: 0 }); t.n++; if (won(r)) t.won++; t.attLost += r.attLost; t.defLost += r.defLost; t.odds += card(r); t.day += Math.min(99, r.odds); };
let held64 = '?', quiet = 0, longest = 0, longestEnd = 0, lastTurn = 0;
const hold = (g: any): string => { const r = g.regions.find((x: any) => x.name === town); return r ? r.ctrl : `no province ${town}`; };
const run = campaign(me, Number(fromS), Number(seedS), { ...plan, each: (g: any) => {
  lastTurn = g.turn;
  let fought = 0;
  for (const r of g.reports) {
    if (seen.has(r.id)) continue;
    seen.add(r.id);
    if (r.yielded || r.town) continue;
    const iAttack = r.attNat === me || r.attackers.some((u: any) => u.nat === me), iDefend = r.defNat === me || r.defenders.some((u: any) => u.nat === me);
    if (!iAttack && !iDefend) continue;
    fought++;
    const side = iAttack ? 'A the player attacks' : 'B the player is attacked', prep = (r.prepared ?? 0) > 0 ? 'prepared line' : 'open';
    if (process.env.ROWS) console.log(`ROW ${g.turn} ${iAttack ? 'A' : 'B'} ${r.attNat} ${r.defNat} ${(r.prepared ?? 0).toFixed(2)} ${card(r).toFixed(3)} ${Math.min(99, r.odds).toFixed(3)} ${won(r) ? 1 : 0} ${r.attLost} ${r.defLost} ${r.attackers.length} ${r.defenders.length} ${r.attackers.filter((u: any) => u.kind === 'art').length} ${String(r.regionName).replace(/ /g, '_')}`);
    add(`${side} | ${prep} | all odds`, r);
    add(`${side} | ${prep} | ${band(card(r))}`, r);
  }
  if (fought) quiet = 0; else if (g.nations[me].atWar.length) { quiet++; if (quiet > longest) { longest = quiet; longestEnd = g.turn; } }
  if (g.turn === 64) held64 = hold(g);
  if ((g.turn - 5) % 12 === 0) {
    const field = g.corps.filter((c: any) => c.nat === me && !c.dead && c.kind !== 'fort' && c.town === undefined);
    console.log(`${date(g.turn)}: field corps ${field.length}, dug in ${field.filter((c: any) => c.dug).length}, works finished ${field.filter((c: any) => c.dug && (c.dugFor ?? 0) >= 3).length}; provinces ${g.regions.filter((x: any) => x.ctrl === me).length}; at war with ${g.nations[me].atWar.length}`);
  }
} }, { until: Number(untilS), quiet: true });
for (const e of run.errors) console.log('ERROR', String(e).slice(0, 600));
for (const k of Object.keys(tab).sort()) {
  const t = tab[k];
  console.log(`BATTLES ${k.padEnd(52)} ${String(t.n).padStart(4)} fought | attacker won ${String(t.won).padStart(4)} (${Math.round((100 * t.won) / t.n)}%) | mean odds ${(t.odds / t.n).toFixed(2)} | on the day ${(t.day / t.n).toFixed(2)} | lost a battle: attacker ${(t.attLost / t.n).toFixed(1)}, defender ${(t.defLost / t.n).toFixed(1)}`);
}
console.log(`TOTAL ${me} seed ${seedS}: played to ${date(lastTurn)}${run.fellOn ? ` (fell ${run.fellOn})` : ''}; ${town} in December 1944: ${held64}; in the last month: ${hold(run.g)}; longest stretch at war with no battle of the player's: ${longest} months, ending ${longest ? date(longestEnd) : '-'}`);
