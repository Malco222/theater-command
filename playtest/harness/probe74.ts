// Version 76, the hemisphere set from a player's seat (the design director: a neutral selling to both sides must not
// out-earn a country at war). A player leads one country of the Americas through the war in one of these ways:
//   idle      no orders at all
//   merchant  stays at peace and signs every trade agreement on offer, the best-paying first, whoever it is with
//   war       takes the Allies' path in the first month and declares war on Germany in month T (default 36, August 1942)
//   warearly  takes the Allies' path in the first month and declares war on Germany in October 1939 (the director's
//             question: does a war nobody can bring to him pay?)
//   league    founds the League with the first willing partner in the first month it can, or joins it
//   axis      takes the Axis path in the first month and declares war on the United States and Britain in month T
// One H line each January and at the end: at peace or at war, income a month and how much of it is trade, the trading
// partners by side, the treasury, the field corps and how many are away from home. One SUM line: the mean income a
// month over the war, the months at war, and the months it traded with both sides at once.
// usage: probe74 ME SEED UNTIL MODE [T]     (build with build70.sh)
import { campaign, date, T, atWar } from './lib';
import { income } from '@tc/campaign/economy';
import { dealsOf, tradeIncome } from '@tc/campaign/trade';
const [me, seedS, untilS = '76', mode = 'idle', tS = '36'] = process.argv.slice(2);
const BUY = ['corps:arm', 'corps:inf', 'air:ftr', 'ship:dd', 'work:factory'];
const plan: any = mode === 'idle' || mode === 'merchant' ? { idle: true }
  : mode === 'war' ? { minOdds: 1.5, land: true, stage: true, path: 'allies', declare: [[Number(tS), 'GER']], buy: BUY }
  : mode === 'warearly' ? { minOdds: 1.5, land: true, stage: true, path: 'allies', declare: [[2, 'GER']], buy: BUY }
  : mode === 'league' ? { minOdds: 1.5, land: true, path: 'league', buy: BUY }
  : { minOdds: 1.5, land: true, path: 'axis', declare: [[Number(tS), 'USA'], [Number(tS), 'UK']], buy: BUY };
const tried = new Set<string>(), swapped: string[] = [], swapFail: string[] = []; let built = 0, housesNow = 0;
let months = 0, sum = 0, sumTrade = 0, warMonths = 0, bothMonths = 0, signed = 0, refused: Record<string, number> = {}, most = 0, last = 0;
const side = (g: any, p: string): string => (g.nations[p]?.align === 'axis' ? 'axis' : g.nations[p]?.align?.startsWith('allies') || atWar(g, p, 'GER') ? 'allies' : 'neutral');
const read = (g: any, tag: string): void => {
  const n = g.nations[me];
  const field = g.corps.filter((c: any) => c.nat === me && !c.dead && c.kind !== 'fort' && c.kind !== 'eng');
  const deals = dealsOf(g, me).map((d: any) => (d.a === me ? d.b : d.a));
  console.log(`H ${me} ${mode} seed ${seedS} ${tag} | ${n.alive ? (n.atWar.length ? 'WAR' : 'peace') : 'fallen'} ${n.align} | income ${income(g, me).toFixed(1)} of it trade ${tradeIncome(g, me).toFixed(1)} | partners ${deals.map((p: string) => `${p}(${side(g, p)})`).join(', ') || '-'} | treasury ${Math.round(n.pp)} | field corps ${field.length}, away ${field.filter((c: any) => g.regions[c.reg].owner !== me).length} | provinces ${g.regions.filter((r: any) => r.ctrl === me).length}`);
};
const run = campaign(me, 1, Number(seedS), { ...plan,
  each: (g: any) => {
    const n = g.nations[me]; if (!n?.alive) return;
    if (mode === 'merchant') {
      const X = T as any;
      // Commerce first: a market hall in the capital and the harbour towns, as far as the treasury goes (from the
      // second level each opens one more trading house).
      const places = g.regions.filter((r: any) => r.ctrl === me && (r.id === n.capital || r.port)).sort((a: any, b: any) => Number(b.id === n.capital) - Number(a.id === n.capital));
      for (const r of places) {
        const tb = X.townBuildings(g, r.id); const o = tb?.offers?.find((x: any) => x.line === 'market' && x.why === null);
        if (o && X.orderBuilding(g, r.id, 'market')) built++;
      }
      // Then the trading houses: sign the best-paying offer; with every house in use, give up the poorest agreement
      // for an offer that pays a third more, if the richer partner will then sign.
      for (let k = 0; k < 8; k++) {
        const book: any = X.tradeLedger(g); if (!book) break;
        const o = book.offers.find((x: any) => x.why === null);
        if (o) { if (!X.orderTrade(g, o.nat)) break; signed++; continue; }
        const full = book.offers.filter((x: any) => x.why && x.why.startsWith('Every trading house')).sort((a: any, b: any) => b.pays - a.pays)[0];
        const worst = [...book.deals].sort((a: any, b: any) => a.pays - b.pays)[0];
        if (full && worst && full.pays > worst.pays * 1.33 && !tried.has(`${full.nat}:${worst.nat}`)) {
          tried.add(`${full.nat}:${worst.nat}`);
          if (X.orderEndTrade(g, worst.nat)) {
            const again: any = X.tradeLedger(g); const now = again.offers.find((x: any) => x.nat === full.nat);
            if (now?.why === null && X.orderTrade(g, full.nat)) { signed++; swapped.push(`${date(g.turn)}: ${worst.nat} for ${full.nat}`); continue; }
            swapFail.push(`${date(g.turn)}: gave up ${worst.nat}, then ${full.nat} said: ${now?.why ?? 'no offer'}`);
            X.orderTrade(g, worst.nat);
          }
        }
        for (const x of book.offers.slice(0, 6)) if (x.why) refused[x.why] = (refused[x.why] ?? 0) + 1;
        break;
      }
      housesNow = (X.tradeLedger(g) as any)?.houses ?? 0;
    }
    const inc = income(g, me); months++; sum += inc; sumTrade += tradeIncome(g, me); if (inc > most) most = inc;
    if (n.atWar.length) warMonths++;
    const sides = new Set(dealsOf(g, me).map((d: any) => side(g, d.a === me ? d.b : d.a)));
    if (sides.has('axis') && sides.has('allies')) bothMonths++;
    if (g.turn === 1 || (g.turn - 5) % 12 === 0) read(g, date(g.turn));
    last = g.turn;
  },
}, { until: Number(untilS) });
read(run.g, `the end (${date(last)})`);
const R = run.rows, notes = R.flatMap((r: any) => r.notes.map((x: string) => `${r.date}: ${x}`)).slice(0, 5).join('; ');
const firstWar = R.find((r: any) => r.wars && r.wars.length)?.date;
console.log(`ACT ${me} ${mode} seed ${seedS}: at war from ${firstWar ?? 'never'}; the player attacked in ${R.filter((r: any) => r.attacksOrdered).length} months, was attacked in ${R.filter((r: any) => r.hitByEnemy).length}; provinces won ${R.reduce((s: number, r: any) => s + r.gained, 0)}, lost ${R.reduce((s: number, r: any) => s + r.lost, 0)}; bought ${R.reduce((s: number, r: any) => s + r.bought.length, 0)} things; corps at the end ${R.length ? R[R.length - 1].myCorps : 0}${notes ? ` | ${notes}` : ''}`);
const why = Object.entries(refused).sort((a, b) => b[1] - a[1]).slice(0, 3).map(([k, v]) => `${k} (${v})`).join('; ');
console.log(`SUM ${me} ${mode} seed ${seedS}: ${months} months read; income a month, the mean ${(sum / Math.max(1, months)).toFixed(1)} and the most ${most.toFixed(1)}; of it trade, the mean ${(sumTrade / Math.max(1, months)).toFixed(1)}; at war in ${warMonths} months; trading with both sides at once in ${bothMonths} months; agreements signed by the player ${signed}${mode === 'merchant' ? `, market halls ordered ${built}, trading houses at the end ${housesNow}, swaps ${swapped.join(', ') || 'none'}${swapFail.length ? `, swaps that failed ${swapFail.slice(0, 3).join(', ')}` : ''}` : ''}${why ? `; offers refused, the commonest reasons: ${why}` : ''}; ${run.g.nations[me].alive ? 'stands' : 'FALLEN'}${run.errors.length ? ' | ERROR ' + run.errors.join(' / ') : ''}`);
