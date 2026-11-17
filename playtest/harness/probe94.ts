// probe94: does WHAT YOU FACE tell the player before an attack what the result's line says after it? (ruling 152, Version 84 on)
//   node out84b/probe94.mjs ME 1 SEED UNTIL PLAN-JSON      (TC_LEVEL sets the level, default colonel; TC_ROWS=N prints the first N attacks)
// A seat is played by the lib's plan. Just before every field attack of the player's the card is read as the page
// makes it (whatYouFace on the preview: the lines, and whether the defence is identified); just after, the result's
// line (report.paid). One FACE line a war:
//   attacks; cards with the block; cards "Not scouted"; cards with tanks sent alone;
//   results that name a cause (infantry to machine guns, guns or snipers; tanks to anti-tank guns, mines, works, tanks):
//     TOLD      the card named that cause before the attack
//     UNKNOWN   the card said "Not scouted" (it could not know)
//     SURPRISE  the card was identified and did not name it
//   the tanks' shock: the card's words against the result's words, same or not.
import { campaign, date, T } from './lib';
import { whatYouFace, gunsKnown } from '@tc/campaign/facing';
const [me, fromS, seedS, untilS, planS] = process.argv.slice(2);
const plan = JSON.parse(planS || '{}');
const ROWS = Number(process.env.TC_ROWS || 0);
const X = T as any;
interface Card { t: number; where: string; lines: { sign: string; text: string; unknown?: boolean }[] | null; known: boolean; tanks: number; foot: number; alone: number; shock: number | null; odds: number; ticketAlone: boolean }
let card: Card | null = null;
const S = { attacks: 0, block: 0, unknown: 0, aloneCards: 0, aloneBig: 0, named: 0, told: 0, hidden: 0, surprise: 0, even: 0, shockBoth: 0, shockSame: 0, tanksIn: 0, noPaid: 0, wonAlone: 0, wonWith: 0, nWith: 0 };
const byYear: Record<string, { attacks: number; unknown: number }> = {};
const surprises: string[] = []; const printed: string[] = [];
const shockOf = (s: string): string | null => { const m = /shock count(?:s|ed) ((?:in full|for [a-z ]+?))[.:]?$/.exec(s.trim()) ?? /shock count(?:s|ed) (in full|for (?:three quarters|two thirds|half|a third|a quarter|a tenth|nothing))/.exec(s); return m ? m[1] : null; };
const beforeAttack = (g: any, _out: any, cands: any[], r: any): void => {
  card = null;
  const a = X.previewAttack?.(g, cands, r);
  if (!a) return;
  let lines: any = null, known = true;
  try { lines = whatYouFace(g, a, r); known = gunsKnown(g, a); } catch (e) { lines = null; }
  const going = cands;
  card = { t: g.turn, where: r.name, lines, known, tanks: going.filter((c: any) => c.kind === 'arm').length, foot: going.filter((c: any) => c.kind === 'inf' || c.kind === 'mot' || c.kind === 'mtn' || c.kind === 'para').length, alone: a.faced?.alone ?? 0, shock: a.faced?.shock ?? null, odds: a.odds, ticketAlone: (a.lines ?? []).some((l: any) => l.label === 'Tanks alone') };
};
const onAttack = (g: any, _out: any, rep: any, r: any): void => {
  const c = card; card = null;
  if (!c) return;
  S.attacks++;
  const y = String(1939 + Math.floor((c.t + 7) / 12));
  (byYear[y] ??= { attacks: 0, unknown: 0 }).attacks++;
  if (c.lines) S.block++;
  const notScouted = !!c.lines?.some((l) => l.unknown);
  if (notScouted) { S.unknown++; byYear[y].unknown++; }
  const aloneLine = c.lines?.find((l) => l.sign === 'tank');
  if (aloneLine || c.alone >= 0.05) { S.aloneCards++; if (c.alone >= 0.25) S.aloneBig++; if (rep.taken) S.wonAlone++; }
  else if (c.tanks > 0) { S.nWith++; if (rep.taken) S.wonWith++; }
  if (c.tanks > 0) S.tanksIn++;
  const said: string = rep.paid?.att ?? '';
  if (!rep.paid) S.noPaid++;
  const m = /^Your (infantry|tanks) paid most of it, to their ([^.]+)\./.exec(said);
  let verdict = said.startsWith('The cost fell evenly') ? 'even' : '';
  if (verdict === 'even') S.even++;
  if (m) {
    S.named++;
    const causes = m[2].split(/, | and /);
    const has = (sign: string) => !!c.lines?.some((l) => l.sign === sign && !l.unknown);
    const toldBy = (cause: string): boolean => m[1] === 'tanks' ? has('anti-tank') || has('tank') : cause === 'machine guns' ? has('machine-gun') : cause === 'snipers' ? has('sniper') : has('machine-gun') || has('mortar') || has('anti-tank');
    const told = causes.some(toldBy);
    if (told) { S.told++; verdict = 'TOLD'; } else if (notScouted) { S.hidden++; verdict = 'UNKNOWN'; } else { S.surprise++; verdict = 'SURPRISE'; if (surprises.length < 6) surprises.push(`${date(c.t)} ${c.where}: card [${(c.lines ?? []).map((l) => l.text).join(' / ') || 'no block'}] result [${said}]`); }
  }
  const cs = c.lines?.map((l) => shockOf(l.text)).find(Boolean) ?? null, rs = shockOf(said);
  if (cs && rs) { S.shockBoth++; if (cs === rs) S.shockSame++; }
  if (printed.length < ROWS) printed.push(`ROW ${date(c.t)} ${c.where}: ${c.tanks} tank corps, ${c.foot} on foot, odds ${c.odds.toFixed(2)}, ${rep.taken ? 'won' : 'thrown back'} | CARD ${(c.lines ?? []).map((l) => `(${l.sign}) ${l.text}`).join(' / ') || 'no block'} | RESULT ${said || 'no line'} | ${verdict}`);
};
const run = campaign(me, Number(fromS), Number(seedS), { ...plan, beforeAttack, onAttack }, { until: Number(untilS), level: process.env.TC_LEVEL || 'colonel', quiet: true });
for (const p of printed) console.log(p);
const pc = (a: number, b: number): string => (b ? `${Math.round((100 * a) / b)}%` : 'none');
console.log(`FACE ${me} seed ${seedS}: attacks ${S.attacks} | cards with the block ${S.block} | not scouted ${S.unknown} (${pc(S.unknown, S.attacks)}) | by year ${Object.keys(byYear).sort().map((y) => `${y}: ${byYear[y].unknown} of ${byYear[y].attacks}`).join(', ')} | tanks in ${S.tanksIn}, sent alone ${S.aloneCards} (cut a quarter or more ${S.aloneBig}), won alone ${S.wonAlone} of ${S.aloneCards}, won with infantry ${S.wonWith} of ${S.nWith} | results: a cause named ${S.named}, even ${S.even}, no line ${S.noPaid} | TOLD ${S.told}, UNKNOWN ${S.hidden}, SURPRISE ${S.surprise} | shock words on both ${S.shockBoth}, the same ${S.shockSame}`);
for (const s of surprises) console.log(`   SURPRISE ${s}`);
if (run.errors.length) console.log(`   RUNERR: ${run.errors[0].slice(0, 300)}`);
