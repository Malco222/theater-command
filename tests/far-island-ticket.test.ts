// The island battle's ticket, Part A: the dressing (release 2, 25 November 2026; the Pacific planner's build brief
// batch3-island-ticket-brief.md and the design director's ruling of 12:39Z, B "the island battle's ticket", with its
// looks narrowed at 13:00Z, D(2)).
//
// "A card, then an island battle": on the whole-world chart the ticket the 3D game is handed for a battle on an island
// of the historian's table (TC.battleLook) carries the island's own latitude and longitude, for the sun, the weather
// and the men's front, and the look its row names: "island" (beach, reef and palms) or "jungle" (a big island's fight
// was inland). Both are looks only. The words come off the table's own columns (tools/make-far-islands.mjs, lookOf):
// jungle 1 gives "jungle"; otherwise "island" only where the islet flag is 1; otherwise no word, and the 3D game dresses
// the field by its climate from the latitude and longitude. A subarctic climate or urban ground (Attu, Kiska, Singapore)
// and Iwo Jima by name (black volcanic ash, no palms) give no word whatever their flags, until the art director has
// scored stills. The two chart islets with no row take "island" by the islet flag, a guess. The land battle's half
// (the header's field.look and sky.latLon behind the "island" row of the rules card) is the ground battles thread's,
// with its own tests (tests/land-island.test.ts on its branch).
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });
import { describe, expect, it } from 'vitest';

// The island's two reads can be shut off for a look (stub.off): the ticket then built has no island in it, the same
// new bridge with its island answers stubbed (tests/island-ticket-off.test.ts says how the old bridge was compared).
const stub = vi.hoisted(() => ({ off: false }));
vi.mock('../src/campaign/islands', async (original) => {
  const real = await original<typeof import('../src/campaign/islands')>();
  return {
    ...real,
    islandOf: (...a: Parameters<typeof real.islandOf>) => (stub.off ? null : real.islandOf(...a)),
    islandLook: (...a: Parameters<typeof real.islandLook>) => (stub.off ? '' : real.islandLook(...a)),
  };
});

import { battleCalls } from '../unreal/TheaterCommand/Rules/battle-bridge';
import { latLonOf } from '../src/battle/air/sky';
import type { FieldResult } from '../src/battle/core/types';
import { landTicket } from '../src/battle/land/ticket';
import { newCampaign, offers } from '../src/battle/ui/campaign';
import { addCorps, atWar, hostileUnits, removeCorps, setWar } from '../src/campaign/game';
import { islandLook, islandNames, islandOf } from '../src/campaign/islands';
import { saveGame } from '../src/campaign/save';
import { FAR_STAGE } from '../src/campaign/stage';
import { candidates, orderAttack } from '../src/campaign/turn';
import type { Corps, Game } from '../src/campaign/types';
import file from '../data/campaign/far-islands.json';
// @ts-expect-error a tool script, plain JavaScript
import { islandRows, lookOf } from '../tools/make-far-islands.mjs';

type Ticket = Record<string, unknown> & { seed: number; place: Record<string, unknown>; look?: string; latLon?: [number, number] };
type Look = { ticket: Ticket | null; why?: string; live?: unknown } | null;
type Calls = ReturnType<typeof battleCalls>;

// The table's looks, stated here so that a changed table shows: 17 jungle, 10 with no word, and the other 16 "island".
const JUNGLE = ['Guam', 'Palau', 'Port Moresby', 'Rabaul', 'Guadalcanal', 'New Georgia', 'Bougainville', 'Admiralty Islands', 'Hollandia', 'Biak',
  'Morotai', 'Espiritu Santo', 'Fiji', 'Samoa', 'Ponape', 'Wewak', 'Halmahera'];
// (in the table's order: the seven large islands that are no islet and have no jungle, and the four named by the ruling)
const NO_WORD = ['Pearl Harbor', 'Iwo Jima', 'Okinawa', 'Formosa', 'Taihoku', 'New Caledonia', 'Attu', 'Kiska', 'Singapore', 'Hong Kong'];
const NO_ROW = ['Addu Atoll', 'Seychelles'];
// The islands an army can march onto from next door, so the only ones an attack card can open a field battle on
// before a landing has a ticket of its own (Part B): 9 of the 43.
const BY_LAND = ['Pearl Harbor', 'Formosa', 'Taihoku', 'Port Moresby', 'Rabaul', 'Hollandia', 'Singapore', 'Hong Kong', 'Wewak'];
// What each of those nine tickets carries: the look ("-" where the row has no word), then latitude and longitude.
const NINE = [
  'Pearl Harbor - 21.47 -158.06', 'Formosa - 22.7 120.4', 'Taihoku - 24.95 121.45', 'Port Moresby jungle -9.45 147.2',
  'Rabaul jungle -4.25 152.12', 'Hollandia jungle -2.53 140.72', 'Singapore - 1.35 103.82', 'Hong Kong - 22.42 114.14',
  'Wewak jungle -3.55 143.63',
];

function rig(human = 'JAP'): { g: Game; TC: Calls } {
  const g = newCampaign(3, human);
  const TC = battleCalls({ game: () => g, attack: (ids, to, f, field) => ({ report: orderAttack(g, ids.map((i) => g.corps.find((c) => c.id === i)!), g.regions[to], f, field) }) });
  return { g, TC };
}

/** The place's own point on the chart in degrees, to a hundredth, as the rules give it. */
const pointOf = (g: Game, name: string): [number, number] => {
  const [lat, lon] = latLonOf(g.byName[name].x, g.byName[name].y);
  return [Math.round(lat * 100) / 100 + 0, Math.round(lon * 100) / 100 + 0];
};

/**
 * An attack on a place by the player's own army from the ground next door, staged by hand: the war set, the ground
 * next door (the neighbour named, or the first) cleared of the enemy and a corps of his put on it, and a garrison of
 * the holder's where the place stands empty. Only the look is read from it.
 */
function attackOn(g: Game, TC: Calls, name: string, from?: string): { corps: Corps; ids: number[]; to: number; look: Look } {
  const me = g.human!, target = g.byName[name];
  const start = from ? g.byName[from] : g.regions[target.nb[0]];
  expect(target.nb, `${name} from ${start.name}`).toContain(start.id);
  setWar(g, me, target.ctrl);
  for (const c of g.corps.filter((x) => x.reg === start.id && atWar(g, x.nat, me))) removeCorps(g, c);
  if (!hostileUnits(g, target, me).some((c) => c.kind !== 'fort')) addCorps(g, target.ctrl, `${name} garrison (staged)`, 'inf', 1, target.id, {});
  const corps = addCorps(g, me, `Corps before ${name} (staged)`, 'inf', 3, start.id, {});
  expect(g.regions[corps.reg].name, `${name}: the staged corps stands next door`).toBe(start.name);
  return { corps, ids: [corps.id], to: target.id, look: TC.battleLook([corps.id], target.id) as Look };
}

describe("the islands' looks, by the table's own columns", () => {
  it('the rule: jungle 1 is jungle; otherwise an islet is island; otherwise no word; subarctic, urban and Iwo Jima have none', () => {
    // (name, ground, jungle, climate, islet), as the table's columns read
    expect(lookOf('Guadalcanal', 'forest', '1', 'tropical_rainforest', '0')).toBe('jungle');
    expect(lookOf('Guam', 'hills', '1', 'tropical_rainforest', '1')).toBe('jungle');
    expect(lookOf('Tarawa', 'plains', '0', 'tropical_rainforest', '1')).toBe('island');
    // a large island that is no islet and has no jungle: no word, its climate dresses it
    expect(lookOf('Okinawa', 'hills', '0', 'subtropical', '0')).toBe('');
    expect(lookOf('Pearl Harbor', 'hills', '0', 'tropical_savanna', '0')).toBe('');
    // no word whatever the flags
    expect(lookOf('Kiska', 'hills', '0', 'subarctic', '1')).toBe('');
    expect(lookOf('Attu', 'mountain', '0', 'subarctic', '0')).toBe('');
    expect(lookOf('Singapore', 'urban', '0', 'tropical_rainforest', '0')).toBe('');
    expect(lookOf('Iwo Jima', 'hills', '0', 'subtropical', '1')).toBe('');
    // (no row of the table answers two of these today; if one did, no word comes first: a look nobody has passed is not sent)
    expect(lookOf('X', 'urban', '1', 'tropical_rainforest', '1')).toBe('');
    expect(lookOf('X', 'forest', '1', 'subarctic', '1')).toBe('');
  });

  it('all 43: 17 jungle, 16 island, and no word for the ten the ruling names', () => {
    expect(FAR_STAGE).toBeTruthy();
    const { g } = rig();
    const names = islandNames();
    expect(names).toHaveLength(43);
    for (const n of [...JUNGLE, ...NO_WORD, ...NO_ROW]) expect(names, n).toContain(n);
    const said: Record<string, string[]> = { jungle: [], island: [], '': [] };
    for (const n of names) {
      const want = JUNGLE.includes(n) ? 'jungle' : NO_WORD.includes(n) ? '' : 'island';
      expect(islandLook(g, g.byName[n]), n).toBe(want);
      said[islandLook(g, g.byName[n])].push(n);
    }
    expect(said.jungle).toHaveLength(17);
    expect(said.island).toHaveLength(16);
    expect(said['']).toHaveLength(10);
    expect(said['']).toEqual(NO_WORD);
    // The file is the tool's own answer, look and all (tests/far-islands.test.ts holds the whole row to the table).
    const made = islandRows() as { islands: Record<string, [number, string, number, string]>; noRow: string[] };
    for (const n of names) expect((file as unknown as { islands: Record<string, unknown[]> }).islands[n][3], n).toBe(made.islands[n][3]);
    // The two chart islets with no row in the table take "island" by the islet flag: a guess, and the file says so.
    expect(made.noRow).toEqual(NO_ROW);
    for (const n of NO_ROW) expect(islandLook(g, g.byName[n]), n).toBe('island');
    expect((file as unknown as { _look: string })._look).toContain('(G) Addu Atoll and Seychelles have no row in the table');
    // ... and says where each stands (13:00Z, C): Addu Atoll's row is ruled but not yet in the table; the Seychelles
    // stay without one.
    expect((file as unknown as { _look: string })._look).toContain('Addu Atoll: its row is ruled (13:00Z, C');
    expect((file as unknown as { _look: string })._look).toContain('Seychelles: it stays without a row (13:00Z, C)');
    // What the rules read of a row is unchanged: an island is still its room, its floor and whether it is an islet.
    expect(islandOf(g, g.byName['Pearl Harbor'])).toEqual({ room: 2, floor: 0.5, islet: false });
    expect(Object.keys(islandOf(g, g.byName['Singapore'])!)).toEqual(['room', 'floor', 'islet']);
    // Ground that is no island of the table has no look: Oahu's other half, Tokyo, Johore, a province of Europe.
    for (const n of ['Honolulu', 'Tokyo', 'Johore', 'Paris']) expect(islandLook(g, g.byName[n]), n).toBe('');
    expect(islandLook(g, null)).toBe('');
  }, 300000);
});

describe("the island battle's ticket on the whole world", () => {
  it('Pearl Harbor from Honolulu and Singapore from Johore carry their own point and no look; Wewak next door carries jungle', () => {
    const { g, TC } = rig();
    // (staged: Japan at war with America and Britain on the first day, a corps of its own put on Honolulu and on Johore)
    const pearl = attackOn(g, TC, 'Pearl Harbor', 'Honolulu').look!.ticket!;
    // No word: Pearl Harbor is no islet and has no jungle (13:00Z, D(2)), so the 3D game dresses it by its own climate.
    expect('look' in pearl).toBe(false);
    // 21.47 N, 158.06 W: the chart's own point for Pearl Harbor, as latLonOf reads it (x 5475, y 1997.2). The brief's
    // figure, derived on paper, is the one the rules give.
    expect(pearl.latLon).toEqual([21.47, -158.06]);
    expect(pearl.latLon).toEqual(pointOf(g, 'Pearl Harbor'));
    // At the ticket's top level beside the scars, never inside the place: the land battle's header sends the place whole.
    expect('look' in pearl.place || 'latLon' in pearl.place).toBe(false);

    const singapore = attackOn(g, TC, 'Singapore', 'Johore').look!.ticket!;
    // 1.35 N, 103.82 E, and no word: a city island's look waits on the art director (the ruling's (3) and D(2)).
    expect(singapore.latLon).toEqual([1.35, 103.82]);
    expect(singapore.latLon).toEqual(pointOf(g, 'Singapore'));
    expect('look' in singapore).toBe(false);
    expect((singapore.place as { terrain: string }).terrain).toBe('urban');

    // A jungle row with a land neighbour: Wewak, from Merauke (staged: Japan at war with Australia).
    const wewak = attackOn(g, TC, 'Wewak', 'Merauke').look!.ticket!;
    expect(wewak.look).toBe('jungle');
    expect(wewak.latLon).toEqual(pointOf(g, 'Wewak'));

    // Both keys are all the island adds: with them deleted the ticket is, to the byte, the one built with the island's
    // reads shut off, which is the ticket the bridge built before.
    for (const [name, from] of [['Pearl Harbor', 'Honolulu'], ['Singapore', 'Johore'], ['Wewak', 'Merauke']]) {
      const ids = candidates(g, g.byName[name]).filter((c) => c.reg === g.byName[from].id).map((c) => c.id), to = g.byName[name].id;
      const now = TC.battleLook(ids, to) as Look;
      stub.off = true;
      const before = TC.battleLook(ids, to) as Look;
      stub.off = false;
      expect(now!.ticket!.latLon, name).toBeDefined();
      expect('latLon' in before!.ticket! || 'look' in before!.ticket!, name).toBe(false);
      delete now!.ticket!.look;
      delete now!.ticket!.latLon;
      expect(JSON.stringify(now), name).toBe(JSON.stringify(before));
    }
  }, 300000);

  it('Hong Kong attacked from Tungkun by the corps that stands there on the first day carries its point and no look', () => {
    const { g, TC } = rig();
    setWar(g, 'JAP', 'UK');
    const hk = g.byName['Hong Kong'];
    const corps = candidates(g, hk);
    expect(corps.map((c) => g.regions[c.reg].name)).toEqual(['Tungkun']);
    const t = (TC.battleLook(corps.map((c) => c.id), hk.id) as Look)!.ticket!;
    expect('look' in t).toBe(false);
    expect(t.latLon).toEqual([22.42, 114.14]);
  }, 300000);

  it('an attack into ground that is not in the table carries neither key', () => {
    const { g, TC } = rig();
    // Japan's first month: every attack it could order is into China.
    const all = offers(g);
    expect(all.length).toBeGreaterThanOrEqual(10);
    let tickets = 0;
    for (const o of all) {
      expect(islandOf(g, o.target), o.target.name).toBeNull();
      const look = TC.battleLook(o.corps.map((c) => c.id), o.target.id) as Look;
      if (!look?.ticket) continue;
      tickets++;
      expect('look' in look.ticket, o.target.name).toBe(false);
      expect('latLon' in look.ticket, o.target.name).toBe(false);
    }
    expect(tickets).toBeGreaterThanOrEqual(10);
    // And beside an island of the table: Johore, across the strait from Singapore, is the mainland (staged: Japan at
    // war with Britain, a corps of its own on Kuala Lumpur).
    const johore = attackOn(g, TC, 'Johore', 'Kuala Lumpur').look!.ticket!;
    expect((johore.place as { name: string }).name).toBe('Johore');
    expect('look' in johore || 'latLon' in johore).toBe(false);
  }, 300000);

  it('each of the nine islands an army can march onto carries its own point, and the word its row has', () => {
    // 34 of the 43 have no land neighbour: no attack card opens a field battle there until a landing has a ticket of
    // its own (the brief's Part B), so their point and look are in the rows and reach no ticket yet.
    const first = rig();
    const reach = islandNames().filter((n) => first.g.byName[n].nb.length > 0);
    expect(reach.sort()).toEqual([...BY_LAND].sort());
    expect(islandNames().length - reach.length).toBe(34);
    // (Japan's own two, Formosa and Taihoku, are attacked by a staged American army; the other seven by Japan's)
    const usa = rig('USA');
    const seen: string[] = [];
    for (const name of BY_LAND) {
      const { g, TC } = first.g.byName[name].ctrl === 'JAP' ? usa : first;
      const staged = attackOn(g, TC, name);
      const look = staged.look;
      expect(look?.ticket, `${name}: ${look?.why ?? 'no battle'}`).toBeTruthy();
      const t = look!.ticket!;
      expect(t.latLon, name).toEqual(pointOf(g, name));
      const word = JUNGLE.includes(name) ? 'jungle' : NO_WORD.includes(name) ? undefined : 'island';
      // (none of the nine is an islet: a player sees jungle on four and no word on five before a landing has a ticket)
      expect(word, name).not.toBe('island');
      expect(t.look, name).toBe(word);
      expect('look' in t, name).toBe(word !== undefined);
      seen.push(`${name} ${t.look ?? '-'} ${t.latLon!.join(' ')}`);
      // (the staged corps leaves the board, so the next island's staging starts clean)
      removeCorps(g, staged.corps);
    }
    // Stated whole, so a moved chart point or a changed row shows:
    expect(seen).toEqual(NINE);
  }, 600000);
});

describe('no rule reads the two keys (the ruling\'s (4)): the battle settled and the battle started are built from tickets of their own', () => {
  // Wewak from Merauke, staged the same way each time (a jungle row, so its look carries both keys): Japan at war with
  // Australia, a corps of Japan's put next door.
  const open = (): { g: Game; TC: Calls; corps: Corps[]; ids: number[]; to: number } => {
    const { g, TC } = rig();
    const staged = attackOn(g, TC, 'Wewak', 'Merauke');
    return { g, TC, corps: [staged.corps], ids: staged.ids, to: staged.to };
  };
  const WEWAK = (g: Game) => pointOf(g, 'Wewak');

  it('a battle fought in the 3D game\'s land rules from the look and handed back: the same result and the same game with both keys deleted', async () => {
    // The 3D game's own doorway to the land rules (its TC is another engine's; here it is this file's global).
    await import('../unreal/TheaterCommand/Rules/land-bridge');
    const Land = (globalThis as unknown as { TC: { landStartTicket(look: unknown, opts: unknown): unknown; landTick(s: number, max?: number): unknown; landResult(): { over: FieldResult | null }; landEnd(): unknown } }).TC;
    const play = (strip: boolean): { over: string; out: string; game: string } => {
      const { g, TC, ids, to } = open();
      const look = TC.battleLook(ids, to) as Look;
      expect(look!.ticket!.look).toBe('jungle');
      expect(look!.ticket!.latLon).toEqual(WEWAK(g));
      if (strip) { delete look!.ticket!.look; delete look!.ticket!.latLon; }
      // The whole answer goes to the land rules as it stands; both sides are fought by the rules' own staffs.
      // A limit, said plainly: in this tree the land rules have no reader of either key at all (the ground battles
      // thread's island option, which reads them into the header, is on its own branch), so this case cannot fail
      // here. Once that branch is joined it still fights with the island option at its default, off; the same case
      // with { me: null, island: true } belongs beside it then, and should give the same result and the same game.
      Land.landStartTicket(look, { me: null });
      for (let i = 0; i < 2000 && !Land.landResult().over; i++) Land.landTick(60, 5000);
      const result = Land.landResult();
      expect(result.over).not.toBeNull();
      const out = TC.battleSettleLand(ids, to, structuredClone(result), look!.ticket!.seed);
      Land.landEnd();
      expect((out as { report?: unknown } | null)?.report).toBeTruthy();
      return { over: JSON.stringify(result.over), out: JSON.stringify(out), game: saveGame(g) };
    };
    const dressed = play(false), bare = play(true);
    expect(dressed.over).toBe(bare.over);
    expect(dressed.out).toBe(bare.out);
    expect(dressed.game).toBe(bare.game);
  }, 600000);

  it('battleStart and battleSettleLand build their tickets afresh, with neither key, and settle one battle to one game', () => {
    const keysOf = (v: unknown, out = new Set<string>()): Set<string> => {
      if (Array.isArray(v)) for (const x of v) keysOf(x, out);
      else if (v && typeof v === 'object') for (const [k, x] of Object.entries(v)) { out.add(k); keysOf(x, out); }
      return out;
    };
    // One game: the look is asked (it carries both keys), then the battle is started and fought by the campaign's own
    // bridge, his staff commanding, and ended.
    const a = open();
    const looked = (a.TC.battleLook(a.ids, a.to) as Look)!.ticket!;
    expect(looked.look).toBe('jungle');
    expect(looked.latLon).toEqual(WEWAK(a.g));
    // (the ticket both calls build, landTicket's own, carries neither)
    const own = landTicket(a.g, a.corps, a.g.regions[a.to])!;
    expect('look' in own || 'latLon' in own).toBe(false);
    expect(own.seed).toBe(looked.seed);
    const started = a.TC.battleStart(a.ids, a.to, { staff: true });
    const sent = keysOf(started);
    expect(sent.has('look') || sent.has('latLon')).toBe(false);
    a.TC.battleSpeed(4);
    let over: FieldResult | null = null;
    for (let i = 0; i < 20000 && !over; i++) over = (a.TC.battleTick(1) as { over: FieldResult | null }).over;
    expect(over).not.toBeNull();
    const ended = a.TC.battleEnd();
    // Its twin: the same look asked, and that field's result handed back as the 3D game hands one back. The same
    // answer and the same game, to the byte: nothing of the look's dress reached either settlement.
    const b = open();
    const seed = (b.TC.battleLook(b.ids, b.to) as Look)!.ticket!.seed;
    const back = b.TC.battleSettleLand(b.ids, b.to, { over: structuredClone(over!) }, seed);
    expect(JSON.stringify(back)).toBe(JSON.stringify(ended));
    expect(saveGame(b.g)).toBe(saveGame(a.g));
  }, 600000);
});
