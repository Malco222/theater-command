// Version 64, "hand over the line": what a relief costs in fieldworks. In a war played by the scripted player, as its
// month opens on the turns given, every pair of the player's field corps in neighbouring provinces of its own, one of
// them dug in beside the enemy, changes places on a copy of the game (the dug-in one steps out, the other steps in),
// and the months of fieldworks on the two provinces are added up before and after: the corps' own works and any works
// standing empty. On a version without the hand-over every month is lost; with it they should all be kept.
// usage: probe43 ME FROM SEED UNTIL PLAN-JSON TURNS(comma) [MOST-PAIRS-A-TURN]
import { campaign, date, saveGame, loadGame, hostileUnits, T } from './lib';
import * as G from '@tc/campaign/game';
const whys: Record<string, number> = {};
const [me, fromS, seedS, untilS, planS, turnsS, mostS = '60'] = process.argv.slice(2);
const turns = new Set(turnsS.split(',').map(Number));
const wk = (c: any): number => (c.dug ? Math.max(0, Math.min(3, c.dugFor ?? 1)) : 0);
const ground = (g: any, ids: number[]): number => {
  let n = 0;
  for (const id of ids) {
    for (const u of g.regions[id].units) if (!u.dead && u.kind !== 'fort' && u.town === undefined) n += wk(u);
    const m = g.flags[`fw:${id}`];
    if (m !== undefined) n += m * (g.flags[`fwk:${id}`] ?? 1);
  }
  return Math.round(n * 100) / 100;
};
const tot: any = { pairs: 0, done: 0, half: 0, noRoad: 0, before: 0, after: 0, worn: 0, wornAfter: 0, freshBefore: 0, freshAfter: 0, kept: 0, full: 0 };
campaign(me, Number(fromS), Number(seedS), { ...JSON.parse(planS),
  first: (g: any) => {
    if (!turns.has(g.turn)) return;
    const text = saveGame(g);
    const beside = (r: any): boolean => r.nb.some((j: number) => hostileUnits(g, g.regions[j], me).length > 0);
    const pairs: [number, number][] = [];
    for (const c of g.corps) {
      if (c.nat !== me || c.dead || c.town !== undefined || ['fort', 'eng', 'art'].includes(c.kind) || !c.dug || wk(c) < 2 || !beside(g.regions[c.reg]) || !T.canOrder(g, c) || c.ap <= 0) continue;
      for (const j of g.regions[c.reg].nb) {
        const r = g.regions[j];
        if (r.ctrl !== me || hostileUnits(g, r, me).length) continue;
        const f = r.units.find((u: any) => u.nat === me && !u.dead && u.town === undefined && u.kind === c.kind && T.canOrder(g, u) && u.ap > 0);
        if (f) { pairs.push([c.id, f.id]); break; }
      }
    }
    let t: any = { pairs: 0, done: 0, half: 0, noRoad: 0, before: 0, after: 0, worn: 0, wornAfter: 0, freshBefore: 0, freshAfter: 0, kept: 0, full: 0 };
    for (const [ci, fi] of pairs.slice(0, Number(mostS))) {
      const h: any = loadGame(text);
      if (!h) continue;
      const c = h.corps.find((x: any) => x.id === ci), f = h.corps.find((x: any) => x.id === fi);
      const from = c.reg, to = f.reg, b = ground(h, [from, to]), cw = wk(c), fw = wk(f);
      t.pairs++;
      // Version 65: the two change places in one order where the rules have it.
      if ((T as any).orderSwap) {
        if (!(T as any).orderSwap(h, c, f)) { t.noRoad++; const why = String((G as any).whyNoSwap?.(h, c, f) ?? 'refused'); whys[why.replace(/^.*? (has not the moves)/, 'A corps $1')] = (whys[why.replace(/^.*? (has not the moves)/, 'A corps $1')] ?? 0) + 1; if (h.regions[to].units.filter((u: any) => !u.dead).length >= 5) t.full++; continue; }
        const a2 = ground(h, [from, to]);
        t.done++; t.before += b; t.after += a2; t.worn += cw; t.wornAfter += wk(c); t.freshBefore += fw; t.freshAfter += wk(f); if (a2 >= b - 0.05) t.kept++;
        if (h.regions[to].units.filter((u: any) => !u.dead).length >= 5) t.fullDone = (t.fullDone ?? 0) + 1;
        continue;
      }
      const mOut = T.reach(h, c);
      if (!mOut.has(to) || mOut.get(to)!.attack || !T.orderMove(h, c, to)) { t.noRoad++; if (h.regions[to].units.filter((u: any) => !u.dead).length >= 5) t.full++; continue; }
      const mIn = T.reach(h, f);
      if (!mIn.has(from) || mIn.get(from)!.attack || !T.orderMove(h, f, from)) { t.half++; continue; }
      const a = ground(h, [from, to]);
      t.done++; t.before += b; t.after += a; t.worn += cw; t.wornAfter += wk(c); t.freshBefore += fw; t.freshAfter += wk(f); if (a >= b - 0.05) t.kept++;
    }
    console.log(`${date(g.turn)}: pairs ${t.pairs}, reliefs carried out ${t.done} (stopped half way ${t.half}, no road ${t.noRoad}, of those a full province ${t.full}); months of works on the two provinces ${t.before.toFixed(1)} before, ${t.after.toFixed(1)} after; the corps stepping in: ${t.freshBefore.toFixed(1)} to ${t.freshAfter.toFixed(1)}; the corps stepping out: ${t.worn.toFixed(1)} to ${t.wornAfter.toFixed(1)}; reliefs that lost nothing ${t.kept}`);
    for (const k of Object.keys(t)) tot[k] = (tot[k] ?? 0) + t[k];
  },
} as any, { until: Number(untilS), quiet: true } as any);
console.log(`TOTAL ${me} seed ${seedS}: pairs ${tot.pairs}, reliefs ${tot.done}, half ${tot.half}, no road ${tot.noRoad} (full ${tot.full}); works before ${tot.before.toFixed(1)}, after ${tot.after.toFixed(1)}; stepping in ${tot.freshBefore.toFixed(1)} to ${tot.freshAfter.toFixed(1)}; stepping out ${tot.worn.toFixed(1)} to ${tot.wornAfter.toFixed(1)}; lost nothing ${tot.kept}`);
console.log(`SWAPS ${me} seed ${seedS}: changed places with both provinces full or the one behind full ${tot.fullDone ?? 0}; refused: ${Object.entries(whys).map(([k, v]) => `${v} ${k}`).join(' | ') || 'none'}`);
