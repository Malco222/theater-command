// Version 61: the jab. Works wear under attack, and troops attacked in a month dig at half pace. Does a one-corps
// attack that is sure to fail (a jab) pay for itself by wearing a line the player cannot yet break?
// usage: probe38 ME FROM SEED AT-TURN PLAN-JSON none|jab|jab2 [MONTHS]
// Plays the plan to AT-TURN. From then on, at the start of each of the player's months, it prints every dug-in enemy
// line beside the player's troops (works in months, the card's odds, its even-chance mark and its chance for the full
// attack). In mode jab the weakest corps that can reach each line the full attack cannot break (odds under the plan's
// bar) attacks it alone first; in jab2 the two weakest do, one after the other. The scripted player then plays as usual.
import { campaign, date, T } from './lib';
import type { Game } from './lib';
import { limitAttackers, hostileUnits, corpsBase, worksOf } from '@tc/campaign/game';
const [me, fromS, seedS, atS, planS, mode = 'none', monthsS = '6'] = process.argv.slice(2);
const at = Number(atS), months = Number(monthsS), plan = JSON.parse(planS || '{"minOdds":1.5}');
const bar = plan.minOdds ?? 1.5;
const X = T as any;
let softBlows = 0, softWon = 0;
let jabs = 0, wornSum = 0, myLost = 0, theirLost = 0, jabWon = 0, startProv = 0, startLines = 0;
const marked = new Set<number>();
const divs = (cs: any[]): number => cs.reduce((s, c) => s + (c.divs ?? 0) * (c.str ?? 1), 0);
const lines = (g: Game): any[] => {
  const ids = new Set<number>();
  for (const c of g.corps) if (c.nat === me && !c.dead && c.reg >= 0 && (c as any).town === undefined) for (const j of g.regions[c.reg].nb) ids.add(j);
  const out: any[] = [];
  for (const j of ids) {
    const r = g.regions[j];
    const foes = hostileUnits(g, r, me).filter((u: any) => u.town === undefined && u.kind !== 'fort');
    const dug = foes.filter((u: any) => u.dug);
    if (!dug.length) continue;
    const all = X.candidates(g, r) as any[];
    const cands = limitAttackers(g, all, r);
    const o = cands.length ? X.attackOutlook(g, cands, r) : null;
    out.push({ r, foes, dug, all, cands, o, works: dug.reduce((s: number, u: any) => s + worksOf(u), 0) / dug.length });
  }
  return out;
};
const run = campaign(me, Number(fromS), Number(seedS), { ...plan, first: (g: Game) => {
  if (g.turn < at || g.turn >= at + months) return;
  const ls = lines(g);
  if (g.turn === at) { startProv = g.regions.filter((r) => r.ctrl === me).length; for (const l of ls) marked.add(l.r.id); startLines = ls.length; }
  const under = ls.filter((l) => l.o && l.o.odds < bar);
  const mean = (xs: number[]): string => (xs.length ? (xs.reduce((a, b) => a + b, 0) / xs.length).toFixed(2) : '-');
  console.log(`${date(g.turn).padEnd(15)} provinces ${g.regions.filter((r) => r.ctrl === me).length} | dug-in lines beside me ${ls.length}, under my bar ${under.length} | their works ${mean(ls.map((l) => l.works))} months (under my bar ${mean(under.map((l) => l.works))}) | card odds on those ${mean(under.map((l) => l.o.odds))}, even at ${mean(under.map((l) => l.o.even ?? 0))}, chance ${mean(under.map((l) => l.o.chance ?? 0))} | lines of ${date(at)} still theirs ${[...marked].filter((id) => g.regions[id].ctrl !== me).length} of ${marked.size}`);
  if (mode === 'none') return;
  if (mode === 'soften') {
    // Jabs first, then the real blow: does a line read easier after one, two and three hopeless attacks by spare corps?
    for (const l of [...under].filter((x) => x.o.odds >= 0.8).sort((a, b) => b.o.odds - a.o.odds).slice(0, 4)) {
      const r = g.regions[l.r.id];
      const main = new Set(l.cands.map((c: any) => c.id));
      const say = (o: any): string => (o ? `${o.odds.toFixed(2)} to 1 (${Math.round((o.chance ?? 0) * 100)}%)` : 'none');
      const chain: string[] = [`full attack ${say(l.o)}`];
      const orgOf = (): string => { const f = hostileUnits(g, r, me).filter((u: any) => u.town === undefined && u.kind !== 'fort'); return (f.reduce((t: number, u: any) => t + (u.org ?? 0), 0) / Math.max(1, f.length)).toFixed(2); };
      const org0 = orgOf(), men0 = divs(l.foes);
      let last = l.o, lostMine = 0, lostTheirs = 0, n = 0;
      for (let k = 0; k < 3; k++) {
        const spare = (X.candidates(g, r) as any[]).filter((c) => !main.has(c.id) && c.kind !== 'art' && c.kind !== 'eng').sort((a, b) => corpsBase(g, a) - corpsBase(g, b))[0];
        if (!spare) break;
        const rep = X.orderAttack(g, [spare], r);
        if (!rep) break;
        n++; jabs++; myLost += rep.attLost ?? 0; theirLost += rep.defLost ?? 0; lostMine += rep.attLost ?? 0; lostTheirs += rep.defLost ?? 0; wornSum += rep.worn ?? 0;
        if (rep.taken || rep.siege || rep.gates) { jabWon++; chain.push('the jab itself WON'); break; }
        const again = (X.candidates(g, r) as any[]).filter((c) => main.has(c.id));
        last = again.length ? X.attackOutlook(g, limitAttackers(g, again, r), r) : null;
        chain.push(`after jab ${k + 1}: ${say(last)}`);
        if (!last) break;
      }
      let end = 'no blow ordered';
      if (n && last && ((last.chance ?? 0) >= 0.6 || last.odds >= bar)) {
        const again = (X.candidates(g, r) as any[]).filter((c) => main.has(c.id));
        const rep = X.orderAttack(g, limitAttackers(g, again, r), r);
        end = rep ? (rep.taken || rep.siege || rep.gates ? 'THE BLOW WON' : 'the blow was thrown back') : 'the blow could not be ordered';
        if (rep && (rep.taken || rep.siege || rep.gates)) softWon++;
        softBlows++;
      }
      if (n) console.log(`   SOFTEN ${r.name} (${l.foes.length} corps, ${men0.toFixed(0)} divisions, works ${l.works.toFixed(1)}): ${chain.join(' | ')} | defenders' organisation ${org0} to ${orgOf()} | jabs cost ${Math.round(lostMine)} against ${Math.round(lostTheirs)} | ${end}`);
    }
    return;
  }
  if (mode.startsWith('hammer')) {
    // The hammer: the full attack again and again in one month with whoever can still reach, each blow meeting the works the
    // last one left. hammer = the best line under the bar, hammer3 = the best three. Only lines the card rates 0.45 to 1 or better.
    const top = mode === 'hammer3' ? 3 : 1;
    for (const l of [...under].filter((x) => x.o.odds >= 0.45).sort((a, b) => b.o.odds - a.o.odds).slice(0, top)) {
      const r = g.regions[l.r.id];
      const chain: string[] = [];
      let mine = 0, theirs = 0, end = 'not taken';
      for (let k = 0; k < 6; k++) {
        const cands = limitAttackers(g, X.candidates(g, r) as any[], r);
        if (!cands.length) break;
        const o = X.attackOutlook(g, cands, r); if (!o) break;
        const sum = X.previewAttack(g, cands, r);
        if (k > 0 && o.odds < 0.45) { chain.push(`blow ${k + 1} not ordered at ${o.odds.toFixed(2)}`); break; }
        const rep = X.orderAttack(g, cands, r);
        if (!rep) break;
        jabs++; myLost += rep.attLost ?? 0; theirLost += rep.defLost ?? 0; mine += rep.attLost ?? 0; theirs += rep.defLost ?? 0; wornSum += rep.worn ?? 0;
        const won = rep.taken || rep.siege || rep.gates;
        chain.push(`blow ${k + 1}: ${cands.length} corps at ${o.odds.toFixed(2)} to 1 (${Math.round((o.chance ?? 0) * 100)}%), works ${Number(sum?.works ?? 0).toFixed(1)}, ${won ? 'WON' : `thrown back, wore ${Number(rep.worn ?? 0).toFixed(1)}`}`);
        if (won) { jabWon++; end = 'TAKEN'; break; }
      }
      console.log(`   HAMMER ${r.name} (${l.foes.length} corps, ${divs(l.foes).toFixed(0)} divisions): ${chain.join(' | ')} | men lost ${Math.round(mine)} against ${Math.round(theirs)} | ${end}`);
    }
    return;
  }
  for (const l of under) {
    const n = mode === 'jab2' ? 2 : 1;
    for (let k = 0; k < n; k++) {
      const r = g.regions[l.r.id];
      const pool = (X.candidates(g, r) as any[]).filter((c) => c.kind !== 'art' && c.kind !== 'eng').sort((a, b) => corpsBase(g, a) - corpsBase(g, b));
      // keep the jab cheap: the weakest corps, and never the last corps in its province
      const c = pool.find((x) => g.regions[x.reg].units.filter((u: any) => u.nat === me && !u.dead && u.kind !== 'fort').length > 1) ?? null;
      if (!c) break;
      const o = X.attackOutlook(g, [c], r);
      if (!o) break;
      const sum = X.previewAttack(g, [c], r); if (sum) o.wear = sum.wear;
      const before = l.works;
      const rep = X.orderAttack(g, [c], r);
      if (!rep) break;
      jabs++; myLost += rep.attLost ?? 0; theirLost += rep.defLost ?? 0; wornSum += rep.worn ?? 0;
      if (rep.taken || rep.siege || rep.gates) jabWon++;
      if (jabs <= 12) console.log(`   JAB ${r.name}: ${c.name} (${c.kind}, ${(c.divs * c.str).toFixed(1)} divisions) on ${l.foes.length} corps (${divs(l.foes).toFixed(0)} divisions), card odds ${o.odds.toFixed(2)}, the card says it takes ${Number(o.wear ?? 0).toFixed(2)} months off works of ${before.toFixed(1)}; battle: ${rep.taken ? 'WON' : 'thrown back'}, worn ${rep.worn ?? '-'}, works left ${rep.works ?? '-'}, lost ${rep.attLost} against ${rep.defLost}`);
    }
  }
} } as any, { until: at + months, quiet: true });
for (const e of run.errors) console.log('ERROR', String(e).slice(0, 400));
const g = run.g;
console.log(`TOTAL ${me} seed ${seedS} ${mode} from ${date(at)} for ${months} months: provinces ${startProv} to ${g.regions.filter((r) => r.ctrl === me).length}; lines of ${date(at)} taken ${[...marked].filter((id) => g.regions[id].ctrl === me).length} of ${startLines}; jabs ${jabs} (won ${jabWon}), months worn ${wornSum.toFixed(1)}, men lost in jabs ${Math.round(myLost)} against ${Math.round(theirLost)}; blows after softening ${softBlows} (won ${softWon}); enemy field divisions at the end ${divs(g.corps.filter((c: any) => !c.dead && c.kind !== 'fort' && g.nations[me].atWar.includes(c.nat))).toFixed(0)}, their mean organisation ${(() => { const cs = g.corps.filter((c: any) => !c.dead && c.kind !== 'fort' && g.nations[me].atWar.includes(c.nat)); return (cs.reduce((t: number, c: any) => t + (c.org ?? 0), 0) / Math.max(1, cs.length)).toFixed(2); })()}; my field divisions at the end ${divs(g.corps.filter((c: any) => c.nat === me && !c.dead && c.kind !== 'fort')).toFixed(0)}${run.fellOn ? `; FELL ${date(run.fellOn)}` : ''}`);
