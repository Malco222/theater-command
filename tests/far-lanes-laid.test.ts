// The Pacific sea lanes laid as data (the design director's rulings of 04:33Z, 12:39Z and 13:00Z, 25 November 2026;
// the Pacific planner's brief batch3-lanes-brief.md, parts 2 and 5; the UI director's names and words of 12:57Z,
// batch3-lanes-ui-words.md).
//
// The fault it mends (finding 50 of the 1.5.1 playtest, the data side): on the whole world a Pacific fleet's page
// had no Pacific lane to offer. The chart carried the Atlantic's lanes and Japan's, and nothing for America's road
// to Hawaii and Australia or Britain's round the Cape to India, so a fleet at San Diego could guard no cargo of its
// own side's and Japan's boats had none to raid.
//
// The form: three rows more in the lanes list of data/campaign/far-books.json, read only with the far side on. A
// lane is where ships can sail. No cargo figure has been read for any of the three, so each is laid with oil 0 and
// sup 0 and flagged in the file ('_lanes'): it is on the pages and open to the player's escorts and raiders, counted
// by the Atlantic lane's own rule (laneLoss), and it brings nothing to any book. What stops each lane is who holds a
// place and who is at war, never the month. The rules read the rows as they read the Atlantic's; only the words are
// new: the stores page says a lane that carries nothing is "Not counted" (resources.ts NOT_COUNTED), and a lane cut
// at the harbours its ships land at ('landing' on the row) says it has no port left to land at. The old chart's
// seven rows stand under them word for word (and, with the far side off, alone: tests/lanes-old-chart.test.ts).
//
// What a staff makes of a lane with nothing laid is the ruling of 12:39Z and the navy's rule to read: its two tests
// stand apart in tests/far-lanes-empty.test.ts. Every state here is set by hand on the first day's chart; no war is
// played.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
// (The files as they lie on disk, read as text: with the far side on, the imported lists are the ones laid over.)
import navyDisk from '../data/campaign/navy.json?raw';
import resourcesDisk from '../data/campaign/resources.json?raw';
import { escortLanes, planEscort } from '../src/campaign/fleets';
import { allied, atWar, createGame, declareWar } from '../src/campaign/game';
import { NAVY, canPass, escort, laneLoss, sail, seaRoutes, whyNotEscort, zoneOf } from '../src/campaign/navy';
import { NOT_COUNTED, RES, keepsStores, laneNotCounted, laneStopped, lanesTo, stores } from '../src/campaign/resources';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { fleetOptions } from '../src/campaign/turn';
import type { Fleet, Game } from '../src/campaign/types';

/** The three rows this batch lays, in the order they stand in the list. */
const NEW = ['america-hawaii', 'america-australia', 'britain-india'];
/** The old chart's rows, as data/campaign/resources.json and navy.json carry them on disk. */
const DISK = { lanes: JSON.parse(resourcesDisk).lanes as { id: string }[], convoys: JSON.parse(navyDisk).convoys as { id: string }[] };

/** A war on the first day's chart, far side on: nobody's seat, or the player's. */
const grand = (human: string | null = null): Game => startOpen(createGame(3, human));
/** Japan at war with America, Britain and Australia, who stand together. */
const pacificWar = (g: Game): Game => {
  for (const foe of ['USA', 'UK', 'AUS']) if (!atWar(g, 'JAP', foe)) declareWar(g, 'JAP', foe);
  for (const nat of ['USA', 'UK', 'AUS']) g.nations[nat].align = 'allies';
  return g;
};
const fleet = (g: Game, nat: string, name: string): Fleet => g.fleets.find((f) => f.nat === nat && f.name === name)!;
const row = (id: string): (typeof RES.lanes)[number] => RES.lanes.find((l) => l.id === id)!;
const seasOf = (g: Game, id: string): number[] => seaRoutes(g).find((r) => r.id === id)!.zones;
const hold = (g: Game, place: string, nat: string): void => { g.byName[place].ctrl = nat; };
/** Something laid on a lane's row for it to carry, as the ruling of 12:39Z lists it: oil, supplies, fields, or a convoy route of its id. */
const carries = (l: (typeof RES.lanes)[number]): boolean => l.oil > 0 || l.sup > 0 || !!l.fields?.length || NAVY.convoys.some((c) => c.id === l.id);
/**
 * How many seas off a harbour each sea lies for a country's fleet, counted as its reach is counted (navy.ts
 * fleetReach): the harbour's own seas 1, and on through straits open to it. 0 for a sea it cannot sail to.
 */
const seasOff = (g: Game, nat: string, port: string): Int16Array => {
  const d = new Int16Array(g.seas.length);
  const q: number[] = [];
  for (const z of g.byName[port].seas) if (!d[z]) { d[z] = 1; q.push(z); }
  for (let i = 0; i < q.length; i++) for (const j of g.seas[q[i]].adj) if (!d[j] && canPass(g, q[i], j, nat)) { d[j] = d[q[i]] + 1; q.push(j); }
  return d;
};

describe('the Pacific sea lanes laid as data (the design director, 04:33Z and 12:39Z of 25 November 2026)', () => {
  it("the old chart's lanes stand word for word under the far rows", () => {
    expect(DISK.lanes.map((l) => l.id)).toEqual(['atlantic', 'gulf', 'french', 'italian', 'german', 'arctic', 'persia']);
    expect(RES.lanes.slice(0, 7)).toEqual(DISK.lanes);
    expect(NAVY.convoys).toEqual(DISK.convoys);
    // The three are laid after Japan's, the southern lane the last of those; no row of the list is put in another's place.
    const ids = RES.lanes.map((l) => l.id);
    expect(ids.slice(ids.indexOf('south-japan') + 1)).toEqual(NEW);
    // Japan's oil from the Indies is a row already, and is not changed.
    expect(row('south-oil')).toMatchObject({ to: 'JAP', zones: ['schina', 'luzonsea', 'luzonstrait', 'echina'], oil: 0, sup: 0, fields: ['Palembang', 'Balikpapan', 'Medan', 'Soerabaja', 'Brunei', 'Yenangyaung', 'Tarakan'], closes: true });
  });

  it('every far lane is whole', () => {
    const g = grand();
    const ids = RES.lanes.map((l) => l.id);
    expect(new Set(ids).size).toBe(ids.length);
    for (const l of RES.lanes) {
      // Every sea it names is a sea of the chart, and every country and place it names is there.
      for (const k of [...l.zones, ...(l.draw ?? [])]) expect(zoneOf(g, k), `${l.id}: the sea ${k}`).toBeGreaterThanOrEqual(0);
      expect(g.nations[l.to], `${l.id}: to ${l.to}`).toBeTruthy();
      if (l.source) expect(g.nations[l.source], `${l.id}: source ${l.source}`).toBeTruthy();
      for (const name of [...(l.held ?? []), ...(l.ports ?? []), ...(l.past ?? []), ...(l.fields ?? [])]) expect(g.byName[name], `${l.id}: ${name}`).toBeTruthy();
      expect(l.text.length, l.id).toBeGreaterThan(20);
    }
    // A far row's seas run one next to the last, so that a ship can sail them in the row's order.
    for (const l of RES.lanes.slice(DISK.lanes.length)) {
      for (let i = 0; i + 1 < l.zones.length; i++) expect(g.seas[zoneOf(g, l.zones[i])].adj, `${l.id}: ${l.zones[i]} to ${l.zones[i + 1]}`).toContain(zoneOf(g, l.zones[i + 1]));
    }
    // The rows with nothing laid for them to carry are exactly the three: a figure can arrive on one only on purpose.
    expect(RES.lanes.filter((l) => !carries(l)).map((l) => l.id)).toEqual(NEW);
    for (const id of NEW) expect([id, row(id).oil, row(id).sup, row(id).fields, row(id).draw]).toEqual([id, 0, 0, undefined, []]);
    // No cargo on a lane whose receiver keeps no stock, where its loss would reach no book (12:39Z, 3).
    for (const l of RES.lanes) if (!keepsStores(g, l.to)) expect([l.id, l.oil, l.sup]).toEqual([l.id, 0, 0]);
    expect(RES.lanes.filter((l) => !keepsStores(g, l.to)).map((l) => l.id)).toEqual([]);
    // The UI director's names (12:57Z), and the harbours its ships land at marked on the three alone.
    expect(NEW.map((id) => row(id).name)).toEqual(['the Hawaii lane', 'the South Pacific lane', 'the Cape lane']);
    expect(RES.lanes.filter((l) => l.landing).map((l) => l.id)).toEqual(NEW);
    // The South Pacific lane is America's (13:00Z, part C), and names no seller: a war with its own receiver cannot cut it.
    expect([row('america-australia').to, row('america-australia').source]).toEqual(['USA', undefined]);
    // Each of the three is a route cargoes travel by sea: what an escort can be bound to and a raider sent against.
    expect(NEW.map((id) => [id, seaRoutes(g).find((r) => r.id === id)?.to, seasOf(g, id).map((z) => g.seas[z].key)])).toEqual([
      ['america-hawaii', 'USA', ['hawaii', 'calrun', 'california']],
      ['america-australia', 'USA', ['california', 'calrun', 'hawaii', 'line', 'samoa', 'fiji', 'tasman']],
      ['britain-india', 'UK', ['arabian', 'cindian', 'sindian']],
    ]);
  });

  it('a lane with no cargo figure adds nothing to any book', () => {
    const g = pacificWar(grand());
    g.turn = 28;
    // Raiders on the Hawaii lane, so that a cargo would show a loss if there were one.
    expect(sail(g, fleet(g, 'JAP', '6th Fleet'), zoneOf(g, 'hawaii'), 'raid')).toBe(true);
    for (const [nat, id] of [['USA', 'america-hawaii'], ['USA', 'america-australia'], ['UK', 'britain-india']]) {
      const s = stores(g, nat)!;
      for (const book of [s.oil, s.sup]) expect(book.income.some((l) => l.key === `lane:${id}` || l.lane === id), `${nat} ${book.good}`).toBe(false);
      const lane = lanesTo(g, nat).find((l) => l.id === id)!;
      expect([lane.full, lane.oil, lane.sup], id).toEqual([{ oil: 0, sup: 0 }, 0, 0]);
      expect(s.lanes.some((l) => l.id === id), id).toBe(true);
    }
    expect(lanesTo(g, 'USA').find((l) => l.id === 'america-hawaii')!.state).toBe('raided');
    // Australia keeps no stock, and no lane is laid to it: the South Pacific lane is America's.
    expect(lanesTo(g, 'AUS')).toEqual([]);
    expect(stores(g, 'AUS')).toBeNull();
    // No convoy route of these ids is laid, so no tons are counted on them.
    for (const id of NEW) { expect(NAVY.convoys.some((c) => c.id === id), id).toBe(false); expect(id in g.convoys, id).toBe(false); }
  });

  it('what stops each lane is who holds its harbours and who is at war, never the month', () => {
    const g = grand();
    const stops = (): unknown[] => NEW.map((id) => laneStopped(g, id));
    // On the first day America stands at nobody's side, so the South Pacific lane waits for one of its harbours to be
    // in American or allied hands; the other two sail.
    const first = [null, { state: 'idle', why: 'It opens once one of Brisbane, Sydney and Melbourne is in friendly hands.' }, null];
    expect(stops()).toEqual(first);
    // The month alone changes nothing: before the war, December 1941, 1944 and the last summer.
    const day = g.turn;
    for (const turn of [0, 28, 60, 75]) { g.turn = turn; expect(stops(), `month ${turn}`).toEqual(first); }
    g.turn = day;

    // The Hawaii lane: Pearl Harbor in hands that are not friendly, and then an enemy's. One port: no comma.
    hold(g, 'Pearl Harbor', 'JAP');
    expect(laneStopped(g, 'america-hawaii')).toEqual({ state: 'idle', why: 'It opens once Pearl Harbor is in friendly hands.' });
    declareWar(g, 'JAP', 'USA');
    expect(laneStopped(g, 'america-hawaii')).toEqual({ state: 'cut', why: 'Japan holds Pearl Harbor and the lane has no port left to land at.' });
    hold(g, 'Pearl Harbor', 'USA');
    expect(laneStopped(g, 'america-hawaii')).toBeNull();

    // The South Pacific lane: it opens when America is at Australia's side.
    declareWar(g, 'JAP', 'AUS');
    for (const nat of ['USA', 'AUS']) g.nations[nat].align = 'allies';
    expect(allied(g, 'AUS', 'USA')).toBe(true);
    expect(laneStopped(g, 'america-australia')).toBeNull();
    // An enemy in all three of its harbours cuts it, and one retaken opens it. Several ports, one holder.
    for (const port of ['Brisbane', 'Sydney', 'Melbourne']) hold(g, port, 'JAP');
    expect(laneStopped(g, 'america-australia')).toEqual({ state: 'cut', why: 'Japan holds Brisbane, Sydney and Melbourne, and the lane has no port left to land at.' });
    // Ports held by more than one enemy.
    if (!atWar(g, 'GER', 'USA')) declareWar(g, 'GER', 'USA');
    hold(g, 'Sydney', 'GER');
    expect(laneStopped(g, 'america-australia')).toEqual({ state: 'cut', why: 'Every port the lane lands at is in enemy hands.' });
    hold(g, 'Sydney', 'AUS');
    expect(laneStopped(g, 'america-australia')).toBeNull();
    for (const port of ['Brisbane', 'Melbourne']) hold(g, port, 'AUS');
    // It names no seller: the lane is America's own, and stops only at its harbours.
    expect(row('america-australia').source).toBeUndefined();

    // The Cape lane: an enemy in Cape Town; or in Bombay and Karachi both, and in one of the two alone it sails.
    expect(atWar(g, 'UK', 'GER')).toBe(true);
    hold(g, 'Cape Town', 'GER');
    expect(laneStopped(g, 'britain-india')).toEqual({ state: 'cut', why: 'Germany holds Cape Town and lets nothing past.' });
    hold(g, 'Cape Town', g.byName['Cape Town'].owner);
    expect(laneStopped(g, 'britain-india')).toBeNull();
    hold(g, 'Bombay', 'GER');
    expect(laneStopped(g, 'britain-india')).toBeNull();
    hold(g, 'Karachi', 'GER');
    expect(laneStopped(g, 'britain-india')).toEqual({ state: 'cut', why: 'Germany holds Bombay and Karachi, and the lane has no port left to land at.' });
    hold(g, 'Bombay', 'UK');
    expect(laneStopped(g, 'britain-india')).toBeNull();
  });

  it("a lane cut at its loading harbours keeps today's words: Formosa's lane, byte for byte", () => {
    const g = grand();
    expect(row('formosa-japan').landing).toBeUndefined();
    declareWar(g, 'UK', 'JAP');
    for (const port of ['Taihoku', 'Formosa']) hold(g, port, 'UK');
    expect(laneStopped(g, 'formosa-japan')).toEqual({ state: 'cut', why: 'The United Kingdom holds Taihoku and Formosa, and nothing sails from them.' });
    // And the Korea lane, cut at the one place it holds ('held'), keeps its road.
    hold(g, 'Pusan', 'UK');
    expect(laneStopped(g, 'korea-japan')).toEqual({ state: 'cut', why: 'The United Kingdom holds Pusan and the road through it is shut.' });
  });

  it('the stores page says a lane that carries nothing is not counted, and a lane that carries something keeps its share', () => {
    const g = pacificWar(grand());
    g.turn = 28;
    // The UI director's words (12:57Z, 2(b)), to the letter.
    expect(NOT_COUNTED).toEqual({ value: 'Not counted', line: 'Its shipping is not counted in the game, so it adds nothing to these stores, and its loss takes nothing from them.' });
    const usa = stores(g, 'USA')!, uk = stores(g, 'UK')!, jap = stores(g, 'JAP')!;
    for (const l of [...usa.lanes, ...uk.lanes, ...jap.lanes]) {
      const nc = laneNotCounted(g, l);
      expect([l.id, nc], l.id).toEqual([l.id, NEW.includes(l.id) ? NOT_COUNTED : null]);
    }
    // Every one of the three is on its receiver's page, with the words in place of a share.
    expect(NEW.map((id) => [...usa.lanes, ...uk.lanes].some((l) => l.id === id))).toEqual([true, true, true]);
    // The words never say "not laid", "0" or "yet".
    for (const w of Object.values(NOT_COUNTED)) expect(w).not.toMatch(/not laid|\b0\b|\byet\b/);
  });

  it('a Pacific fleet is offered Pacific lanes (finding 50, the data side)', () => {
    const g = pacificWar(grand('USA'));
    const pf = fleet(g, 'USA', 'Pacific Fleet'), af = fleet(g, 'USA', 'Asiatic Fleet');
    expect([pf.port, af.port]).toEqual(['San Diego', 'Manila']);

    // The seas between, counted as a fleet's reach is counted, the base's own sea as 1.
    const off = (nat: string, port: string, seas: string[]): number[] => { const d = seasOff(g, nat, port); return seas.map((k) => d[zoneOf(g, k)]); };
    expect(NAVY.range).toBe(3);
    expect(off('USA', 'San Diego', ['california', 'calrun', 'hawaii'])).toEqual([1, 2, 3]);
    expect(off('USA', 'Pearl Harbor', ['hawaii', 'calrun', 'california'])).toEqual([1, 2, 3]);
    expect(off('AUS', 'Sydney', ['tasman'])).toEqual([1]);
    expect(off('UK', 'Colombo', ['arabian'])).toEqual([2]);
    expect(off('UK', 'Singapore', ['cindian'])).toEqual([3]);
    // From Manila the nearest sea of any of the three lanes is the Central Indian Ocean, six seas off.
    const all = [...new Set(NEW.flatMap((id) => row(id).zones))];
    const manila = off('USA', 'Manila', all);
    expect([Math.min(...manila), all[manila.indexOf(Math.min(...manila))]]).toEqual([6, 'cindian']);

    // The Pacific Fleet can be bound to either of America's roads; the Asiatic Fleet at Manila to neither.
    for (const id of ['america-hawaii', 'america-australia']) {
      expect(planEscort(g, pf, id).why, id).toBeNull();
      expect(whyNotEscort(g, af, id), id).toMatch(/out of reach/);
    }
    // Australia's fleet at Sydney stands on the South Pacific lane, America's ally's escort; Britain's at Singapore reaches the Cape lane.
    expect(whyNotEscort(g, fleet(g, 'AUS', 'Australian Fleet'), 'america-australia')).toBeNull();
    expect(whyNotEscort(g, fleet(g, 'UK', 'China Fleet'), 'britain-india')).toBeNull();

    // The player's page for the Pacific Fleet lists both, each with nothing against it.
    g.phase = g.order.indexOf('USA');
    const page = fleetOptions(g, pf.id)!.lanes;
    for (const id of ['america-hawaii', 'america-australia']) expect(page.find((l) => l.lane === id)?.why, id).toBeNull();
    expect(page.find((l) => l.lane === 'america-hawaii')).toMatchObject({ name: 'the Hawaii lane', to: 'USA', bound: false });

    // His order binds it, in a sea of the lane.
    expect(escort(g, pf, 'america-hawaii')).toBe(true);
    expect([pf.stance, pf.lane]).toEqual(['escort', 'america-hawaii']);
    expect(seasOf(g, 'america-hawaii')).toContain(pf.zone);
    expect(escortLanes(g, 'USA').find((l) => l.id === 'america-hawaii')!.escorts).toEqual([pf.id]);
    // And the order is kept through a save.
    const h = loadGame(saveGame(g))!;
    const kept = fleet(h, 'USA', 'Pacific Fleet');
    expect([kept.stance, kept.lane, kept.zone]).toEqual(['escort', 'america-hawaii', pf.zone]);
    expect(planEscort(h, kept, 'america-hawaii').bound).toBe(true);
  });

  it("raiders on a far lane are counted by the Atlantic's rule, once", () => {
    const g = pacificWar(grand());
    g.turn = 28;
    const pf = fleet(g, 'USA', 'Pacific Fleet'), boats = fleet(g, 'JAP', '6th Fleet');
    expect(boats.ships.ss).toBeGreaterThan(0);
    // With no raider out, nothing is lost on it.
    expect(laneLoss(g, 'USA', seasOf(g, 'america-hawaii'), 'america-hawaii')).toMatchObject({ raid: 0, loss: 0 });
    expect(sail(g, boats, zoneOf(g, 'hawaii'), 'raid')).toBe(true);
    const ships = (): string => JSON.stringify(g.fleets.map((f) => [f.id, f.ships, f.lost, f.zone, f.stance, f.lane ?? null]));
    const before = ships();

    // Raiders against escorts, as the convoy war counts them (laneLoss, the rule the Atlantic lane is read by).
    const m = laneLoss(g, 'USA', seasOf(g, 'america-hawaii'), 'america-hawaii');
    expect(m.raid).toBeGreaterThan(0);
    expect(m.loss).toBeGreaterThan(0);
    // Once: the stores page and the escorts' page read that one count, and no convoy route counts the lane again.
    const lane = lanesTo(g, 'USA').find((l) => l.id === 'america-hawaii')!;
    expect([lane.raid, lane.guard, lane.flow, lane.state]).toEqual([m.raid, m.guard, 1 - m.loss, 'raided']);
    const listed = escortLanes(g, 'USA').find((l) => l.id === 'america-hawaii')!;
    expect([listed.raid, listed.guard, listed.loss, listed.state]).toEqual([m.raid, m.guard, m.loss, 'raided']);
    expect(NAVY.convoys.some((c) => c.id === 'america-hawaii')).toBe(false);
    expect('america-hawaii' in g.convoys).toBe(false);

    // What the Pacific Fleet would do for it as its escort: fewer cargoes lost, by the same count.
    const plan = planEscort(g, pf, 'america-hawaii');
    expect(plan.why).toBeNull();
    expect([plan.now, plan.raid, plan.guardNow]).toEqual([m.loss, m.raid, m.guard]);
    expect(plan.guardThen).toBeGreaterThan(plan.guardNow);
    expect(plan.then).toBeLessThan(plan.now);
    // The count throws no dice and sinks nothing: no ship of either side is lost by it, and no fleet is moved.
    expect(ships()).toBe(before);
  });
});
