// Who sets foot on the player's home soil, where and when. usage: probe62 ME SEED UNTIL PLAN-JSON
// One HOME line for every month a home province of the player's is in other hands: the province, who holds it, the
// corps standing in it, and the battle reports of that month fought there.
import { campaign, date } from './lib';
const [me, seedS, untilS, planS] = process.argv.slice(2);
const plan = JSON.parse(planS || '{}');
const seen = new Set<number>();
const run = campaign(me, 1, Number(seedS), { ...plan, each: (g: any) => {
  const lost = g.regions.filter((x: any) => x.owner === me && !x.overseas && x.ctrl !== me);
  const reps = g.reports.filter((r: any) => !seen.has(r.id)); for (const r of reps) seen.add(r.id);
  for (const x of lost) {
    const here = x.units.map((u: any) => `${u.nat} ${u.kind}`).join(', ') || 'nobody';
    const fights = reps.filter((r: any) => r.reg === x.id || r.region === x.id || r.where === x.name).map((r: any) => `${r.attNat}>${r.defNat}${r.landing ? ' by sea' : ''}${r.won ? ' won' : ''}`).join('; ');
    console.log(`HOME ${me} seed ${seedS} ${date(g.turn)}: ${x.name} held by ${x.ctrl}; standing there: ${here}; fights: ${fights || '-'}; ${x.ctrl} sealift ${g.nations[x.ctrl]?.sealift}`);
  }
} } as any, { until: Number(untilS || 76) });
console.log(`HOME ${me} seed ${seedS}: done at ${date(run.g.turn)}`);
