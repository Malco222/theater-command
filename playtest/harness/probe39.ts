// Version 61: the undecided computer-only war. Why is a war nobody won by January 1947 still going?
// usage: probe39 SEEDS [UNTIL] [LOSER]
// Plays computer-only wars. Each January from 1943 it prints the great powers' provinces. At the end it lists every
// province the loser (Germany unless named) still holds: its troops, their works, the enemies beside it, the odds a
// full attack by the strongest of them would get (what the roads carry), and when it was last attacked.
import { start, date, T, atWar, hostileUnits } from './lib';
import { limitAttackers, worksOf } from '@tc/campaign/game';
import { besieged, siegeView, whyNotStorm, besiegerOf } from '@tc/campaign/siege';
const args = process.argv.slice(2);
const seeds = (args[0] ?? '9').split(',').map(Number);
const until = Number(args[1] ?? 89);
const loser = args[2] ?? 'GER';
const X = T as any;
const divs = (cs: any[]): number => cs.reduce((s, c) => s + (c.divs ?? 0) * (c.str ?? 1), 0);
for (const seed of seeds) {
  const g = start(null, 1, seed);
  const last: Record<number, number> = {}; const count: Record<number, number> = {};
  let lastRep = 0; const byHalf: Record<string, number> = {};
  console.log(`\n== seed ${seed}`);
  while (g.turn <= until) {
    const t = g.turn;
    if (t >= 41 && (t - 41) % 12 === 0) console.log(`${date(t).padEnd(15)} provinces ${['GER', 'SOV', 'UK', 'USA', 'ITA', 'FRA'].map((n) => `${n} ${g.regions.filter((r) => r.ctrl === n).length}${g.nations[n]?.alive ? '' : ' (gone)'}`).join(', ')}`);
    T.runTurnAI(g);
    for (const r of g.reports.slice(lastRep) as any[]) {
      const reg = g.regions.find((x) => x.name === r.regionName);
      if (reg) { last[reg.id] = t; if (t > until - 24) count[reg.id] = (count[reg.id] ?? 0) + 1; }
      if (t > until - 36) { const k = `${1939 + Math.floor((t + 7) / 12)}${(t + 7) % 12 < 6 ? 'a' : 'b'}`; byHalf[k] = (byHalf[k] ?? 0) + 1; }
    }
    lastRep = g.reports.length;
  }
  const L = g.nations[loser];
  console.log(`${date(g.turn).padEnd(15)} provinces ${['GER', 'SOV', 'UK', 'USA', 'ITA', 'FRA'].map((n) => `${n} ${g.regions.filter((r) => r.ctrl === n).length}${g.nations[n]?.alive ? '' : ' (gone)'}`).join(', ')}`);
  console.log(`battles by half year in the last three years: ${Object.entries(byHalf).map(([k, v]) => `${k} ${v}`).join(', ')}`);
  console.log(`${loser}: alive ${L.alive}, capital ${g.regions[(L as any).capital]?.name} held by ${g.regions[(L as any).capital]?.ctrl}; at war with ${L.atWar.join(' ')}`);
  const held = g.regions.filter((r) => r.ctrl === loser);
  const front = held.filter((r) => r.nb.some((j) => atWar(g, loser, g.regions[j].ctrl)));
  console.log(`${loser} holds ${held.length} provinces, ${front.length} beside an enemy. Those beside an enemy:`);
  for (const r of front.sort((a, b) => (last[b.id] ?? 0) - (last[a.id] ?? 0)).slice(0, 14)) {
    const mine = r.units.filter((u: any) => !u.dead && (u.nat === loser || !atWar(g, loser, u.nat)) && u.kind !== 'fort');
    const field = mine.filter((u: any) => u.town === undefined), dug = field.filter((u: any) => u.dug);
    const works = dug.length ? dug.reduce((s: number, u: any) => s + worksOf(u), 0) / dug.length : 0;
    const by: Record<string, any[]> = {};
    for (const j of r.nb) for (const u of g.regions[j].units as any[]) if (!u.dead && atWar(g, loser, u.nat) && u.kind !== 'fort' && u.town === undefined) (by[u.nat] ??= []).push(u);
    const foes = Object.entries(by).sort((a, b) => divs(b[1]) - divs(a[1]));
    let say = 'no enemy field corps beside it';
    if (foes.length) {
      const [nat, cs] = foes[0];
      const going = limitAttackers(g, cs.filter((c) => c.kind !== 'eng'), r);
      const a = X.previewAttack(g, going, r);
      say = `${foes.map(([n, c]) => `${n} ${c.length} corps (${divs(c).toFixed(0)} divisions)`).join(', ')}; a full ${nat} attack: ${a ? `${going.length} corps at ${a.odds.toFixed(2)} to 1, prepared ${Number(a.prepared ?? 0).toFixed(2)}, works ${Number(a.works ?? 0).toFixed(1)}` : 'none possible'}`;
      if (a && (r.id === (L as any).capital || args[3] === 'lines')) console.log('      ' + (a.lines ?? []).map((l: any) => `${l.label ?? l.name ?? ''}: ${l.text ?? l.value ?? ''}`).join(' | ').slice(0, 900));
    }
    if (besieged(g, r) && args[3] === 'dump') { const v: any = siegeView(g, r.id); const by = besiegerOf(g, r); console.log('      SIEGE by ' + by + ', garrison ' + ((r as any).garr ?? []).map((u: any) => `${u.nat} ${u.name} ${(u.divs * u.str).toFixed(1)}`).join(', ') + ' | may not storm because: ' + (by ? whyNotStorm(g, r, by) : '-') + ' | ' + (v?.lines ?? []).join(' ').slice(0, 1200)); }
    console.log(`   ${r.name.padEnd(18)} ${field.length} corps in the field (${divs(field).toFixed(0)} divisions), ${mine.length - field.length} in the town, works ${works.toFixed(1)}${besieged(g, r) ? ', BESIEGED' : ''} | ${say} | last battle ${last[r.id] ? date(last[r.id]) : 'never'}, ${count[r.id] ?? 0} in the last two years`);
  }
}
