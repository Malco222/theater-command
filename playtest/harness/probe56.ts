// Version 68 on: can a player in this seat win World Domination? usage: probe56 ME FROM SEED UNTIL PLAN-JSON
// Brian, 2026-11-15: "I also want each faction to have an actual chance to global domination". The first target told
// to him is about one war in ten at Colonel for a careful player.
// Plays the scripted player's war from any seat (the same plans as probe54), and as soon as the rules call the war
// between the alliances decided (a great power has fallen and no two great powers still standing are at war) orders
// World Domination: every alliance is dissolved and the last nation standing wins. Plan option "dominate":false takes
// the victory and plays on in peace instead (the control). "giveUp": N stops a war that is still undecided at turn N
// (default 88, December 1946), so that a queue does not spend its time on wars that cannot be decided.
// "landLog": true also prints why the player's landings were not made (the last lines of the landing log and a count by reason).
// Version 72: "cross": true puts the rules builder's crossing routine (prepareCrossing) in the player's hands, "crossFoe" names the
// one enemy to cross to; a CROSS line then says what came of it (the first month ashore on that enemy's homeland, the months
// ashore, the most corps and provinces there, every landing ordered, the lift at the end) and "crossLog": true prints the routine's own log.
// Prints one DOM line a war: when the war was decided and for whom, when the last act began and with how many
// nations, how it ended (WON, the player fell, or how many nations were left), and how near the player came: the
// share of the map's worth held at the end and at the most, the strongest rival, the nations beaten after the last
// act began, and who was left. Tables: domtab.py.
import * as NV from '@tc/campaign/navy';
import { campaign, date, T } from './lib';
const [me, fromS, seedS, untilS, planS] = process.argv.slice(2);
const plan = JSON.parse(planS || '{}');
const X = T as any;
const giveUp = Number(plan.giveUp ?? 88);
const MAJORS = ['GER', 'ITA', 'UK', 'FRA', 'SOV', 'USA', 'JAP'];
const AXIS = new Set(['GER', 'ITA', 'JAP', 'HUN', 'ROM', 'BUL', 'FIN', 'SVK']);
let decidedAt = 0, victor = '', beaten = '', standingThen = '', domAt = 0, domN = 0, refused = '', lastTurn = 0, peak = 0, peakAt = 0, shareAtDom = 0, leftAtDom = 0, peaceAt = 0;
const byDec: string[] = [];
const fellAfter: string[] = [];
const seenFall = new Set<string>();
let aliveThen: string[] = [];
let crossFoe = '', crossFirst = 0, crossMonths = 0, crossMost = 0, crossHeld = 0, crossRun: any = null, liftMost = 0;
const share = (g: any): { mine: number; rows: any[]; left: number; winner: string | null } => { const b = X.standings(g); const row = b.rows.find((r: any) => r.nat === me); return { mine: row ? row.share : 0, rows: b.rows, left: b.left, winner: b.winner }; };
const run = campaign(me, Number(fromS), Number(seedS), { ...plan,
  first: (g: any, r0: any) => {
    // Version 73: a month, two and three after each landing: is the sea the card spoke of closed, is the force fed, is it still there?
    for (const e of ((r0 ?? crossRun)?.holdLog ?? []) as any[]) {
      const age = g.turn - e.turn;
      if (age < 1 || age > 3) continue;
      const land = g.regions[e.beach].owner;
      const mine = g.corps.filter((c: any) => c.nat === me && !c.dead && c.kind !== 'fort' && g.regions[c.reg].owner === land && !g.regions[c.reg].overseas);
      (e.after ??= []).push({ age, closed: (NV as any).closedTo(g, e.sea, me), there: mine.length, unfed: mine.filter((c: any) => c.supplied === false).length });
    }
    const la = X.lastAct?.(g);
    if (!la) return;
    if (la.decided && !decidedAt) { decidedAt = g.turn; victor = la.decided.victor ?? 'nobody'; beaten = la.decided.beaten.join('+'); standingThen = la.decided.standing.join(','); }
    if (la.open && !la.on) {
      if (plan.dominate === false) { if (!peaceAt && X.orderKeepPeace?.(g)) peaceAt = g.turn; }
      else if (X.orderDomination(g)) { domAt = g.turn; const s = share(g); domN = s.left; shareAtDom = s.mine; leftAtDom = s.left; aliveThen = s.rows.map((r: any) => r.nat); }
      else if (!refused) refused = String(X.whyNotOrderDomination(g));
    }
    if (!decidedAt && g.turn >= giveUp) throw new Error('GIVEUP');
  },
  each: (g: any, r: any) => {
    lastTurn = g.turn;
    if (r) crossRun = r;
    liftMost = Math.max(liftMost, g.nations[me]?.sealift ?? 0);
    const foe = crossRun?.crossFoe ?? plan.crossFoe;
    if (plan.cross && foe) {
      crossFoe = foe;
      const there = g.corps.filter((c: any) => c.nat === me && !c.dead && c.kind !== 'fort' && g.regions[c.reg].owner === foe && !g.regions[c.reg].overseas).length;
      const held = g.regions.filter((x: any) => x.owner === foe && !x.overseas && x.ctrl === me).length;
      if (there || held) { crossMonths++; if (!crossFirst) crossFirst = g.turn; }
      crossMost = Math.max(crossMost, there); crossHeld = Math.max(crossHeld, held);
    }
    const s = share(g);
    if (s.mine > peak) { peak = s.mine; peakAt = g.turn; }
    if (g.turn % 12 === 4) byDec.push(`${1939 + Math.floor((g.turn + 7) / 12)} ${Math.round(s.mine * 100)}%`);
    if (domAt) for (const id of aliveThen) { const n = g.nations[id]; if (!n.alive && !seenFall.has(id)) { seenFall.add(id); fellAfter.push(`${id} ${n.fellOn ? `${date(n.fellOn.turn)}→${n.fellOn.to}` : `${date(g.turn)} left the map`}`); } }
  },
} as any, { until: Number(untilS), quiet: true } as any);
const gaveUp = run.errors.some((e) => String(e).includes('GIVEUP'));
for (const e of run.errors) if (!String(e).includes('GIVEUP')) console.log('ERROR', String(e).slice(0, 600));
const g: any = run.g, N = g.nations, M = N[me];
const s = share(g);
const stands = !!M.alive;
const side = AXIS.has(me) ? 'axis' : 'allies';
const won = s.winner === me;
const rival = s.rows.filter((r: any) => r.nat !== me)[0];
const rank = stands ? s.rows.findIndex((r: any) => r.nat === me) + 1 : 0;
const fallen = MAJORS.filter((p) => N[p] && N[p].fellOn).map((p) => `${p} ${date(N[p].fellOn.turn)}${N[p].alive ? ' (back)' : ''}`);
// Why the war was never decided: the great powers still standing and at war with each other.
const live = ['GER', 'ITA', 'UK', 'FRA', 'SOV'].filter((p) => N[p]?.alive);
const pairs: string[] = [];
for (let i = 0; i < live.length; i++) for (let j = i + 1; j < live.length; j++) if (N[live[i]].atWar.includes(live[j])) pairs.push(`${live[i]}-${live[j]}`);
const rivals71 = live.filter((p) => p !== me && !N[p].dormant);
const end = won ? `WON ${date(g.flags['dom:won'] ?? lastTurn)}` : !stands ? `the player fell ${date(M.fellOn?.turn ?? lastTurn)} to ${M.fellOn?.to ?? '?'}` : `played to ${date(lastTurn)}${gaveUp ? ' (given up: undecided)' : ''}: ${s.left} nations left`;
console.log(`DOM ${me} seed ${seedS} (${plan.depth ? plan.depth : 'by hand'}${plan.idle ? ', idle' : ''}): war decided ${decidedAt ? `${date(decidedAt)} for ${victor} (beaten ${beaten}; standing ${standingThen || 'none'}); the player's side ${victor === side ? 'won' : 'did not win'}` : `never (great powers fallen: ${fallen.join(', ') || 'none'}; still at war: ${pairs.join(', ') || 'no two'})`}`
  + `; last act ${domAt ? `begun ${date(domAt)} with ${domN} nations, the player holding ${Math.round(shareAtDom * 100)}%` : peaceAt ? `declined ${date(peaceAt)}` : refused ? `refused: ${refused}` : 'never open'}`
  + `; ${end}`
  + `; share of the map's worth at the end ${Math.round(s.mine * 100)}% (rank ${rank || '-'} of ${s.left}), at the most ${Math.round(peak * 100)}% in ${peakAt ? date(peakAt) : '-'}`
  + `; strongest rival ${rival ? `${rival.nat} ${Math.round(rival.share * 100)}% and ${Math.round(rival.divisions)} divisions` : 'none'}; the player's army ${Math.round(s.rows.find((r: any) => r.nat === me)?.divisions ?? 0)} divisions`
  + `; fell after the last act began ${fellAfter.length}${fellAfter.length ? ` (${fellAfter.slice(0, 40).join(', ')})` : ''}`
  + `; left at the end: ${s.rows.map((r: any) => `${r.nat} ${Math.round(r.share * 100)}%`).join(', ')}`
  + `; share each December: ${byDec.join(', ')}`
  + `; by the rule of Version 71 (the last great power standing, holding three quarters of the map's worth): rival great powers left ${rivals71.join(' ') || 'none'}, the player ${stands ? `holds ${Math.round(s.mine * 100)}%` : 'has fallen'}`);
if (plan.cross) {
  const log: string[] = (run as any).crossLog ?? [];
  const landed = log.filter((l) => l.includes('LANDED'));
  const ashore = landed.filter((l) => l.includes(': ashore')).length;
  const firstLine = log[0] ?? '';
  const lowest = log.map((l) => { const m = /caught ([\d.]+)/.exec(l); return m ? Number(m[1]) : 9; }).reduce((a, b) => Math.min(a, b), 9);
  const barred = log.filter((l) => l.includes('BARRED')).length;
  // Month by month: was the way open at a danger the player bears, and did a force stand ready on the quay?
  const bears = Number(plan.landRisk ?? 0.25);
  let openM = 0, readyM = 0, bothM = 0, quays = new Set<string>(), switches = 0, lastQuay = '';
  for (const l of log) {
    const m = /caught ([\d.]+)/.exec(l), r = /ready (\d+) of (\d+)/.exec(l), q = /^\w+ \d{4} (.+?) > /.exec(l);
    if (!m || !r) continue;
    const open = Number(m[1]) <= bears && !l.includes('BARRED'), ready = Number(r[1]) >= Math.min(Number(r[2]), 2);
    if (open) openM++; if (ready) readyM++; if (open && ready) bothM++;
    if (q) { quays.add(q[1]); if (lastQuay && q[1] !== lastQuay) switches++; lastQuay = q[1]; }
  }
  console.log(`CROSS ${me} seed ${seedS} against ${crossFoe || plan.crossFoe || 'any'}: ${log.length ? `crossing prepared from ${firstLine.split(' ').slice(0, 2).join(' ')} for ${log.length} months (${barred} barred), the lowest danger ${lowest < 9 ? lowest.toFixed(2) : '-'}` : 'no crossing was ever on offer'}; the way open at a danger the player bears in ${openM} months, a force ready on the quay in ${readyM}, both in ${bothM}; the plan changed quay ${switches} times (${[...quays].join(', ')}); landings ordered ${landed.length}, ashore ${ashore}; first ashore ${crossFirst ? date(crossFirst) : 'never'}, months ashore ${crossMonths}, most corps there ${crossMost}, most provinces held ${crossHeld}; ${crossFoe && g.nations[crossFoe] ? (g.nations[crossFoe].alive ? `${crossFoe} stands` : `${crossFoe} fell ${g.nations[crossFoe].fellOn ? date(g.nations[crossFoe].fellOn.turn) : ''}`) : ''}; lift at the end ${g.nations[me].sealift}, at the most ${liftMost}; landing risk borne ${plan.landRisk ?? 0.25}`);
  // Version 73: what the landing card said of the sea behind the beach before each landing the routine ordered.
  const told = landed.filter((l) => l.includes('the sea behind:'));
  if (told.length) console.log(`HOLD ${me} seed ${seedS}: before ${told.length} landings the card said the sea behind the beach would be closed in ${told.filter((l) => l.includes('would be closed')).length} and would stay open in ${told.filter((l) => l.includes('stays open')).length}`);
  for (const l of landed.slice(0, 12)) console.log('   ' + l);
  if (plan.crossLog) for (const l of log) console.log('  LOG ' + l);
}
{
  const hl: any[] = (run as any).holdLog ?? [];
  for (const e of hl) {
    const af: any[] = e.after ?? [];
    const one = af.find((x) => x.age === 1);
    console.log(`HELD ${me} seed ${seedS}: ${date(e.turn)} ${e.name} with ${e.corps} corps: the card said the ${run.g.seas[e.sea].name} ${e.open ? 'stays open' : 'would be closed'} (enemy ${e.foe}, own ${e.own}, need ${e.need}${e.port ? '' : ', no harbour'}); a month later it was ${one ? (one.closed ? 'CLOSED' : 'open') : '?'}, the force ${one ? (one.there ? (one.unfed ? `unfed (${one.unfed} of ${one.there} corps)` : 'fed') : 'gone') : '?'}; closed in any of the three months after: ${af.some((x) => x.closed) ? 'yes' : 'no'}; corps on that ground after 1, 2, 3 months: ${[1, 2, 3].map((k) => af.find((x) => x.age === k)?.there ?? '?').join(', ')}`);
  }
}
if (plan.landLog) {
  const log: string[] = (run as any).landLog ?? [];
  const why: Record<string, number> = {};
  for (const l of log) { const k = l.replace(/^\w+ \d{4} /, '').replace(/^.*?: /, '').replace(/[\d.]+/g, 'N').slice(0, 70); why[k] = (why[k] ?? 0) + 1; }
  console.log(`LANDINGS ${log.length} lines; by reason: ${Object.entries(why).sort((a, b) => b[1] - a[1]).slice(0, 8).map(([k, v]) => `${v}x ${k}`).join(' | ')}`);
  for (const l of log.slice(-10)) console.log('   ' + l);
  const rows = (run as any).rows as any[];
  const notes = rows.flatMap((r) => r.notes.filter((x: string) => x.startsWith('landing')).map((x: string) => `${r.date}: ${x}`));
  console.log(`LANDED ${notes.length}: ${notes.slice(-6).join(' | ')}`);
  console.log(`SEALIFT ${g.nations[me].sealift}; ports held ${g.regions.filter((r: any) => r.port && r.ctrl === me).length}`);
}
