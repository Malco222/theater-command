// Version 68: a fair shot from every seat, with history as par. usage: probe54 ME FROM SEED UNTIL PLAN-JSON
// (probe53 with a PAR part on the SEAT line. Par, as the builder told Brian: Germany still holds Berlin at the end of
// 1944; the Soviet Union beats Germany before May 1945 and never loses Moscow; Britain is ashore in the west to stay
// before June 1944 and is never invaded; France still holds Paris at the end of 1940; Italy still holds Rome at the
// end of 1943. "The west" is the mainland of France, the Low Countries, Germany and Denmark; "to stay" is an unbroken
// hold from that month until Germany falls or the war ends. Also how near the country came: the most of its home
// provinces in enemy hands at once, the months an enemy stood on home soil, and the Soviet levies called.)
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
const WEST = new Set(['FRA', 'BEL', 'NED', 'LUX', 'GER', 'DEN']);
const capAt: Record<number, string> = {};
const seaHeld: string[] = []; let seaAll = 0;
let capLost = 0, capBack = 0, homeLostMost = 0, homeMonths = 0, ashoreFrom = 0, ashoreFirst = 0, invaded = 0, least = 9999, gerFellSeen = 0;
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
  const M0 = g.nations[me];
  if (M0.alive) {
    const cap0 = g.regions[M0.capital];
    capAt[g.turn] = cap0.ctrl;
    if (cap0.ctrl !== me && !capLost) capLost = g.turn;
    if (cap0.ctrl === me && capLost && !capBack) capBack = g.turn;
    const homeLost = g.regions.filter((x: any) => x.owner === me && !x.overseas && x.ctrl !== me).length;
    if (homeLost > homeLostMost) homeLostMost = homeLost;
    if (homeLost) { homeMonths++; if (!invaded) invaded = g.turn; }
    if (g.turn % 12 === 4) { const o = g.regions.filter((x: any) => x.owner === me && x.overseas); seaHeld.push(`${1939 + Math.floor((g.turn + 7) / 12)} ${o.filter((x: any) => x.ctrl === me).length}`); seaAll = o.length; }
    if (!gerFellSeen) {
      const west = g.regions.filter((x: any) => x.ctrl === me && x.owner !== me && WEST.has(x.owner) && !x.overseas && x.name !== 'Corsica').length;
      if (west) { if (!ashoreFrom) ashoreFrom = g.turn; if (!ashoreFirst) ashoreFirst = g.turn; } else ashoreFrom = 0;
    }
    if (g.nations.GER && g.nations.GER.fellOn && !g.nations.GER.alive && !gerFellSeen) gerFellSeen = g.turn;
  }
  const mine = g.regions.filter((x: any) => x.ctrl === me).length;
  if (M0.alive && mine < least) least = mine;
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
const gerFell = fell.GER && !N.GER?.alive ? fell.GER : 0;
let par = false, parWhy = '';
if (me === 'GER') { par = capAt[64] === 'GER'; parWhy = `Berlin at the end of 1944: ${capAt[64] === 'GER' ? 'held' : capAt[64] ? `held by ${capAt[64]}` : 'Germany had fallen'}`; }
else if (me === 'SOV') { par = stands && !!gerFell && gerFell < 69 && !capLost; parWhy = `Germany ${gerFell ? `fell ${date(gerFell)}` : 'stands'}; Moscow ${capLost ? `lost ${date(capLost)}${capBack ? `, back ${date(capBack)}` : ''}` : 'never lost'}`; }
else if (me === 'UK') { par = stands && !!ashoreFrom && ashoreFrom < 58 && !invaded; parWhy = `ashore to stay ${ashoreFrom ? `from ${date(ashoreFrom)}` : 'never'}${ashoreFirst && ashoreFirst !== ashoreFrom ? ` (first ashore ${date(ashoreFirst)})` : ''}; ${invaded ? `invaded ${date(invaded)}` : 'never invaded'}; Germany ${gerFell ? `fell ${date(gerFell)}` : 'stands'}`; }
else if (me === 'FRA') { par = capAt[16] === 'FRA'; parWhy = `Paris at the end of 1940: ${capAt[16] === 'FRA' ? 'held' : capAt[16] ? `held by ${capAt[16]}` : 'France had fallen'}`; }
else if (me === 'ITA') { par = capAt[52] === 'ITA'; parWhy = `Rome at the end of 1943: ${capAt[52] === 'ITA' ? 'held' : capAt[52] ? `held by ${capAt[52]}` : 'Italy had fallen'}; Germany ${gerFell ? `fell ${date(gerFell)}` : 'stands'}`; }
const levies = Number(g.flags?.['lv:SOV'] ?? 0);
const near = `home provinces in enemy hands at the worst ${homeLostMost}, an enemy on home soil in ${homeMonths} months${invaded ? ` from ${date(invaded)}` : ''}, fewest provinces held ${least === 9999 ? 0 : least}; capital ${capLost ? `first lost ${date(capLost)}${capBack ? `, back ${date(capBack)}` : ''}` : 'never lost while the country stood'}; Soviet levies ${levies}; overseas provinces held each December of ${seaAll}: ${seaHeld.join(', ') || '-'}`;
const fallen = POWERS.filter((p) => fell[p]).map((p) => `${p} ${date(fell[p])}${N[p]?.alive ? ' (back)' : ''}`).join(', ') || 'none';
const standing = POWERS.filter((p) => N[p] && N[p].alive).map((p) => (N[p].dormant ? `${p} (not in the war)` : p)).join(' ') || 'none';
console.log(`SEAT ${me} seed ${seedS}: played to ${date(lastTurn)}; ${stands ? 'stands' : `fell ${date(myFall)}`}; result ${result}${result === 'won' ? ` (${chief} fell ${date(chiefFell)})` : ''}; capital ${cap?.name ?? '?'} held by ${cap?.ctrl ?? '?'}; great powers fallen: ${fallen}; standing: ${standing}; at war ${atWarMonths} months${firstWar ? ` from ${date(firstWar)}` : ''}, a battle of the player's in ${foughtMonths}, longest silence ${longest} months${longest ? ` ending ${date(longestEnd)}` : ''}; provinces at the start ${began}, most ${most}, at the end ${g.regions.filter((x: any) => x.ctrl === me).length}; par ${par ? 'BEATEN' : 'missed'} (${parWhy}); ${near}; depth ${plan.depth ?? 'by hand'}`);
