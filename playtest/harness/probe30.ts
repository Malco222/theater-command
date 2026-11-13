// Version 58: why a front stands still. Each month, before the scripted player chooses, every attack on offer:
// how many there are, the best odds, how many reach the player's own bar (minOdds), how many are on a line that is
// dug in and fed, the card's chance on the best of them, and what the player then did.
// usage: probe30 ME FROM SEED UNTIL PLAN-JSON [FIRST-MONTH-TO-PRINT] [DETAIL-MONTH] [MY-SCORCHED-EARTH-POLICY]
// DETAIL-MONTH prints every option of that month: province, ground, attackers and defenders, odds, chance, the even-chance mark,
// and for the best three the attack card's own lines (what the map fed into the fight).
import { campaign, date, T } from './lib';
const [me, fromS, seedS, untilS, planS, firstS = '1', detailS = '0', myPolicy = ''] = process.argv.slice(2);
const plan = JSON.parse(planS || '{"minOdds":1.5}');
const bar = plan.minOdds ?? 1.5;
let cur: any = null, attacks = 0, wonN = 0, polSet = false;
const divs = (cs: any[]): number => cs.reduce((s, c) => s + (c.divs ?? 0) * (c.str ?? 1), 0);
const run = campaign(me, Number(fromS), Number(seedS), { ...plan,
  onLook: (g: any, options: any[], pass: number) => {
    if (pass !== 0) return;
    const live = options.filter((o) => o.out);
    const prep = live.filter((o) => (o.out.prepared ?? 0) > 0 || (o.out.even ?? 0) > 1.25);
    const best = [...live].sort((a, b) => b.odds - a.odds)[0];
    cur = { n: live.length, over: live.filter((o) => o.odds >= bar).length, prep: prep.length, best: best ? best.odds : 0, bestChance: best ? best.out.chance : 0, bestEven: best ? best.out.even : 0, bestName: best ? best.r.name : '-',
      sure: live.filter((o) => o.out.chance >= 0.995).length, even: live.filter((o) => o.out.chance >= 0.5).length };
    if (g.turn === Number(detailS)) for (const [i, o] of [...live].sort((a, b) => b.odds - a.odds).entries()) {
      if (i < 3) {
        const defsAll = g.corps.filter((c: any) => c.reg === o.r.id && !c.dead && c.nat !== me && c.town === undefined);
        const say = (cs: any[]): string => { const d = cs.reduce((t, c) => t + c.divs, 0) || 1; const w = (f: (c: any) => number): string => (cs.reduce((t, c) => t + f(c) * c.divs, 0) / d).toFixed(2); return `${cs.length} corps, ${d} divisions on paper, strength ${w((c) => c.str)}, organisation ${w((c) => c.org)}, experience ${w((c) => c.xp)}, national quality ${w((c) => g.nations[c.nat].quality)}${cs.some((c) => g.nations[c.nat].slowCommand) ? ', slow command (attack at 85%)' : ''}; kinds ${[...new Set(cs.map((c) => c.kind))].map((k) => `${k} ${cs.filter((c) => c.kind === k).length}`).join(', ')}`; };
        console.log(`        THE TROOPS     attackers: ${say(o.cands)}`);
        console.log(`        THE TROOPS     defenders: ${say(defsAll)}`);
      }
      if (i < 3) for (const l of o.out.lines ?? []) console.log(`        ${String(l.label).padEnd(14)} ${String(l.value).slice(0, 70).padEnd(70)} ${String(l.effect ?? '').slice(0, 200)}`);
      const defs = g.corps.filter((c: any) => c.reg === o.r.id && !c.dead && c.nat !== me && c.town === undefined);
      console.log(`   ${o.r.name.padEnd(18)} ${String(o.r.terrain ?? '').padEnd(9)} attackers ${String(o.cands.length).padStart(2)} corps ${divs(o.cands).toFixed(0).padStart(3)} divisions | defenders ${String(defs.length).padStart(2)} corps ${divs(defs).toFixed(0).padStart(3)} divisions, dug in ${defs.filter((c: any) => c.dug).length} | odds ${o.odds.toFixed(2)} | chance ${Math.round(o.out.chance * 100)}% | even at ${Number(o.out.even ?? 0).toFixed(1)}`);
    }
  },
  onAttack: (_g: any, _out: any, rep: any) => { if (rep.yielded || rep.town) return; attacks++; if (rep.taken || rep.siege || rep.gates) wonN++; },
  each: (g: any) => {
    if (myPolicy && !polSet) polSet = !!T.orderScorchPolicy(g, myPolicy as any);
    if (g.turn >= Number(firstS) && g.nations[me].atWar.length) {
      const mine = g.corps.filter((c: any) => c.nat === me && !c.dead && c.kind !== 'fort' && c.town === undefined);
      console.log(`${date(g.turn).padEnd(15)} provinces ${String(g.regions.filter((x: any) => x.ctrl === me).length).padStart(3)} | field corps ${String(mine.length).padStart(3)} (${divs(mine).toFixed(0)} divisions), guns ${mine.filter((c: any) => c.kind === 'art').length} | attacks on offer ${String(cur?.n ?? 0).padStart(2)}, at ${bar} to 1 or better ${String(cur?.over ?? 0).padStart(2)}, on a prepared line ${String(cur?.prep ?? 0).padStart(2)}, an even chance or better ${String(cur?.even ?? 0).padStart(2)}, sure ${String(cur?.sure ?? 0).padStart(2)} | best ${cur ? `${cur.best.toFixed(2)} to 1 at ${cur.bestName} (${Math.round(cur.bestChance * 100)}%, even at ${Number(cur.bestEven).toFixed(1)})` : '-'} | attacked ${attacks}, won ${wonN}`);
    }
    cur = null; attacks = 0; wonN = 0;
  } }, { until: Number(untilS), quiet: true });
for (const e of run.errors) console.log('ERROR', String(e).slice(0, 600));
