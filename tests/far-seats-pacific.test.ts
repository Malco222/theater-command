// The Pacific's six seats (ruling 209): China, Australia, New Zealand, Thailand, Manchukuo and Mongolia, each on the
// door's second row from September 1939. Their rows wait in data/campaign/far-seats-pacific.json, keyed by the file
// each block is laid over when the far side's switch is on (src/campaign/far-rows.ts, stage.json). With the switch
// off nothing in the game reads that file.
//
// The first half runs with the switch off or on: the rows are in the forms the game's own files use, and pass the
// gates those files pass. The second half runs only with the switch on (tests/far-on.test.ts runs this file that
// way): it needs the Pacific's map, and for Manchukuo and Mongolia the client's tie (far.ts; the rider to ruling
// 209(c)). It is the six tests of a client's tie, each for both clients, led by the computer and by a player.
import { describe, expect, it } from 'vitest';
import seats from '../data/campaign/far-seats-pacific.json';
import grandData from '../data/campaign/grand.json';
import leagueData from '../data/campaign/league.json';
import worldData from '../data/campaign/world.json';
import { monthlyDiplomacy, signPeace } from '../src/campaign/diplomacy';
import { atWar, bloc, createGame, declareWar, moveCorps, startPhase } from '../src/campaign/game';
import { pathOptions } from '../src/campaign/league';
import { startOpen } from '../src/campaign/staff';
import { eliteRule, traitRule } from '../src/campaign/traits';
import { declarable } from '../src/campaign/turn';
import { KIND, type Game } from '../src/campaign/types';

const SIX = ['CHN', 'AUS', 'NZL', 'THA', 'MAN', 'MGL'];
const S = seats as any;
const W = worldData as any;
type Row = [string, string, string, number, string, { xp?: number; str?: number; hold?: number }];
const corps = S.world.corps as Row[];
/** True once the rows are laid over the game's own files: the far side's switch is on. */
const merged = W.nations.CHN !== undefined;

describe('the rows as they wait', () => {
  it('has the six seats and no other, each small and from 1939 only', () => {
    expect(S.grand.nations.map((n: any) => n.id)).toEqual(SIX);
    for (const n of S.grand.nations) {
      expect(n.small, n.id).toBe(true);
      expect(Object.keys(n.years), n.id).toEqual(['1939']);
      // The door's own floor, and the 45 words a seat's own text is held to on the new door (sheet 33).
      expect(n.years['1939'].length, n.id).toBeGreaterThan(40);
      expect(n.years['1939'].split(/\s+/).length, n.id).toBeLessThanOrEqual(45);
    }
    // ITS PATH on the door's paper must be true of the seat: a row for each that is bound, and none for Thailand,
    // which is free and carries the four choices as the door already words them.
    expect(Object.keys(S.door.path).sort()).toEqual(['AUS', 'CHN', 'MAN', 'MGL', 'NZL']);
    expect(S.door.path.AUS).toBe(S.door.path.NZL);
    expect(S.door.path.CHN).toMatch(/^At war with Japan from the first day/);
    expect(S.door.path.MAN).toMatch(/^Japan's client\. At war with China from the first day/);
    expect(S.door.path.MGL).toMatch(/^The Soviet Union's client\./);
    // The first day's war is said once on the door, on the ITS PATH row and not in the opening text above it; and
    // a row that names the tie itself (id dominion or client) is left off the door, which keeps it on the Abilities
    // page (the UI director's pass, sheet 33).
    expect(S.grand.nations.find((n: any) => n.id === 'MAN').years['1939']).not.toMatch(/first day/);
    for (const id of ['AUS', 'NZL']) expect(S.traits.nations[id].traits.map((t: any) => t.id), id).toContain('dominion');
    for (const id of ['MAN', 'MGL']) expect(S.traits.nations[id].traits.map((t: any) => t.id), id).toContain('client');
    // None is on the door already, and none is a country the League bars.
    const door = (grandData as any).nations.map((n: any) => n.id);
    for (const id of SIX) { if (!merged) expect(door, id).not.toContain(id); expect((leagueData as any).never, id).not.toContain(id); }
  });

  it('writes each nation in the form world.json has, with a part of the world and a place in the order of play', () => {
    const keys = new Set(['name', 'adj', 'color', 'quality', 'air', 'resilience', 'align', 'seaSupply', 'sealift']);
    expect(Object.keys(S.world.nations)).toEqual(SIX);
    expect(S.world.order).toEqual(SIX);
    for (const id of SIX) {
      const n = S.world.nations[id];
      if (!merged) expect(W.nations[id], id).toBeUndefined();
      for (const k of Object.keys(n)) expect(keys.has(k), `${id}.${k}`).toBe(true);
      expect(n.color, id).toMatch(/^#[0-9a-f]{6}$/);
      // A small country is never balanced toward the great powers (209(b)): none is as good as a great power's army.
      expect(n.quality, id).toBeGreaterThan(0); expect(n.quality, id).toBeLessThan(1);
      expect(n.resilience, id).toBeGreaterThan(0); expect(n.resilience, id).toBeLessThan(1);
      expect(['allies', 'independent', 'leaning_axis'], id).toContain(n.align);
    }
    // The League's parts as ruled: Asia for China, Thailand and the two clients, the Pacific for the two dominions.
    // (A new country falls into 'the Americas' unless its part is named.) The clients join no League all the same:
    // that bar is the mend's, and T6 holds it.
    expect(S.world.parts.of).toEqual({ CHN: 'asia', THA: 'asia', MAN: 'asia', MGL: 'asia', AUS: 'pacific', NZL: 'pacific' });
    expect(S.world.parts.names).toEqual({ asia: 'Asia', pacific: 'the Pacific' });
    // A seat with a coast and a division to send has shipping for one corps, as Canada has.
    for (const id of ['AUS', 'NZL']) expect(S.world.nations[id].sealift, id).toBe(W.nations.CAN.sealift);
  });

  it('gives every seat an army in the form of world.json corps, and never more than three corps to a province', () => {
    const at: Record<string, number> = {};
    for (const [nat, name, kind, divs, place, o] of corps) {
      expect(SIX, name).toContain(nat);
      expect(name.length, name).toBeGreaterThan(3);
      expect((KIND as any)[kind], name).toBeDefined();
      expect(divs, name).toBeGreaterThan(0);
      expect(o.str ?? 1, name).toBeGreaterThan(0); expect(o.str ?? 1, name).toBeLessThanOrEqual(1);
      at[place] = (at[place] ?? 0) + 1;
    }
    for (const id of SIX) expect(corps.some((c) => c[0] === id), id).toBe(true);
    for (const [place, k] of Object.entries(at)) expect(k, place).toBeLessThanOrEqual(3);
    // The record's weakness: nobody but China starts with ten divisions, and China's are thin.
    const divs = (id: string): number => corps.filter((c) => c[0] === id).reduce((s, c) => s + c[3], 0);
    for (const id of SIX.slice(1)) expect(divs(id), id).toBeLessThan(10);
    for (const c of corps.filter((c) => c[0] === 'CHN')) expect(c[5].str ?? 1, c[1]).toBeLessThan(1);
  });

  it('writes each fleet in the form navy.json has, with a name that says Fleet', () => {
    for (const f of S.world.fleets) {
      expect(SIX, f.name).toContain(f.nat);
      expect(f.name, f.name).toMatch(/Fleet$/);
      expect(Object.keys(f.ships).sort(), f.name).toEqual(['bb', 'ca', 'cv', 'dd', 'ss']);
      expect(f.ships.bb + f.ships.cv, f.name).toBe(0);
      expect(f.admiral.length, f.name).toBeGreaterThan(3);
    }
  });

  it("passes the door's own gate: an ability for every seat, two entries counting its unit, each well formed", () => {
    for (const id of SIX) {
      const row = S.traits.nations[id];
      expect(row.traits.length, id).toBeGreaterThan(0);
      expect(row.traits.length + (row.unit ? 1 : 0), id).toBeGreaterThanOrEqual(2);
      expect(new Set(row.traits.map((t: any) => t.id)).size, id).toBe(row.traits.length);
      for (const t of row.traits) {
        expect(t.name.length, `${id}.${t.id}`).toBeGreaterThan(3);
        expect(t.text.length, `${id}.${t.id}`).toBeGreaterThan(20);
        for (const m of [t.attack, t.defence, t.fleet, t.drafts, t.incomeMul]) if (m !== undefined) { expect(m).toBeGreaterThan(1); expect(m).toBeLessThanOrEqual(1.5); }
        for (const k of t.when?.kinds ?? []) expect((KIND as any)[k], `${id}.${t.id}`).toBeDefined();
        const rules = ['attack', 'defence', 'income', 'incomeMul', 'oil', 'relocate', 'drafts', 'cost', 'sealift', 'alert', 'partisans', 'fleet', 'seaLoss'].filter((k) => t[k] !== undefined);
        expect(t.builtin ? rules.length === 0 && !!t.rule : rules.length > 0 && !t.rule, `${id}.${t.id}`).toBe(true);
        expect(traitRule(t).length, `${id}.${t.id}`).toBeGreaterThan(10);
      }
      const u = row.unit;
      if (!u) continue;
      expect((KIND as any)[u.kind], id).toBeDefined();
      expect(u.most, id).toBeGreaterThan(0);
      // The form's 'from' is a year.
      if (u.from !== undefined) expect(u.from, id).toBeGreaterThanOrEqual(1939);
      expect(u.attack !== undefined || u.defence !== undefined, id).toBe(true);
      expect(eliteRule(u)).toMatch(/At most \d at once\.$/);
    }
  });

  it('adds no number in a seat\'s favour beyond the record\'s one ability: every other row is one the game already has', () => {
    for (const id of SIX) {
      const live = S.traits.nations[id].traits.filter((t: any) => !t.builtin);
      expect(live.length + (S.traits.nations[id].unit ? 1 : 0), id).toBeLessThanOrEqual(1);
    }
  });

  it("gives every seat the clerk's two name lists for its officers, in the form commanders.json has", () => {
    expect(Object.keys(S.commanders.names)).toEqual(SIX);
    for (const id of SIX) {
      const { first, last } = S.commanders.names[id];
      expect(first.length, id).toBeGreaterThanOrEqual(6);
      expect(last.length, id).toBeGreaterThanOrEqual(6);
      expect(new Set([...first, ...last]).size, id).toBe(first.length + last.length);
      // A name list draws officers nobody has heard of: none is a man on a fleet's row.
      for (const f of S.world.fleets) for (const part of f.admiral.split(' ')) expect(last, `${id} ${part}`).not.toContain(part);
    }
  });

  it('binds the two dominions as Canada is bound, and the two clients by a row of their own', () => {
    expect(W.follow.CAN).toBe('UK');
    expect(S.world.follow).toEqual({ AUS: 'UK', NZL: 'UK' });
    for (const id of ['AUS', 'NZL']) expect(S.world.nations[id].align, id).toBe(W.nations.CAN.align);
    // A client is not a follower (the rider to 209(c)): its patron, and the neighbours whose wars with the patron it shares.
    expect(S.world.clients).toEqual({
      MAN: { patron: 'JAP', wars: ['CHN', 'SOV', 'MGL'] },
      MGL: { patron: 'SOV', wars: ['JAP', 'MAN'] },
    });
    // Neither client has a seat in an alliance at the start.
    for (const id of ['MAN', 'MGL']) expect(['independent', 'leaning_axis', 'leaning_allies'], id).toContain(S.world.nations[id].align);
    // China is at war with Japan from the first day (ruling 197), and Manchukuo with China beside Japan.
    expect(S.setup.wars).toEqual([['CHN', 'JAP'], ['CHN', 'MAN']]);
  });

  it('carries the garrisons of Australia\'s territories as dated rows that do not hold up its arsenal', () => {
    // The historian's rows, in setup-1939's form: [turn, nation, name, kind, divisions, province, options].
    const rows = S['setup-1939'].formations;
    expect(rows.map((r: any) => [r[0], r[5]])).toEqual([[20, 'Rabaul'], [19, 'Port Moresby'], [29, 'Port Moresby']]);
    for (const [turn, nat, name, kind, divs, , o] of rows) {
      expect(nat, name).toBe('AUS');
      expect(turn, name).toBeGreaterThan(1);
      expect(Object.keys(KIND), name).toContain(kind);
      // A battalion or a brigade: one division on paper at the record's own weight, never a full one, and it draws
      // to no more (ruling 234(b)1: the record's numbers first. Rider b534(a): Lark Force 0.15. Rider b541: Port
      // Moresby 0.25 in all, the 49th Battalion 0.1 and the 30th Brigade's two battalions 0.15).
      expect(divs, name).toBe(1);
      const weight = name.startsWith('Lark Force') ? 0.15 : name.startsWith('49th Battalion') ? 0.1 : 0.15;
      expect(o.str, name).toBe(weight);
      expect(o.est, name).toBe(weight);
      expect(Object.keys(o).every((k) => ['xp', 'str', 'hold', 'est'].includes(k)), name).toBe(true);
    }
    // They are 'formations' and never 'reinforcements': a computer-run country orders nothing new until its last
    // 'reinforcements' row has come (economy.ts lastScheduled), and a row of January 1942 would idle Australia's
    // arsenal for twenty-eight months.
    expect((S['setup-1939'] as any).reinforcements).toBeUndefined();
  });
});

// ---- The switch on: the Pacific's map is attached, and the client's tie is in far.ts ----------------------------

const grand = (human: string | null = null, seed = 3): Game => startOpen(createGame(seed, human));
const month = (g: Game): void => monthlyDiplomacy(g);
const wars = (g: Game, id: string): string[] => [...g.nations[id].atWar].sort();
/** Every case is run with the client led by the computer and by a player. */
const LED: [string, (client: string) => string | null][] = [['led by the computer', () => null], ['in a player\'s hands', (c) => c]];

describe.runIf(merged)('the seats on the map (the far side\'s switch on)', () => {
  it('opens a campaign from each seat with its army on its own ground and its capital in its hands', () => {
    for (const id of SIX) {
      const g = grand(id);
      const n = g.nations[id];
      expect(n.alive, id).toBe(true);
      expect(g.regions[n.capital].ctrl, id).toBe(id);
      const mine = g.corps.filter((c) => c.nat === id && !c.dead);
      // (China's file of the record's rows lays two first-day rows of China's beside the seats file's: Wenchow's
      // garrison and the 10th Group Army's Taichow part, the design director's rulings of 00:43Z and 05:04Z,
      // 21 November 2026.)
      expect(mine.length, id).toBe(corps.filter((c) => c[0] === id).length + (id === 'CHN' ? 2 : 0));
      for (const c of mine) expect(g.regions[c.reg].ctrl, `${id} ${c.name}`).toBe(id);
    }
  });

  it('lands the garrisons of Australia\'s territories in their month and place, each thin and never a full division', () => {
    // A dated row's strength is read (game.ts startPhase; the mend ruled with paper 18): each lands as thin as its row says.
    const g = grand();
    const rows = [...S['setup-1939'].formations].sort((a: any, b: any) => a[0] - b[0]);
    for (const [turn, , name, , , at, o] of rows) {
      g.turn = turn;
      startPhase(g, 'AUS');
      const c = g.corps.find((x) => x.nat === 'AUS' && x.name === name);
      expect(c, name).toBeDefined();
      expect(g.regions[c!.reg].name, name).toBe(at);
      expect(c!.str, name).toBeLessThan(o.str + 0.2);
      expect(c!.divs * c!.str, name).toBeLessThan(1);
    }
  });

  it('puts the dominions at war with Germany on the first day, and China at war with Japan and in no alliance', () => {
    const g = grand();
    for (const id of ['AUS', 'NZL']) { expect(atWar(g, id, 'GER'), id).toBe(true); expect(bloc(g.nations[id]), id).toBe('allies'); }
    expect(atWar(g, 'CHN', 'JAP')).toBe(true);
    expect(bloc(g.nations.CHN)).toBeNull();
    expect(wars(g, 'THA')).toEqual([]);
  });
});

describe.runIf(merged)("a client's tie: Manchukuo and Mongolia (the rider to ruling 209(c); far.ts)", () => {
  for (const [how, who] of LED) {
    it(`T1, the first day, ${how}: Manchukuo is at war with China beside Japan, and Mongolia with nobody`, () => {
      expect(wars(grand(who('MAN')), 'MAN')).toEqual(['CHN']);
      expect(wars(grand(who('MGL')), 'MGL')).toEqual([]);
    });

    it(`T2, far wars, ${how}: neither follows its patron into a war beyond its own border, whatever alliance the patron joins`, () => {
      const a = grand(who('MAN'));
      for (const far of ['USA', 'UK', 'NED']) declareWar(a, 'JAP', far);
      a.nations.JAP.align = 'axis_cobelligerent';
      for (let k = 0; k < 6; k++) month(a);
      expect(wars(a, 'MAN')).toEqual(['CHN']);
      expect(bloc(a.nations.MAN)).toBeNull();

      const b = grand(who('MGL'));
      declareWar(b, 'GER', 'SOV');
      b.nations.SOV.align = 'allies_cobelligerent';
      for (let k = 0; k < 6; k++) month(b);
      expect(wars(b, 'MGL')).toEqual([]);
      expect(bloc(b.nations.MGL)).toBeNull();
    });

    for (const [first, second] of [['JAP', 'SOV'], ['SOV', 'JAP']]) {
      it(`T3, the border war begun by ${first}, ${how}: each client is in it beside its patron that month, and out of it when the patrons make peace`, () => {
        for (const client of ['MAN', 'MGL']) {
          const g = grand(who(client));
          declareWar(g, first, second);
          month(g);
          expect(wars(g, 'MAN'), client).toEqual(['CHN', 'MGL', 'SOV']);
          expect(wars(g, 'MGL'), client).toEqual(['JAP', 'MAN']);
          signPeace(g, 'JAP', 'SOV');
          month(g);
          expect(wars(g, 'MAN'), client).toEqual(['CHN']);
          expect(wars(g, 'MGL'), client).toEqual([]);
        }
      });
    }

    it(`T4, the patron's duty, ${how}: a country at war with a client is at war with its patron the same month`, () => {
      const a = grand(who('MAN'));
      declareWar(a, 'UK', 'MAN');
      month(a);
      expect(atWar(a, 'UK', 'JAP')).toBe(true);
      // And it drags the client into nothing else of Japan's. (Britain's dominions follow her into her war, as they do into every war of hers.)
      expect(wars(a, 'MAN').filter((id) => W.follow[id] !== 'UK')).toEqual(['CHN', 'UK']);

      const b = grand(who('MGL'));
      declareWar(b, 'GER', 'MGL');
      month(b);
      expect(atWar(b, 'GER', 'SOV')).toBe(true);
    });

    it(`T5, no sanctuary, ${how}: the patron's army stands on its client's ground, and that ground shelters nobody`, () => {
      const g = grand(who('MAN'));
      const on = (nat: string, owner: string) => g.corps.filter((c) => c.nat === nat && !c.dead && g.regions[c.reg].owner === owner);
      // Japan's army in Manchuria is put down where it stood: every one of the planner's rows that stands on
      // Manchukuo's ground (six corps since the 3rd Army's row was cut in two).
      const rows = (W.corps as Row[]).filter((c) => c[0] === 'JAP' && g.byName[c[4]]?.owner === 'MAN').length;
      expect(rows).toBeGreaterThanOrEqual(5);
      expect(on('JAP', 'MAN').length).toBe(rows);
      const hailar = g.regions.find((r) => r.name === 'Hailar')!;
      const beside = hailar.nb.map((i) => g.regions[i]).find((r) => r.ctrl === 'SOV')!;
      const red = g.corps.find((c) => c.nat === 'SOV' && !c.dead)!;
      red.reg = beside.id; beside.units.push(red);
      // At peace the frontier is shut to him. In the border war it is not, whoever began it.
      expect(moveCorps(g, red, hailar)).toBe(false);
      declareWar(g, 'SOV', 'JAP');
      month(g);
      red.ap = red.apMax;
      // Japan's 6th Army stands at Hailar beside Manchukuo's cavalry (two rows of it: its 7th Division is a row of
      // its own, rider b641 to ruling 234), and a corps marches into no province an enemy stands in: it fights for it.
      expect(hailar.units.map((u) => u.nat).sort()).toEqual(['JAP', 'JAP', 'MAN']);
      expect(moveCorps(g, red, hailar)).toBe(false);
      // With the garrison set aside the frontier itself is open to him: the ground is at war with him, and shelters nobody.
      hailar.units = [];
      expect(moveCorps(g, red, hailar)).toBe(true);
      // A Soviet corps may stand in Mongolia in the same way.
      const h = grand(who('MGL'));
      const east = h.regions.find((r) => r.name === 'Choibalsan')!;
      const from = east.nb.map((i) => h.regions[i]).find((r) => r.ctrl === 'SOV')!;
      const guest = h.corps.find((c) => c.nat === 'SOV' && !c.dead)!;
      guest.reg = from.id; from.units.push(guest);
      expect(moveCorps(h, guest, east)).toBe(true);
    });
  }

  it('T6, in the player\'s hands: every other war is his own choice, and he may turn on his patron', () => {
    for (const [client, patron] of [['MAN', 'JAP'], ['MGL', 'SOV']]) {
      const g = grand(client);
      // A small country's own card: it is in no alliance, so it may choose one or stand alone. It joins no League.
      const card = pathOptions(g, client);
      expect(card.filter((o) => o.id === 'axis' || o.id === 'allies' || o.id === 'alone').length, client).toBe(3);
      expect(card.some((o) => (o.id === 'found' || o.id === 'join') && o.ok), client).toBe(false);
      // The patron is on his list of countries to declare war on, and the war is real: the two are not allies.
      expect(declarable(g), client).toContain(patron);
      declareWar(g, client, patron);
      expect(atWar(g, client, patron), client).toBe(true);
    }
  });

  it('T6, the tie broken by the client\'s own act: the row reads only while client and patron are at peace with each other', () => {
    // A client at war with its patron is a small country on its own from that month: the row sends it nowhere.
    const a = grand('MAN');
    declareWar(a, 'MAN', 'JAP');
    declareWar(a, 'SOV', 'JAP');
    month(a);
    expect(atWar(a, 'MAN', 'SOV')).toBe(false);
    expect(atWar(a, 'MAN', 'MGL')).toBe(false);
    // And war on it is no longer war on its patron.
    declareWar(a, 'UK', 'MAN');
    month(a);
    expect(atWar(a, 'UK', 'JAP')).toBe(false);

    // So is a client that stands in an alliance at war with its patron: the row never sets two members of one alliance at war.
    const b = grand('MGL');
    b.nations.MGL.align = 'axis';
    b.nations.JAP.align = 'axis_cobelligerent';
    declareWar(b, 'JAP', 'SOV');
    month(b);
    expect(atWar(b, 'MGL', 'JAP')).toBe(false);
    expect(atWar(b, 'MGL', 'MAN')).toBe(false);
  });

  it('when a patron surrenders its client lives on with its own ground and its own wars', () => {
    // As built, and unchanged by the mend: nothing ends a client. Written down so that a change is a choice.
    const g = grand();
    g.nations.JAP.alive = false;
    month(g);
    expect(g.nations.MAN.alive).toBe(true);
    expect(wars(g, 'MAN')).toEqual(['CHN']);
  });
});
