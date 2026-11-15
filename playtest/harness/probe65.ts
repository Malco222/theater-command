// Version 74: "a war saved in mid-course loads back identical". A war with nobody playing is saved at each month in
// AT and loaded back; the loaded copy is then played beside the war that never left memory for MONTHS months, and the
// two are compared month by month (the save of each, with the keys of every table put in order).
// node out74/probe65.mjs SEED AT(a,b,c) [MONTHS=6]     (build with build70.sh; TC_EUROPE=1 keeps the map of Europe)
import { start, date, T, saveGame, loadGame } from './lib';
const [seedS = '25', atS = '10,30,50', monthsS = '6'] = process.argv.slice(2);
const at = atS.split(',').map(Number), months = Number(monthsS);
const canon = (v: unknown): string => JSON.stringify(v, (_k, x) => (x && typeof x === 'object' && !Array.isArray(x) ? Object.fromEntries(Object.entries(x).sort(([p], [q]) => (p < q ? -1 : p > q ? 1 : 0))) : x));
const g: any = start(null, 1, Number(seedS), 'colonel', 3);
const last = Math.max(...at) + months;
type Fork = { t0: number; a: any; left: number; diffs: string[]; same0: string; kb: number };
const forks: Fork[] = [];
const say = (f: Fork): void => console.log(`SAVE seed ${seedS} at ${date(f.t0)} (${f.kb} KB, ${g.regions.length} provinces): save, load, save again gives ${f.same0}; played ${months - f.left} months on from memory and from the save: ${f.diffs.length ? 'THEY PART: ' + f.diffs.join(' | ') : 'the same war every month'}`);
while (g.turn <= last) {
  if (at.includes(g.turn)) {
    const text = saveGame(g); const a: any = loadGame(text); const kb = Math.round(text.length / 1024);
    if (!a) console.log(`SAVE seed ${seedS} at ${date(g.turn)}: could not be loaded (${kb} KB)`);
    else {
      const again = saveGame(a); const x = JSON.parse(text), y = JSON.parse(again);
      const same0 = again === text ? 'the same text' : canon(x) === canon(y) ? 'the same contents, keys in another order' : `DIFFERENT in ${Object.keys(x).filter((k) => canon(x[k]) !== canon(y[k])).join(', ')}`;
      forks.push({ t0: g.turn, a, left: months, diffs: [], same0, kb });
    }
  }
  T.runTurnAI(g);
  for (const f of forks) if (f.left > 0) {
    let err = '';
    try { T.runTurnAI(f.a); } catch (e: any) { err = String(e?.stack ?? e).split('\n').slice(0, 3).join(' / '); }
    f.left--;
    if (err) { f.diffs.push(`${date(g.turn - 1)}: ERROR in the loaded war: ${err}`); f.left = 0; say(f); continue; }
    const x = JSON.parse(saveGame(g)), y = JSON.parse(saveGame(f.a));
    const d = Object.keys(x).filter((key) => canon(x[key]) !== canon(y[key]));
    if (d.length) { f.diffs.push(`${date(g.turn - 1)}: ${d.slice(0, 8).join(', ')}`); if (f.diffs.length >= 2) f.left = 0; }
    if (f.left === 0) say(f);
  }
}
