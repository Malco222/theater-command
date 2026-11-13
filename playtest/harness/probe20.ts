// Scorched earth from the player's seat (Version 56 on): the same war with a computer nation's policy as shipped or
// forced, so the only difference is what gets burned. Prints the player's ground, army and money each quarter, what
// lies wrecked on ground he holds, and how much scorched earth news he is shown.
// usage: probe20 ME FROM SEED UNTIL PLAN-JSON [WHO=POLICY,...] [MYPOLICY]     e.g. probe20 GER 1 7 60 '<plan>' SOV=never
import { campaign, date, T, ledger } from './lib';
import { PRIZES, prizesAt } from '@tc/campaign/prizes';
const [me, fromS, seedS, untilS, planS, forced, mine] = process.argv.slice(2);
const plan = planS ? JSON.parse(planS) : { minOdds: 1.4 };
if (forced && forced !== '-') for (const kv of forced.split(',')) { const [who, pol] = kv.split('='); (PRIZES as any).ai.policy[who] = pol; }
let h0 = 0, set = false;
const perMonth: number[] = [];
const kinds: Record<string, number> = {};
let seized = 0, wreckNews = 0, allNews = 0, warNews = 0;
campaign(me, Number(fromS), Number(seedS), { ...plan, each: (g) => {
  if (mine && !set) { set = T.orderScorchPolicy(g, mine as any); }
  const hs = g.headlines.slice(h0); h0 = g.headlines.length;
  allNews += hs.length; if (g.nations[me].atWar.length) warNews += hs.length;
  const burns = hs.filter((h) => h.text.startsWith('Scorched earth:'));
  perMonth.push(burns.length);
  for (const h of burns) for (const k of ['oilfields', 'factory district', 'airfield', 'harbour']) if (h.text.includes(k)) kinds[k] = (kinds[k] ?? 0) + 1;
  seized += hs.filter((h) => /though the town holds/.test(h.text)).length;
  wreckNews += hs.filter((h) => /set the oilfields|blow up the factory district|plough up the airfield|block the harbour|blow up the rail junction/.test(h.text)).length;
  if (process.env.DUMP) for (const h of hs) if (h.text.startsWith('Scorched earth:') || /though the town holds/.test(h.text) || /set the oilfields|blow up the factory district|plough up the airfield|block the harbour|blow up the rail junction/.test(h.text)) console.log('NEWS ' + date(g.turn) + ' | ' + h.text);
  if ((g.turn - 1) % 3 === 0 || burns.length >= 4) {
    let out = 0, outMine: Record<string, number> = {};
    for (const r of g.regions) { if (r.ctrl !== me) continue; const v = prizesAt(g, r.id, me); if (!v) continue; for (const p of v.prizes) if (p.out > 0 && p.out < 99) { out++; outMine[p.kind] = (outMine[p.kind] ?? 0) + 1; } }
    const L = (T as any).ledgerOf ? null : null;
    const my = g.corps.filter((c) => c.nat === me && !c.dead);
    const Lg = ledger(g, me) as any;
    console.log(`${date(g.turn)}: provinces ${g.regions.filter((r) => r.ctrl === me).length}, divisions ${Math.round(my.reduce((s, c) => s + c.divs, 0))}, treasury ${Math.round(Lg.treasury * Lg.scale)} net ${Math.round(Lg.net * Lg.scale)}; wrecked places on my ground ${out} ${JSON.stringify(outMine)}; scorched earth news this month ${burns.length}${burns.length >= 4 ? ' <== ' + burns.slice(0, 2).map((b) => b.text.slice(0, 90)).join(' | ') : ''}`);
  }
} }, { until: Number(untilS) });
const tot = perMonth.reduce((s, x) => s + x, 0);
console.log(`TOTAL scorched earth headlines ${tot} in ${perMonth.length} months; months with any ${perMonth.filter((x) => x > 0).length}; most in one month ${Math.max(0, ...perMonth)}; months with 3 or more ${perMonth.filter((x) => x >= 3).length}; by place ${JSON.stringify(kinds)}; "though the town holds" news ${seized}; wrecked-by-order news ${wreckNews}; all headlines ${allNews} (${warNews} in months at war)`);
console.log('per month: ' + perMonth.join(''));
