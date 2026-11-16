// Version 79 (the design director's ruling 92): in the war nobody plays, every corps of the western Allies that
// comes to the continent of Europe by ship, and what became of it six months on. An arrival is a corps that opens a
// month off the continent (at home, in Britain, on an island, beyond Europe) and ends it on the continent. Kinds:
//   landing   told by the rules' own landing hook that month (a staff's landing over a beach)
//   ferry     not a landing, and the ground it stands on was its own side's as the month opened (a ship into a
//             harbour already held)
//   ferry+    not a landing, and it ended the month on ground the enemy held as the month opened (shipped in and
//             marched on, or the ground changed hands that month)
// Six months on each corps is alive or dead. A dead corps is told by where it died against its arrival: IN PLACE
// (the province it arrived in), BEHIND (another province its side held when it arrived) or OUT (ground the enemy
// held when it arrived: it marched out of the bridgehead), and by whether it was cut off from supply at the last
// month end it was seen alive or when it died.
// One SHIP line a group (flag, month, province, kind), one SUM line a war, and one ITALY line a war (ruling 94: when Italy
// signed, who held its mainland that month, the first western corps on Germany's home ground and where they came from).
// usage: [TC_DIAL=3] probe84 SEEDS(a,b,c) [UNTIL=80]     (build with build70.sh)
import { start, date, T, atWar, allied } from './lib';
import { landingWatch } from '@tc/campaign/ai';
import { beyondEurope } from '@tc/campaign/world';
const [seedsS = '17', untilS = '80'] = process.argv.slice(2);
const WATCH = new Set(['UK', 'USA', 'CAN']);
const OFF = new Set(['UK', 'IRE', 'ICE']);
for (const seed of seedsS.split(',').map(Number)) {
  const g: any = start(null, 1, seed, 'colonel', Number(process.env.TC_DIAL ?? 3));
  const isle = new Map<number, boolean>();
  const island = (id: number): boolean => { if (isle.has(id)) return isle.get(id)!; const seen = new Set<number>([id]); const q = [id];
    for (let i = 0; i < q.length && seen.size <= 8; i++) for (const j of g.regions[q[i]].nb) if (!seen.has(j)) { seen.add(j); q.push(j); }
    isle.set(id, seen.size <= 8); return seen.size <= 8; };
  const onCont = (id: number): boolean => { const r = g.regions[id]; return !!r && !r.overseas && !beyondEurope(r) && !OFF.has(r.owner) && !island(id); };
  type A = { id: number; nat: string; t: number; reg: number; kind: string; ctrl: string[]; dead?: boolean; where?: string; cut?: boolean; done?: boolean; diedT?: number };
  const arr: A[] = []; const last = new Map<number, { reg: number; sup: boolean }>();
  let landed = new Set<number>(); let gerFell = 0, err = '';
  // The Italian road (ruling 94): Italy's signing, who holds its mainland that month, the first western corps on Germany's home ground.
  let itaFell = 0, itaAt = '', firstSov = 0, westBefore = 0; let firstWest: any = null;
  const gerHome = (r: any): boolean => r.owner === 'GER' && !r.overseas;
  landingWatch.tell = (e: any) => { for (const c of e.force ?? []) landed.add(c.id); };
  const friend = (nat: string, ctrl: string): boolean => ctrl === nat || allied(g, nat, ctrl);
  const close = (a: A, c: any, t: number): void => {
    a.done = true; a.dead = !c || !!c.dead;
    if (!a.dead) return;
    const reg = c ? c.reg : last.get(a.id)?.reg ?? a.reg; const l = last.get(a.id);
    a.where = reg === a.reg ? 'IN PLACE' : friend(a.nat, a.ctrl[reg]) ? 'BEHIND' : 'OUT'; a.cut = (l ? !l.sup : false) || (c ? !c.supplied : false); a.diedT = t;
  };
  try {
    while (g.turn <= Number(untilS)) {
      const t = g.turn; landed = new Set<number>();
      const reg0 = new Map<number, number>(); for (const c of g.corps) if (!c.dead && WATCH.has(c.nat)) reg0.set(c.id, c.reg);
      const ctrl0: string[] = g.regions.map((r: any) => r.ctrl);
      T.runTurnAI(g);
      const byId = new Map<number, any>(g.corps.map((c: any) => [c.id, c]));
      for (const a of arr) { if (a.done) continue; const c = byId.get(a.id); if (!c || c.dead) close(a, c, t); else if (t >= a.t + 6) close(a, c, t); }
      for (const c of g.corps) {
        if (c.dead || !WATCH.has(c.nat)) continue;
        const r0 = reg0.get(c.id);
        if (r0 !== undefined && r0 !== c.reg && !onCont(r0) && onCont(c.reg) && !g.regions[r0].nb.includes(c.reg) && c.kind !== 'fort') {
          const kind = landed.has(c.id) ? 'landing' : friend(c.nat, ctrl0[c.reg]) ? 'ferry' : 'ferry+';
          arr.push({ id: c.id, nat: c.nat, t, reg: c.reg, kind, ctrl: ctrl0 });
        }
        last.set(c.id, { reg: c.reg, sup: !!c.supplied });
      }
      if (!itaFell && g.nations.ITA && !g.nations.ITA.alive) { itaFell = g.nations.ITA.fellOn?.turn ?? t;
        let al = 0, ax = 0, ot = 0, door = 0;
        for (const r of g.regions) { if (r.owner !== 'ITA' || r.overseas || beyondEurope(r) || island(r.id)) continue;
          if (r.ctrl === 'GER' || allied(g, 'GER', r.ctrl)) ax++; else if (r.ctrl === 'UK' || r.ctrl === 'USA' || allied(g, 'UK', r.ctrl)) { al++; if (r.nb.some((j: number) => gerHome(g.regions[j]))) door++; } else ot++; }
        itaAt = `of Italy's mainland provinces that month ${al} are the Allies' (${door} of them touching Germany's home ground), ${ax} Germany's side's, ${ot} others'`; }
      { let west = false; const from: Record<string, number> = {}; const who = new Set<string>(); const names = new Set<string>(); const ids: number[] = [];
        for (const c of g.corps) { if (c.dead || c.kind === 'fort') continue; const r = g.regions[c.reg]; if (!gerHome(r)) continue;
          if (c.nat === 'SOV' && !firstSov) firstSov = t;
          if (WATCH.has(c.nat)) { west = true; if (!firstWest) { const r0 = reg0.get(c.id); const f = r0 === undefined ? null : g.regions[r0]; const k = f ? (beyondEurope(f) || OFF.has(f.owner) || f.overseas ? 'the sea' : f.owner) : 'new'; from[k] = (from[k] ?? 0) + 1; who.add(c.nat); names.add(`${r.name}${f ? ` from ${f.name}` : ''}`); ids.push(c.id); } } }
        if (west && !firstWest) firstWest = { t, from, who: [...who].join(' '), names: [...names].join(', '), ids };
        if (west && t < 57) westBefore++; }
      if (!gerFell && g.nations.GER && !g.nations.GER.alive) gerFell = t;
      if (gerFell && t >= gerFell + 6) break;
    }
  } catch (e: any) { err = ` | ERROR in ${date(g.turn)}: ${String(e?.stack ?? e).split('\n').slice(0, 3).join(' / ')}`; }
  landingWatch.tell = null;
  const groups = new Map<string, A[]>();
  for (const a of arr) { const k = `${a.nat}|${a.t}|${a.reg}|${a.kind}`; if (!groups.has(k)) groups.set(k, []); groups.get(k)!.push(a); }
  const sum: Record<string, number[]> = {};
  for (const [k, as] of groups) {
    const [nat, tS, regS, kind] = k.split('|'); const r = g.regions[Number(regS)];
    const done = as.filter((a) => a.done), dead = done.filter((a) => a.dead);
    const n = (w: string, cut: boolean): number => dead.filter((a) => a.where === w && !!a.cut === cut).length;
    const key = `${nat} ${kind}`; const s = (sum[key] ??= [0, 0, 0, 0, 0, 0, 0, 0, 0]);
    s[0] += as.length; s[1] += done.length; s[2] += dead.length; s[3] += n('IN PLACE', true); s[4] += n('IN PLACE', false); s[5] += n('BEHIND', true); s[6] += n('BEHIND', false); s[7] += n('OUT', true); s[8] += n('OUT', false);
    console.log(`SHIP seed ${seed}: ${nat} ${date(Number(tS))} at ${r.name} (${r.owner}) | ${kind} | corps ${as.length} | six months on: dead ${dead.length} of ${done.length}` + (dead.length ? ` | in place: cut off ${n('IN PLACE', true)}, in supply ${n('IN PLACE', false)}; behind: cut off ${n('BEHIND', true)}, in supply ${n('BEHIND', false)}; marched out: cut off ${n('OUT', true)}, in supply ${n('OUT', false)} | died ${[...new Set(dead.map((a) => date(a.diedT!)))].join(', ')}` : '') + (done.length < as.length ? ` | ${as.length - done.length} not followed six months (the war ended)` : ''));
  }
  { const d = (t: number): string => (t ? date(t) : 'never');
    let came = '';
    if (firstWest) { const as = arr.filter((a) => firstWest.ids.includes(a.id)); const tally: Record<string, number> = {};
      for (const a of as) { const k = `${a.kind} at ${g.regions[a.reg].name} (${g.regions[a.reg].owner}) ${date(a.t)}`; tally[k] = (tally[k] ?? 0) + 1; }
      came = Object.entries(tally).map(([k, n]) => `${n} by ${k}`).join(', ') || 'not by ship (it was on the continent from the start or crossed a strait)'; }
    console.log(`ITALY seed ${seed}: Italy signed ${d(itaFell)}` + (itaFell ? `; ${itaAt}` : '') + ` | first western corps on Germany's home ground ${firstWest ? `${date(firstWest.t)}: ${firstWest.who}, ${firstWest.names}; the ground they stood on the month before: ${Object.entries(firstWest.from).map(([k, n]) => `${k} ${n}`).join(', ')}; they had come to the continent: ${came}` : 'never'} | months from Italy's signing to that: ${firstWest && itaFell ? firstWest.t - itaFell : 'n/a'} | first Soviet corps there ${d(firstSov)} | months before May 1944 with western corps there: ${westBefore} | Germany ${gerFell ? `fell ${date(gerFell)}` : 'stands'}`); }
  console.log(`SUM seed ${seed}: Germany ${gerFell ? `fell ${date(gerFell)}` : 'stands'} | ` + Object.entries(sum).map(([k, s]) => `${k}: corps ${s[0]}, followed ${s[1]}, dead ${s[2]} (in place ${s[3]}+${s[4]}, behind ${s[5]}+${s[6]}, marched out ${s[7]}+${s[8]}; cut off + in supply)`).join(' | ') + err);
}
