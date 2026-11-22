// The sky over an air battle (src/battle/air/sky.ts): a cloud deck under the bombers only on a day history had one
// (the art director's ruling of 19 November 04:40Z), drawn from data/battle/air-cloud.json by where the target lies
// and the month, from the battle's own seed. Shown, never counted: the battle reads nothing of it.
import { describe, expect, it } from 'vitest';
import cloudData from '../data/battle/air-cloud.json';
import { createGame, setWar } from '../src/campaign/game';
import { planRaid, raidTargets } from '../src/campaign/airwar';
import { startOpen } from '../src/campaign/staff';
import { setWeather, skyAt, weatherOn, WEATHER } from '../src/campaign/weather';
import { runOut } from '../src/battle/core/clock';
import { airBattle } from '../src/battle/air/sim';
import { CLOUD_THICK, CUMULUS_MOST, DECK_LEAST, cloudOf, cloudZone, coverOf, deckChance, latLonOf, skyWord } from '../src/battle/air/sky';
import { airTicket } from '../src/battle/air/ticket';

const K = Math.cos((50 * Math.PI) / 180) * 40;
/** The map's projection (tools/build-geo-far.mjs): longitude counted eastward from 141 W, 40 units a degree of latitude from 71.4 N. */
const proj = (lat: number, lon: number): [number, number] => [((lon < -141 ? lon + 360 : lon) + 11) * K, (71.4 - lat) * 40];

const PLACES: Record<string, [number, number, string | null]> = {
  London: [51.5, -0.1, 'Northwest and central Europe'],
  Ploesti: [44.9, 26.0, 'Po valley, Danube basin'],
  Tokyo: [35.7, 139.7, 'Japan Pacific side'],
  Honolulu: [21.3, -157.9, 'Hawaii, Midway and Johnston'],
  'New York': [40.7, -74.0, 'Northeastern United States'],
  'mid-Atlantic': [40, -40, null],
};

describe('the cloud table', () => {
  const zones = cloudData.zones;
  it('has twelve months of chances in 0..1 in every box, and ranges that run upward', () => {
    for (const z of [...zones, cloudData.fallback]) {
      expect(z.months).toHaveLength(12);
      for (const p of z.months) expect(p >= 0 && p <= 1).toBe(true);
      expect(z.base_m[0]).toBeLessThanOrEqual(z.base_m[1]);
      expect(z.tops_m[0]).toBeLessThanOrEqual(z.tops_m[1]);
    }
    for (const z of cloudData.zones) for (const x of z.seasons ?? []) {
      for (const m of x.months) expect(Number.isInteger(m) && m >= 0 && m < 12, z.zone).toBe(true);
      expect(x.base_m[0]).toBeLessThanOrEqual(x.base_m[1]);
      expect(x.tops_m[0]).toBeLessThanOrEqual(x.tops_m[1]);
    }
  });
  it('has no two boxes overlapping, so the order of the boxes only settles a shared edge', () => {
    for (let i = 0; i < zones.length; i++) for (let j = i + 1; j < zones.length; j++) {
      const a = zones[i].box, b = zones[j].box;
      const lat = Math.min(a.latN, b.latN) - Math.max(a.latS, b.latS), lon = Math.min(a.lonE, b.lonE) - Math.max(a.lonW, b.lonW);
      expect(lat > 0 && lon > 0, `${zones[i].zone} / ${zones[j].zone}`).toBe(false);
    }
  });
  it('every year rule names a zone in the table', () => {
    for (const a of cloudData.adjust) expect(zones.some((z) => z.zone.startsWith(a.zone))).toBe(true);
  });
});

describe('where a target lies', () => {
  it('reads latitude and longitude back from a map point on either side of the world', () => {
    for (const [name, [lat, lon]] of Object.entries(PLACES)) {
      const [x, y] = proj(lat, lon);
      const [la, lo] = latLonOf(x, y);
      expect(la, name).toBeCloseTo(lat, 6);
      expect(lo, name).toBeCloseTo(lon, 6);
    }
  });
  it('finds the zone a known target lies in, and the fallback out at sea', () => {
    for (const [name, [lat, lon, zone]] of Object.entries(PLACES)) {
      const z = cloudZone(lat, lon);
      if (zone === null) expect(z, name).toBeNull();
      else expect(z?.zone.startsWith(zone), `${name}: ${z?.zone}`).toBe(true);
    }
  });
  it("takes the first box for a region whose seed sits on a box's edge, though the map keeps x and y only to 0.1 unit", () => {
    // Seeds from data/campaign/*.mjs that lie on an edge between two boxes, and the zone their own point is in.
    const EDGE: [string, number, number, string | null][] = [
      ['Penza', 53.2, 45.0, 'Volga-Urals'], ['Hue', 16.46, 107.5, 'Annam coast'], ['Dalmatia', 43.6, 16.7, 'Italy south of 44N'],
      ['San Diego', 33.0, -116.6, 'Pacific coast of North America'], ['Aomori', 40.75, 140.8, 'Japan Sea side'], ['Acre', -9.0, -70.0, 'Amazon basin'],
      ['Nevada', 39.4, -117.0, null],
    ];
    const r1 = (v: number): number => Math.round(v * 10) / 10;
    for (const [name, lat, lon, zone] of EDGE) {
      const [x, y] = proj(lat, lon);
      const got = cloudOf(r1(x), r1(y), { year: 1944, month: 0 }, 1).zone;
      expect(got, name).toBe(cloudZone(lat, lon)?.zone ?? 'elsewhere');
      if (zone === null) expect(got, name).toBe('elsewhere');
      else expect(got.startsWith(zone), `${name}: ${got}`).toBe(true);
    }
  });
});

describe('the sky drawn for a battle', () => {
  const [lx, ly] = proj(51.5, -0.1);
  it('is the same every time for the same battle', () => {
    for (let s = 0; s < 50; s++) expect(cloudOf(lx, ly, { year: 1944, month: 1 }, s)).toEqual(cloudOf(lx, ly, { year: 1944, month: 1 }, s));
  });
  it('has a deck about as often as the table says, place by place and month by month', () => {
    const N = 4000;
    for (const [lat, lon] of Object.values(PLACES)) for (const month of [0, 6]) {
      const [x, y] = proj(lat, lon);
      const when = { year: 1944, month };
      const p = deckChance(cloudZone(lat, lon), when);
      let decks = 0;
      for (let s = 0; s < N; s++) if (cloudOf(x, y, when, 7919 * s + 13).kind === 'deck') decks++;
      expect(Math.abs(decks / N - p), `${lat},${lon} month ${month}: ${decks / N} vs ${p}`).toBeLessThan(0.03);
    }
  });
  it('draws one base and one top inside the table ranges, the tops at least 300 m above the base', () => {
    for (const [lat, lon] of Object.values(PLACES)) {
      const [x, y] = proj(lat, lon);
      for (let s = 0; s < 500; s++) {
        const c = cloudOf(x, y, { year: 1943, month: s % 12 }, s);
        expect(c.base).toBeGreaterThanOrEqual(c.baseRange[0]);
        expect(c.base).toBeLessThanOrEqual(c.baseRange[1]);
        expect(c.top - c.base).toBeGreaterThanOrEqual(CLOUD_THICK);
        expect(c.top).toBeGreaterThanOrEqual(c.topsRange[0]);
        expect(c.top).toBeLessThanOrEqual(Math.max(c.topsRange[1], c.base + CLOUD_THICK));
      }
    }
  });
  it("reads each month's chance and each place's ranges straight from the table", () => {
    for (const z of cloudData.zones) {
      const lat = (z.box.latS + z.box.latN) / 2, lon = (z.box.lonW + z.box.lonE) / 2;
      expect(cloudZone(lat, lon)?.zone).toBe(z.zone);
      const [x, y] = proj(lat, lon);
      for (let m = 0; m < 12; m++) {
        expect(deckChance(cloudZone(lat, lon), { year: 1944, month: m }), `${z.zone} month ${m}`).toBe(z.months[m]);
        const c = cloudOf(x, y, { year: 1944, month: m }, m);
        expect(c.chance).toBe(z.months[m]);
        // A season's own heights where the table gives them (three zones), else the zone's.
        const h = z.seasons?.find((x) => x.months.includes(m)) ?? z;
        expect(c.baseRange, `${z.zone} month ${m}`).toEqual(h.base_m);
        expect(c.topsRange, `${z.zone} month ${m}`).toEqual(h.tops_m);
      }
    }
    const [ox, oy] = proj(40, -40);
    for (let m = 0; m < 12; m++) {
      const c = cloudOf(ox, oy, { year: 1944, month: m }, m);
      expect([c.zone, c.chance]).toEqual(['elsewhere', cloudData.fallback.months[m]]);
      expect([c.baseRange, c.topsRange]).toEqual([cloudData.fallback.base_m, cloudData.fallback.tops_m]);
    }
  });
  it('takes the date rules: north-west Europe before October 1943 (blind bombing) is a tenth less likely to be under a deck', () => {
    const nw = cloudData.zones.find((z) => z.zone.startsWith('Northwest and central Europe'))!;
    const at = (year: number, month: number) => deckChance(cloudZone(51.5, -0.1), { year, month });
    for (let m = 0; m < 12; m++) expect(at(1942, m)).toBeCloseTo(Math.max(0, nw.months[m] - 0.1), 6);
    for (let m = 0; m < 9; m++) expect(at(1943, m)).toBeCloseTo(Math.max(0, nw.months[m] - 0.1), 6);
    for (let m = 9; m < 12; m++) expect(at(1943, m)).toBe(nw.months[m]);
    for (let m = 0; m < 12; m++) expect(at(1944, m)).toBe(nw.months[m]);
    const t = cloudZone(35.7, 139.7);
    expect(deckChance(t, { year: 1943, month: 0 })).toBe(deckChance(t, { year: 1945, month: 0 }));
  });
});

describe('how much of the sky is cloud (Brian, 20 November: not always cloudy, some days more cloudy than others)', () => {
  it('names the cover by the eighths: clear, fair, scattered, broken, overcast', () => {
    expect([0, 0.06, 0.13, 0.31, 0.32, 0.56, 0.57, 0.93, 0.94, 1].map(skyWord)).toEqual(
      ['clear', 'clear', 'fair', 'fair', 'scattered', 'scattered', 'broken', 'broken', 'overcast', 'overcast']);
  });
  it('keeps a deck broken to overcast and a cumulus day clear to scattered', () => {
    for (let i = 0; i <= 100; i++) {
      const u = i / 100;
      expect(coverOf('deck', 0.5, u)).toBeGreaterThanOrEqual(DECK_LEAST);
      expect(coverOf('deck', 0.5, u)).toBeLessThanOrEqual(1);
      for (const p of [0, 0.3, 1]) {
        expect(coverOf('cumulus', p, u)).toBeGreaterThanOrEqual(0);
        expect(coverOf('cumulus', p, u)).toBeLessThanOrEqual(CUMULUS_MOST);
      }
    }
  });
  it("gives every kind of day somewhere: clear, fair, scattered, broken and overcast all come up, the word always the cover's", () => {
    const N = 3000;
    const seen: Record<string, number> = {};
    for (const [lat, lon] of Object.values(PLACES)) for (const month of [0, 3, 6, 9]) {
      const [x, y] = proj(lat, lon);
      for (let s = 0; s < N / 4; s++) {
        const c = cloudOf(x, y, { year: 1944, month }, 104729 * s + 7);
        expect(c.sky).toBe(skyWord(c.cover));
        expect(c.kind === 'deck' ? c.cover >= DECK_LEAST : c.cover <= CUMULUS_MOST).toBe(true);
        seen[c.sky] = (seen[c.sky] ?? 0) + 1;
      }
    }
    for (const w of ['clear', 'fair', 'scattered', 'broken', 'overcast']) expect(seen[w] ?? 0, w).toBeGreaterThan(N / 40);
  });
  it('clears the sky more often where decks are rare: about three cumulus days in ten clear at a chance of 0.1, one in ten at 0.6', () => {
    const N = 4000;
    const clear = (p: number) => {
      let n = 0;
      for (let i = 0; i < N; i++) if (skyWord(coverOf('cumulus', p, (i + 0.5) / N)) === 'clear') n++;
      return n / N;
    };
    expect(clear(0.1)).toBeGreaterThan(0.25);
    expect(clear(0.1)).toBeLessThan(0.35);
    expect(clear(0.6)).toBeGreaterThan(0.06);
    expect(clear(0.6)).toBeLessThan(0.14);
  });
  it('closes about one deck in five', () => {
    const N = 4000;
    let n = 0;
    for (let i = 0; i < N; i++) if (skyWord(coverOf('deck', 0.5, (i + 0.5) / N)) === 'overcast') n++;
    expect(n / N).toBeGreaterThan(0.17);
    expect(n / N).toBeLessThan(0.26);
  });
});

describe('the ticket carries the sky', () => {
  function ticket() {
    const g = startOpen(createGame(7, 'GER'));
    setWar(g, 'GER', 'UK'); setWar(g, 'GER', 'FRA');
    const t = raidTargets(g, 'bomb', 'GER').find((x) => planRaid(g, 'bomb', x.id, undefined, 'GER').place === 'Alsace')!;
    const day = planRaid(g, 'bomb', t.id, undefined, 'GER');
    const ids = [...day.picked, ...day.seats.filter((s) => !s.why && !s.picked && s.kind === 'ftr').map((s) => s.id)];
    return { g, tk: airTicket(g, 'bomb', t.id, ids, 'GER').ticket! };
  }
  it("is drawn for the target's region and month from the battle's seed", () => {
    const { g, tk } = ticket();
    const T = g.regions[tk.place.target];
    expect(tk.place.cloud).toEqual(cloudOf(T.x, T.y, tk.place.when!, tk.seed));
    expect(tk.place.cloud!.zone.startsWith('Northwest and central Europe')).toBe(true);
  });
  it('changes nothing the battle counts: the same battle without its sky ends the same', () => {
    const { tk } = ticket();
    const sky = (cloud: typeof tk.place.cloud) => ({ ...tk, place: { ...tk.place, cloud } });
    const play = (t: typeof tk) => {
      const s = airBattle(t); s.ai.att = true; s.ai.def = true; runOut(s);
      return { outcome: s.outcome(), history: s.history().map((e) => `${e.t} ${e.what} ${e.unit ?? ''} ${e.text}`) };
    };
    const bare = play(sky(undefined));
    expect(play(tk)).toEqual(bare);
    // Whatever the draw: a deck from the ground to well above the bombers, and high thin cumulus, play out the same.
    expect(play(sky({ ...tk.place.cloud!, kind: 'deck', base: 300, top: 9000, chance: 1 }))).toEqual(bare);
    expect(play(sky({ ...tk.place.cloud!, kind: 'cumulus', base: 4000, top: 6000, chance: 0 }))).toEqual(bare);
    // And a closed overcast plays out as a clear day does.
    expect(play(sky({ ...tk.place.cloud!, kind: 'deck', cover: 1, sky: 'overcast' }))).toEqual(play(sky({ ...tk.place.cloud!, kind: 'cumulus', cover: 0, sky: 'clear' })));
  });
});

describe("the sky follows the rules' weather (ruling 127; Brian's standing rule: the weather varies battle to battle from one draw, and the 3D uses it)", () => {
  /** The play file's k-th battle of the German raid on Alsace in a month, with the campaign's weather on; null when it cannot fly. */
  function battle(k: number, turn: number, wx = true) {
    const g = startOpen(createGame(7, 'GER'));
    setWar(g, 'GER', 'UK'); setWar(g, 'GER', 'FRA');
    setWeather(g, wx);
    g.seed = (7 + 7919 * k) >>> 0;
    g.turn = turn;
    const t = raidTargets(g, 'bomb', 'GER').find((x) => planRaid(g, 'bomb', x.id, undefined, 'GER').place === 'Alsace');
    if (!t) return null;
    const day = planRaid(g, 'bomb', t.id, undefined, 'GER');
    if (day.why) return null;
    const ids = [...day.picked, ...day.seats.filter((s) => !s.why && !s.picked && s.kind === 'ftr').map((s) => s.id)];
    const tk = airTicket(g, 'bomb', t.id, ids, 'GER').ticket;
    return tk ? { g, tk, sky: skyAt(g, t.id) } : null;
  }

  it("draws a deck under the card's cloud and cumulus under its clear sky; both come up from battle to battle", () => {
    const hits = new Map<string, number>();
    let flown = 0;
    for (let k = 0; k < 12; k++) for (const turn of [1, 4, 7, 10]) {
      const b = battle(k, turn);
      if (!b) continue;
      flown++;
      const c = b.tk.place.cloud!;
      // The ticket the 3D game is handed carries the campaign's own sky, and its deck or cumulus is that sky's.
      expect(c.weather, `battle ${k}, turn ${turn}`).toBe(b.sky);
      expect(c.kind, `battle ${k}, turn ${turn}`).toBe(b.sky === 'clear' ? 'cumulus' : 'deck');
      // The heights are still the battle's own draw inside the table's ranges.
      const T = b.g.regions[b.tk.place.target];
      expect(c).toEqual(cloudOf(T.x, T.y, b.tk.place.when!, b.tk.seed, b.sky));
      expect(c.base).toBeGreaterThanOrEqual(c.baseRange[0]);
      expect(c.base).toBeLessThanOrEqual(c.baseRange[1]);
      hits.set(`${turn}:${b.sky}`, b.tk.place.bombs!.hit);
    }
    expect(flown).toBeGreaterThan(24);
    const skies = new Set([...hits.keys()].map((x) => x.split(':')[1]));
    expect(skies.has('clear') && skies.has('cloud')).toBe(true);
    // The same draw is the one the card priced: under cloud the bombs find the target as much less often as the rules say.
    let both = 0;
    for (const turn of [1, 4, 7, 10]) {
      const clear = hits.get(`${turn}:clear`), cloud = hits.get(`${turn}:cloud`);
      if (clear === undefined || cloud === undefined) continue;
      expect(cloud / clear).toBeCloseTo(WEATHER.cloud.bombsDay, 9);
      both++;
    }
    expect(both).toBeGreaterThan(0);
  });

  it("without the campaign's weather the table draws the deck, as before", () => {
    for (let k = 0; k < 4; k++) {
      const b = battle(k, 1, false)!;
      expect(weatherOn(b.g)).toBe(false);
      const T = b.g.regions[b.tk.place.target];
      expect(b.tk.place.cloud!.weather).toBeUndefined();
      expect(b.tk.place.cloud).toEqual(cloudOf(T.x, T.y, b.tk.place.when!, b.tk.seed));
    }
  });
});
