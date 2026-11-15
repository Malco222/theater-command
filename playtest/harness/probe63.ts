// Version 72: why does the crossing routine gather nobody on the quay? usage: probe63 ME SEED UNTIL PLAN-JSON FROM-TURN TO-TURN
// Plays the domination war as probe56 does (World Domination ordered the month it is offered) and, in the months asked for, prints
// the quay the routine chose, who stands there, and for the strongest spare corps why each cannot go there by rail or by road.
import { campaign, date, T } from './lib';
import * as G from '@tc/campaign/game';
import * as R from '@tc/campaign/rails';
import * as N from '@tc/campaign/navy';
const [me, seedS, untilS, planS, aS, bS] = process.argv.slice(2);
const plan = JSON.parse(planS || '{}');
const X = T as any, GG = G as any;
const a = Number(aS), b = Number(bS);
campaign(me, 1, Number(seedS), { ...plan,
  first: (g: any, run: any) => { const la = X.lastAct?.(g); if (la?.open && !la.on) X.orderDomination(g); look(g, run, 'before the orders'); },
  each: (g: any, run: any) => { if (process.env.AFTER) look(g, run, 'after the orders'); },
} as any, { until: Number(untilS), quiet: true } as any);
function look(g: any, run: any, when: string): void {
    if (g.turn < a || g.turn > b) return;
    const q = g.regions[run?.crossQuay ?? -1];
    const n = g.nations[me];
    const field = (u: any): boolean => u.nat === me && !u.dead && u.kind !== 'fort' && u.kind !== 'eng' && !u.airborne && !u.town;
    const all = g.corps.filter(field);
    console.log(`== ${date(g.turn)} ${when}: quay ${q?.name ?? 'none'} (held by ${q?.ctrl}, owner ${q?.owner}); there: ${q ? q.units.filter((u: any) => !u.dead).map((u: any) => `${u.nat} ${u.kind}${u.town ? ' town' : ''} ap${u.ap}/${u.apMax}`).join(', ') || 'nobody' : ''}; field corps ${all.length}; with 4 or more moves left ${all.filter((c: any) => c.ap >= 4).length}; can be ordered ${all.filter((c: any) => T.canOrder(g, c)).length}; lift ${n.sealift}; crossFoe ${run?.crossFoe}`);
    if (!q) return;
    // The men ashore on the enemy's homeland: how strong, fed or not, who stands against them, and whether the sea behind them is shut.
    const foe = run?.crossFoe;
    if (foe) {
      const there = g.regions.filter((r: any) => r.owner === foe && !r.overseas && (r.ctrl === me || r.units.some((u: any) => u.nat === me && !u.dead)));
      for (const r of there) console.log(`   ASHORE at ${r.name}: held by ${r.ctrl}, port ${!!r.port} | ours ${r.units.filter((u: any) => u.nat === me && !u.dead).map((u: any) => `${u.kind} ${u.divs}x${u.str.toFixed(2)}${u.supplied === false ? ' UNSUPPLIED' : ''}`).join(', ') || 'nobody'} | theirs here ${r.units.filter((u: any) => u.nat !== me && !u.dead).map((u: any) => `${u.nat} ${u.kind} ${u.divs}x${u.str.toFixed(2)}${u.town ? ' in the town' : ''}`).join(', ') || 'nobody'}; garrison ${(r.garr ?? []).length} | next door ${r.nb.map((j: number) => g.regions[j]).filter((x: any) => x.units.some((u: any) => u.nat === foe && !u.dead)).map((x: any) => `${x.name} ${x.units.filter((u: any) => u.nat === foe && !u.dead).length} corps`).join(', ') || 'no enemy'} | seas: ${r.seas.map((z: number) => `${g.seas[z].name}${N.closedTo(g, z, me) ? ' CLOSED to us' : ' open'}`).join(', ')}`);
      if (!there.length && (run?.crossLog ?? []).some((l: string) => l.includes('LANDED'))) console.log('   nobody ashore');
      const lastB = (g.reports ?? []).filter((b: any) => b.turn >= g.turn - 1 && (b.attNat === me || b.defNat === me) && g.regions[b.region]?.owner === foe).slice(-4);
      for (const b of lastB) console.log(`   BATTLE ${date(b.turn)} at ${b.regionName}: ${b.attNat} attacks ${b.defNat}, odds ${Number(b.odds).toFixed(2)}, ${b.taken ? 'taken' : 'held'}`);
    }
    const top = all.filter((c: any) => c.reg !== q.id && !g.regions[c.reg].overseas).sort((x: any, y: any) => y.divs * y.str - x.divs * x.str).slice(0, 8);
    for (const c of top) {
      const here = g.regions[c.reg];
      let reach = -1, has = false;
      try { const rr = GG.railReach(g, c); reach = rr.size; has = rr.has(q.id); } catch (e) { reach = -2; }
      console.log(`   ${c.kind} ${Math.round(c.divs * c.str)} divisions at ${here.name} (held by ${here.ctrl}): ap ${c.ap}/${c.apMax}, canOrder ${T.canOrder(g, c)}, ptWatch ${c.ptWatch === g.turn}, rail reach ${reach} provinces, the quay among them ${has}, room on the quay ${GG.hasRoom ? GG.hasRoom(g, q, c) : '?'}, barred ${GG.barred ? GG.barred(g, c, q) : '?'}, by road ${T.reach(g, c).has(q.id)}`);
    }
    const RR = (GG.railCut ? GG : null);
    const d = run?.g ? null : null;
    const withMoves = all.filter((c: any) => c.reg !== q.id && c.ap >= 4 && !g.regions[c.reg].overseas);
    let canRail = 0, canRoad = 0; for (const c of withMoves) { try { if (GG.railReach(g, c).has(q.id)) canRail++; } catch {} if (T.reach(g, c).has(q.id)) canRoad++; }
    console.log(`   corps with a month's moves left, not overseas: ${withMoves.length}; of them the quay is in rail reach of ${canRail}, in road reach of ${canRoad}`);
    const cut = g.regions.filter((r: any) => r.ctrl === me && R.railCut(g, r.id));
    const engs = g.corps.filter((c: any) => c.nat === me && !c.dead && c.kind === 'eng');
    console.log(`   provinces the player holds with the line cut: ${cut.length} of ${g.regions.filter((r: any) => r.ctrl === me).length}; the quay's own line cut: ${R.railCut(g, q.id)}; cut within two provinces of the quay: ${cut.filter((r: any) => Math.hypot(r.x - q.x, r.y - q.y) < 140).map((r: any) => r.name).join(', ') || 'none'}; engineer corps ${engs.length} (${engs.map((c: any) => `${g.regions[c.reg].name}${c.job ? ' at work: ' + (c.job.kind ?? c.job.key ?? 'job') : ''}`).slice(0, 8).join('; ')})`);
    const near = all.filter((c: any) => c.reg !== q.id).map((c: any) => ({ c, d: Math.hypot(g.regions[c.reg].x - q.x, g.regions[c.reg].y - q.y) })).sort((x: any, y: any) => x.d - y.d).slice(0, 5);
    console.log(`   nearest corps to the quay: ${near.map((x: any) => `${g.regions[x.c.reg].name} ap${x.c.ap}/${x.c.apMax} ${T.canOrder(g, x.c) ? '' : 'NOT orderable'}`).join('; ')}`);
}
