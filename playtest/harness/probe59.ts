// Version 70: wars with nobody at the controls at one stop of the history dial, played to a late month, to find wars
// that never end. One line a war: when the great attacks fell, which great powers fell and when, when the war was
// decided (a great power fallen and no two great powers still fighting) or who is still fighting whom, where the
// book was closed, the marches, the rule checks that failed, and any error.
// node out70/probe59.mjs STOP SEEDS(a,b,c) [UNTIL=124]     (build with build70.sh)
import { start, date, T, check } from './lib';
const [stopS = '3', seedsS = '1', untilS = '124'] = process.argv.slice(2);
const MAJ = ['GER', 'ITA', 'UK', 'FRA', 'SOV'];
const WARS: [string, string, string][] = [['west', 'GER', 'BEL'], ['nor', 'GER', 'NOR'], ['ita', 'ITA', 'UK'], ['gre', 'ITA', 'GRE'], ['yug', 'GER', 'YUG'], ['east', 'GER', 'SOV']];
for (const seed of seedsS.split(',').map(Number)) {
  const g: any = start(null, 1, seed, 'colonel', Number(stopS));
  const first: Record<string, number> = {}, fell: Record<string, number> = {}, back: Record<string, number> = {};
  const notes: string[] = [], problems: string[] = []; const seen = new Set<string>();
  let head = 0, decided = 0, err = '', marches = 0, treaties = 0;
  try {
    while (g.turn <= Number(untilS)) {
      const t = g.turn;
      T.runTurnAI(g);
      check(g, 'month end', problems, seen);
      for (const [k, a, b] of WARS) if (!first[k] && g.nations[a]?.atWar.includes(b)) first[k] = t;
      for (const n of MAJ) { if (!g.nations[n].alive && !fell[n]) fell[n] = t; if (g.nations[n].alive && fell[n] && !back[n]) back[n] = t; }
      for (const h of g.headlines.slice(head)) {
        const x: string = h.text; let m: RegExpExecArray | null;
        if ((m = /^History's book is closed in (.+?): (.+?)\./.exec(x))) notes.push(`book closed in ${m[1]} ${date(h.turn)} (${m[2]})`);
        else if (/ marches for /.test(x)) marches++;
        else if (/ sign a treaty: /.test(x)) treaties++;
      }
      head = g.headlines.length;
      const live = MAJ.filter((p) => g.nations[p].alive);
      const fighting = live.some((a) => g.nations[a].atWar.some((e: string) => live.includes(e)));
      if (!decided && Object.keys(fell).length && !fighting) decided = t;
    }
  } catch (e: any) { err = ` | ERROR in ${date(g.turn)}: ${String(e?.stack ?? e).split('\n').slice(0, 4).join(' / ')}`; }
  const live = MAJ.filter((p) => g.nations[p].alive);
  const pairs: string[] = []; for (let i = 0; i < live.length; i++) for (let j = i + 1; j < live.length; j++) if (g.nations[live[i]].atWar.includes(live[j])) pairs.push(`${live[i]}-${live[j]}`);
  const d = (k: string): string => (first[k] ? date(first[k]) : 'never');
  console.log(`WATCH seed ${seed} stop ${stopS} to ${date(Math.min(g.turn, Number(untilS)))}: west ${d('west')} | nor ${d('nor')} | ita ${d('ita')} | gre ${d('gre')} | yug ${d('yug')} | east ${d('east')} | fell: ${MAJ.filter((n) => fell[n]).map((n) => `${n} ${date(fell[n])}${back[n] ? ` (back ${date(back[n])})` : ''}`).join(', ') || 'nobody'} | ${decided ? `decided ${date(decided)}` : `NOT DECIDED (still at war: ${pairs.join(' ') || 'nobody'})`}${pairs.length && decided ? ` (at war again at the end: ${pairs.join(' ')})` : ''} | marches ${marches}, treaties ${treaties} | ${notes.join('; ') || 'book closed nowhere'} | rule checks failed ${problems.length}${problems.length ? `: ${problems.slice(0, 3).join(' ;; ').slice(0, 300)}` : ''}${err}`);
}
