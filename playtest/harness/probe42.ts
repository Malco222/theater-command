// Version 64: writes saves for the page at three moments worth looking at, each as the player's month opens:
//   -soften: an attack on a dug-in line is on offer at odds between 0.9 and 1.6 to 1, the works are two months old or
//            more, a corps can probe it and the bombers can reach it;
//   -fold:   two of the player's worn corps stand together and can be folded;
//   -left:   fieldworks stand empty on a province the player holds.
// Prints what to click, for ui26.cjs.
// usage: probe42 ME FROM SEED UNTIL PLAN-JSON OUT-PREFIX [FIRST]
import { writeFileSync } from 'node:fs';
import { campaign, date, saveGame, T } from './lib';
const [me, fromS, seedS, untilS, planS, out, firstS = '24'] = process.argv.slice(2);
const done: Record<string, boolean> = {};
const run = campaign(me, Number(fromS), Number(seedS), { ...JSON.parse(planS),
  first: (g: any) => {
    if (g.turn < Number(firstS)) return;
    if (!done.fold) {
      for (const r of g.regions) {
        const ch = ((T as any).foldChoicesAt?.(g, r.id) ?? []).filter((v: any) => v.why === null);
        if (!ch.length) continue;
        done.fold = true;
        writeFileSync(`${out}-fold.json`, saveGame(g));
        writeFileSync(`${out}-fold.txt`, JSON.stringify({ turn: g.turn, date: date(g.turn), reg: r.id, name: r.name, choices: ch.map((v: any) => ({ keep: v.keepName, gone: v.goneName, str: v.str, men: v.men, back: v.back })) }));
        console.log(`${date(g.turn)}: saved fold. At ${r.name} (${r.id}): ${ch.map((v: any) => `${v.goneName} into ${v.keepName} (${Math.round(v.str * 100)}% after, ${v.back} back)`).join('; ')}`);
        break;
      }
    }
    if (!done.left) {
      const r = g.regions.find((x: any) => (T as any).fieldworksLeft?.(g, x.id));
      if (r) {
        const w = (T as any).fieldworksLeft(g, r.id);
        done.left = true;
        writeFileSync(`${out}-left.json`, saveGame(g));
        writeFileSync(`${out}-left.txt`, JSON.stringify({ turn: g.turn, date: date(g.turn), reg: r.id, name: r.name, works: w, all: g.regions.filter((x: any) => (T as any).fieldworksLeft(g, x.id)).map((x: any) => x.name) }));
        console.log(`${date(g.turn)}: saved left. Works stand empty at ${r.name} (${r.id}): ${JSON.stringify(w)}; in all ${g.regions.filter((x: any) => (T as any).fieldworksLeft(g, x.id)).length} provinces`);
      }
    }
  },
  onLook: (g: any, options: any[], pass: number) => {
    if (done.soften || pass !== 0 || g.turn < Number(firstS)) return;
    const fit = options.filter((o) => o.out && o.odds >= 0.9 && o.odds <= 1.6).map((o) => ({ o, s: (T as any).softenOptions?.(g, o.cands, o.r) })).filter((x) => x.s && x.s.works >= 2 && x.s.probe.why === null && x.s.bomb && x.s.bomb.why === null && x.s.bomb.wear > 0).sort((a, b) => b.o.odds - a.o.odds)[0];
    if (!fit) return;
    done.soften = true;
    const by: Record<number, number> = {};
    for (const c of fit.o.cands) by[c.reg] = (by[c.reg] ?? 0) + 1;
    const from = Number(Object.keys(by).sort((a, b) => by[Number(b)] - by[Number(a)])[0]);
    writeFileSync(`${out}-soften.json`, saveGame(g));
    writeFileSync(`${out}-soften.txt`, JSON.stringify({ turn: g.turn, date: date(g.turn), target: fit.o.r.id, targetName: fit.o.r.name, from, fromName: g.regions[from].name, odds: fit.o.odds, chance: fit.o.out.chance, even: fit.o.out.even, attackers: fit.o.cands.map((c: any) => c.id), soften: fit.s }));
    console.log(`${date(g.turn)}: saved soften. Attack ${fit.o.r.name} (${fit.o.r.id}) from ${g.regions[from].name} (${from}) with ${fit.o.cands.length} corps: odds ${fit.o.odds.toFixed(2)}, chance ${Math.round(fit.o.out.chance * 100)}%; ${JSON.stringify(fit.s)}`);
  } }, { until: Number(untilS), quiet: true });
for (const e of (run as any).errors ?? []) console.log('ERROR', e);
console.log(`saved: ${Object.keys(done).join(', ') || 'nothing'}`);
