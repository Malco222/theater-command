// Version 76: a year in a small country's seat (the design director: play Brazil and Argentina once each; can it join
// the Allies, join the Axis and found a League, and does every year give the player something to do).
// A player leads the country by hand in one of these ways:
//   look     takes no path and declares no war
//   allies   takes the Allies' path in the first month, and declares war on Germany as soon as the rules allow it
//   axis     takes the Axis path in the first month, and declares war on the first country the rules allow, from month T
//   league   founds the League with the first willing partner, or joins it
// One Y line in the first month, each January and at the end: where the country stands, the paths open today (and
// why not), the countries willing to found the League, whom it may declare war on, the trade offers that can be signed,
// what the arsenal will sell today, and the attacks open to its corps. One YEAR line a calendar year: the months with
// a battle, with something bought, with a move, and the months in which the scripted player gave no order at all.
// usage: probe75 ME SEED UNTIL MODE [T]     (build with build70.sh)
import { campaign, date, T, atWar } from './lib';
import { income } from '@tc/campaign/economy';
import { dealsOf, tradeIncome } from '@tc/campaign/trade';
const [me, seedS, untilS = '76', mode = 'look', tS = '13'] = process.argv.slice(2);
const BUY = ['corps:arm', 'corps:inf', 'air:ftr', 'ship:dd', 'ship:tp', 'work:factory'];
const base: any = { minOdds: 1.5, land: true, stage: true, buy: BUY };
let declared = '', seenNews = 0, wasAtWar = false;
const news = new Map<number, string[]>();
const X = T as any;
const short = (s: string, n = 70): string => (s.length > n ? s.slice(0, n - 1) + '…' : s);
const look = (g: any, tag: string): void => {
  const n = g.nations[me]; if (!n?.alive) { console.log(`Y ${me} ${mode} seed ${seedS} ${tag} | FALLEN`); return; }
  const paths = (X.paths(g) as any[]).map((p) => `${p.id} ${p.now ? 'NOW' : p.ok ? 'yes' : 'no'}${!p.ok && p.why ? ` (${short(p.why, 60)})` : ''}`).join('; ') || 'none';
  const fo = (X.leaguePartners(g) as any[]);
  const willing = fo.filter((f) => f.ok).map((f) => f.nat);
  const inv = (X.leagueInvites(g) as any[]).filter((f) => f.ok).map((f) => f.nat);
  const dec = X.declarable(g) as string[];
  const book = X.tradeLedger(g);
  const open = book ? book.offers.filter((o: any) => o.why === null).map((o: any) => `${o.nat} ${o.pays?.toFixed?.(1) ?? ''}`) : [];
  const why = book ? [...new Set(book.offers.filter((o: any) => o.why).map((o: any) => short(o.why, 50)))].slice(0, 2) : [];
  const ars = (X.arsenalOffers(g, n.capital) as any[]);
  const can = ars.filter((o) => o.why === null).map((o) => o.key);
  const field = g.corps.filter((c: any) => c.nat === me && !c.dead && c.kind !== 'fort' && c.kind !== 'eng');
  const foes = n.atWar.filter((x: string) => g.nations[x]?.alive);
  console.log(`Y ${me} ${mode} seed ${seedS} ${tag} | ${foes.length ? `WAR with ${foes.join(' ')}` : 'peace'} ${n.align}${n.league ? ' LEAGUE' : ''} | income ${income(g, me).toFixed(1)} trade ${tradeIncome(g, me).toFixed(1)} treasury ${Math.round(n.pp)} capital ${n.pc?.toFixed?.(1) ?? '-'} | paths: ${paths} | League founders willing: ${willing.join(' ') || 'none'} of ${fo.length}${inv.length ? `; may invite ${inv.join(' ')}` : ''} | may declare on: ${dec.join(' ') || 'nobody'} | trade: ${dealsOf(g, me).length} of ${book?.houses ?? '?'} houses, can sign ${open.join(', ') || 'none'}${why.length ? ` [${why.join(' / ')}]` : ''} | arsenal sells ${can.length} of ${ars.length}: ${can.slice(0, 8).join(' ') || '-'} | field corps ${field.length}, away ${field.filter((c: any) => g.regions[c.reg].owner !== me).length}, sealift ${n.sealift ?? 0} | provinces ${g.regions.filter((r: any) => r.ctrl === me).length} | the United States at war with ${g.nations.USA?.atWar.filter((x: string) => g.nations[x]?.alive).join(' ') || 'nobody'}; League members ${Object.values(g.nations).filter((x: any) => x.league && x.alive).map((x: any) => x.id).join(' ') || 'none'}`);
};
const plan: any = { ...base, ...(mode === 'allies' ? { path: 'allies' } : mode === 'axis' ? { path: 'axis' } : mode === 'league' ? { path: 'league' } : {}),
  first: (g: any) => {
    const n = g.nations[me]; if (!n?.alive) return;
    // News that names the player's country, by year (what comes to the player without an order of the player's own).
    for (let i = seenNews; i < g.headlines.length; i++) { const h = g.headlines[i]; if (h.nat === me || h.text.includes(n.name)) { const y = 1939 + Math.floor((h.turn + 7) / 12); news.set(y, [...(news.get(y) ?? []), h.text]); } }
    seenNews = g.headlines.length;
    if (!wasAtWar && n.atWar.length) { wasAtWar = true; console.log(`W ${me} is at war from ${date(g.turn)} with ${n.atWar.join(' ')}`); }
    if (mode === 'league' && n.league) for (const f of (X.leagueInvites(g) as any[])) if (f.ok && X.whyNotLeagueInvite(g, f.nat) === null && X.orderLeagueInvite(g, f.nat)) console.log(`I ${me} invited ${f.nat} into the League in ${date(g.turn)} (${f.why})`);
    if (g.turn === 1 || (g.turn - 5) % 12 === 0) look(g, date(g.turn));
    // The war the player wants: the Allies' player on Germany, the Axis player on whoever the rules allow.
    if (mode === 'allies' && !atWar(g, me, 'GER') && g.nations.GER?.alive && X.declarable(g).includes('GER') && X.orderDeclare(g, 'GER')) { declared = `GER in ${date(g.turn)}`; console.log(`D ${me} declared war on Germany in ${date(g.turn)}`); }
    if (mode === 'axis' && !declared && g.turn >= Number(tS)) {
      const dec = (X.declarable(g) as string[]).filter((x) => g.nations[x]?.align !== 'axis');
      const on = ['USA', 'UK', 'FRA'].find((x) => dec.includes(x)) ?? dec[0];
      if (on && X.orderDeclare(g, on)) { declared = `${on} in ${date(g.turn)}`; console.log(`D ${me} declared war on ${on} in ${date(g.turn)}; the rules allowed ${dec.join(' ')}`); }
    }
  },
};
const run = campaign(me, 1, Number(seedS), plan, { until: Number(untilS) });
look(run.g, `the end (${date(Math.min(run.g.turn, Number(untilS)))})`);
// The years.
const years = new Map<number, any>();
for (const r of run.rows) {
  const y = 1939 + Math.floor((r.turn + 7) / 12);
  const o = years.get(y) ?? { months: 0, battle: 0, hit: 0, bought: 0, moved: 0, quiet: 0, won: 0, ordered: 0, gained: 0, lost: 0, options: 0, notes: [] as string[], kinds: {} as Record<string, number> };
  o.months++; o.ordered += r.attacksOrdered; o.won += r.attacksWon; o.gained += r.gained; o.lost += r.lost;
  if (r.attacksOrdered) o.battle++;
  if (r.hitByEnemy) o.hit++;
  if (r.bought.length) o.bought++;
  if (r.moves) o.moved++;
  if (r.attackOptions) o.options++;
  if (!r.attacksOrdered && !r.bought.length && !r.moves && !r.notes.length) o.quiet++;
  for (const b of r.bought) { const k = b.split(':')[0]; o.kinds[k] = (o.kinds[k] ?? 0) + 1; }
  o.notes.push(...r.notes.map((x) => `${r.date}: ${short(x, 60)}`));
  years.set(y, o);
}
for (const [y, o] of years) console.log(`YEAR ${me} ${mode} seed ${seedS} ${y}: ${o.months} months | the player attacked in ${o.battle} (${o.ordered} attacks, ${o.won} won), was attacked in ${o.hit}, had an enemy next to a corps in ${o.options} | bought in ${o.bought} (${Object.entries(o.kinds).map(([k, v]) => `${k} ${v}`).join(', ') || 'nothing'}) | moved corps in ${o.moved} | no order at all in ${o.quiet} | provinces won ${o.gained}, lost ${o.lost}${o.notes.length ? ` | ${o.notes.slice(0, 4).join('; ')}` : ''} | news naming the country ${(news.get(y) ?? []).length}${(news.get(y) ?? []).length ? `: ${(news.get(y) ?? []).slice(0, 3).map((x) => short(x, 110)).join(' / ')}` : ''}`);
const tot = run.rows.reduce((s, r) => ({ a: s.a + (r.attacksOrdered ? 1 : 0), h: s.h + (r.hitByEnemy ? 1 : 0), q: s.q + (!r.attacksOrdered && !r.bought.length && !r.moves && !r.notes.length ? 1 : 0) }), { a: 0, h: 0, q: 0 });
console.log(`SUM ${me} ${mode} seed ${seedS}: ${run.rows.length} months; declared ${declared || 'no war'}; a battle of the player's own in ${tot.a} months, attacked in ${tot.h}, no order at all in ${tot.q}; ${run.g.nations[me].alive ? 'stands' : 'FALLEN'}; problems ${run.problems.filter((p) => !p.startsWith('[note]')).length}${run.errors.length ? ' | ERROR ' + run.errors.join(' / ') : ''}`);
