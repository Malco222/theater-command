// The sea lanes measured: a whole-world war nobody plays, read for the gate of the Pacific's batch 3 (the design
// director's rulings of 04:33Z and 12:39Z, 25 November 2026; the Pacific planner's brief batch3-lanes-brief.md, part
// 8, readings 1 to 8). A sibling of the navy's and the Pacific's probes.
//
//   esbuild tools/probes/lane-probe.ts --bundle --platform=node --format=esm --alias:@tc=<tree>/src --outfile=<out>.mjs --log-level=warning
//   TC_FAR=1 node <out>.mjs SEED [STOP] [MONTHS]        one JSON object on stdout
//
// It reads only: the war is the computer's own, the probe calls nothing but functions the rules already have, and it
// writes no file. It reads the lanes list as it finds it, so the same file is run on the head and on a base without
// the three rows, side by side: a lane the tree does not carry reads "not in this tree's list".
//
// The readings, for the three lanes laid with this batch and every sea lane that feeds Japan, sampled as each month
// ends (after everyone's play):
//   1. months open, raided, cut and idle; for each change of a stop the month, the sentence, and who held the lane's
//      places that month (the check that no stop came by the date);
//   2. raid, guard and loss share each month the receiver is at war (laneLoss): the mean and the worst month;
//   3. cargo lost in the books, full less carried: "not laid" for a lane that carries nothing laid;
//   4. fleet-months bound to the lane as its escort, and boat-months of raiders in its seas;
//   5. boats sunk in the lane's seas and destroyers lost by fleets bound to it, from each fleet's `lost` before and
//      after the month (a loss is laid to the lane if the fleet stood on it at either end of the month);
//   6. tons: "not laid" where no convoy route of the lane's id stands (every Pacific lane today);
//   7. the station of Japan's Advance Submarine Fleet, month by month;
//   8. the control: the Atlantic route's total tons a war, which this batch should not move.
import { allied, atWar, createGame } from '@tc/campaign/game';
import { escortLanes } from '@tc/campaign/fleets';
import { setStop } from '@tc/campaign/history';
import * as navy from '@tc/campaign/navy';
const { NAVY, atSea, boatsOut, laneLoss, seaRoutes } = navy;
import { RES, laneStopped, lanesTo } from '@tc/campaign/resources';
import { startOpen } from '@tc/campaign/staff';
import { runTurnAI } from '@tc/campaign/turn';

// Node's own handle on the command line and the error stream; declared here because the tree carries no node types.
declare const process: { argv: string[]; stderr: { write(s: string): void } };

const seed = +process.argv[2], stop = +(process.argv[3] ?? 3), months = +(process.argv[4] ?? 76);
const g: any = startOpen(createGame(seed, null));
if (stop !== 3) setStop(g, stop, true);
const far = g.regions.some((r: any) => r.name === 'Tokyo');
const round = (x: number, n = 4): number => Math.round(x * 10 ** n) / 10 ** n;

/** The three rows of this batch, by id: read where the tree's list carries them. */
const NEW = ['america-hawaii', 'america-australia', 'britain-india'];
type Row = { id: string; name: string; to: string; zones: string[]; oil: number; sup: number; source?: string; held?: string[]; ports?: string[]; past?: string[]; fields?: string[] };
const rows = RES.lanes as unknown as Row[];
/**
 * Something laid on a lane for it to carry (12:39Z): the navy's own predicate (navy.ts laneCarries) where the tree has
 * it, and on a tree before it the ruling's list read from the row (oil, supplies, fields, or a convoy route of its id).
 */
const navyCarries = (navy as any).laneCarries as ((g: any, id: string) => boolean) | undefined;
// (A row with no seas on the chart is no route of the navy's, and is read from the row.)
const isRoute = (id: string): boolean => seaRoutes(g).some((r: any) => r.id === id);
const carries = (l: Row): boolean => (navyCarries && isRoute(l.id) ? navyCarries(g, l.id) : l.oil > 0 || l.sup > 0 || !!l.fields?.length || NAVY.convoys.some((c: any) => c.id === l.id));
const watched = seaRoutes(g).filter((r: any) => NEW.includes(r.id) || r.to === 'JAP').filter((r: any) => rows.some((l) => l.id === r.id));

type State = 'open' | 'raided' | 'cut' | 'idle' | 'receiver fallen';
interface Tally {
  name: string; to: string; seas: string[]; carries: boolean;
  /** Months in each state as the escorts' page reads it (escortLanes), and months its receiver was at war. */
  months: Record<State, number>;
  receiverAtWar: number;
  /** Months nothing sailed on it by the lane's own stop (laneStopped): cut by an enemy or the seller, or idle. */
  stopped: { cut: number; idle: number };
  /** Each change of what stops it: the month, the state, the sentence, and who held its places then. */
  stops: { t: number; state: string; why: string; holders: Record<string, string>; sellerAtWar?: boolean }[];
  /** Months the escorts' page read another state than the lane's own stop gives: how many, and the first few. */
  pageDiffers: { months: number; first: { t: number; page: string; stop: string }[] };
  loss: { months: number; mean: number; worst: number; worstAt: number; meanRaid: number; meanGuard: number; raidedMonths: number };
  cargoLost: { oil: number; sup: number } | 'not laid';
  escortFleetMonths: number; escorts: Record<string, number>;
  raiderBoatMonths: number; raiderBoatsOutMonths: number; raiders: Record<string, number>;
  boatsSunkInItsSeas: number; destroyersLostByItsEscorts: number;
  tons: { last: number; total: number } | 'not laid';
}
const tallies: Record<string, Tally> = {};
const sums: Record<string, { loss: number; raid: number; guard: number }> = {};
const last: Record<string, string> = {};
for (const r of watched) {
  const l = rows.find((x) => x.id === r.id)!;
  tallies[r.id] = {
    name: r.name, to: r.to, seas: r.zones.map((z: number) => g.seas[z].key), carries: carries(l),
    months: { open: 0, raided: 0, cut: 0, idle: 0, 'receiver fallen': 0 }, receiverAtWar: 0, stopped: { cut: 0, idle: 0 },
    stops: [], pageDiffers: { months: 0, first: [] },
    loss: { months: 0, mean: 0, worst: 0, worstAt: -1, meanRaid: 0, meanGuard: 0, raidedMonths: 0 },
    cargoLost: carries(l) ? { oil: 0, sup: 0 } : 'not laid',
    escortFleetMonths: 0, escorts: {}, raiderBoatMonths: 0, raiderBoatsOutMonths: 0, raiders: {},
    boatsSunkInItsSeas: 0, destroyersLostByItsEscorts: 0,
    tons: 'not laid',
  };
  sums[r.id] = { loss: 0, raid: 0, guard: 0 };
  last[r.id] = '';
}

/** Every fleet as the month's end leaves it: what it has lost so far, where it is and what it is doing. */
type Seen = { nat: string; name: string; ss: number; dd: number; zone: number; sea: boolean; lane: string | null };
const seeFleets = (): Map<number, Seen> => new Map(g.fleets.map((f: any) => [f.id, { nat: f.nat, name: f.name, ss: f.lost?.ss ?? 0, dd: f.lost?.dd ?? 0, zone: f.zone, sea: atSea(f), lane: f.stance === 'escort' ? f.lane ?? null : null }]));

const advance: string[] = [];
const japWars: { t: number; with: string }[] = [];
let japAtWar = new Set<string>(g.nations.JAP?.atWar ?? []);
let before = seeFleets();
let slow = 0, total = 0, gone = 0;

const read = (): void => {
  const after = seeFleets();
  for (const r of watched) {
    const T = tallies[r.id], l = rows.find((x) => x.id === r.id)!, to = g.nations[r.to];
    // 1. The lane's state as the escorts' page reads it (escortLanes), and the lane's own stop beside it (laneStopped).
    const page = to?.alive ? escortLanes(g, r.to).find((x: any) => x.id === r.id) : undefined;
    const stopNow = to?.alive ? laneStopped(g, r.id) : null;
    const state: State = page ? page.state : 'receiver fallen';
    T.months[state]++;
    if (to?.alive && to.atWar.length) T.receiverAtWar++;
    if (stopNow) T.stopped[stopNow.state as 'cut' | 'idle']++;
    if (page && stopNow && page.state !== stopNow.state) { T.pageDiffers.months++; if (T.pageDiffers.first.length < 5) T.pageDiffers.first.push({ t: g.turn, page: page.state, stop: stopNow.state }); }
    const key = stopNow ? `${stopNow.state}|${stopNow.why}` : '';
    if (key !== last[r.id]) {
      const holders: Record<string, string> = {};
      for (const name of [...(l.held ?? []), ...(l.ports ?? []), ...(l.past ?? []), ...(l.fields ?? [])]) { const p = g.byName[name]; if (p) holders[name] = p.ctrl; }
      T.stops.push({ t: g.turn, state: stopNow ? stopNow.state : 'sails again', why: stopNow?.why ?? '', holders, ...(l.source && g.nations[l.source] ? { sellerAtWar: atWar(g, l.source, r.to) } : {}) });
      last[r.id] = key;
    }
    // 2. Raiders against escorts, each month the receiver is at war and the lane sails.
    if (to?.alive && to.atWar.length && !stopNow) {
      const m = laneLoss(g, r.to, r.zones, r.id);
      T.loss.months++;
      sums[r.id].loss += m.loss; sums[r.id].raid += m.raid; sums[r.id].guard += m.guard;
      if (m.raid > 0) T.loss.raidedMonths++;
      if (m.loss > T.loss.worst) { T.loss.worst = m.loss; T.loss.worstAt = g.turn; }
    }
    // 3. Cargo lost in the books, for a lane that carries something laid.
    if (T.cargoLost !== 'not laid' && to?.alive) {
      const lane = lanesTo(g, r.to).find((x: any) => x.id === r.id);
      if (lane) { T.cargoLost.oil += lane.full.oil - lane.oil; T.cargoLost.sup += lane.full.sup - lane.sup; }
    }
    // 4. Escorts bound to it, and raiders in its seas.
    for (const f of g.fleets) {
      if (f.stance === 'escort' && f.lane === r.id) { T.escortFleetMonths++; T.escorts[`${f.nat} ${f.name}`] = (T.escorts[`${f.nat} ${f.name}`] ?? 0) + 1; }
      if (to && f.ships.ss > 0 && atSea(f) && r.zones.includes(f.zone) && atWar(g, f.nat, r.to)) {
        T.raiderBoatMonths += f.ships.ss; T.raiderBoatsOutMonths += boatsOut(g, f);
        T.raiders[`${f.nat} ${f.name}`] = (T.raiders[`${f.nat} ${f.name}`] ?? 0) + f.ships.ss;
      }
    }
    // 5. Boats sunk in its seas, and destroyers lost by its escorts, since the month before.
    for (const [id, now] of after) {
      const was = before.get(id);
      if (!was) continue;
      const foe = !!to && (atWar(g, now.nat, r.to));
      if (foe && now.ss > was.ss && ((was.sea && r.zones.includes(was.zone)) || (now.sea && r.zones.includes(now.zone)))) T.boatsSunkInItsSeas += now.ss - was.ss;
      const friend = now.nat === r.to || allied(g, now.nat, r.to);
      if (friend && now.dd > was.dd && (was.lane === r.id || now.lane === r.id)) T.destroyersLostByItsEscorts += now.dd - was.dd;
    }
    // 6. Tons, where a convoy route of its id stands.
    const c = g.convoys[r.id];
    if (c) T.tons = { last: c.tons, total: c.total };
  }
  for (const id of before.keys()) if (!after.has(id)) gone++;
  before = after;
  // 7. Japan's Advance Submarine Fleet.
  const adv = g.fleets.find((f: any) => f.nat === 'JAP' && f.name === 'Advance Submarine Fleet');
  advance.push(adv ? `T${g.turn} ${g.seas[adv.station]?.key ?? '?'} (${adv.ships.ss} boats, ${adv.stance} from ${adv.port})` : `T${g.turn} none`);
  const now = new Set<string>(g.nations.JAP?.alive ? g.nations.JAP.atWar : []);
  for (const x of now) if (!japAtWar.has(x)) japWars.push({ t: g.turn, with: x });
  japAtWar = now;
};

for (let t = 1; t < months; t++) {
  const t0 = Date.now();
  runTurnAI(g);
  const ms = Date.now() - t0; total += ms; if (ms > slow) slow = ms;
  read();
}

for (const r of watched) {
  const T = tallies[r.id], n = Math.max(1, T.loss.months);
  T.loss.mean = round(sums[r.id].loss / n); T.loss.meanRaid = round(sums[r.id].raid / n, 2); T.loss.meanGuard = round(sums[r.id].guard / n, 2); T.loss.worst = round(T.loss.worst);
  if (T.cargoLost !== 'not laid') T.cargoLost = { oil: round(T.cargoLost.oil, 2), sup: round(T.cargoLost.sup, 2) };
}
// The Advance Submarine Fleet's stations, as runs of months on one sea.
const runs: { from: number; to: number; sea: string }[] = [];
for (const line of advance) {
  const m = /^T(\d+) (\S+)/.exec(line)!;
  const t = +m[1], sea = m[2], top = runs[runs.length - 1];
  if (top && top.sea === sea && top.to === t - 1) top.to = t; else runs.push({ from: t, to: t, sea });
}
const convoys: Record<string, { total: number }> = {};
for (const [id, c] of Object.entries(g.convoys as Record<string, any>)) convoys[id] = { total: c.total };

console.log(JSON.stringify({
  seed, stop, months, far, turn: g.turn,
  list: rows.map((l) => l.id),
  theThree: Object.fromEntries(NEW.map((id) => [id, rows.some((l) => l.id === id) ? 'laid' : "not in this tree's list"])),
  nothingLaid: rows.filter((l) => !carries(l)).map((l) => l.id),
  lanes: tallies,
  japanAtWar: japWars,
  advanceSubmarineFleet: { runs, byMonth: advance },
  control: { atlanticTotalTons: g.convoys.atlantic?.total ?? 0, convoys },
  notes: { fleetsStoodDownOrLost: gone },
}));
// The wall clock goes to the error stream, so that two runs of one tree print the same JSON byte for byte and a
// head-beside-base comparison needs no key dropped.
process.stderr.write(`ms ${JSON.stringify({ total, avg: Math.round(total / Math.max(1, months - 1)), slowest: slow })}\n`);
