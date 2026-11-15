// Version 76: what a small Axis country's early war on the United States does to America's own war.
// A player leads ME on the Axis path (taken in the first month), keeps the army at home and declares war on the
// United States in month T (0: never, the base line). One U line each January and at the end: whom the United States
// is at war with, where it stands between the blocs, its war work, its field corps at home and overseas, whether
// Germany stands, and whether the player's country was touched. One END line: when America went to war with Germany
// (or never), when Germany fell (or that it stands), and the news that named the United States.
// usage: probe76 ME SEED UNTIL T     (build with build70.sh)
import { campaign, date, T, atWar } from './lib';
import { bloc } from '@tc/campaign/game';
import { income } from '@tc/campaign/economy';
const [me, seedS, untilS = '76', tS = '13'] = process.argv.slice(2);
const X = T as any; const at = Number(tS);
let usGer = 0, gerFell = 0, declared = 0, hit = 0, last = 0; const news: string[] = []; let seen = 0;
const read = (g: any, tag: string): void => {
  const u = g.nations.USA, n = g.nations[me];
  const f = g.corps.filter((c: any) => c.nat === 'USA' && !c.dead && c.kind !== 'fort' && c.kind !== 'eng');
  const mine = g.corps.filter((c: any) => c.nat === me && !c.dead && c.kind !== 'fort');
  console.log(`U ${me} T${at} seed ${seedS} ${tag} | the United States: ${u.align}, bloc ${bloc(u) ?? 'none'}, at war with ${u.atWar.filter((x: string) => g.nations[x]?.alive).join(' ') || 'nobody'}, income ${income(g, 'USA').toFixed(0)}, war work ${g.flags['ww:USA'] ?? '-'}, field corps ${f.length} (overseas ${f.filter((c: any) => g.regions[c.reg].owner !== 'USA').length}, on ${me}'s soil ${f.filter((c: any) => g.regions[c.reg].owner === me).length}) | Germany ${g.nations.GER.alive ? 'stands' : 'fallen'} | ${me}: ${n.alive ? 'stands' : 'FALLEN'}, income ${income(g, me).toFixed(1)}, provinces ${g.regions.filter((r: any) => r.ctrl === me).length}, corps ${mine.length}`);
};
const plan: any = { minOdds: 1.5, path: 'axis', buy: ['corps:inf', 'air:ftr', 'work:fort'],
  first: (g: any) => {
    const n = g.nations[me]; last = g.turn;
    for (let i = seen; i < g.headlines.length; i++) { const h = g.headlines[i]; if (/United States|America/.test(h.text) && /war|joins|declares/i.test(h.text) && news.length < 8) news.push(`${date(h.turn)}: ${h.text.slice(0, 150)}`); }
    seen = g.headlines.length;
    if (!usGer && atWar(g, 'USA', 'GER')) usGer = g.turn;
    if (!gerFell && !g.nations.GER.alive) gerFell = g.turn;
    if (!n?.alive) return;
    if (at && !declared && g.turn >= at && X.declarable(g).includes('USA') && X.orderDeclare(g, 'USA')) declared = g.turn;
    if (g.turn === 1 || (g.turn - 5) % 12 === 0) read(g, date(g.turn));
  },
};
const run = campaign(me, 1, Number(seedS), plan, { until: Number(untilS) });
read(run.g, `the end (${date(last)})`);
hit = run.rows.filter((r: any) => r.hitByEnemy).length;
console.log(`END ${me} T${at} seed ${seedS}: declared war on the United States ${declared ? date(declared) : 'never'}; America at war with Germany from ${usGer ? date(usGer) : 'NEVER'}; Germany ${gerFell ? `fell ${date(gerFell)}` : 'STANDS at the end'}; the player was attacked in ${hit} months of ${run.rows.length}; ${run.g.nations[me].alive ? 'stands' : 'FALLEN'}${run.errors.length ? ' | ERROR ' + run.errors.join(' / ') : ''} | news: ${news.join(' / ') || 'none'}`);
