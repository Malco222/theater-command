// Version 65: why a front sleeps. usage: probe45 ME FROM SEED UNTIL PLAN-JSON [FOE]
// Plays the scripted player's war and, as each of his months opens from January 1942, notes what the enemy staff (the
// Soviet one unless named) is doing on his front: a general offensive on or off, a main effort and where, a rest; how
// many corps stand each side of the line; the best odds an attack by the corps beside any one province of the line
// would get as it stands, and with every corps a province further back brought up as far as there is room; and how
// much of the player's line stands in works. Then the longest run of months with no battle of the player's is printed
// month by month, with a count of the reasons nothing was named.
import { campaign, date, T, hostileUnits, allied } from './lib';
import { limitAttackers } from '@tc/campaign/game';
import * as STAFF from '@tc/campaign/staff';
const [me, fromS, seedS, untilS, planS, foe = 'SOV'] = process.argv.slice(2);
const X = T as any, S = STAFF as any;
const divs = (cs: any[]): number => cs.reduce((s, c) => s + (c.divs ?? 0) * (c.str ?? 1), 0);
const rows: { turn: number; text: string; best: number; massed: number; on: number; eff: boolean; rest: boolean; ratio: number; fr: number; fq: number; busyWith: string; op: boolean }[] = [];
const seen = new Set<number>();
const fought: Record<number, number> = {};
const townMine: Record<number, number> = {}, townTheirs: Record<number, number> = {}, gaveUp: Record<number, number> = {}, walked: Record<number, number> = {};
const run = campaign(me, Number(fromS), Number(seedS), { ...JSON.parse(planS),
  first: (g: any) => {
    if (g.turn < 29 || !g.nations[foe]?.alive || !g.nations[me].atWar.includes(foe)) return;
    const fit = (c: any): boolean => !c.dead && c.kind !== 'fort' && c.kind !== 'eng' && c.town === undefined && (c.nat === foe || allied(g, c.nat, foe));
    const line = g.regions.filter((r: any) => (r.ctrl === me || allied(g, r.ctrl, me)) && r.units.some((u: any) => u.nat === me && !u.dead) && r.nb.some((j: number) => g.regions[j].ctrl === foe));
    const mine = line.flatMap((r: any) => r.units.filter((u: any) => u.nat === me && !u.dead && u.kind !== 'fort' && u.town === undefined));
    const theirs = new Set<any>();
    let best = 0, massed = 0, at = '', atM = '';
    for (const r of line) {
      const doors = r.nb.filter((j: number) => g.regions[j].ctrl === foe);
      const front = doors.flatMap((j: number) => g.regions[j].units.filter(fit));
      for (const c of front) theirs.add(c);
      const go = limitAttackers(g, front.filter((c: any) => c.org >= 0.35), r);
      const o = go.length ? X.previewAttack(g, go, r)?.odds ?? 0 : 0;
      if (o > best) { best = o; at = r.name; }
      let room = 0; const back: any[] = []; const got = new Set<number>(doors);
      for (const j of doors) room += Math.max(0, 5 - g.regions[j].units.filter((u: any) => !u.dead).length);
      for (const j of doors) for (const k of g.regions[j].nb) { if (got.has(k)) continue; got.add(k); if (g.regions[k].ctrl === foe) back.push(...g.regions[k].units.filter(fit)); }
      back.sort((a, b) => b.divs * b.str - a.divs * a.str);
      const go2 = limitAttackers(g, [...front, ...back.slice(0, room)].filter((c: any) => c.org >= 0.35), r);
      const o2 = go2.length ? X.previewAttack(g, go2, r)?.odds ?? 0 : 0;
      if (o2 > massed) { massed = o2; atM = r.name; }
    }
    const on = g.flags[`st:on:${foe}:${me}`] ?? 0, eff = g.flags[`st:eff:${foe}`], rest = (g.flags[`st:rest:${foe}:${me}`] ?? 0) > g.turn || (g.flags[`st:effRest:${foe}`] ?? 0) > g.turn;
    const dug = mine.filter((c: any) => c.dug), th = [...theirs];
    const ratio = divs(mine) > 0 ? divs(th) / divs(mine) : 0;
    // The staff's own reckoning of the front the player stands on: the balance of strength, and how long it has been quiet
    // (it counts a fight with any enemy on that front, the player's allies too).
    const F: any[] = S.fronts ? S.fronts(g, foe) : [];
    const f = F.find((x) => x.enemies.includes(me));
    const fr = f ? f.ratio : 0;
    const qs: [string, number][] = f && S.quietWith ? f.enemies.filter((e: string) => g.nations[e]?.alive).map((e: string) => [e, S.quietWith(g, foe, e)]) : [];
    const fq = qs.length ? Math.min(...qs.map((x) => x[1])) : 99;
    const busyWith = qs.filter((x) => x[1] === fq).map((x) => x[0]).join('+');
    const op = !!g.flags[`st:op:${foe}`];
    rows.push({ turn: g.turn, best, massed, on, eff: !!eff, rest, ratio, fr, fq, busyWith, op,
      text: `${date(g.turn).padEnd(15)} ${foe}: ${on ? `general offensive since ${date(on)}` : 'no general offensive'}${eff ? `, main effort at ${g.regions[eff - 1].name}` : ''}${rest ? ', resting' : ''}${g.flags[`st:op:${foe}`] ? ', an operation in hand' : ''}; ${th.length} corps (${divs(th).toFixed(0)} divisions) beside the player's ${mine.length} (${divs(mine).toFixed(0)}), ${ratio.toFixed(2)} to 1; best attack as it stands ${best.toFixed(2)} to 1 (${at}), massed ${massed.toFixed(2)} (${atM}); the staff reckons the front at ${fr.toFixed(2)} to 1 and quiet for ${fq} months (last fought ${busyWith}); the player's line: ${line.length} provinces, ${dug.length} of ${mine.length} corps in works (mean ${dug.length ? (dug.reduce((s: number, c: any) => s + Math.min(3, c.dugFor ?? 1), 0) / dug.length).toFixed(1) : '0'} months)` });
  },
  each: (g: any) => {
    for (const r of g.reports) {
      if (seen.has(r.id)) continue; seen.add(r.id);
      const inIt = r.attNat === me || r.defNat === me || r.attackers.some((u: any) => u.nat === me) || r.defenders.some((u: any) => u.nat === me) || (r.gone ?? []).some((u: any) => u.nat === me);
      const byMe = r.attNat === me || r.attackers.some((u: any) => u.nat === me);
      if (r.town && inIt) { if (byMe) townMine[r.turn] = (townMine[r.turn] ?? 0) + 1; else townTheirs[r.turn] = (townTheirs[r.turn] ?? 0) + 1; }
      if (r.yielded && inIt) { if (byMe) walked[r.turn] = (walked[r.turn] ?? 0) + 1; else gaveUp[r.turn] = (gaveUp[r.turn] ?? 0) + 1; }
      if (r.yielded || r.town) continue;
      if (r.attNat === me || r.defNat === me || r.attackers.some((u: any) => u.nat === me) || r.defenders.some((u: any) => u.nat === me)) fought[r.turn] = (fought[r.turn] ?? 0) + 1;
    }
  },
} as any, { until: Number(untilS), quiet: true } as any);
// The longest run of months from January 1942 in which nothing was fought.
let bestA = 0, bestB = -1, a = -1;
const last = rows.length ? rows[rows.length - 1].turn : 0;
for (let t = 29; t <= last; t++) { if (!fought[t]) { if (a < 0) a = t; if (t - a > bestB - bestA || bestB < 0) { bestA = a; bestB = t; } } else a = -1; }
const chr = S.characterOf ? S.characterOf(run.g, foe) : '?';
const C = S.RIVALS?.characters?.[chr];
console.log(`${me} seed ${seedS}: the ${foe} staff is ${chr}${C ? ` (names a main effort after ${C.quiet} quiet months, at ${C.strength} of the player's strength on the front, where a massed attack reaches ${C.local} to 1)` : ''}. Longest silence ${bestB >= bestA ? bestB - bestA + 1 : 0} months, ${date(bestA)} to ${date(bestB)}:`);
const stretch = rows.filter((r) => r.turn >= bestA - 1 && r.turn <= bestB + 1);
for (const r of stretch) console.log('   ' + r.text + (fought[r.turn] ? ` | ${fought[r.turn]} battles this month` : '') + (townMine[r.turn] ? ` | the player stormed a town ${townMine[r.turn]} times` : '') + (townTheirs[r.turn] ? ` | a town of the player's was stormed ${townTheirs[r.turn]} times` : '') + (gaveUp[r.turn] ? ` | ground given up without a fight ${gaveUp[r.turn]} times` : '') + (walked[r.turn] ? ` | the player walked into empty ground ${walked[r.turn]} times` : ''));
const q = rows.filter((r) => !fought[r.turn] && !fought[r.turn - 1]);
const need = C?.local ?? 1.5, wait = C?.quiet ?? 4, strong = C?.strength ?? 0.85;
// Why nothing was named in a quiet month, in the order the staff asks: a general offensive on, an effort on, a rest or an
// operation in hand, the front not yet quiet long enough (by the staff's count), too weak, no sector at its bar; else unexplained.
const why: Record<string, number> = {};
const allies: Record<string, number> = {};
for (const r of q) {
  // What last set the staff's clock: the month it counts from is r.turn - r.fq.
  const t0 = r.turn - r.fq;
  const reset = fought[t0] ? 'it was waiting out its quiet months after a battle' : townMine[t0] ? "the player's own assault on a town set its clock back" : townTheirs[t0] ? "its assault on a town of the player's set its clock back" : walked[t0] ? "the player's walk into empty ground set its clock back" : gaveUp[t0] ? 'ground given up without a fight set its clock back' : 'something this program cannot see set its clock back';
  const k = r.on ? 'a general offensive was on in name' : r.eff ? 'a main effort was on elsewhere on the front' : r.rest || r.op ? 'resting, or an operation in hand' : r.fq < wait ? `not quiet long enough for it: ${reset}` : r.fr < strong ? 'too weak on the front by its own count' : r.massed < need ? 'no sector where a massed attack reached its bar' : 'nothing in the way that this program can see';
  why[k] = (why[k] ?? 0) + 1;
  if (k.startsWith('not quiet long enough')) allies[r.busyWith] = (allies[r.busyWith] ?? 0) + 1;
}
console.log(`QUIET ${me} seed ${seedS} ${chr}: months with no battle this month or last ${q.length} of ${rows.length}; of them: a general offensive was on in ${q.filter((r) => r.on).length}, a main effort in ${q.filter((r) => r.eff).length}, a rest in ${q.filter((r) => r.rest).length}; the best massed attack reached the staff's bar of ${need} in ${q.filter((r) => r.massed >= need).length}, 1.25 in ${q.filter((r) => r.massed >= 1.25).length}, 1.0 in ${q.filter((r) => r.massed >= 1).length}; median massed odds ${q.length ? q.map((r) => r.massed).sort((x, y) => x - y)[Math.floor(q.length / 2)].toFixed(2) : '-'}; median strength on the line ${q.length ? q.map((r) => r.ratio).sort((x, y) => x - y)[Math.floor(q.length / 2)].toFixed(2) : '-'} to 1`);
console.log(`WHY ${me} seed ${seedS} ${chr}: ${Object.entries(why).sort((a2, b2) => b2[1] - a2[1]).map(([k, v]) => `${v} ${k}`).join(' | ')}${Object.keys(allies).length ? ` || fighting meanwhile: ${Object.entries(allies).map(([k, v]) => `${k} ${v}`).join(', ')}` : ''}`);
