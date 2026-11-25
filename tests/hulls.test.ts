// The names of the battleships and carriers that go down, for the card that marks one lost (Brian's memorial card):
// a record only, drawn apart from the war's dice.
import { describe, expect, it } from 'vitest';
import hulls from '../data/campaign/hulls.json';
import navy from '../data/campaign/navy.json';
import world from '../data/campaign/world.json';
import { createGame } from '../src/campaign/game';
import { hullAt, nameLosses } from '../src/campaign/hulls';
import { sail, seaBattles, zoneOf } from '../src/campaign/navy';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, endHuman, humanUp, orderRaid, stepAI } from '../src/campaign/turn';
import { syncWings, wingsOf } from '../src/campaign/carriers';
import type { Fleet, Game, Ships } from '../src/campaign/types';

type Row = { name: string; crew: number; from?: number; fleet?: string; pair?: boolean; type?: 'battlecruiser'; later?: { from: number; name?: string; fleet?: string }[] };
const NAVIES = hulls.navies as unknown as Record<string, Partial<Record<'bb' | 'cv', Row[]>>>;
const lose = (bb: number, cv = 0): Ships => ({ bb, cv, ca: 0, dd: 0, ss: 0 });
const fleet = (g: Game, name: string): Fleet => g.fleets.find((f) => f.name === name)!;
const rows = (nat: string, cls: 'bb' | 'cv'): Row[] => NAVIES[nat][cls]!;
// The names a nation's ships bear in a month (turn), of those `pick` takes; `fleetAt` is a ship's fleet in that month.
const names = (nat: string, cls: 'bb' | 'cv', pick: (r: Row) => boolean, turn = 1): string[] => rows(nat, cls).filter(pick).map((r) => hullAt(r, turn).name);
const fleetAt = (r: Row, turn: number): string | undefined => hullAt(r, turn).fleet;

describe('the record of names', () => {
  it('gives every fleet that begins with battleships or carriers as many names as it has ships', async () => {
    const g = createGame(1, null);
    for (const f of g.fleets) for (const cls of ['bb', 'cv'] as const) {
      if (!f.ships[cls]) continue;
      expect(names(f.nat, cls, (r) => fleetAt(r, 1) === f.name && (r.from ?? 1) <= 1).length, `${f.name} (${f.nat}) ${cls}`).toBeGreaterThanOrEqual(f.ships[cls]);
    }
    // And every fleet row of the record, the far side's and the Americas' too, from the month it forms.
    type FleetRow = { nat: string; name: string; from?: number; ships: Partial<Ships> };
    // (the far side's file, where this tree has it)
    const farFile = '../data/campaign/far-world.json';
    const far = await import(/* @vite-ignore */ farFile).then((m) => m.default).catch(() => ({ world: { fleets: [] }, navy: { fleets: [] } }));
    const fleetRows = [...navy.fleets, ...world.fleets, ...far.world.fleets, ...far.navy.fleets] as unknown as FleetRow[];
    expect(fleetRows.length).toBeGreaterThan(navy.fleets.length);
    // (A far row is found under the far chart's fleet for its class where the record names the old chart's: 'far',
    // America's carriers, which are the Pacific Fleet's on the old chart and a fleet of their own on the far one.)
    const farRows = new Set<unknown>([...far.world.fleets, ...far.navy.fleets]);
    const alias = (hulls as unknown as { far?: Record<string, Partial<Record<'bb' | 'cv', Record<string, string>>>> }).far ?? {};
    for (const f of fleetRows) for (const cls of ['bb', 'cv'] as const) {
      if (!f.ships[cls]) continue;
      const at = (r: Row): string | undefined => { const x = fleetAt(r, f.from ?? 1); return farRows.has(f) && x !== undefined ? alias[f.nat]?.[cls]?.[x] ?? x : x; };
      expect(names(f.nat, cls, (r) => at(r) === f.name && (r.from ?? 1) <= (f.from ?? 1)).length, `${f.name} (${f.nat}) ${cls}`).toBeGreaterThanOrEqual(f.ships[cls]!);
    }
    for (const nat of Object.keys(NAVIES)) for (const cls of ['bb', 'cv'] as const) for (const r of NAVIES[nat][cls] ?? []) {
      expect(r.crew, r.name).toBeGreaterThan(400);
      expect(r.crew, r.name).toBeLessThan(3500);
    }
    for (const cls of ['bb', 'cv'] as const) for (const s of hulls.share[cls]) { expect(s).toBeGreaterThan(0); expect(s).toBeLessThanOrEqual(1); }
    // A ship's changes come after she joins, in the order of the months (the namer takes the last that has come).
    for (const nat of Object.keys(NAVIES)) for (const cls of ['bb', 'cv'] as const) for (const r of NAVIES[nat][cls] ?? []) {
      let at = r.from ?? 1;
      for (const x of r.later ?? []) { expect(x.from, r.name).toBeGreaterThan(at); at = x.from; }
    }
  });
});

describe('a battleship or carrier that goes down', () => {
  it('is one still afloat of the fleet that lost her, with her crew and a real share of it lost, and never named twice', () => {
    const g = createGame(4, null);
    const home = names('UK', 'bb', (r) => fleetAt(r, 1) === 'Home Fleet' && (r.from ?? 1) <= 1);
    const seen = new Set<string>();
    for (let k = 0; k < home.length; k++) {
      const [h] = nameLosses(g, 'UK', 'Home Fleet', lose(1), `t:${k}`);
      expect(home).toContain(h.name);
      expect(seen.has(h.name!)).toBe(false);
      seen.add(h.name!);
      const row = rows('UK', 'bb').find((r) => r.name === h.name)!;
      expect(h).toMatchObject({ cls: 'bb', crew: row.crew });
      expect(hulls.share.bb.map((s) => Math.round(row.crew * s))).toContain(h.lost);
    }
    // The Home Fleet's own are all gone: the next is another of Britain's afloat, never one not yet built.
    const [next] = nameLosses(g, 'UK', 'Home Fleet', lose(1), 't:next');
    expect(home).not.toContain(next.name);
    expect(names('UK', 'bb', (r) => (r.from ?? 1) <= g.turn)).toContain(next.name);
    expect(names('UK', 'bb', (r) => (r.from ?? 1) > g.turn)).not.toContain(next.name);
  });

  it('is nameless once the record runs out, with a crew of her class', () => {
    const g = createGame(4, null);
    const all = names('GER', 'bb', (r) => (r.from ?? 1) <= 1);
    const out = nameLosses(g, 'GER', 'Battle Fleet', lose(all.length + 1), 'k');
    expect(out.slice(0, all.length).map((h) => h.name).sort()).toEqual([...all].sort());
    expect(out[all.length]).toMatchObject({ name: null, cls: 'bb', crew: hulls.crew.bb });
    // A navy with no record at all: nameless too.
    expect(nameLosses(g, 'NED', 'Dutch East Indies Fleet', lose(0, 1), 'k')).toEqual([{ name: null, cls: 'cv', crew: hulls.crew.cv, lost: expect.any(Number) }]);
  });

  it('two light carriers counted as one go down together', () => {
    const g = createGame(4, null);
    g.turn = 48;
    rows('USA', 'cv').forEach((r, i) => { if (!r.pair) g.flags[`hull:USA:cv:${i}`] = 1; });
    const [h] = nameLosses(g, 'USA', 'Pacific Carrier Fleet', lose(0, 1), 'k');
    expect(h).toMatchObject({ name: 'Independence and Princeton', pair: true, crew: 3138 });
  });

  it('bears the name she had that month, and is drawn from the fleet she was with that month (the historian\'s check)', () => {
    // Marat is Petropavlovsk from May 1943 (turn 45), Coral Sea is Anzio from September 1944 (turn 61).
    const sov = (turn: number): string | null => { const x = createGame(4, null); x.turn = turn; x.flags['hull:SOV:bb:1'] = 1; return nameLosses(x, 'SOV', 'Baltic Fleet', lose(1), 'k')[0].name; };
    expect(sov(44)).toBe('Marat');
    expect(sov(45)).toBe('Petropavlovsk');
    const coral = rows('USA', 'cv').findIndex((r) => r.name === 'Coral Sea');
    expect(hullAt(rows('USA', 'cv')[coral], 60).name).toBe('Coral Sea');
    expect(hullAt(rows('USA', 'cv')[coral], 61).name).toBe('Anzio');
    // Ark Royal is with the Home Fleet from September 1939 and with Force H from turn 11.
    const ark = rows('UK', 'cv').find((r) => r.name === 'Ark Royal')!;
    expect(names('UK', 'cv', (r) => fleetAt(r, 1) === 'Home Fleet' && (r.from ?? 1) <= 1)).toContain('Ark Royal');
    expect(fleetAt(ark, 11)).toBe('Force H');
    const forceH = new Set<string>();
    for (let s = 1; s <= 8; s++) { const x = createGame(s, null); x.turn = 11; forceH.add(nameLosses(x, 'UK', 'Force H', lose(0, 1), 'k')[0].name!); }
    expect([...forceH]).toEqual(['Ark Royal']);
    // Hood, Renown and Repulse were battlecruisers: the card can print it.
    for (const r of rows('UK', 'bb')) expect(r.type === 'battlecruiser', r.name).toBe(['Hood', 'Renown', 'Repulse'].includes(r.name));
    const bc = (() => { const x = createGame(4, null); rows('UK', 'bb').forEach((r, i) => { if (r.name !== 'Hood') x.flags[`hull:UK:bb:${i}`] = 1; }); return nameLosses(x, 'UK', 'Home Fleet', lose(1), 'k')[0]; })();
    expect(bc).toMatchObject({ name: 'Hood', type: 'battlecruiser' });
  });

  it('is the same for the same game and fight, and leaves the war\'s dice alone', () => {
    const a = createGame(9, null), b = createGame(9, null);
    const dice = a.rng.state;
    expect(nameLosses(a, 'JAP', 'Combined Fleet', lose(2, 1), 'sea:3:d0')).toEqual(nameLosses(b, 'JAP', 'Combined Fleet', lose(2, 1), 'sea:3:d0'));
    expect(a.rng.state).toBe(dice);
    // Other games name other ships.
    const seen = new Set<string>();
    for (let s = 1; s <= 12; s++) seen.add(nameLosses(createGame(s, null), 'JAP', 'Combined Fleet', lose(1), 'sea:3:d0')[0].name!);
    expect(seen.size).toBeGreaterThan(2);
  });
});

describe('the fights that sink them', () => {
  it('a fleet action names each side\'s battleships and carriers lost on its report, and the names keep with the game', () => {
    let found = false;
    for (let seed = 1; seed <= 40 && !found; seed++) {
      const g = createGame(seed, null);
      const home = fleet(g, 'Home Fleet'), battle = fleet(g, 'Battle Fleet');
      const norw = zoneOf(g, 'norw');
      sail(g, home, norw, 'patrol');
      sail(g, battle, norw, 'patrol');
      const dice = g.rng.state;
      const r = seaBattles(g, 'UK')[0];
      expect(g.rng.state).toBe(dice);
      for (const side of [...r.att, ...r.def]) {
        const big = side.sunk.bb + side.sunk.cv;
        expect(side.hulls?.length ?? 0).toBe(big);
        for (const h of side.hulls ?? []) expect(names(side.nat, h.cls, (x) => fleetAt(x, g.turn) === side.fleet, g.turn)).toContain(h.name);
      }
      const d = r.def[0];
      if (!d.sunk.bb) continue;
      found = true;
      const back = loadGame(saveGame(g))!;
      expect(back.seaReports.at(-1)!.def[0].hulls).toEqual(d.hulls);
      const i = rows('GER', 'bb').findIndex((x) => x.name === d.hulls![0].name);
      expect(back.flags[`hull:GER:bb:${i}`]).toBe(g.turn);
    }
    expect(found).toBe(true);
  });

  it('a carrier strike names what it sank on its report', () => {
    let found = false;
    for (let seed = 1; seed <= 30 && !found; seed++) {
      const g = startOpen(createGame(seed, 'UK'));
      for (let m = 0; m < 12; m++) {
        let guard = 0;
        while (!humanUp(g) && guard++ < 200) stepAI(g);
        beginHuman(g);
        if (g.nations.UK.atWar.length && g.nations.GER.atWar.includes('UK')) break;
        endHuman(g);
      }
      const home = fleet(g, 'Home Fleet'), foe = fleet(g, 'Battle Fleet');
      home.ships.cv = 6;
      for (const f of [home, foe]) { f.zone = zoneOf(g, 'natl'); f.stance = 'patrol'; f.lane = undefined; }
      syncWings(g);
      const strike = wingsOf(g, home.id).find((w) => w.kind === 'bmr')!;
      const rep = orderRaid(g, 'fleet', foe.id, [strike.id])!;
      const side = rep.ships![0];
      expect(side.hulls?.length ?? 0).toBe(side.sunk.bb + side.sunk.cv);
      if (!side.sunk.bb) continue;
      found = true;
      expect(names('GER', 'bb', () => true, g.turn)).toContain(side.hulls![0].name);
    }
    expect(found).toBe(true);
  });
});
