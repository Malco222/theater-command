// Version 79 and later: what an Italian player sees of the armistice of 1943 (the events mussolini-falls and
// italian-armistice in data/campaign/diplomacy.json). usage: probe85 SEED [UNTIL=76] [PLAN-JSON] (a plan's "secure":true
// sets "Your office is secure"; build with build70.sh)
// One ARM line a war: the month each of the six grounds the events read (Sicily, Sardinia, Calabria, Apulia, Naples,
// Rome) first went to an Ally, the month Mussolini fell, the month Italy signed and to whom, what Italy had as the
// month before the signing closed (field corps, home provinces held of its own, which of the six the Allies held,
// Allied corps on Italy's home ground), and the news lines that named the danger before it came.
// TC_ME=GER (with Germany's seat plan as PLAN-JSON) plays another seat and lists every news line about Italy that
// player gets from January 1943 to the month after Italy leaves the war (ruling 99's question).
import { campaign, date } from './lib';
const [seedS, untilS, planS] = process.argv.slice(2);
const ME = process.env.TC_ME || 'ITA';
const plan = ME === 'ITA' ? { minOdds: 1.4, declare: [[10, 'FRA'], [10, 'UK'], [14, 'GRE']], land: true, ...JSON.parse(planS || '{}') } : JSON.parse(planS || '{}');
const SIX = ['Sicily', 'Sardinia', 'Calabria', 'Apulia', 'Naples', 'Rome'];
const ALLY = (g: any, id: string): boolean => g.nations[id]?.align === 'allies';
const lostAt: Record<string, number> = {};
let fellAt = 0, signedAt = 0, seen = 0, before = '', news: string[] = [];
const snap = (g: any): string => {
  const corps = g.corps.filter((c: any) => c.nat === 'ITA' && !c.dead && c.kind !== 'fort').length;
  const home = g.regions.filter((r: any) => r.owner === 'ITA' && !r.overseas);
  const held = home.filter((r: any) => r.ctrl === 'ITA').length;
  const six = SIX.filter((n) => ALLY(g, g.byName[n]?.ctrl));
  const foes = home.reduce((s: number, r: any) => s + r.units.filter((u: any) => ALLY(g, u.nat) && u.kind !== 'fort').length, 0);
  const ger = home.reduce((s: number, r: any) => s + r.units.filter((u: any) => u.nat === 'GER' && u.kind !== 'fort').length, 0);
  return `${corps} Italian field corps, ${held} of ${home.length} home provinces held, the Allies hold ${six.join(' and ') || 'none of the six'}, ${foes} Allied corps and ${ger} German corps on Italy's home ground`;
};
const look = (g: any): void => {
  for (const n of SIX) if (!lostAt[n] && ALLY(g, g.byName[n]?.ctrl)) lostAt[n] = g.turn;
  for (const h of g.headlines.slice(seen)) {
    const x: string = h.text;
    if (/Fascist Grand Council/.test(x)) fellAt = h.turn;
    if (/Italy has signed an armistice/.test(x)) signedAt = h.turn;
    if (ME === 'ITA' ? h.turn >= 44 && /Mussolini|armistice|Badoglio|Sicily|Sardinia/.test(x) : h.turn >= 41 && (!g.nations.ITA.fellOn || h.turn <= g.nations.ITA.fellOn.turn + 1) && /Ital|Mussolini|armistice|Badoglio|Sicily|Sardinia|Rome\b|Cagliari|Palermo/.test(x) && !/ bombs /.test(x)) news.push(`${date(h.turn)} [${h.kind}]: ${x.slice(0, ME === 'ITA' ? 150 : 260)}`);
  }
  seen = g.headlines.length;
  if (g.nations.ITA.alive) before = `${date(g.turn)}: ${snap(g)}`;
};
const run = campaign(ME, 1, Number(seedS), { ...plan, first: look, each: look } as any, { until: Number(untilS || 76), quiet: true } as any);
const g: any = run.g; look(g);
const I = g.nations.ITA;
console.log(`ARM seed ${seedS}${ME === 'ITA' ? '' : ` (the player is ${ME})`}${plan.secure ? ' (office secure)' : ''}: first lost to an Ally: ${SIX.map((n) => `${n} ${lostAt[n] ? date(lostAt[n]) : 'never'}`).join(', ')} | Mussolini fell ${fellAt ? date(fellAt) : 'never'} | Italy ${I.alive ? 'stands' : `out ${date(I.fellOn.turn)} (${I.fellOn.how}, to ${I.fellOn.to})`}${signedAt ? ` | armistice told ${date(signedAt)}` : ''} | the last month it stood: ${before}`);
for (const n of news.slice(0, ME === 'ITA' ? 14 : 60)) console.log(`   NEWS ${n}`);
