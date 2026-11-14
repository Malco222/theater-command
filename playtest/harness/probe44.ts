// Version 65: a war that is won on land and never ends. usage: probe44 SEEDS [UNTIL]
// Plays computer-only wars. From the month the Soviet Union or Germany falls it prints, each January and at the end:
// who is still at war with whom; for each pair of great powers still at war where their ground touches (the provinces,
// the field corps and divisions on each side, the odds a full attack would get either way) and, where it does not
// touch, what lies between them; what each staff says it is doing (a general offensive, a main effort, the defensive);
// how near each is to giving in; the battles, landings and air raids of the last twelve months; and where the British
// and American armies stand (at home, overseas).
import { start, date, T, atWar, allied, hostileUnits, collapseScore } from './lib';
import { limitAttackers } from '@tc/campaign/game';
const args = process.argv.slice(2);
const seeds = (args[0] ?? '4').split(',').map(Number);
const until = Number(args[1] ?? 89);
const X = T as any;
const GREAT = ['GER', 'ITA', 'UK', 'USA', 'SOV', 'FRA'];
const divs = (cs: any[]): number => cs.reduce((s, c) => s + (c.divs ?? 0) * (c.str ?? 1), 0);
for (const seed of seeds) {
  const g: any = start(null, 1, seed);
  console.log(`\n== seed ${seed}`);
  let lastRep = 0, lastHl = 0, since = 0;
  const year: { t: number; kind: string; a: string; d: string; where: string }[] = [];
  const look = (): void => {
    const live = Object.values(g.nations).filter((n: any) => n.alive && !n.dormant) as any[];
    const pairs: string[] = [];
    for (const a of live) for (const e of a.atWar) if (a.id < e && g.nations[e]?.alive) pairs.push(`${a.id}-${e}`);
    console.log(`${date(g.turn)}: provinces ${GREAT.map((n) => `${n} ${g.regions.filter((r: any) => r.ctrl === n).length}${g.nations[n]?.alive ? '' : ' (gone)'}`).join(', ')}`);
    console.log(`   at war: ${pairs.join(' ') || 'nobody'}`);
    for (const p of pairs) {
      const [a, b] = p.split('-');
      if (!GREAT.includes(a) || !GREAT.includes(b)) continue;
      const touch: [any, any][] = [];
      for (const r of g.regions) if (r.ctrl === a) for (const j of r.nb) if (g.regions[j].ctrl === b) touch.push([r, g.regions[j]]);
      if (!touch.length) { console.log(`   ${a} and ${b}: their ground touches nowhere`); continue; }
      const A = new Set(touch.map((t) => t[0])), B = new Set(touch.map((t) => t[1]));
      const field = (rs: Set<any>, nat: string): any[] => [...rs].flatMap((r) => r.units.filter((u: any) => !u.dead && u.kind !== 'fort' && u.town === undefined && (u.nat === nat || allied(g, u.nat, nat))));
      const fa = field(A, a), fb = field(B, b);
      let bestAB = 0, bestBA = 0, whereAB = '', whereBA = '';
      for (const [r, q] of touch) {
        const go = limitAttackers(g, r.units.filter((u: any) => !u.dead && u.kind !== 'fort' && u.kind !== 'eng' && u.town === undefined && allied(g, u.nat, a) || u.nat === a && !u.dead && u.kind !== 'fort' && u.kind !== 'eng' && u.town === undefined), q);
        const o = go.length ? X.previewAttack(g, go, q)?.odds ?? 0 : 0; if (o > bestAB) { bestAB = o; whereAB = `${r.name} at ${q.name}`; }
        const back = limitAttackers(g, q.units.filter((u: any) => !u.dead && u.kind !== 'fort' && u.kind !== 'eng' && u.town === undefined && (u.nat === b || allied(g, u.nat, b))), r);
        const o2 = back.length ? X.previewAttack(g, back, r)?.odds ?? 0 : 0; if (o2 > bestBA) { bestBA = o2; whereBA = `${q.name} at ${r.name}`; }
      }
      console.log(`   ${a} and ${b}: ground touches at ${touch.length} borders (${[...new Set(touch.map((t) => `${t[0].name}/${t[1].name}`))].slice(0, 6).join(', ')}); ${a} has ${fa.length} corps (${divs(fa).toFixed(0)} divisions) on them, ${b} ${fb.length} (${divs(fb).toFixed(0)}); best attack ${a} on ${b}: ${bestAB ? `${bestAB.toFixed(2)} to 1 (${whereAB})` : 'none'}; ${b} on ${a}: ${bestBA ? `${bestBA.toFixed(2)} to 1 (${whereBA})` : 'none'}`);
    }
    for (const n of GREAT) {
      const N = g.nations[n]; if (!N?.alive) continue;
      const v = X.rivalStaff ? X.rivalStaff(g, n) : null;
      const mine = g.corps.filter((c: any) => c.nat === n && !c.dead && c.kind !== 'fort');
      const home = mine.filter((c: any) => g.regions[c.reg].owner === n);
      const ships = (g.fleets ?? []).filter((f: any) => f.nat === n && !f.dead).length, air = (g.air ?? g.airFleets ?? []).filter((f: any) => f.nat === n && !f.dead).length;
      console.log(`   ${n}: ${v ? `${v.label}. ${v.line}` : 'no staff view'} | near to giving in ${collapseScore(g, n).toFixed(2)} | army ${mine.length} corps (${divs(mine).toFixed(0)} divisions), ${home.length} of them on its own soil; treasury ${Math.round(N.pp)}; fleets ${ships}, air groups ${air}`);
    }
    const cut = g.turn - 12, y = year.filter((e) => e.t > cut);
    const tally: Record<string, number> = {};
    for (const e of y) { const k = `${e.kind} ${e.a} on ${e.d}`; tally[k] = (tally[k] ?? 0) + 1; }
    console.log(`   the last twelve months: ${Object.entries(tally).sort((p, q) => q[1] - p[1]).slice(0, 14).map(([k, v]) => `${k} ${v}`).join('; ') || 'nothing at all'}`);
  };
  while (g.turn <= until) {
    T.runTurnAI(g);
    for (const r of g.reports.slice(lastRep) as any[]) year.push({ t: r.turn, kind: r.town ? 'assault' : 'battle', a: r.attNat, d: r.defNat, where: r.regionName });
    lastRep = g.reports.length;
    for (const h of g.headlines.slice(lastHl) as any[]) {
      const t: string = h.text;
      const k = /\blands? (at|in|on)\b|goes ashore|landing/i.test(t) ? 'landing' : /\bbombs?\b|\braids?\b|fights? for the sky|strikes? at shipping|strikes? the/i.test(t) ? 'air raid' : /sunk|sinks|convoy|U-boat|submarine/i.test(t) ? 'sea' : /asks for terms|surrenders|armistice|peace/i.test(t) ? 'peace news' : '';
      if (k) year.push({ t: h.turn, kind: k, a: h.nat ?? '?', d: '-', where: '' });
      if (k === 'peace news' || k === 'landing') console.log(`      [${date(h.turn)}] ${t.slice(0, 220)}`);
    }
    lastHl = g.headlines.length;
    if (!since && (!g.nations.SOV.alive || !g.nations.GER.alive)) { since = g.turn; console.log(`-- ${!g.nations.SOV.alive ? 'the Soviet Union' : 'Germany'} has fallen by ${date(g.turn)}`); look(); }
    else if (since && (g.turn - 5) % 12 === 0) look();
  }
  console.log('-- at the end'); look();
}
