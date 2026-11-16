// Version 77: every landing a computer's staff makes in a war with nobody playing (the design director's rulings 40,
// 43 and 60: where and when Britain, America and Canada first land, and what the landings cost).
// The rules' own hook (landingWatch in ai.ts) tells each landing; the probe only listens, so the war is not nudged.
// One LAND line a landing: who, when, where (whose ground), how many corps, whether it was a first wave or a later
// one into the same lodgment, whether the beach was warned, and six months on how many of those corps were dead and
// who held the beach. One FIRST line a war: each nation's first landing and its first on the western mainland.
// usage: probe78 SEEDS(a,b,c) [UNTIL=80]     (build with build70.sh)
import { start, date, T } from './lib';
import { landingWatch } from '@tc/campaign/ai';
const [seedsS = '17', untilS = '80'] = process.argv.slice(2);
const WEST = new Set(['FRA', 'BEL', 'NED', 'LUX', 'GER', 'DEN']);
for (const seed of seedsS.split(',').map(Number)) {
  const g: any = start(null, 1, seed, 'colonel', 3);
  type L = { t: number; nat: string; reg: number; name: string; owner: string; west: boolean; ids: number[]; wave: boolean; warned: boolean; taken: boolean; dead?: number; held?: string; done?: boolean };
  const lands: L[] = []; let gerFell = 0, err = '';
  landingWatch.tell = (e: any) => {
    const r = e.to; if (!r) return;
    lands.push({ t: g.turn, nat: e.nat, reg: r.id ?? g.regions.indexOf(r), name: r.name, owner: r.owner, west: WEST.has(r.owner) && !r.overseas && r.name !== 'Corsica', ids: (e.force ?? []).map((c: any) => c.id), wave: !!e.wave, warned: !!e.warned, taken: !!e.taken });
  };
  const close = (l: L): void => {
    const alive = new Set(g.corps.filter((c: any) => !c.dead).map((c: any) => c.id));
    l.dead = l.ids.filter((id) => !alive.has(id)).length; l.held = g.regions[l.reg]?.ctrl ?? '?'; l.done = true;
  };
  try {
    while (g.turn <= Number(untilS)) {
      const t = g.turn;
      T.runTurnAI(g);
      for (const l of lands) if (!l.done && t >= l.t + 6) close(l);
      if (!gerFell && g.nations.GER && !g.nations.GER.alive) gerFell = t;
      if (gerFell && t >= gerFell + 6) break;
    }
  } catch (e: any) { err = ` | ERROR in ${date(g.turn)}: ${String(e?.stack ?? e).split('\n').slice(0, 3).join(' / ')}`; }
  landingWatch.tell = null;
  for (const l of lands) {
    const six = l.done ? `six months on: dead ${l.dead} of ${l.ids.length}, beach held by ${l.held}` : 'six months on: the war was over or the run ended';
    console.log(`LAND seed ${seed}: ${l.nat} ${date(l.t)} at ${l.name} (${l.owner})${l.west ? ' WEST' : ''} | corps ${l.ids.length} | ${l.wave ? 'later wave' : 'first wave'}${l.warned ? ', warned' : ''}${l.taken ? '' : ', thrown back'} | ${six}`);
  }
  const firsts = ['UK', 'USA', 'CAN', 'BRA', 'ITA', 'GER', 'SOV', 'FRA'].map((n) => {
    const mine = lands.filter((l) => l.nat === n && !l.wave); if (!mine.length) return `${n} none`;
    const f = mine[0], w = mine.find((l) => l.west);
    return `${n} ${date(f.t)} ${f.name} (${f.owner}), ${mine.length} landings; western mainland ${w ? `${date(w.t)} ${w.name} (${w.owner})` : 'never'}`;
  });
  console.log(`FIRST seed ${seed}: Germany ${gerFell ? `fell ${date(gerFell)}` : 'stands'} | ${firsts.join(' | ')}${err}`);
}
