// The sandbox's reputations (reputation.ts, data/campaign/reputation.json; Brian, 19 November 2026: "i want airplane
// squadrons to get nick names if they keep winning battles and get a reputation", "for example the Red Tails", "same
// with tanks and battalions of troops"). Air groups, corps and fleets earn a name on a run of wins, in their own
// country's style or history's own, with the paint that goes with it; the player's units fight a little harder for
// it. Behind g.flags.sandbox: off, nothing is written and nothing changes.
import { describe, expect, it } from 'vitest';
import { createGame } from '../src/campaign/game';
import {
  REPUTATION, armOf, drawName, isBlocked, landWin, markOf, markWords, realOf, repAir, repBattle, repEdge, roomForName, shipNick, styleOf, tierIndex, tierOf,
} from '../src/campaign/reputation';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { inherit, storyAttack, storyEdge, storyFallen } from '../src/campaign/stories';
import { runTurnAI } from '../src/campaign/turn';
import type { AirFleet, Corps, Fleet, Game, Storied } from '../src/campaign/types';

const grand = (human: string | null = null, seed = 3): Game => startOpen(createGame(seed, human));
const sand = (human: string | null = null, seed = 3): Game => { const g = grand(human, seed); g.flags.sandbox = 1; return g; };
const corpsOf = (g: Game, nat: string, kind?: string): Corps[] => g.corps.filter((c) => c.nat === nat && !c.dead && c.kind !== 'fort' && (!kind || c.kind === kind));
const N = REPUTATION.named;

/** Wins a unit enough big battles, running, to make its name. */
function famous(g: Game, kind: 'corps' | 'air' | 'fleet', u: Corps | AirFleet | Fleet, foe: Corps | AirFleet | Fleet, at: number): void {
  for (let i = 0; i < N.run; i++) repBattle(g, kind, [u], [foe], at, 2);
}

describe('the data', () => {
  it('has tiers rising in points and edge, a style for every great power, and paint for every arm', () => {
    const T = REPUTATION.tiers;
    for (let i = 1; i < T.length; i++) { expect(T[i].at).toBeGreaterThan(T[i - 1].at); expect(T[i].edge).toBeGreaterThanOrEqual(T[i - 1].edge); }
    expect(T[T.length - 1].edge).toBeLessThanOrEqual(0.06);
    expect(N.rep).toBe(T[1].at);
    for (const nat of ['USA', 'UK', 'GER', 'SOV', 'JAP', 'ITA', 'FRA', '_default']) {
      const st = REPUTATION.styles[nat];
      expect(st, nat).toBeDefined();
      expect(st.forms.length, nat).toBeGreaterThan(1);
      for (const c of st.colours) expect(REPUTATION.colours[c], `${nat} ${c}`).toMatch(/^#[0-9a-f]{6}$/);
      for (const [many, one] of st.beasts) { expect(many, nat).toMatch(/^[A-Z]/); expect(one, nat).toMatch(/^[a-z]/); }
      for (const [f] of st.forms) if (f.includes('{adj}')) expect(st.adj.length, nat).toBeGreaterThan(3);
    }
    for (const arm of ['air', 'tank', 'troops', 'fleet'] as const) {
      expect(REPUTATION.parts[arm].length).toBeGreaterThan(1);
      expect(REPUTATION.emblemAt[arm].says).toContain('{emblem}');
    }
    // A country with none of its own borrows one.
    expect(styleOf('CAN')).toBe(REPUTATION.styles.UK);
    expect(styleOf('BRA')).toBe(REPUTATION.styles._default);
  });

  it("lists history's names only for formations the game has, and blocks them for everyone else", () => {
    const g = grand();
    const names = new Set([...g.corps, ...g.stories.fallen].map((c) => `${c.nat}:${c.name}`));
    expect(names.has('UK:7th Armoured Division')).toBe(true);
    for (const r of REPUTATION.real) {
      expect(r.why.length, r.name).toBeGreaterThan(20);
      expect(r.says, r.name).toMatch(/\.$/);
      expect(REPUTATION.colours[r.mark.colour], r.name).toBeDefined();
    }
    expect(isBlocked('the Red Tails')).toBe(true);
    expect(isBlocked('the Desert Rats')).toBe(true);
    expect(isBlocked('the Blue Ibexes')).toBe(false);
  });
});

describe('the switch', () => {
  it('is off by default: no points, no names, no edge', () => {
    const g = grand('GER');
    const [c] = corpsOf(g, 'GER'), [p] = corpsOf(g, 'POL');
    famous(g, 'corps', c, p, c.reg);
    expect(c.rep).toBeUndefined();
    expect(c.run).toBeUndefined();
    expect(c.mark).toBeUndefined();
    const f = g.airFleets.find((x) => x.nat === 'GER')!;
    f.rep = 30;
    expect(repEdge(g, f)).toBe(0);
    expect(storyEdge(g, f)).toBe(1);
    expect(shipNick(g, 'GER', 'Thunderer', 9)).toBeUndefined();
  });
});

describe('making a name', () => {
  it('a run of hard wins makes a name, with its paint, and the player is told', () => {
    const g = sand('GER');
    const c = corpsOf(g, 'GER', 'arm')[0], p = corpsOf(g, 'POL')[0];
    const before = g.headlines.length;
    for (let i = 0; i < N.run - 1; i++) repBattle(g, 'corps', [c], [p], c.reg, 2);
    expect(c.run).toBe(N.run - 1);
    expect(c.mark).toBeUndefined();
    repBattle(g, 'corps', [c], [p], c.reg, 2);
    expect(c.rep).toBe(N.run * REPUTATION.gain.big);
    expect(c.best).toBe(N.run);
    expect(c.nick).toBeTruthy();
    expect(c.mark).toBeDefined();
    expect(armOf('corps', c)).toBe('tank');
    expect(tierOf(c)?.word).toBe('Known');
    const news = g.headlines.slice(before).find((h) => h.kind === 'honour' && h.text.includes(c.nick!));
    expect(news?.text).toContain(`${c.name} has won four battles running, and the army has begun to call it ${c.nick}.`);
    expect(news?.text).toContain(markOf('corps', c));
    // Once only: more wins add points, never a second name.
    const nick = c.nick, mark = c.mark;
    famous(g, 'corps', c, p, c.reg);
    expect(c.nick).toBe(nick);
    expect(c.mark).toBe(mark);
    expect(tierIndex(c)).toBeGreaterThanOrEqual(2);
  });

  it('a defeat ends the run and costs points, never below none; the losers of a walkover still lose', () => {
    const g = sand();
    const [c] = corpsOf(g, 'GER'), [p] = corpsOf(g, 'POL');
    repBattle(g, 'corps', [c], [p], c.reg, 1);
    repBattle(g, 'corps', [c], [p], c.reg, 1);
    expect(c.rep).toBe(2);
    p.rep = 1; p.run = 1;
    repBattle(g, 'corps', [p], [c], p.reg, 0);
    expect(p.rep).toBe(1); // a walkover is worth nothing to the winner
    expect(c.run).toBe(0);
    expect(c.rep).toBe(0);
    repBattle(g, 'corps', [p], [c], p.reg, 1);
    expect(c.rep).toBe(0);
    // A unit that never had a name to lose writes nothing.
    const [q] = corpsOf(g, 'POL').slice(1);
    repBattle(g, 'corps', [c], [q], c.reg, 1);
    expect(q.rep).toBeUndefined();
  });

  it('weighs a land battle by its odds: a walkover makes no name, a win against the odds a big one', () => {
    expect(landWin(true, 4, true)).toBe(0);
    expect(landWin(true, 2, false)).toBe(1);
    expect(landWin(true, 2, true)).toBe(2);
    expect(landWin(true, 1, false)).toBe(2);
    expect(landWin(false, 1.3, false)).toBe(2);
    expect(landWin(false, 0.7, false)).toBe(1);
    expect(landWin(false, 0.3, false)).toBe(0);
  });

  it("gives a historical formation history's name and paint", () => {
    const g = sand('UK');
    const c = g.corps.find((x) => x.nat === 'UK' && x.name === '7th Armoured Division')!;
    const it = g.corps.find((x) => x.nat === 'ITA' && x.kind !== 'fort')!;
    const before = g.headlines.length;
    famous(g, 'corps', c, it, c.reg);
    expect(c.nick).toBe('the Desert Rats');
    expect(c.mark).toEqual({ at: 'turret', colour: 'red', emblem: 'jerboa' });
    expect(markOf('corps', c)).toBe(realOf(c)!.says);
    const news = g.headlines.slice(before).find((h) => h.text.includes('the Desert Rats'))!;
    expect(news.text).toContain(realOf(c)!.why);
  });

  it('keeps a name an honour already gave, and adds the paint', () => {
    const g = sand('GER');
    const [c] = corpsOf(g, 'GER', 'inf'), [p] = corpsOf(g, 'POL');
    c.nick = 'the Anvil';
    const before = g.headlines.length;
    famous(g, 'corps', c, p, c.reg);
    expect(c.nick).toBe('the Anvil');
    expect(c.mark).toBeDefined();
    expect(c.mark!.emblem).toBeUndefined();
    expect(g.headlines.slice(before).some((h) => h.text.includes('knows it everywhere as the Anvil'))).toBe(true);
  });

  it('tells the player of a name the enemy has made, and of its fall', () => {
    const g = sand('POL');
    const [c] = corpsOf(g, 'GER'), [p] = corpsOf(g, 'POL');
    famous(g, 'corps', c, p, c.reg);
    expect(g.headlines.some((h) => h.text.startsWith(`The German ${c.name} has won four battles running, and our soldiers have a name for it: ${c.nick}.`))).toBe(true);
    c.rep = REPUTATION.tiers[REPUTATION.famousTier].at;
    p.rep = 0; p.run = 0;
    repBattle(g, 'corps', [p], [c], p.reg, 1);
    // Beating a unit the enemy fears is worth more, and is news all along the line.
    expect(p.rep).toBe(REPUTATION.gain.win + REPUTATION.gain.famous);
    expect(g.headlines.some((h) => h.text.startsWith(`${c.nick}, the German ${c.name}, has been beaten.`))).toBe(true);
  });

  it('an army talks about its best few only', () => {
    const g = sand();
    const all = corpsOf(g, 'GER');
    const live = g.corps.filter((c) => c.nat === 'GER' && !c.dead).length;
    const room = Math.max(1, Math.ceil(N.share * live));
    for (const c of all.slice(0, room)) c.mark = { at: 'scarf', colour: 'red' };
    expect(roomForName(g, 'corps', 'GER')).toBe(false);
    const c = all[room], [p] = corpsOf(g, 'POL');
    famous(g, 'corps', c, p, c.reg);
    expect(c.rep).toBe(N.run * REPUTATION.gain.big);
    expect(c.mark).toBeUndefined();
    // When one of the named falls, there is room again, and the next win names it.
    all[0].dead = true;
    repBattle(g, 'corps', [c], [p], c.reg, 1);
    expect(c.mark).toBeDefined();
  });
});

describe('names in each country\'s style', () => {
  it('never makes up history\'s names, never repeats one in an army, and paints what it says', () => {
    for (const nat of ['USA', 'UK', 'GER', 'SOV', 'JAP', 'ITA', 'FRA', 'POL', 'CAN']) {
      const g = sand(null, 11);
      const seen = new Set<string>();
      const st = styleOf(nat);
      for (let i = 0; i < 40; i++) {
        const kind = i % 3 === 0 ? 'air' : 'corps';
        const u = (kind === 'air'
          ? { id: 9000 + i, nat, name: `Group ${i}`, base: g.corps[0].reg, leader: { name: 'Hank Miller', level: 1, xp: 0 } }
          : { id: 9000 + i, nat, name: `Corps ${i}`, kind: i % 2 ? 'arm' : 'inf', reg: g.corps[0].reg, general: null }) as unknown as Corps;
        const d = drawName(g, kind, u, g.corps[0].reg)!;
        expect(d, `${nat} ${i}`).toBeDefined();
        expect(isBlocked(d.nick), d.nick).toBe(false);
        expect(seen.has(d.nick), d.nick).toBe(false);
        seen.add(d.nick);
        // Park it on a unit of the army so the next draw sees it taken.
        (kind === 'air' ? g.airFleets : g.corps).push({ ...(u as object), nick: d.nick } as never);
        expect(st.colours, d.nick).toContain(d.mark.colour);
        const arm = armOf(kind, u);
        if (d.mark.emblem) {
          expect(st.beasts.map((b) => b[1]), d.nick).toContain(d.mark.emblem);
          expect(d.mark.at).toBe(REPUTATION.emblemAt[arm].at);
        } else {
          const part = REPUTATION.parts[arm].find((p) => p.at === d.mark.at);
          // A paint name says the paint: 'the Red Tails' are red, on the tail.
          if (part && d.nick.includes(part.name)) expect(d.nick.toLowerCase()).toContain(d.mark.colour);
        }
        const words = markWords(arm, d.mark);
        expect(words, d.nick).toMatch(/^Its .+\.$/);
        expect(words).not.toMatch(/\{|\}|a orange|a (?=[aeiou])/);
      }
    }
  });

  it('is the same name for the same unit in the same war, and another in another war', () => {
    const a = sand(null, 4), b = sand(null, 4), c = sand(null, 5);
    const pick = (g: Game): string => {
      const u = corpsOf(g, 'USA')[0] ?? corpsOf(g, 'UK')[0];
      return drawName(g, 'corps', u, u.reg)!.nick;
    };
    expect(pick(a)).toBe(pick(b));
    const many = [5, 6, 7, 8, 9].map((s) => pick(sand(null, s)));
    expect(new Set([pick(c), ...many]).size).toBeGreaterThan(1);
  });
});

describe('the air war and the sea', () => {
  it('in the air, the side that brought down more won; a raid nobody met is no battle', () => {
    const g = sand();
    const [a, b] = g.airFleets.filter((f) => f.nat === 'GER'), [d] = g.airFleets.filter((f) => f.nat === 'UK');
    repAir(g, a.base, [a], [], { sky: false, sunk: 0, attLost: 3, defLost: 0 });
    expect(a.rep).toBeUndefined();
    repAir(g, a.base, [a], [d], { sky: false, sunk: 0, attLost: 4, defLost: 9 });
    expect(a.rep).toBe(1);
    repAir(g, a.base, [a], [d], { sky: true, sunk: 0, attLost: 9, defLost: 4 });
    expect(a.rep).toBe(3);
    repAir(g, a.base, [b], [d], { sky: false, sunk: 0, attLost: 12, defLost: 2 });
    expect(d.rep).toBe(2);
    expect(b.rep).toBeUndefined();
    repAir(g, a.base, [a], [d], { sky: false, sunk: 0, attLost: 5, defLost: 5 });
    expect(a.run).toBe(2);
  });

  it('an air group that keeps winning is named for its paint or its badge, a carrier wing as well', () => {
    const g = sand('USA');
    const f = g.airFleets.find((x) => x.nat === 'USA')!, foe = g.airFleets.find((x) => x.nat !== 'USA')!;
    famous(g, 'air', f, foe, f.base);
    expect(f.nick).toBeTruthy();
    expect(armOf('air', f)).toBe('air');
    expect([...REPUTATION.parts.air.map((p) => p.at), REPUTATION.emblemAt.air.at, REPUTATION.bandAt.air.at]).toContain(f.mark!.at);
  });

  it('a fleet makes its name in fleet actions, and a ship her crew names once she has her stars', () => {
    const g = sand('UK');
    const f = g.fleets.find((x) => x.nat === 'UK')!, foe = g.fleets.find((x) => x.nat === 'GER')!;
    famous(g, 'fleet', f, foe, f.zone);
    expect(f.nick).toBeTruthy();
    expect(f.nick).not.toMatch(/ of (?!the )[A-Z]/); // 'of the North Sea'
    expect(shipNick(g, 'UK', 'Valiant', REPUTATION.ship.stars - 1)).toBeUndefined();
    const s = shipNick(g, 'UK', 'Valiant', REPUTATION.ship.stars)!;
    expect(s).toMatch(/^(Old|the Lucky) [A-Z]/);
    expect(shipNick(g, 'UK', 'Valiant', 9)).toBe(s);
  });
});

describe('what a name is worth', () => {
  it("is a small edge for the player's own units, and none for a computer's", () => {
    const g = sand('GER');
    const f = g.airFleets.find((x) => x.nat === 'GER')!, e = g.airFleets.find((x) => x.nat === 'UK')!;
    const base = storyEdge(g, f);
    f.rep = REPUTATION.tiers[1].at;
    expect(storyEdge(g, f)).toBeCloseTo(base + REPUTATION.tiers[1].edge, 10);
    e.rep = 99;
    expect(storyEdge(g, e)).toBe(1);
    const [c] = corpsOf(g, 'GER');
    const target = g.regions.find((r) => r.owner === 'POL' && r.id !== c.home)!;
    const plain = storyAttack(g, c, target, []);
    c.rep = REPUTATION.tiers[3].at;
    expect(storyAttack(g, c, target, [])).toBeCloseTo(plain + REPUTATION.tiers[3].edge, 10);
  });

  it('goes on the memorial wall with a fallen corps, and to the corps raised in its name; saves keep it', () => {
    const g = sand('GER');
    const [c] = corpsOf(g, 'GER', 'inf'), [p] = corpsOf(g, 'POL');
    famous(g, 'corps', c, p, c.reg);
    const nick = c.nick, mark = c.mark;
    const back = loadGame(saveGame(g))!;
    const again = back.corps.find((x) => x.id === c.id)!;
    expect(again.nick).toBe(nick);
    expect(again.mark).toEqual(mark);
    expect(again.rep).toBe(c.rep);
    storyFallen(g, c, 'destroyed', 'POL', c.reg);
    const wall = g.stories.fallen[g.stories.fallen.length - 1];
    expect(wall.mark).toEqual(mark);
    const [heir] = corpsOf(g, 'GER', 'inf').filter((x) => x.id !== c.id && !x.nick);
    inherit(g, heir, wall.id);
    expect(heir.nick).toBe(nick);
    expect(heir.mark).toEqual(mark);
  });
});

describe('a war in the sandbox', () => {
  it('names a few of the best, the same ones each time, and none off', () => {
    const run = (sb: boolean): Game => { const g = grand(null, 5); if (sb) g.flags.sandbox = 1; while (g.turn <= 12) runTurnAI(g); return g; };
    const a = run(true), b = run(true);
    const named = (g: Game): string[] => [...g.corps, ...g.airFleets, ...g.fleets].filter((u: Storied) => u.mark).map((u) => `${u.nat}:${u.name}=${u.nick}`).sort();
    expect(named(a).length).toBeGreaterThan(0);
    expect(named(a)).toEqual(named(b));
    for (const nat of Object.keys(a.nations)) for (const kind of ['corps', 'air', 'fleet'] as const) {
      const all = kind === 'corps' ? a.corps.filter((c) => c.nat === nat && !c.dead) : kind === 'air' ? a.airFleets.filter((f) => f.nat === nat) : a.fleets.filter((f) => f.nat === nat);
      expect(all.filter((x) => x.mark).length, `${nat} ${kind}`).toBeLessThanOrEqual(Math.max(1, Math.ceil(N.share * all.length)) + 1);
    }
    const off = run(false);
    expect([...off.corps, ...off.airFleets, ...off.fleets, ...off.stories.fallen].some((u: Storied) => u.rep !== undefined || u.run !== undefined || u.mark !== undefined)).toBe(false);
  }, 240000);
});
