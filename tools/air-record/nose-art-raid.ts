// The nose art's raid fixtures (unreal/TheaterCommand/Tools/air_show/nose_art_raid.json, for its cloud test
// air_nose_art_test.cpp), made with the rules as the bridge hands them to the 3D game: an American campaign (seed 7) at
// war with Germany, three of its bomber groups (I Bomber Command, the 1st and 2nd Bombardment Groups) based in
// Lincolnshire with 20 missions on their records, raiding the Rhineland as the raid card picks it (raidGroups). Two raids:
//   "seats": the campaign as it opens (its groups in id order), fought out by the computer on both sides, its board
//            and its views (one every 10 s on the way in, every second from the first contact) until several of its
//            named bombers have fallen in the picture, and 20 s more.
//   "spliced": the same campaign with the 2nd Bombardment Group's row ahead of the others in g.airFleets, as the
//            campaign's growCommands and splitOff leave a newer group ahead of older ones: the ticket's force order is
//            not ascending by corps. Its board and first view only.
// Each with "forces": the rules' own id and corps of each bomber box (the sim's units), in the order the rules give.
// Nothing is booked, and nothing here is read by the rules or the game.
//
//   npx esbuild tools/air-record/nose-art-raid.ts --bundle --platform=node --format=esm --outfile=dist/nose-art-raid.mjs --log-level=warning
//   node dist/nose-art-raid.mjs unreal/TheaterCommand/Tools/air_show/nose_art_raid.json [named model falls, 8]
import { writeFileSync } from 'node:fs';
import { planRaid, raidTargets } from '../../src/campaign/airwar';
import { createGame, setWar } from '../../src/campaign/game';
import { startOpen } from '../../src/campaign/staff';
import type { Game } from '../../src/campaign/types';
import { SKY } from '../../src/battle/air/book';
import { AirSim } from '../../src/battle/air/sim';
import { airTicket, raidGroups } from '../../src/battle/air/ticket';
import type { AirUnit, AirPlace } from '../../src/battle/air/types';
import type { FieldTicket } from '../../src/battle/core/types';

const r1 = (x: number): number => Math.round(x * 10) / 10;
const r3 = (x: number): number => Math.round(x * 1000) / 1000;

/** One air unit as the bridge hands it to the 3D game (battle-bridge.ts unitOut, for an air group; record.ts's copy). */
function unitOut(u: AirUnit): Record<string, unknown> {
  const loop = u.act === 'idle' || u.act === 'down' ? '' : SKY.cues.loops[u.air];
  const extra = { ...(u.extra ?? {}), n: u.n, n0: u.n0, band: u.band, alt: Math.round(u.alt), job: u.job.do, per: u.per };
  return {
    id: u.id, side: u.side, kind: u.kind, name: u.name, x: r1(u.x), y: r1(u.y), face: r3(u.face), str: r3(u.str), mor: r3(u.mor), state: u.state,
    figures0: u.figures0, figures: u.figures, w: u.w, d: u.d, act: u.act, speed: r1(u.speed), engaged: u.engaged ?? -1, cover: u.cover ?? '', loop, order: null, extra,
  };
}

function campaign(splice: boolean): { g: Game; tk: FieldTicket<AirPlace> } {
  const g = startOpen(createGame(7, 'USA'));
  setWar(g, 'USA', 'GER');
  const lincs = g.regions.findIndex((r) => r.name === 'Lincolnshire');
  if (lincs < 0) throw new Error('no Lincolnshire');
  for (const f of g.airFleets) {
    if (f.nat !== 'USA' || f.bmr <= 0) continue;
    f.rec = { ...(f.rec ?? {}), missions: 20 };
    if ([22, 57, 58].includes(f.id)) f.base = lincs;
  }
  if (splice) {
    const k = g.airFleets.findIndex((f) => f.id === 58);
    const [row] = g.airFleets.splice(k, 1);
    g.airFleets.splice(g.airFleets.findIndex((f) => f.id === 22), 0, row);
  }
  g.phase = g.order.indexOf('USA');
  const t = raidTargets(g, 'bomb', 'USA').find((x) => planRaid(g, 'bomb', x.id, undefined, 'USA').place === 'Rhineland');
  if (!t) throw new Error('no raid on the Rhineland');
  const ids = raidGroups(g, 'bomb', t.id, 'USA');
  const tk = airTicket(g, 'bomb', t.id, ids, 'USA').ticket!;
  return { g, tk };
}

function boardOf(tk: FieldTicket<AirPlace>): Record<string, unknown> {
  const P = tk.place;
  const place = { kind: P.kind, mission: P.mission, name: P.name, nat: P.nat, when: P.when, models: P.models, bombers: P.bombers };
  return { w: SKY.w, h: SKY.h, scale: SKY.scale, target: SKY.target, flakR: SKY.flakR, bands: SKY.bands.alt, place, title: tk.title, date: tk.date, att: { nat: tk.att.nat, adj: tk.att.adj, human: tk.att.human }, def: { nat: tk.def.nat, adj: tk.def.adj, human: tk.def.human } };
}

function raid(splice: boolean, mostNamed: number): Record<string, unknown> {
  const { tk } = campaign(splice);
  tk.att.human = true; tk.def.human = false;
  const s = new AirSim(tk, { show: true });
  s.ai.att = true; s.ai.def = true;
  const forces = s.units.filter((u) => u.side === 'att' && u.air === 'bmr').map((u) => ({ id: u.id, corps: u.corps, name: u.name }));
  const views: Record<string, unknown>[] = [];
  let next = 0, events: unknown[] = [], named = 0, endT = Infinity, met = false;
  const take = (): void => {
    const v = s.view();
    events.push(...v.events);
    for (const e of v.events) {
      const x = e.extra as { bomber?: { art: number }; models?: number; model?: number } | undefined;
      if (e.what === 'air:lost' && x?.bomber && (x.models ?? 1) > 0) named++;
      met = met || e.what === 'contact';
    }
    if (named >= mostNamed && endT === Infinity) endT = s.t + 20;
    if (s.t + 1e-9 < next && !s.over) return;
    // (a view every 10 s on the way in, as the show takes any spacing; a view a second from the first contact)
    next = met ? next + 1 : next + 10;
    views.push({ t: r1(v.t), units: v.units.map((u) => unitOut(u as AirUnit)), events });
    events = [];
  };
  take();
  let guard = 0;
  while (mostNamed > 0 && !s.over && s.t < endT && guard++ < 200_000) { s.step(); take(); }
  const lost = views.flatMap((v) => (v.events as { what: string; t: number; unit: number; extra?: { bomber?: { seat: number; art: number; name: string }; models?: number; model?: number } }[])
    .filter((e) => (e.what === 'air:lost' || e.what === 'air:straggle') && e.extra?.bomber)
    .map((e) => `${e.t.toFixed(0)} ${e.what} u${e.unit} m${e.extra!.models ?? e.extra!.model} seat ${e.extra!.bomber!.seat} art ${e.extra!.bomber!.art}`));
  console.error(`${splice ? 'spliced' : 'seats'}: forces ${forces.map((f) => `${f.id}=${f.corps}`).join(' ')}; ${views.length} views to ${s.t.toFixed(0)} s; named:\n  ${lost.join('\n  ')}`);
  return { sky: boardOf(tk), forces, views };
}

const out = {
  about: 'Two raids\' TC.airStart answers as the rules make them (base df247649; made by tools/air-record/nose-art-raid.ts, bundled from the rules with esbuild as tools/air-record/record.ts is): a new American campaign, seed 7, at war with Germany, three of its bomber groups (I Bomber Command, the 1st and 2nd Bombardment Groups) based in Lincolnshire with 20 missions on their records, raiding the Rhineland as the raid card picks it. Each its board (place trimmed to its kind, mission, name, nation, month, models and bombers, the seats of campaign/bombers.ts), "forces" (the rules\' own id and corps of each bomber box, in the order the rules number them) and its views through the bridge\'s unitOut. "seats": the campaign as it opens, fought out by the computer on both sides, a view every 10 s on the way in and every second from the first contact, until several of its named bombers have fallen in the picture (their losses\' extra.bomber) and 20 s more. "spliced": the same campaign with the 2nd Bombardment Group\'s row moved ahead of the others in g.airFleets, as the campaign\'s growCommands and splitOff leave a newer group ahead of older ones, so that the ticket\'s force order is not ascending by corps; its first view only. For the nose art\'s cloud test, Tools/air_show/air_nose_art_test.cpp.',
  seats: raid(false, Number(process.argv[3] ?? 8)),
  spliced: raid(true, 0),
};
writeFileSync(process.argv[2], JSON.stringify(out));
console.error(`${(JSON.stringify(out).length / 1e3).toFixed(0)} KB`);
