// A queue for a town (release 2; Brian 25 November 2026 13:41Z, "easy to manage the cities, recruitment, ect"; the
// design director's ruling seamless-r2-1125.md part B; src/campaign/townqueue.ts). A queued building is a press of
// BUILD made for the player at the opening of his month, straight after his staff's month, and nothing more. The gate:
// 1. off byte-identical;
// 2. the proof: a war with queued builds against the same war with the same BUILD presses made by hand at each
//    month's opening after the staff's month: identical month hashes over 24 months;
// 3. money short: the item waits, later items go on, the oldest is pressed first next month;
// 4. a lost town drops its queue unpaid;
// 5. the staff leaves queues alone.
import { afterEach, describe, expect, it, vi } from 'vitest';
import { civicAdvice, civicAI, civicLevel, civicLine, whyNotCivic } from '../src/campaign/buildings';
import { setDesk } from '../src/campaign/depth';
import { createGame } from '../src/campaign/game';
import { standDown } from '../src/campaign/liberation';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { TOWNQUEUE_STAGE } from '../src/campaign/stage';
import { addQueue, dropQueue, pressQueue, queueHeads, queueOf, queuedAt, setTownQueue, townQueueOn, whyNotQueue, type Queued } from '../src/campaign/townqueue';
import { beginHuman, endHuman, humanUp, orderBuilding, orderCancelBuild, stepAI, townBuildings } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

/** A grand campaign with the player's first month open. */
function opened(human = 'GER', seed = 3, queue = true): Game {
  const g = startOpen(createGame(seed, human));
  if (queue) setTownQueue(g, true);
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  beginHuman(g);
  return g;
}
/** Ends the player's month and opens the next. */
function month(g: Game): void {
  endHuman(g);
  for (let k = 0; !humanUp(g) && k < 200; k++) stepAI(g);
  beginHuman(g);
}
const QUEUE_NEWS = /^(From your queue, the builders begin |The .+ (waits for [\d,]+ more\.|comes off the queue\.)|.+ is lost, and )/;
const queueNews = (g: Game, turn = g.turn) => g.headlines.filter((h) => h.turn === turn && QUEUE_NEWS.test(h.text));
/** The war as saved, less the queue's own record: its field and its lines of news. */
function war(g: Game): string {
  const d = JSON.parse(saveGame(g)) as { townQueue?: unknown; headlines: { text: string }[] };
  delete d.townQueue;
  d.headlines = d.headlines.filter((h) => !QUEUE_NEWS.test(h.text));
  return JSON.stringify(d);
}
const hash = (s: string): number => { let h = 7; for (let i = 0; i < s.length; i++) h = (h * 31 + s.charCodeAt(i)) % 2147483647; return h; };
const civicOrders = (g: Game, nat: string, region: number) => g.nations[nat].queue.filter((b) => b.what === 'civic' && b.region === region);

afterEach(() => { vi.restoreAllMocks(); });

describe('the switch', () => {
  it('is off where the stage leaves it off, and a new war carries no flag', () => {
    expect(TOWNQUEUE_STAGE).toBe(false);
    const g = startOpen(createGame(3, 'GER'));
    expect(g.flags.townQueue).toBeUndefined();
    expect(townQueueOn(g)).toBe(false);
  });

  it('off, nothing can be queued and nothing is written; turned off, every queue goes', () => {
    const g = opened('GER', 3, false);
    const town = civicAdvice(g, 'GER').home[0];
    expect(whyNotQueue(g, 'GER', town, 'school')).toMatch(/switched off/);
    expect(addQueue(g, 'GER', town, 'school')).toBeNull();
    pressQueue(g);
    expect(g.townQueue).toBeUndefined();
    expect(saveGame(g)).not.toContain('townQueue');
    setTownQueue(g, true);
    g.nations.GER.pp = 0;
    expect(addQueue(g, 'GER', town, 'school')).not.toBeNull();
    expect(saveGame(g)).toContain('"townQueue"');
    setTownQueue(g, false);
    expect(g.townQueue).toBeUndefined();
    expect(g.flags.townQueue).toBeUndefined();
    expect(queueOf(g, 'GER')).toEqual([]);
  });
});

describe('what can be queued', () => {
  it('BUILD comes first where it can be pressed now; behind the builders a town queues only what it has room for', () => {
    const g = opened();
    const [berlin, essen] = civicAdvice(g, 'GER').home;
    expect(townBuildings(g, essen)!.plots.length).toBe(3);
    expect(whyNotQueue(g, 'GER', essen, 'market')).toMatch(/BUILD puts it up now/);
    expect(orderBuilding(g, essen, 'camp')).not.toBeNull();
    // Three plots: the drill ground going up and two more lines; the third new line has no plot.
    expect(addQueue(g, 'GER', essen, 'market')).toEqual({ region: essen, line: 'market', level: 1 });
    expect(addQueue(g, 'GER', essen, 'school')).toEqual({ region: essen, line: 'school', level: 1 });
    expect(whyNotQueue(g, 'GER', essen, 'hospital')).toMatch(/taken or spoken for/);
    // A line already queued rises a level at a time, as far as it goes.
    expect(addQueue(g, 'GER', essen, 'market')!.level).toBe(2);
    expect(addQueue(g, 'GER', essen, 'market')!.level).toBe(3);
    expect(whyNotQueue(g, 'GER', essen, 'market')).toMatch(/as far as this goes/);
    expect(whyNotQueue(g, 'GER', essen, 'camp')).toBeNull();
    // The ground is checked as BUILD checks it.
    expect(whyNotQueue(g, 'GER', berlin, 'farm')).toMatch(/ground here will not carry it/);
    // Taking one off takes the levels of its line that need it.
    expect(dropQueue(g, 'GER', essen, 2)).toBe(true);
    expect(queuedAt(g, 'GER', essen).map((q) => `${q.line}:${q.level}`)).toEqual(['market:1', 'school:1']);
    expect(dropQueue(g, 'GER', essen, 0)).toBe(true);
    expect(queuedAt(g, 'GER', essen).map((q) => `${q.line}:${q.level}`)).toEqual(['school:1']);
    expect(dropQueue(g, 'GER', essen, 0)).toBe(true);
    expect(g.townQueue).toBeUndefined();
  });

  it('a save keeps the queue, and loads it back', () => {
    const g = opened();
    const essen = civicAdvice(g, 'GER').home[1];
    orderBuilding(g, essen, 'camp');
    addQueue(g, 'GER', essen, 'market');
    const h = loadGame(saveGame(g))!;
    expect(queueOf(h, 'GER')).toEqual([{ region: essen, line: 'market', level: 1 }]);
    expect(saveGame(h)).toBe(saveGame(g));
  });
});

describe('the gate', () => {
  it('1. on with nothing queued, the war is the war with the switch off, month for month', () => {
    const on = opened('GER', 3, true), off = opened('GER', 3, false);
    const bare = (g: Game): string => saveGame(g).replace('"townQueue":1,', '').replace(',"townQueue":1', '');
    expect(bare(on)).toBe(bare(off));
    for (let m = 0; m < 12; m++) {
      month(on);
      month(off);
      expect(hash(bare(on))).toBe(hash(bare(off)));
      expect(saveGame(off)).not.toContain('townQueue');
    }
  });

  /**
   * The player's own presses, made by hand at the opening of his month after his staff's month, through the town
   * plan's offers and BUILD: oldest first, one building at a time in a town, a building the treasury cannot pay for
   * holding its town until next month, one that cannot be built at all given up.
   */
  function pressByHand(g: Game, list: Queued[]): void {
    const held = new Set<number>();
    for (const it of [...list]) {
      if (held.has(it.region)) continue;
      const t = townBuildings(g, it.region);
      const drop = (x: Queued): void => { list.splice(list.indexOf(x), 1); };
      if (!t || !t.mine) { for (const x of list.filter((y) => y.region === it.region)) drop(x); held.add(it.region); continue; }
      const stands = civicLevel(g, it.line, it.region);
      if (stands >= it.level) { drop(it); continue; }
      if (t.plots.some((p) => p.building)) { held.add(it.region); continue; }
      const o = t.offers.find((x) => x.line === it.line)!;
      if (o.why && !o.poor) { drop(it); continue; }
      held.add(it.region);
      if (o.poor) continue;
      expect(orderBuilding(g, it.region, it.line)).not.toBeNull();
      if (stands + 1 >= it.level) drop(it);
    }
  }

  for (const [nat, seed] of [['GER', 3], ['UK', 17]] as const) {
    it(`2. the proof (${nat}, seed ${seed}): queued presses and the same presses by hand make the same war, 24 months`, () => {
      const a = opened(nat, seed), b = opened(nat, seed);
      const towns = civicAdvice(a, nat).home.slice(0, 5);
      // The same opening in both: a building by hand in each town, then the treasury nearly empty, so that the
      // first months' presses wait for money.
      for (const g of [a, b]) {
        for (const r of towns) orderBuilding(g, r, 'camp');
        g.nations[nat].pp = 3;
      }
      const wish = ['market', 'school', 'market', 'hospital', 'market', 'school', 'hall'];
      const byHand: Queued[] = [];
      for (const r of towns) for (const l of wish) { const q = addQueue(a, nat, r, l); if (q) byHand.push({ ...q }); }
      expect(byHand.length).toBeGreaterThan(15);
      expect(war(a)).toBe(war(b));
      let begun = 0, waited = 0;
      for (let m = 0; m < 24; m++) {
        // The same spending elsewhere in both wars: what is left in the treasury at the month's end goes on other
        // things, so that each opening's presses must share one month's earnings.
        for (const g of [a, b]) g.nations[nat].pp = Math.min(g.nations[nat].pp, 1);
        month(a);
        month(b);
        pressByHand(b, byHand);
        const said = queueNews(a).map((h) => h.text).join(' ');
        if (said.includes('the builders begin')) begun++;
        if (said.includes('waits for')) waited++;
        expect(hash(war(a)), `month ${a.turn}`).toBe(hash(war(b)));
        expect(queueOf(a, nat)).toEqual(byHand);
      }
      expect(war(a)).toBe(war(b));
      // The proof pressed something and waited for money along the way.
      expect(begun).toBeGreaterThan(2);
      expect(waited).toBeGreaterThan(0);
    });
  }

  it('3. money short: the item waits, later items go on, and the oldest is pressed first next month', () => {
    const g = opened();
    const [berlin, essen, hamburg] = civicAdvice(g, 'GER').home;
    const me = g.nations.GER;
    me.pp = 0;
    // Berlin's school (10) first, Essen's hospital (6) after: free builders, nothing in the treasury, so both queue.
    addQueue(g, 'GER', berlin, 'school');
    addQueue(g, 'GER', essen, 'hospital');
    me.pp = 8;
    pressQueue(g);
    expect(civicOrders(g, 'GER', berlin)).toHaveLength(0);
    expect(civicOrders(g, 'GER', essen)).toHaveLength(1);
    expect(me.pp).toBeCloseTo(2);
    expect(queueOf(g, 'GER')).toEqual([{ region: berlin, line: 'school', level: 1 }]);
    const said = queueNews(g).map((h) => h.text);
    expect(said).toEqual(['From your queue, the builders begin the aid post at Essen. The technical school at Berlin waits for 800 more.']);
    expect(queueHeads(g, 'GER')).toEqual([expect.objectContaining({ short: 800, after: '' })]);
    // Next month: Hamburg's market is queued behind Berlin's school, and there is money for both. Berlin goes first.
    me.pp = 0;
    addQueue(g, 'GER', hamburg, 'market');
    me.pp = 30;
    pressQueue(g);
    const school = civicOrders(g, 'GER', berlin)[0], market = civicOrders(g, 'GER', hamburg)[0];
    expect(school && market).toBeTruthy();
    expect(school.id).toBeLessThan(market.id);
    expect(g.townQueue).toBeUndefined();
  });

  it('4. a lost town drops its queue unpaid, and the rest stands', () => {
    const g = opened();
    const [berlin, essen] = civicAdvice(g, 'GER').home;
    const me = g.nations.GER;
    me.pp = 0;
    addQueue(g, 'GER', essen, 'hospital');
    addQueue(g, 'GER', essen, 'market');
    addQueue(g, 'GER', berlin, 'school');
    g.regions[essen].ctrl = 'FRA';
    me.pp = 4;
    pressQueue(g);
    expect(me.pp).toBe(4);
    expect(civicOrders(g, 'GER', essen)).toHaveLength(0);
    expect(queueOf(g, 'GER')).toEqual([{ region: berlin, line: 'school', level: 1 }]);
    expect(queueNews(g).map((h) => h.text).join(' ')).toContain('Essen is lost, and the 2 buildings queued there with it: nothing had been paid.');
  });

  it('5. the staff leaves queues alone: with towns and trade handed over it builds only where nothing is queued', () => {
    const g = opened();
    const [berlin] = civicAdvice(g, 'GER').home;
    const me = g.nations.GER;
    expect(setDesk(g, 'towns', true)).toBe(true);
    me.pp = 0;
    addQueue(g, 'GER', berlin, 'school');
    me.pp = 500;
    g.flags['cvb:GER'] = 500;
    const before = JSON.stringify(g.townQueue);
    civicAI(g, 'GER', 0, false, false);
    const ordered = me.queue.filter((b) => b.what === 'civic' && b.ordered === g.turn);
    expect(ordered.length).toBeGreaterThan(0);
    expect(ordered.some((b) => b.region === berlin)).toBe(false);
    expect(JSON.stringify(g.townQueue)).toBe(before);
    // Without the queue the staff's first pick is Berlin itself.
    const h = opened();
    expect(setDesk(h, 'towns', true)).toBe(true);
    h.nations.GER.pp = 500;
    h.flags['cvb:GER'] = 500;
    civicAI(h, 'GER', 0, false, false);
    expect(h.nations.GER.queue.some((b) => b.what === 'civic' && b.region === berlin)).toBe(true);
  });
});

// ---- The board, the town plan and the chip (campaign-bridge.ts) ----

type Call = (...a: unknown[]) => unknown;
const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;
interface PageButton { label: string; act: string; ok: boolean; tip: string }
interface PageRow { title: string; lines: string[]; buttons: PageButton[] }
interface Page { id: string; title: string; sections: { heading: string; text: string; rows: PageRow[] }[] }
interface HomeTown { region: number; town: string; state: string; queue: { line: string; label: string; act: string }[]; wait: string; then: { line: string; label: string; ok: boolean; act: string; cost: number }[] }
interface Board { towns: HomeTown[]; tiles: { k: string; sub: string }[]; counts: { waits: number } }
interface Acted { ok: boolean; note: string; view: { todo: { k: string; t: string; tip: string }[] } }
const act = (a: string): Acted => TC().act(a) as Acted;
const board = (): Board => TC().homeFront() as Board;

describe('the board and the town plan', () => {
  afterEach(() => { delete (globalThis as { TC_HOME_FRONT?: unknown }).TC_HOME_FRONT; });

  it('off, the board and the town plan offer no queue, and the acts refuse', () => {
    TC().start({ human: 'GER', seed: 3, view: 'grand' });
    const h = board();
    expect(h.towns.every((t) => t.queue.length === 0 && t.then.length === 0 && t.wait === '')).toBe(true);
    expect(h.counts.waits).toBe(0);
    const t = h.towns.find((x) => x.state === 'ready')!;
    expect(act(`civic:${t.region}:camp`).ok).toBe(true);
    const r = act(`queue:${t.region}:market`);
    expect(r.ok).toBe(false);
    expect(r.note).toMatch(/switched off/);
    const plan = TC().page(`town:${t.region}`) as Page;
    expect(plan.sections.flatMap((s) => s.rows.flatMap((x) => x.buttons)).some((b) => b.label === 'QUEUE')).toBe(false);
  });

  it('on, a town going up offers QUEUE, the queue shows on its row and its plan, and TAKE IT OFF takes it off', () => {
    TC().start({ human: 'GER', seed: 3, view: 'grand' });
    (globalThis as { TC_HOME_FRONT?: unknown }).TC_HOME_FRONT = true;
    const on = act('townqueuerule:on');
    expect(on.ok).toBe(true);
    expect(on.note).toMatch(/^Building queues are on/);
    const t = board().towns.find((x) => x.state === 'ready')!;
    expect(t.then).toEqual([]);
    expect(act(`civic:${t.region}:camp`).ok).toBe(true);
    let row = board().towns.find((x) => x.region === t.region)!;
    expect(row.state).toBe('going');
    expect(row.then.length).toBeGreaterThan(0);
    expect(row.then.every((b) => b.ok && b.act.startsWith(`queue:${t.region}:`))).toBe(true);
    const q = act(row.then[0].act);
    expect(q.ok).toBe(true);
    expect(q.note).toMatch(/is queued\. It is ordered for you at the opening of a month, once the builders at .+ are free and the treasury can pay, at that day's price\.$/);
    row = board().towns.find((x) => x.region === t.region)!;
    expect(row.queue).toHaveLength(1);
    expect(row.wait).toMatch(/^Goes up after the drill ground\.$/);
    // The rules' own room shows it twice: on the town's row going up, and in its own section.
    const home = TC().page('home') as Page;
    const going = home.sections.find((s) => s.heading === 'GOING UP IN YOUR TOWNS')!.rows.find((x) => x.title.includes(row.town))!;
    expect(going.lines.join(' ')).toMatch(/^Due .+ Queued: .+\. Goes up after the drill ground\.$/);
    expect(going.buttons.some((b) => b.label === 'QUEUE')).toBe(true);
    const queued = home.sections.find((s) => s.heading === 'QUEUED IN YOUR TOWNS')!;
    expect(queued.rows).toHaveLength(1);
    // The town plan lists it, and offers QUEUE where BUILD cannot be pressed.
    const plan = TC().page(`town:${t.region}`) as Page;
    expect(plan.sections.map((s) => s.heading)).toEqual(['THE PLOTS', 'QUEUED HERE', 'WHAT CAN BE BUILT HERE']);
    expect(plan.sections[2].rows.flatMap((x) => x.buttons).filter((b) => b.label === 'QUEUE').length).toBeGreaterThan(0);
    const off = act(row.queue[0].act);
    expect(off.ok).toBe(true);
    expect(off.note).toMatch(/comes off the queue\. Nothing had been paid for it\.$/);
    expect(board().towns.find((x) => x.region === t.region)!.queue).toEqual([]);
    expect(act('townqueuerule:off').note).toMatch(/^Building queues are off/);
  });

  it('a queued building waiting for money is counted on the towns tile and the chip', () => {
    TC().start({ human: 'GER', seed: 3, view: 'grand' });
    (globalThis as { TC_HOME_FRONT?: unknown }).TC_HOME_FRONT = true;
    act('townqueuerule:on');
    // Spend the treasury down on buildings by hand, then queue in a town whose builders are free and the money short.
    let h = board();
    for (const t of h.towns) { if (t.state === 'ready') act(`civic:${t.region}:${t.then[0]?.line ?? 'camp'}`); }
    h = board();
    const short = h.towns.find((t) => t.state === 'short' && t.then.length);
    expect(short).toBeTruthy();
    expect(act(short!.then[0].act).ok).toBe(true);
    h = board();
    const row = h.towns.find((t) => t.region === short!.region)!;
    expect(row.wait).toMatch(/^Waits for [\d,]+ more\.$/);
    expect(h.counts.waits).toBe(1);
    expect(h.tiles.find((t) => t.k === 'towns')!.sub).toMatch(/  ·  1 waiting$/);
    const chip = (TC().view() as Acted['view']).todo.find((t) => t.k === 'page:home');
    expect(chip).toBeTruthy();
    expect(chip!.t).toMatch(/WAIT/);
    expect(chip!.tip).toMatch(/waits for [\d,]+ more\./);
  });
});

describe('build from the map (the province panel)', () => {
  interface Plots { best: string; queued: string; offers: { line: string; why: string; queue: string; queueLabel: string }[] }
  interface Offer { line: string; level: number; why: string; queue: string; queueLabel: string; queueLevel: number; queueCost: number; queueMonths: number; queueAfter: string }
  const plotsOf = (region: number): Plots => (TC().province(region) as { plots: Plots }).plots;

  it('names the staff\'s pick for the town, and QUEUE presses only with the queue on and BUILD shut', () => {
    TC().start({ human: 'GER', seed: 3, view: 'grand' });
    const t = board().towns.find((x) => x.state === 'ready')!;
    let p = plotsOf(t.region);
    expect(p.best).not.toBe('');
    expect(p.offers.find((o) => o.line === p.best)!.why).toBe('');
    expect(p.offers.every((o) => o.queue === '' && o.queueLabel === '')).toBe(true);
    expect(p.queued).toBe('');
    expect(act(`civic:${t.region}:${p.best}`).ok).toBe(true);
    // Off: nothing to queue, and the pick is gone with the builders busy.
    p = plotsOf(t.region);
    expect(p.offers.every((o) => o.queue === '')).toBe(true);
    act('townqueuerule:on');
    p = plotsOf(t.region);
    const next = p.offers.find((o) => o.line === p.best)!;
    expect(next.queue).toBe(`queue:${t.region}:${p.best}`);
    expect(next.queueLabel).not.toBe('');
    expect(act(next.queue).ok).toBe(true);
    expect(plotsOf(t.region).queued).toMatch(/^Queued: .+\. Goes up after the .+\.$/);
  });

  it('a press of QUEUE is shown as the building it queues: its name, level, price and months, behind what is going up', () => {
    TC().start({ human: 'GER', seed: 3, view: 'grand' });
    act('townqueuerule:on');
    const t = board().towns.find((x) => x.state === 'ready')!;
    expect(act(`civic:${t.region}:market`).ok).toBe(true);
    const l = civicLine('market')!;
    // The market hall is going up: the offer still describes it, and QUEUE queues the merchant quarter, at its own price.
    let o = (plotsOf(t.region).offers as Offer[]).find((x) => x.line === 'market')!;
    expect(o.level).toBe(1);
    expect(o.queueLevel).toBe(2);
    expect(o.queueLabel).toBe(l.levels[1]);
    expect(o.queueCost).toBe(Math.round(l.cost[1] * 100));
    expect(o.queueMonths).toBe(l.months[1]);
    expect(o.queueAfter).toBe(l.levels[0]);
    expect(act(o.queue).ok).toBe(true);
    // And with that queued, the next press is the level above it.
    o = (plotsOf(t.region).offers as Offer[]).find((x) => x.line === 'market')!;
    expect(o.queueLevel).toBe(3);
    expect(o.queueCost).toBe(Math.round(l.cost[2] * 100));
    expect(o.queueAfter).toBe(l.levels[1]);
    // Off, every one of them is empty.
    act('townqueuerule:off');
    o = (plotsOf(t.region).offers as Offer[]).find((x) => x.line === 'market')!;
    expect([o.queue, o.queueLabel, o.queueAfter, o.queueLevel, o.queueCost, o.queueMonths]).toEqual(['', '', '', 0, 0, 0]);
  });
});

describe('mends from the second read', () => {
  interface Going { going: { id: number; act: string; cancel: boolean } | null }

  it('a building taken back takes with it what was queued behind it in its line, and says so; the rest stands', () => {
    TC().start({ human: 'GER', seed: 3, view: 'grand' });
    act('townqueuerule:on');
    const t = board().towns.find((x) => x.state === 'ready')!;
    expect(act(`civic:${t.region}:market`).ok).toBe(true);
    expect(act(`queue:${t.region}:market`).ok).toBe(true);
    // Another line queued in the same town, where it has room for one: it does not need the market, and stays.
    const other = board().towns.find((x) => x.region === t.region)!.then.find((b) => b.line !== 'market' && b.ok);
    if (other) expect(act(other.act).ok).toBe(true);
    const going = (board().towns.find((x) => x.region === t.region) as unknown as Going).going!;
    expect(going.cancel).toBe(true);
    const r = act(going.act);
    expect(r.ok).toBe(true);
    expect(r.note).toMatch(/^The order is taken back and the money returned\. The merchant quarter at .+, queued behind it, comes off the queue too: nothing had been paid\.$/);
    expect(board().towns.find((x) => x.region === t.region)!.queue.map((q) => q.line)).toEqual(other ? [other.line] : []);
    // A building taken back with nothing queued behind it says what it always said.
    expect(act(`civic:${t.region}:market`).ok).toBe(true);
    const again = (board().towns.find((x) => x.region === t.region) as unknown as Going).going!;
    expect(act(again.act).note).toBe('The order is taken back and the money returned.');
  });

  it('rules alone: what still stands on a level queued below it stays when a building is taken back', () => {
    const g = opened();
    const town = civicAdvice(g, 'GER').home.find((r) => !civicOrders(g, 'GER', r).length && civicLevel(g, 'market', r) === 0 && whyNotCivic(g, 'GER', g.regions[r], 'market') === null)!;
    g.nations.GER.pp = 0;
    // Short of money: the market hall is queued; then the treasury is found and BUILD puts it up by hand.
    expect(addQueue(g, 'GER', town, 'market')!.level).toBe(1);
    g.nations.GER.pp = 500;
    const b = orderBuilding(g, town, 'market')!;
    expect(b).not.toBeNull();
    expect(addQueue(g, 'GER', town, 'market')!.level).toBe(2);
    expect(orderCancelBuild(g, b.id)).toBe(true);
    // The market hall queued by itself is still there, so the merchant quarter behind it is too.
    expect(queuedAt(g, 'GER', town).map((q) => q.level)).toEqual([1, 2]);
  });

  it('the board names and prices the level the presses will order next, as the news does', () => {
    const g = opened();
    const town = civicAdvice(g, 'GER').home.find((r) => !civicOrders(g, 'GER', r).length && civicLevel(g, 'market', r) === 0 && whyNotCivic(g, 'GER', g.regions[r], 'market') === null)!;
    g.nations.GER.pp = 500;
    const b = orderBuilding(g, town, 'market')!;
    expect(b).not.toBeNull();
    expect(addQueue(g, 'GER', town, 'market')!.level).toBe(2);
    expect(addQueue(g, 'GER', town, 'market')!.level).toBe(3);
    const l = civicLine('market')!;
    let head = queueHeads(g, 'GER').find((x) => x.q.region === town)!;
    expect(head.q.level).toBe(2);
    expect(head.cost).toBe(Math.round(l.cost[1] * 100));
    expect(head.name.startsWith(`the ${l.levels[1].toLowerCase()} at `)).toBe(true);
    // A level that stands by the next opening is no town's head: the presses take it off and go on to the next.
    g.townQueue!.GER.unshift({ region: town, line: 'market', level: 1 });
    head = queueHeads(g, 'GER').find((x) => x.q.region === town)!;
    expect(head.q.level).toBe(2);
  });

  it('a country that falls loses every building it queued: freed, it starts with no queue', () => {
    const g = opened();
    const [berlin, essen] = civicAdvice(g, 'GER').home;
    g.nations.GER.pp = 0;
    expect(addQueue(g, 'GER', berlin, 'school')).not.toBeNull();
    expect(addQueue(g, 'GER', essen, 'school')).not.toBeNull();
    expect(standDown(g)).toBe('GER');
    expect(g.townQueue).toBeUndefined();
    expect((JSON.parse(saveGame(g)) as { townQueue?: unknown }).townQueue).toBeUndefined();
  });
});
