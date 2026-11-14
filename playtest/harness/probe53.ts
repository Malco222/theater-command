// Version 67: a fair shot from every seat. usage: probe53 ME FROM SEED UNTIL PLAN-JSON
// Plays the scripted player's war from any country's seat and prints one SEAT line: whether the player's country
// stands at the end and who holds its capital, which great powers fell and when, how the war went for its side
// (won: the chief enemy fell while the player stands; lost: the player's country fell; undecided), and how much of
// the war the player had a battle to fight in. The chief enemy of an Axis seat is the Soviet Union, of an Allied
// seat Germany. One line a war, so that the seats can be set side by side (seattab.py).
import { campaign, date } from './lib';
const [me, fromS, seedS, untilS, planS] = process.argv.slice(2);
const plan = JSON.parse(planS || '{}');
const POWERS = ['GER', 'ITA', 'UK', 'FRA', 'SOV', 'USA', 'JAP'];
const AXIS = new Set(['GER', 'ITA', 'JAP', 'HUN', 'ROM', 'BUL', 'FIN']);
const fell: Record<string, number> = {};
const seen = new Set<number>();
let atWarMonths = 0, foughtMonths = 0, lastTurn = 0, firstWar = 0, quiet = 0, longest = 0, longestEnd = 0, began = 0, most = 0;
const run = campaign(me, Number(fromS), Number(seedS), { ...plan, each: (g: any) => {
  lastTurn = g.turn;
  let fought = 0;
  for (const r of g.reports) {
    if (seen.has(r.id)) continue; seen.add(r.id);
    if (r.yielded || r.town) continue;
    if (r.attNat === me || r.defNat === me || r.attackers.some((u: any) => u.nat === me) || r.defenders.some((u: any) => u.nat === me)) fought++;
  }
  // (A power not yet in the war is dormant, not fallen: only a recorded fall counts.)
  for (const p of POWERS) { const n = g.nations[p]; if (n && n.fellOn && !fell[p]) fell[p] = n.fellOn.turn; }
  const mine = g.regions.filter((x: any) => x.ctrl === me).length;
  if (!began) began = mine;
  if (mine > most) most = mine;
  if (g.nations[me].alive && g.nations[me].atWar.length) {
    atWarMonths++; if (!firstWar) firstWar = g.turn;
    if (fought) { foughtMonths++; quiet = 0; } else { quiet++; if (quiet > longest) { longest = quiet; longestEnd = g.turn; } }
  }
} } as any, { until: Number(untilS), quiet: true } as any);
for (const e of run.errors) console.log('ERROR', String(e).slice(0, 600));
const g: any = run.g, N = g.nations, M = N[me];
const stands = !!M.alive;
const axis = AXIS.has(me);
const chief = axis ? 'SOV' : 'GER';
const myFall = !stands ? (M.fellOn?.turn ?? (run as any).fellOn ?? lastTurn) : 0;
const chiefFell = fell[chief] && !N[chief]?.alive ? fell[chief] : 0;
const result = !stands ? 'lost' : chiefFell ? 'won' : 'undecided';
const cap = g.regions[M.capital];
const fallen = POWERS.filter((p) => fell[p]).map((p) => `${p} ${date(fell[p])}${N[p]?.alive ? ' (back)' : ''}`).join(', ') || 'none';
const standing = POWERS.filter((p) => N[p] && N[p].alive).map((p) => (N[p].dormant ? `${p} (not in the war)` : p)).join(' ') || 'none';
console.log(`SEAT ${me} seed ${seedS}: played to ${date(lastTurn)}; ${stands ? 'stands' : `fell ${date(myFall)}`}; result ${result}${result === 'won' ? ` (${chief} fell ${date(chiefFell)})` : ''}; capital ${cap?.name ?? '?'} held by ${cap?.ctrl ?? '?'}; great powers fallen: ${fallen}; standing: ${standing}; at war ${atWarMonths} months${firstWar ? ` from ${date(firstWar)}` : ''}, a battle of the player's in ${foughtMonths}, longest silence ${longest} months${longest ? ` ending ${date(longestEnd)}` : ''}; provinces at the start ${began}, most ${most}, at the end ${g.regions.filter((x: any) => x.ctrl === me).length}; depth ${plan.depth ?? 'by hand'}`);
