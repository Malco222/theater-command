// Version 76: does the computer's Britain ever take back ground a small Axis country took from it (the Falklands)?
// A player leads ME on the Axis path, declares war on Britain in month T and lands where the script can. One F line
// each January and at the end: the British ground ME holds, whether Germany stands, whom Britain is at war with, and
// where Britain's field corps stand. One END line: when ME first held British ground, when Britain took it back
// (or never), when Germany fell, and the months Britain had it in enemy hands after Germany fell.
// usage: probe77 ME SEED UNTIL [T]     (build with build70.sh)
import { campaign, date, T, atWar } from './lib';
const [me, seedS, untilS = '124', tS = '13'] = process.argv.slice(2);
const X = T as any;
let declared = 0, took = 0, back = 0, gerFell = 0, last = 0, after = 0, peace = 0;
const held = (g: any): any[] => g.regions.filter((r: any) => r.owner === 'UK' && r.ctrl === me);
const read = (g: any, tag: string): void => {
  const uk = g.nations.UK;
  const f = g.corps.filter((c: any) => c.nat === 'UK' && !c.dead && c.kind !== 'fort' && c.kind !== 'eng');
  const away = f.filter((c: any) => g.regions[c.reg].owner !== 'UK');
  const where: Record<string, number> = {};
  for (const c of away) { const o = g.regions[c.reg].owner; where[o] = (where[o] ?? 0) + 1; }
  console.log(`F ${me} seed ${seedS} ${tag} | British ground in ${me}'s hands: ${held(g).map((r: any) => r.name).join(', ') || 'none'} | Germany ${g.nations.GER.alive ? 'stands' : 'fallen'} | Britain at war with ${uk.atWar.filter((x: string) => g.nations[x]?.alive).join(' ') || 'nobody'}; field corps ${f.length}, at home or on its own ground ${f.length - away.length}, abroad ${Object.entries(where).sort((a, b) => b[1] - a[1]).slice(0, 5).map(([k, v]) => `${k} ${v}`).join(' ') || '0'}; sealift ${uk.sealift ?? 0} | ${me}: ${g.nations[me].alive ? 'stands' : 'FALLEN'}, corps ${g.corps.filter((c: any) => c.nat === me && !c.dead && c.kind !== 'fort').length}, on that ground ${g.corps.filter((c: any) => c.nat === me && !c.dead && g.regions[c.reg].owner === 'UK').length}`);
};
const plan: any = { minOdds: 1.5, land: true, stage: true, path: 'axis', buy: ['corps:inf', 'ship:tp', 'air:ftr'],
  first: (g: any) => {
    last = g.turn;
    if (!gerFell && !g.nations.GER.alive) gerFell = g.turn;
    const h = held(g).length;
    if (!took && h) took = g.turn;
    if (took && !back && !h) back = g.turn;
    if (took && !back && gerFell && atWar(g, me, 'UK')) after++;
    if (declared && !peace && !atWar(g, me, 'UK')) peace = g.turn;
    if (!g.nations[me]?.alive) return;
    if (!declared && g.turn >= Number(tS) && X.declarable(g).includes('UK') && X.orderDeclare(g, 'UK')) declared = g.turn;
    if (g.turn === 1 || (g.turn - 5) % 12 === 0) read(g, date(g.turn));
  },
};
const run = campaign(me, 1, Number(seedS), plan, { until: Number(untilS) });
read(run.g, `the end (${date(last)})`);
console.log(`END ${me} seed ${seedS}: declared war on Britain ${declared ? date(declared) : 'never'}; first held British ground ${took ? date(took) : 'never'}; Britain took it back ${back ? date(back) : 'NEVER'}; Germany ${gerFell ? `fell ${date(gerFell)}` : 'STANDS at the end'}; months after Germany fell with British ground still in ${me}'s hands and the war on: ${after}; ${peace ? `peace with Britain from ${date(peace)}` : 'still at war with Britain at the end'}; ${run.g.nations[me].alive ? 'stands' : 'FALLEN'}${run.errors.length ? ' | ERROR ' + run.errors.join(' / ') : ''}`);
