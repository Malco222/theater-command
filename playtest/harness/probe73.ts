// Version 75: why the computer's America lands in Norway before France. In a war with nobody playing, each month the
// Americans have corps on a British quay, a copy of the war is asked (so the war itself is not nudged) what a landing
// by three of them would meet on every enemy harbour in reach: barred, too dangerous for the staff (open.json
// descent.risk), warned by its own forecast (the sea behind could be closed; heeded on the mainland since ruling 30),
// guarded at odds under descent.odds, an army within a march over descent.near times the force, or clear. One WATCH line a month by shore (the west: France, the Low Countries, Germany, Denmark; Norway; others),
// and a LANDED line for every landing the American staff makes (ai.ts landingWatch).
// node out75b/probe73.mjs SEED [UNTIL=70] [NAT=USA]     (build with build70.sh)
import { start, date, T, saveGame, loadGame, atWar, hostileUnits } from './lib';
import { landingWatch } from '@tc/campaign/ai';
import { heedsCard, seaFoe, seaBase, lodgment } from '@tc/campaign/staff';
import { landingTargets, planLanding } from '@tc/campaign/navy';
import openData from '../../../../../../home/user/tc75/data/campaign/open.json';
const [seedS = '18', untilS = '70', nat = 'USA'] = process.argv.slice(2);
const D = (openData as any).descent, RISK = D.risk;
const WEST = new Set(['FRA', 'BEL', 'NED', 'GER', 'DEN']);
const g: any = start(null, 1, Number(seedS), 'colonel', 3);
landingWatch.tell = (e: any) => { if (e.nat === nat) console.log(`LANDED ${nat} seed ${seedS} ${date(g.turn)}: ${e.to.name} (${e.to.owner}) with ${e.force.length} corps${e.wave ? ', a later wave' : ''}; warned ${e.warned ? 'YES' : 'no'}; heeded there ${heedsCard(e.to, e.wave) ? 'yes' : 'no (an island)'}; taken ${e.taken ? 'yes' : 'no'}`); };
while (g.turn <= Number(untilS)) {
  const t = g.turn;
  if (g.nations[nat]?.atWar.length && g.nations.GER?.alive) {
    const c: any = loadGame(saveGame(g));
    if (c) {
      const quays: Record<number, any[]> = {};
      for (const u of c.corps) { const r = c.regions[u.reg]; if (u.nat === nat && !u.dead && u.kind !== 'fort' && u.kind !== 'eng' && r.owner === 'UK' && !r.overseas && r.port && u.supplied && u.str >= 0.7) (quays[r.id] ??= []).push(u); }
      const best = Object.values(quays).sort((a, b) => b.length - a.length)[0];
      if (best) {
        const force = best.slice(0, 3);
        const men = force.reduce((q: number, u: any) => q + u.divs * u.str, 0);
        const tally: Record<string, { n: number; barred: number; risky: number; warned: number; guarded: number; army: number; clear: string[] }> = {};
        for (const r of landingTargets(c, force)) {
          if (!r.port || !atWar(c, nat, r.ctrl)) continue;
          const k = r.overseas ? 'colonies and islands overseas' : WEST.has(r.owner) ? 'the west' : r.owner === 'NOR' ? 'Norway' : 'elsewhere';
          const T2 = (tally[k] ??= { n: 0, barred: 0, risky: 0, warned: 0, guarded: 0, army: 0, clear: [] });
          T2.n++;
          const p: any = planLanding(c, force, r);
          if (p.why) T2.barred++;
          else if (p.risk > RISK) T2.risky++;
          else if (heedsCard(r) && p.hold && !p.hold.open) T2.warned++;
          else if (p.defended && (!p.assess || p.assess.odds < D.odds)) T2.guarded++;
          else {
            // The army within a march of the beach (ai.ts descent: no more than 'near' times the force's own strength).
            let near = 0;
            for (const j of [r.id, ...r.nb]) for (const u of hostileUnits(c, c.regions[j], nat)) if (u.kind !== 'fort') near += u.divs * u.str;
            if (near > D.near * men) T2.army++; else T2.clear.push(r.name);
          }
        }
        // What the staff itself holds (asked of the copy, since seaFoe and seaBase keep flags): for each of its two
        // forces (0 the army at home, 1 an army standing idle on a far shore) the enemy it means to cross against,
        // its quay and the corps ready there, the men it already has ashore, and the month before which it waits.
        const staff = [0, 1].map((slot) => {
          const sf: any = seaFoe(c, nat, slot); if (!sf) return `force ${slot}: no crossing meant`;
          const q: any = seaBase(c, nat, slot); const sfx = slot ? ':o' : '';
          const ready = q ? q.units.filter((u: any) => u.nat === nat && !u.dead && u.kind !== 'fort' && u.kind !== 'eng' && !u.airborne && u.supplied && u.str >= 0.7 && u.ap >= u.apMax).length : 0;
          const l: any = lodgment(c, nat, slot); const next = c.flags[`st:seaNext:${nat}${sfx}`] ?? 0; const aim = c.regions[(c.flags[`st:aim:${nat}${sfx}`] ?? 0) - 1];
          const lf = c.regions[(c.flags[`st:lodged:${nat}${sfx}`] ?? 0) - 1];
          return `force ${slot}: against ${sf.foe}${sf.second ? ' (a second front)' : ''}, quay ${q ? `${q.name} (${q.units.filter((u: any) => !u.dead).length} corps of all nations in it: ${Object.entries(q.units.filter((u: any) => !u.dead).reduce((m: any, u: any) => { m[u.nat] = (m[u.nat] ?? 0) + 1; return m; }, {})).map(([k, v]) => `${k} ${v}`).join(', ') || 'empty'})` : 'NONE'} with ${ready} ready (needs ${sf.L.force}), ashore ${l ? `${l.at.name}${l.short ? ' (too few)' : ''} ports ${l.ports.map((x: any) => x.name).join('/') || 'none'}` : lf ? `${lf.name} (no longer a front)` : 'nobody'}, eye on ${aim ? aim.name : 'no beach'}${next > t ? `, waits until ${date(next)}` : ''}`;
        }).join('; ');
        console.log(`STAFF ${nat} seed ${seedS} ${date(t)}: ${staff}`);
        console.log(`WATCH ${nat} seed ${seedS} ${date(t)} from ${c.regions[force[0].reg].name} with ${force.length} of ${Object.values(quays).reduce((s, a) => s + a.length, 0)} corps on British quays (the staff's limit on danger ${RISK}): ${Object.entries(tally).map(([k, v]) => `${k}: ${v.n} harbours, too dangerous ${v.risky}, warned ${v.warned}, guarded at poor odds ${v.guarded}, an army too strong within a march ${v.army}, clear ${v.clear.length}${v.clear.length ? ` (${v.clear.slice(0, 4).join(', ')})` : ''}`).join(' | ') || 'no enemy harbour in reach'}`);
      }
    }
  }
  T.runTurnAI(g);
  if (g.nations.GER && !g.nations.GER.alive) { console.log(`Germany fell ${date(t)}`); break; }
}
