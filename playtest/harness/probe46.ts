// Version 65: what becomes of a landing in a computer-only war. usage: probe46 SEED FROM-TURN UNTIL NAT
// From FROM-TURN on, in every month NAT's news speaks of a landing: which of its corps changed province, lost men or
// died in its own move; where they stand after it and who holds that ground; and then, nation by nation for the rest
// of that month and the next, what happens to them (battles, moves, news of the place).
import { start, date, T } from './lib';
const [seedS, fromS, untilS, nat = 'UK'] = process.argv.slice(2);
const g: any = start(null, 1, Number(seedS));
let hl = 0, rep = 0;
let watch: Set<number> = new Set(), watchUntil = 0;
const where = (c: any): string => `${c.name}: ${c.dead ? 'DEAD' : `${g.regions[c.reg].name} (owner ${g.regions[c.reg].owner}, held by ${g.regions[c.reg].ctrl}${c.town !== undefined ? ', in the town' : ''})`} ${Math.round(c.divs * c.str * 10) / 10} divisions`;
while (g.turn <= Number(untilS)) {
  const t = g.turn;
  while (g.turn === t) {
    const before = new Map<number, { reg: number; str: number }>(g.corps.filter((c: any) => c.nat === nat && !c.dead).map((c: any) => [c.id, { reg: c.reg, str: c.divs * c.str }]));
    const who = (T as any).stepAI(g);
    const news = (g.headlines.slice(hl) as any[]).filter((h) => h.turn >= Number(fromS));
    const landed = who === nat && news.some((h) => /from the sea|thrown back into the sea/.test(h.text) && h.nat === nat);
    if (landed) {
      console.log(`\n${date(t)}, ${nat}'s own move:`);
      for (const h of news.filter((h) => h.nat === nat)) console.log(`      news: ${h.text.slice(0, 260)}`);
      for (const c of g.corps.filter((x: any) => x.nat === nat)) {
        const b = before.get(c.id); if (!b) continue;
        if (c.dead || c.reg !== b.reg) { console.log(`      ${g.regions[b.reg].name} -> ${where(c)} (had ${Math.round(b.str * 10) / 10})`); if (!['UK', 'EGY', 'ITA'].includes(g.regions[c.reg].owner) || c.dead) watch.add(c.id); }
      }
      watchUntil = t + 2;
    } else if (watch.size && t <= watchUntil) {
      const bat = (g.reports.slice(rep) as any[]).filter((r) => [...(r.attackers ?? []), ...(r.defenders ?? [])].some((u: any) => watch.has(u.id)));
      const moved = g.corps.filter((c: any) => watch.has(c.id) && (c.dead || before.get(c.id)?.reg !== c.reg));
      const places = new Set(g.corps.filter((c: any) => watch.has(c.id) && !c.dead).map((c: any) => g.regions[c.reg].name));
      const local = news.filter((h) => [...places].some((p) => h.text.includes(p)));
      if (bat.length || moved.length || local.length) {
        console.log(`   ${date(t)} after ${who}:`);
        for (const r of bat) console.log(`      battle: ${r.attNat} on ${r.defNat} at ${r.regionName}${r.town ? ' (the town)' : ''}: ${r.taken ? 'taken' : r.yielded ? 'yielded' : 'held'}; attackers ${r.attackers?.length} corps lost ${r.attLost}, defenders ${r.defenders?.length} lost ${r.defLost}`);
        for (const c of moved) console.log(`      ${where(c)}`);
        for (const h of local) console.log(`      news: ${h.text.slice(0, 260)}`);
      }
    }
    if (who === nat && watch.size && t <= watchUntil && !landed) console.log(`   ${date(t)} after ${nat}'s move they stand: ${g.corps.filter((c: any) => watch.has(c.id)).map(where).join('; ')}`);
    hl = g.headlines.length; rep = g.reports.length;
  }
}
