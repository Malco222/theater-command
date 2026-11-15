// Version 68 on: can a player in this seat win World Domination? usage: probe56 ME FROM SEED UNTIL PLAN-JSON
// Brian, 2026-11-15: "I also want each faction to have an actual chance to global domination". The first target told
// to him is about one war in ten at Colonel for a careful player.
// Plays the scripted player's war from any seat (the same plans as probe54), and as soon as the rules call the war
// between the alliances decided (a great power has fallen and no two great powers still standing are at war) orders
// World Domination: every alliance is dissolved and the last nation standing wins. Plan option "dominate":false takes
// the victory and plays on in peace instead (the control). "giveUp": N stops a war that is still undecided at turn N
// (default 88, December 1946), so that a queue does not spend its time on wars that cannot be decided.
// "landLog": true also prints why the player's landings were not made (the last lines of the landing log and a count by reason).
// Prints one DOM line a war: when the war was decided and for whom, when the last act began and with how many
// nations, how it ended (WON, the player fell, or how many nations were left), and how near the player came: the
// share of the map's worth held at the end and at the most, the strongest rival, the nations beaten after the last
// act began, and who was left. Tables: domtab.py.
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
const share = (g: any): { mine: number; rows: any[]; left: number; winner: string | null } => { const b = X.standings(g); const row = b.rows.find((r: any) => r.nat === me); return { mine: row ? row.share : 0, rows: b.rows, left: b.left, winner: b.winner }; };
const run = campaign(me, Number(fromS), Number(seedS), { ...plan,
  first: (g: any) => {
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
  each: (g: any) => {
    lastTurn = g.turn;
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
const end = won ? `WON ${date(g.flags['dom:won'] ?? lastTurn)}` : !stands ? `the player fell ${date(M.fellOn?.turn ?? lastTurn)} to ${M.fellOn?.to ?? '?'}` : `played to ${date(lastTurn)}${gaveUp ? ' (given up: undecided)' : ''}: ${s.left} nations left`;
console.log(`DOM ${me} seed ${seedS} (${plan.depth ? plan.depth : 'by hand'}${plan.idle ? ', idle' : ''}): war decided ${decidedAt ? `${date(decidedAt)} for ${victor} (beaten ${beaten}; standing ${standingThen || 'none'}); the player's side ${victor === side ? 'won' : 'did not win'}` : `never (great powers fallen: ${fallen.join(', ') || 'none'}; still at war: ${pairs.join(', ') || 'no two'})`}`
  + `; last act ${domAt ? `begun ${date(domAt)} with ${domN} nations, the player holding ${Math.round(shareAtDom * 100)}%` : peaceAt ? `declined ${date(peaceAt)}` : refused ? `refused: ${refused}` : 'never open'}`
  + `; ${end}`
  + `; share of the map's worth at the end ${Math.round(s.mine * 100)}% (rank ${rank || '-'} of ${s.left}), at the most ${Math.round(peak * 100)}% in ${peakAt ? date(peakAt) : '-'}`
  + `; strongest rival ${rival ? `${rival.nat} ${Math.round(rival.share * 100)}% and ${Math.round(rival.divisions)} divisions` : 'none'}; the player's army ${Math.round(s.rows.find((r: any) => r.nat === me)?.divisions ?? 0)} divisions`
  + `; fell after the last act began ${fellAfter.length}${fellAfter.length ? ` (${fellAfter.slice(0, 40).join(', ')})` : ''}`
  + `; left at the end: ${s.rows.map((r: any) => `${r.nat} ${Math.round(r.share * 100)}%`).join(', ')}`
  + `; share each December: ${byDec.join(', ')}`);
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
