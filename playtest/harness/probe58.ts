// Version 70: the history dial from a player's seat. usage: probe58 ME FROM SEED UNTIL PLAN-JSON   (the plan's "stop"
// sets the dial, 1 By the book to 5 Anything goes; build with build70.sh)
// probe57 (the SEAT line with par) and one DIAL line a war: when the great attacks fell (the west, Norway, Italy's
// war, Greece, Yugoslavia, the east), where history's book was closed and why, which claims were pressed and marched
// for and how long the warning was (the rules promise two months), who stood by whom, treaties, who joined which
// alliance, and for claims on the player's own ground the month each could first be read on the Powers tab, the month
// it was pressed and the month it was marched for.
import { campaign, date, T } from './lib';
const [me, fromS, seedS, untilS, planS] = process.argv.slice(2);
const plan = JSON.parse(planS || '{}');
const X = T as any;
const WARS: [string, string, string][] = [['west', 'GER', 'BEL'], ['nor', 'GER', 'NOR'], ['ita', 'ITA', 'UK'], ['gre', 'ITA', 'GRE'], ['yug', 'GER', 'YUG'], ['east', 'GER', 'SOV']];
const firstAt: Record<string, number> = {};
const dialNotes: string[] = [], bookNotes: string[] = [];
const pressAt: Record<string, number> = {};
const marches: { key: string; at: number; gap: number | null }[] = [];
const claimSeen: Record<string, { claims: number; pressing: number; marched: number; nat: string; name: string }> = {};
let seenHead = 0, ids: Record<string, string> | null = null, decidedAt = 0, decidedWhy = '';
const bare = (x: string): string => x.replace(/^the /i, '').trim();
const scan = (g: any): void => {
  if (!ids) { ids = {}; for (const n of Object.values(g.nations) as any[]) ids[bare(n.name)] = n.id; }
  for (const h of g.headlines.slice(seenHead)) {
    const x: string = h.text; let m: RegExpExecArray | null;
    if ((m = /^(.+?) declares war on (.+?)\.$/.exec(x))) { const a = ids[bare(m[1])], b = ids[bare(m[2])]; for (const [k, p, q] of WARS) if (!firstAt[k] && ((a === p && b === q) || (a === q && b === p))) firstAt[k] = h.turn; }
    else if ((m = /^History's book is closed in (.+?): (.+?)\./.exec(x))) bookNotes.push(`${m[1]} ${date(h.turn)} (${m[2]})`);
    else if ((m = /^(.+?) presses its claim to (.+?):/.exec(x))) { const k = `${m[1]}>${m[2]}`; if (!pressAt[k]) pressAt[k] = h.turn; dialNotes.push(`press ${k} ${date(h.turn)}`); }
    else if ((m = /^(.+?) marches for (.+?):/.exec(x))) { const k = `${m[1]}>${m[2]}`; const gap = pressAt[k] ? h.turn - pressAt[k] : null; marches.push({ key: k, at: h.turn, gap }); dialNotes.push(`MARCH ${k} ${date(h.turn)} (${gap === null ? 'no warning' : `${gap} months after the warning`})`); }
    else if ((m = /^(.+?) stands by (.+?):/.exec(x))) dialNotes.push(`aid ${m[1]}>${m[2]} ${date(h.turn)}`);
    else if ((m = /^(.+?) and (.+?) sign a treaty: (.+?) is/.exec(x))) dialNotes.push(`TREATY ${m[1]}/${m[2]}: ${m[3]} ${date(h.turn)}`);
    else if ((m = /^(.+?) joins (the Axis|the Allies)\./.exec(x))) dialNotes.push(`join ${m[1]}>${m[2].slice(4)} ${date(h.turn)}`);
    else if (/^Nobody has read the history books/.test(x)) dialNotes.push(`DEAL ${date(h.turn)}`);
  }
  seenHead = g.headlines.length;
  if (!decidedAt) { let la: any = null; try { la = X.lastAct?.(g); } catch { la = null; } if (la?.decided) { decidedAt = g.turn; decidedWhy = String(la.why ?? ''); } }
  for (const [k, a, b] of WARS) if (!firstAt[k] && g.nations[a]?.atWar.includes(b)) firstAt[k] = g.turn;
  for (const c of (X.claimsOf?.(g) ?? []) as any[]) if (c.onYou) { const e = (claimSeen[c.id] ??= { claims: 0, pressing: 0, marched: 0, nat: c.nat, name: c.name }); if (!(e as any)[c.state]) (e as any)[c.state] = g.turn; }
};
const POWERS = ['GER', 'ITA', 'UK', 'FRA', 'SOV', 'USA', 'JAP'];
const AXIS = new Set(['GER', 'ITA', 'JAP', 'HUN', 'ROM', 'BUL', 'FIN']);
const fell: Record<string, number> = {};
const seen = new Set<number>();
let atWarMonths = 0, foughtMonths = 0, lastTurn = 0, firstWar = 0, quiet = 0, longest = 0, longestEnd = 0, began = 0, most = 0;
const WEST = new Set(['FRA', 'BEL', 'NED', 'LUX', 'GER', 'DEN']);
const capAt: Record<number, string> = {};
const seaHeld: string[] = []; let seaAll = 0;
let capLost = 0, capBack = 0, homeLostMost = 0, homeMonths = 0, ashoreFrom = 0, ashoreFirst = 0, invaded = 0, least = 9999, gerFellSeen = 0;
let eastAt = 0, itaWar = 0, parisOct: boolean | null = null, provFrom = 0;
const run = campaign(me, Number(fromS), Number(seedS), { ...plan, first: (g: any) => {
  scan(g);
  if (parisOct === null && g.turn >= 14 && g.nations.FRA) parisOct = !!g.nations.FRA.alive && g.regions[g.nations.FRA.capital]?.ctrl === 'FRA';
}, each: (g: any) => {
  scan(g);
  lastTurn = g.turn;
  let fought = 0;
  for (const r of g.reports) {
    if (seen.has(r.id)) continue; seen.add(r.id);
    if (r.yielded || r.town) continue;
    if (r.attNat === me || r.defNat === me || r.attackers.some((u: any) => u.nat === me) || r.defenders.some((u: any) => u.nat === me)) fought++;
  }
  // (A power not yet in the war is dormant, not fallen: only a recorded fall counts.)
  for (const p of POWERS) { const n = g.nations[p]; if (n && n.fellOn && !fell[p]) fell[p] = n.fellOn.turn; }
  if (!eastAt && g.nations.SOV?.atWar.includes('GER')) eastAt = g.turn;
  if (!itaWar && g.nations.ITA?.atWar.includes('UK')) itaWar = g.turn;
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
      const prov = g.regions.filter((x: any) => x.ctrl === me && x.owner !== me && WEST.has(x.owner) && !x.overseas && x.name !== 'Corsica').length;
      if (prov) { if (!provFrom) provFrom = g.turn; } else provFrom = 0;
      const west = g.corps.filter((c: any) => c.nat === me && !c.dead && c.kind !== 'fort' && g.regions[c.reg].owner !== me && WEST.has(g.regions[c.reg].owner) && !g.regions[c.reg].overseas && g.regions[c.reg].name !== 'Corsica').length;
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
let par = false, old = false, parWhy = '';
const fraFell = fell.FRA ?? 0;
if (me === 'GER') { old = capAt[64] === 'GER'; par = old && !!fraFell; parWhy = `Berlin at the end of 1944: ${capAt[64] === 'GER' ? 'held' : capAt[64] ? `held by ${capAt[64]}` : 'Germany had fallen'}; France ${fraFell ? `fell ${date(fraFell)}` : 'never beaten'}`; }
else if (me === 'SOV') { old = stands && !!gerFell && gerFell < 69 && !capLost; par = stands && !!gerFell && !!eastAt && gerFell - eastAt < 47 && !capLost; parWhy = `Germany ${gerFell ? `fell ${date(gerFell)}${eastAt ? `, ${gerFell - eastAt} months after the invasion` : ''}` : 'stands'}; the east opened ${eastAt ? date(eastAt) : 'never'}; Moscow ${capLost ? `lost ${date(capLost)}${capBack ? `, back ${date(capBack)}` : ''}` : 'never lost'}`; }
else if (me === 'UK') { par = stands && !!ashoreFrom && ashoreFrom < 58 && !invaded; old = par; parWhy = `ashore to stay ${ashoreFrom ? `from ${date(ashoreFrom)}` : 'never'}${ashoreFirst && ashoreFirst !== ashoreFrom ? ` (first ashore ${date(ashoreFirst)})` : ''}; ${invaded ? `invaded ${date(invaded)}` : 'never invaded'}; Germany ${gerFell ? `fell ${date(gerFell)}` : 'stands'}; by provinces held ${provFrom ? `from ${date(provFrom)}` : 'never'}`; }
else if (me === 'FRA') { old = capAt[16] === 'FRA'; par = parisOct === true; parWhy = `Paris as October 1940 opens: ${parisOct === true ? 'held' : 'lost'}; at the end of 1940: ${capAt[16] === 'FRA' ? 'held' : 'lost'}`; }
else if (me === 'ITA') { old = capAt[52] === 'ITA'; par = old && itaWar > 0 && itaWar <= 16; parWhy = `Rome at the end of 1943: ${capAt[52] === 'ITA' ? 'held' : capAt[52] ? `held by ${capAt[52]}` : 'Italy had fallen'}; at war with Britain ${itaWar ? `from ${date(itaWar)}` : 'never'}; Germany ${gerFell ? `fell ${date(gerFell)}` : 'stands'}`; }
const levies = Number(g.flags?.['lv:SOV'] ?? 0);
const near = `home provinces in enemy hands at the worst ${homeLostMost}, an enemy on home soil in ${homeMonths} months${invaded ? ` from ${date(invaded)}` : ''}, fewest provinces held ${least === 9999 ? 0 : least}; capital ${capLost ? `first lost ${date(capLost)}${capBack ? `, back ${date(capBack)}` : ''}` : 'never lost while the country stood'}; Soviet levies ${levies}; overseas provinces held each December of ${seaAll}: ${seaHeld.join(', ') || '-'}`;
const fallen = POWERS.filter((p) => fell[p]).map((p) => `${p} ${date(fell[p])}${N[p]?.alive ? ' (back)' : ''}`).join(', ') || 'none';
const standing = POWERS.filter((p) => N[p] && N[p].alive).map((p) => (N[p].dormant ? `${p} (not in the war)` : p)).join(' ') || 'none';
console.log(`SEAT ${me} seed ${seedS}: played to ${date(lastTurn)}; ${stands ? 'stands' : `fell ${date(myFall)}`}; result ${result}${result === 'won' ? ` (${chief} fell ${date(chiefFell)})` : ''}; capital ${cap?.name ?? '?'} held by ${cap?.ctrl ?? '?'}; great powers fallen: ${fallen}; standing: ${standing}; at war ${atWarMonths} months${firstWar ? ` from ${date(firstWar)}` : ''}, a battle of the player's in ${foughtMonths}, longest silence ${longest} months${longest ? ` ending ${date(longestEnd)}` : ''}; provinces at the start ${began}, most ${most}, at the end ${g.regions.filter((x: any) => x.ctrl === me).length}; par ${par ? 'BEATEN' : 'missed'} (${parWhy}); ${near}; depth ${plan.depth ?? 'by hand'}; old line ${old ? 'beaten' : 'missed'}; the east opened ${eastAt ? date(eastAt) : 'never'}; Italy at war with Britain ${itaWar ? `from ${date(itaWar)}` : 'never'}`);
scan(g);
const stopNow = Number((globalThis as any).__tcStopOf?.(g) ?? 3);
const dd = (k: string): string => (firstAt[k] ? date(firstAt[k]) : 'never');
const ok2 = marches.filter((m) => m.gap !== null && m.gap >= 2).length, short2 = marches.filter((m) => m.gap !== null && m.gap < 2).length, none2 = marches.filter((m) => m.gap === null).length;
const mine = Object.values(claimSeen).map((c) => `${c.name} by ${c.nat}: readable ${c.claims ? date(c.claims) : 'never before it was pressed'}, pressed ${c.pressing ? date(c.pressing) : 'never'}, marched ${c.marched ? date(c.marched) : 'never'}`);
const MAJ = ['GER', 'ITA', 'UK', 'FRA', 'SOV'].filter((p) => g.nations[p]?.alive);
const stillWar: string[] = []; for (let i = 0; i < MAJ.length; i++) for (let j = i + 1; j < MAJ.length; j++) if (g.nations[MAJ[i]].atWar.includes(MAJ[j])) stillWar.push(`${MAJ[i]}-${MAJ[j]}`);
const ended = decidedAt ? `war decided ${date(decidedAt)}` : `war NOT decided by ${date(g.turn)} (great powers still at war: ${stillWar.join(' ') || 'none'})`;
const amb = Object.keys(g.flags).filter((k) => k.startsWith('amb:')).map((k) => `${k}=${g.flags[k]}`);
console.log(`DIAL ${me} seed ${seedS} stop ${stopNow} from ${date(Number(fromS))}: west ${dd('west')} | nor ${dd('nor')} | ita ${dd('ita')} | gre ${dd('gre')} | yug ${dd('yug')} | east ${dd('east')} | ${ended} | book closed: ${bookNotes.join('; ') || 'nowhere'} | marches ${marches.length}: warned two months or more ahead ${ok2}, less ${short2}, not at all ${none2} | ${dialNotes.join('; ') || 'no claims, treaties or joiners'} | claims on the player: ${mine.join('; ') || 'none'} | flags: ${amb.slice(0, 30).join(' ') || '-'}`);
