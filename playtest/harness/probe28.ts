// Version 58: prepared defences from the player's seat.
// usage: probe28 ME FROM SEED UNTIL PLAN-JSON [TOWN]
// Every field battle the player's corps took part in, split by who attacked and whether the defence was prepared
// (dug in and in supply): how many, how many the attacker won, by odds, and the losses on each side.
// The odds are the attack card's (the two sides' strength before the day's luck: aPower over dPower); the report's own
// `odds` is the figure after the luck, which is always 1.2 or better for a win. Both are printed.
// With ROWS=1 in the environment every battle is also printed as a row: month, A or B (the player attacks or is attacked), attacker,
// defender, the prepared share, the card's odds, the odds on the day, won, men lost on each side, corps on each side, artillery corps in the attack, province.
// Each January: how many of the player's field corps are dug in. Who holds TOWN (Berlin unless named) in December 1944
// and in the last month. The longest run of months with no battle of the player's at all.
import { campaign, date } from './lib';
const [me, fromS, seedS, untilS, planS, town = 'Berlin'] = process.argv.slice(2);
const plan = JSON.parse(planS || '{"minOdds":1.5}');
const seen = new Set<number>();
type Row = { n: number; won: number; attLost: number; defLost: number; odds: number; day: number };
const tab: Record<string, Row> = {};
const won = (r: any): boolean => !!(r.taken || r.siege || r.gates);
const band = (o: number): string => (o < 1 ? '0 under 1' : o < 1.5 ? '1 1 to 1.5' : o < 2 ? '2 1.5 to 2' : o < 3 ? '3 2 to 3' : '4 3 or more');
const card = (r: any): number => (r.dPower > 0 ? Math.min(99, r.aPower / r.dPower) : 99);
const add = (k: string, r: any): void => { const t = (tab[k] ??= { n: 0, won: 0, attLost: 0, defLost: 0, odds: 0, day: 0 }); t.n++; if (won(r)) t.won++; t.attLost += r.attLost; t.defLost += r.defLost; t.odds += card(r); t.day += Math.min(99, r.odds); };
let held64 = '?', quiet = 0, longest = 0, longestEnd = 0, lastTurn = 0;
let quietLate = 0, longestLate = 0, longestLateEnd = 0;   // Version 63: the same count from January 1942 on, so that the lull before the eastern war does not hide a silent front
const wornBy = { mine: [0, 0], theirs: [0, 0] };   // Version 61: attacks thrown back that still wore the works, and the months they took off
const myAttacksByYear: Record<number, number> = {};
// Version 63: the player's defences (how many on a prepared line, met tired, against guns), the men he lost in rings, and how much of his line stood in works.
const defs = { n: 0, lost: 0, prep: 0, prepLost: 0, tired: 0, guns: 0 };
const rings = { closed: 0, men: 0, gaveUp: 0, gaveUpMen: 0 };
const lineLog: string[] = [];
let seenHl = 0;
const num = (t: string): number => Number(t.replace(/,/g, ''));
const hold = (g: any): string => { const r = g.regions.find((x: any) => x.name === town); return r ? r.ctrl : `no province ${town}`; };
const atk64: Record<number, { n: number; won: number; mine: number; theirs: number; prep: number; prepWon: number }> = {};
const took64: Record<string, number> = {};
const goals64 = me === 'GER' ? ['Moscow', 'Leningrad', 'Kiev', 'Stalingrad'] : ['Berlin', 'Warsaw', 'Rome', 'Paris'];
const run = campaign(me, Number(fromS), Number(seedS), { ...plan, each: (g: any) => {
  lastTurn = g.turn;
  for (const nm of goals64) if (!(nm in took64)) { const r = g.regions.find((x: any) => x.name === nm); if (r && r.ctrl === me && r.owner !== me) took64[nm] = g.turn; }
  let fought = 0;
  for (const r of g.reports) {
    if (seen.has(r.id)) continue;
    seen.add(r.id);
    if (r.yielded || r.town) continue;
    const iAttack = r.attNat === me || r.attackers.some((u: any) => u.nat === me), iDefend = r.defNat === me || r.defenders.some((u: any) => u.nat === me);
    if (!iAttack && !iDefend) continue;
    fought++;
    if (typeof r.worn === 'number') { const w = iAttack ? wornBy.mine : wornBy.theirs; w[0]++; w[1] += r.worn; }
    if (iAttack) { const y = 1939 + Math.floor((g.turn + 7) / 12); myAttacksByYear[y] = (myAttacksByYear[y] ?? 0) + 1; }
    if (iAttack) { const y = 1939 + Math.floor((g.turn + 7) / 12); const a = (atk64[y] = atk64[y] ?? { n: 0, won: 0, mine: 0, theirs: 0, prep: 0, prepWon: 0 }); a.n++; if (won(r)) a.won++; a.mine += r.attLost ?? 0; a.theirs += r.defLost ?? 0; if ((r.prepared ?? 0) > 0) { a.prep++; if (won(r)) a.prepWon++; } }
    const side = iAttack ? 'A the player attacks' : 'B the player is attacked', prep = (r.prepared ?? 0) > 0 ? 'prepared line' : 'open';
    if (!iAttack) {
      defs.n++; if (won(r)) defs.lost++;
      if ((r.prepared ?? 0) > 0) { defs.prep++; if (won(r)) defs.prepLost++; }
      if ((r.lines ?? []).some((l: any) => l.label === 'Earlier attacks')) defs.tired++;
      if (r.attackers.some((u: any) => u.kind === 'art')) defs.guns++;
    }
    if (process.env.ROWS) console.log(`ROW ${g.turn} ${iAttack ? 'A' : 'B'} ${r.attNat} ${r.defNat} ${(r.prepared ?? 0).toFixed(2)} ${card(r).toFixed(3)} ${Math.min(99, r.odds).toFixed(3)} ${won(r) ? 1 : 0} ${r.attLost} ${r.defLost} ${r.attackers.length} ${r.defenders.length} ${r.attackers.filter((u: any) => u.kind === 'art').length} ${String(r.regionName).replace(/ /g, '_')}`);
    add(`${side} | ${prep} | all odds`, r);
    add(`${side} | ${prep} | ${band(card(r))}`, r);
  }
  const adj = g.nations[me].adj;
  for (const h of g.headlines.slice(seenHl)) {
    const t: string = h.text;
    if (!t.includes(adj)) continue;
    let m = t.match(/^The ring closes at .*?: (\d+) .*? corps, ([\d,]+) men/);
    if (m) { rings.closed++; rings.men += num(m[2]); continue; }
    m = t.match(/^([\d,]+) .*? troops surrender at/);
    if (m && t.includes(` ${adj} troops`)) { rings.gaveUp++; rings.gaveUpMen += num(m[1]); continue; }
    m = t.match(/lays down its arms: \d+ corps, ([\d,]+) men/);
    if (m) { rings.gaveUp++; rings.gaveUpMen += num(m[1]); }
  }
  seenHl = g.headlines.length;
  if (g.turn >= 22 && (g.turn - 5) % 6 === 0) {
    const hostile = (id: number): boolean => g.regions[id].units.some((u: any) => !u.dead && g.nations[me].atWar.includes(u.nat));
    const line = g.corps.filter((c: any) => c.nat === me && !c.dead && c.kind !== 'fort' && c.town === undefined && g.regions[c.reg].nb.some(hostile));
    lineLog.push(`${date(g.turn).replace(/^(...)\w* /, '$1 ')} ${line.filter((c: any) => c.dug).length}/${line.length}`);
  }
  if (fought) quiet = 0; else if (g.nations[me].atWar.length) { quiet++; if (quiet > longest) { longest = quiet; longestEnd = g.turn; } }
  if (g.turn >= 29) { if (fought) quietLate = 0; else if (g.nations[me].atWar.length) { quietLate++; if (quietLate > longestLate) { longestLate = quietLate; longestLateEnd = g.turn; } } }
  if (g.turn === 64) held64 = hold(g);
  if ((g.turn - 5) % 12 === 0) {
    const field = g.corps.filter((c: any) => c.nat === me && !c.dead && c.kind !== 'fort' && c.town === undefined);
    console.log(`${date(g.turn)}: field corps ${field.length}, dug in ${field.filter((c: any) => c.dug).length}, works finished ${field.filter((c: any) => c.dug && (c.dugFor ?? 0) >= 3).length}; provinces ${g.regions.filter((x: any) => x.ctrl === me).length}; at war with ${g.nations[me].atWar.length}`);
  }
} }, { until: Number(untilS), quiet: true });
for (const e of run.errors) console.log('ERROR', String(e).slice(0, 600));
for (const k of Object.keys(tab).sort()) {
  const t = tab[k];
  console.log(`BATTLES ${k.padEnd(52)} ${String(t.n).padStart(4)} fought | attacker won ${String(t.won).padStart(4)} (${Math.round((100 * t.won) / t.n)}%) | mean odds ${(t.odds / t.n).toFixed(2)} | on the day ${(t.day / t.n).toFixed(2)} | lost a battle: attacker ${(t.attLost / t.n).toFixed(1)}, defender ${(t.defLost / t.n).toFixed(1)}`);
}
const R = run as any;
console.log(`TOTAL ${me} seed ${seedS}: played to ${date(lastTurn)}${run.fellOn ? ` (fell ${run.fellOn})` : ''}; ${town} in December 1944: ${held64}; in the last month: ${hold(run.g)}; longest stretch at war with no battle of the player's: ${longest} months, ending ${longest ? date(longestEnd) : '-'}; thrown back but wore the works: my attacks ${wornBy.mine[0]} (${wornBy.mine[1].toFixed(1)} months), attacks on me ${wornBy.theirs[0]} (${wornBy.theirs[1].toFixed(1)} months); attacks under my bar to wear a line ${(run as any).grinds ?? 0}${(run as any).secondBlows !== undefined ? `, second blows on a line struck that month ${(run as any).secondBlows} (won ${(run as any).secondWon ?? 0})` : ''}; my field attacks by year ${Object.entries(myAttacksByYear).map(([y, n]) => `${y} ${n}`).join(', ')}; provinces at the end ${run.g.regions.filter((x: any) => x.ctrl === me).length}; defences ${defs.n}, lost ${defs.lost}, on a prepared line ${defs.prep} (lost ${defs.prepLost}), met tired ${defs.tired}, against guns ${defs.guns}; rings closed on the player ${rings.closed} (${rings.men} men), surrenders ${rings.gaveUp} (${rings.gaveUpMen} men); works kept from attacks ${(run as any).dugKept ?? 0}; longest silence from January 1942: ${longestLate} months${longestLate ? `, ending ${date(longestLateEnd)}` : ''}; line corps in works: ${lineLog.join(', ')}; took ${Object.entries(took64).map(([k, v]) => `${k} ${date(v)}`).join(', ') || 'none of ' + goals64.join(', ')}; attacks by year (fought, won, on a prepared line won of fought, my men, theirs): ${Object.entries(atk64).map(([y, a]) => `${y} ${a.n}, ${a.won}, ${a.prepWon} of ${a.prep}, ${a.mine}, ${a.theirs}`).join(' | ')}; Version 64: folds ${R.folds ?? 0} (in the line ${R.foldsInLine ?? 0}, men back ${R.foldBack ?? 0}), cadres raised ${R.cadresRaised ?? 0}; reliefs ${R.rotations ?? 0} (works found by ${R.rotKept ?? 0}, months handed over ${(R.rotWorksAfter ?? 0).toFixed(1)} of ${(R.rotWorksBefore ?? 0).toFixed(1)}; no relief near ${R.rotNone ?? 0}, no road ${R.rotNoRoad ?? 0}, half done ${R.rotHalf ?? 0}); probes ${R.probes ?? 0} (men ${R.probeMen ?? 0}, months off ${(R.probeWear ?? 0).toFixed(1)}, corps named ${R.probeFresh ?? 0}); line raids ${R.lineRaids ?? 0} (groups ${R.lineRaidGroups ?? 0}, months off ${(R.lineRaidWear ?? 0).toFixed(1)}, refused ${R.lineRaidsRefused ?? 0}); softened ${R.softN ?? 0} lines, odds ${(R.softN ? (R.softOddsBefore ?? 0) / R.softN : 0).toFixed(2)} to ${(R.softN ? (R.softOddsAfter ?? 0) / R.softN : 0).toFixed(2)}, lifted over the bar ${R.softLifted ?? 0}`);
