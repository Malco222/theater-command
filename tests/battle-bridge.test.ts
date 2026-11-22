// The 3D game's doorway to the field battles (unreal/TheaterCommand/Rules/battle-bridge.ts): a real campaign attack
// fought through it, the way the game will call it, and settled through the campaign's own attack order.
import { describe, expect, it } from 'vitest';
import { battleCalls, planOut, unitOut } from '../unreal/TheaterCommand/Rules/battle-bridge';
import { LAND, practiceTicket, PRACTICE_GROUNDS, LandBattle, sampleTicket, withTeams } from '../src/battle/land';
import { newCampaign, offers } from '../src/battle/ui/campaign';
import { attackOutlook, orderAttack } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';

function rig(): { g: Game; TC: ReturnType<typeof battleCalls>; fought: (number | undefined)[] } {
  const g = newCampaign();
  const fought: (number | undefined)[] = [];
  const TC = battleCalls({
    game: () => g,
    attack: (ids, to, f) => {
      fought.push(f);
      const report = orderAttack(g, ids.map((i) => g.corps.find((c) => c.id === i)!), g.regions[to], f);
      return { report };
    },
  });
  return { g, TC, fought };
}

describe('the battle doorway for the 3D game', () => {
  it('opens paused on the attack card\'s own ticket, and sends the ground once', () => {
    const { g, TC } = rig();
    const o = offers(g)[0];
    const ids = o.corps.map((c) => c.id);
    expect(TC.battleLook(ids, o.target.id)!.ticket!.reckoning.odds).toBeCloseTo(o.odds);
    const st = TC.battleStart(ids, o.target.id)!;
    expect(st.field.ground.length).toBe(st.field.cols * st.field.rows);
    expect(st.view.speed).toBe(0);
    expect(TC.battleTick(5).t).toBe(0);
    const u = (st.view.units as { side: string; loop: string; figures0: number }[]).find((x) => x.side === 'att')!;
    expect(u.figures0).toBeGreaterThan(0);
    expect(typeof u.loop).toBe('string');
  });
  it('a fight is its shots and beds: no block plays a fight loop, but men fighting among buildings get the street battle', () => {
    const b = new LandBattle(practiceTicket(PRACTICE_GROUNDS[0], { seed: 7, humanAtt: true }));
    const foot = b.units.find((u) => u.kind === 'inf')!, tank = b.units.find((u) => u.kind === 'arm'), gun = b.units.find((u) => u.kind === 'art');
    foot.act = 'fight'; foot.cover = 'open';
    expect(unitOut(foot).loop).toBe('');
    foot.cover = 'buildings';
    expect(unitOut(foot).loop).toBe('street_battle');
    expect((unitOut(foot).drops as { takes: string[] }).takes).toEqual(['mg_town_01', 'mg_town_02', 'mg_town_03']);
    if (tank) { tank.act = 'fight'; tank.cover = 'open'; expect(unitOut(tank).loop).toBe('tank_idle_loop'); }
    if (gun) { gun.act = 'fight'; gun.cover = 'buildings'; expect(unitOut(gun).loop).toBe(''); expect((unitOut(gun).drops as { takes: string[] }).takes[0]).toBe('far_guns_01'); }
  });
  it('a defence the card did not know comes as plain stands until his blocks have fought it (b464(2))', () => {
    const { g, TC } = rig();
    const o = offers(g).find((x) => attackOutlook(g, x.corps, x.target)?.exact === false)!;
    const st = TC.battleStart(o.corps.map((c) => c.id), o.target.id)!;
    // HAND IT TO THE STAFF's question gives the card's band, never the chance at the true odds (b453).
    const look = attackOutlook(g, o.corps, o.target)!;
    const ch = TC.battleChance() as { chance?: number; lo?: number; hi?: number; contact: boolean };
    expect(ch.chance).toBeUndefined();
    expect(ch.lo).toBeLessThanOrEqual(ch.hi!);
    expect([ch.lo, ch.hi]).toEqual([look.chanceLo, look.chanceHi].map((x) => Math.round(x * 1000) / 1000));
    type Out = { id: number; side: string; known?: boolean; name?: string; kind?: string; str?: number; figures?: number; w: number; d: number; state: string };
    const units = st.view.units as Out[];
    for (const u of units.filter((x) => x.side === 'def')) {
      expect(u.known).toBe(false);
      expect([u.name, u.kind, u.str, u.figures]).toEqual([undefined, undefined, undefined, undefined]);
      expect([u.w, u.d, u.state]).toEqual([150, 40, 'unknown']);
    }
    expect(units.filter((x) => x.side === 'att').every((x) => x.known === undefined && !!x.name)).toBe(true);
    const mine = units.filter((x) => x.side === 'att').map((x) => x.id);
    TC.battleOrder({ do: 'attack', units: mine, target: units.find((x) => x.side === 'def')!.id });
    TC.battleSpeed(4);
    let v = TC.battleView() as { units: Out[]; over: unknown };
    for (let i = 0; i < 400 && !v.over && v.units.every((x) => x.side === 'att' || x.known === false); i++) v = TC.battleTick(1) as typeof v;
    // Met in the fight, the block comes whole, as any block does.
    const met = v.units.find((x) => x.side === 'def' && x.known === undefined)!;
    expect(met.name).toBeTruthy();
    expect(met.str).toBeGreaterThan(0);
  });
  it('takes the player\'s orders, runs the clock, and settles through the campaign with the field\'s R', () => {
    const { g, TC, fought } = rig();
    const o = offers(g)[0];
    const ids = o.corps.map((c) => c.id);
    const st = TC.battleStart(ids, o.target.id)!;
    const units = st.view.units as { id: number; side: string }[];
    const mine = units.filter((x) => x.side === 'att').map((x) => x.id), foe = units.find((x) => x.side === 'def')!.id;
    expect(TC.battleOrder({ do: 'attack', units: mine, target: foe }).ok).toBe(true);
    expect(TC.battleSpeed(4).speed).toBe(4);
    for (let i = 0; i < 30; i++) TC.battleTick(1);
    expect(TC.battleView().t).toBeGreaterThan(100);
    expect(TC.battleEnd()).toBeNull();
    TC.battleOrder({ do: 'dice' });
    const v = TC.battleView() as { over: { R: number } | null };
    expect(v.over).not.toBeNull();
    const out = TC.battleEnd() as { report: { odds: number } | null };
    expect(fought).toEqual([v.over!.R]);
    expect(out.report!.odds).toBeCloseTo(v.over!.R);
    expect(() => TC.battleView()).toThrow();
  });
  it('says why an army broke: each bar\'s cause in the view, and the same at the break in the event, the result and the end (b496(2))', () => {
    const { g, TC } = rig();
    const o = offers(g)[0];
    const st = TC.battleStart(o.corps.map((c) => c.id), o.target.id)!;
    const units = st.view.units as { id: number; side: string }[];
    TC.battleOrder({ do: 'attack', units: units.filter((x) => x.side === 'att').map((x) => x.id), target: units.find((x) => x.side === 'def')!.id });
    TC.battleSpeed(4);
    type Cause = { side: string; bar: number; by: string };
    type View = { over: { broke?: string; brokeBy?: Cause } | null; events: { what: string; brokeBy?: Cause }[]; breaking: Record<string, number>; breakingBy: Record<string, Cause> };
    let v = st.view as unknown as View;
    expect(v.breakingBy.att.side).toBe('att');
    const said: View['events'] = [];
    for (let i = 0; i < 200 && !v.over; i++) {
      v = TC.battleTick(1) as unknown as View;
      said.push(...v.events);
      for (const s of ['att', 'def']) expect(v.breakingBy[s].bar).toBeCloseTo(v.breaking[s], 3);
    }
    const why = v.over!.brokeBy!;
    expect(why.side).toBe(v.over!.broke);
    expect(why.bar).toBe(1);
    expect(said.find((e) => e.what === 'broken')!.brokeBy).toEqual(why);
    expect(v.breakingBy[why.side]).toEqual(why);
    expect((TC.battleEnd() as { brokeBy?: Cause }).brokeBy).toEqual(why);
  });
  it('sends the computer attack\'s plan for the pictures, never its style\'s name; a gun\'s aim; a lone tank\'s mark (ruling 239)', () => {
    const b = new LandBattle(sampleTicket('plains', 1.3, { seed: 3, style: { att: 'bold', def: 'methodical' } }));
    b.ai.att = b.ai.def = true;
    for (let i = 0; i < 30 && !b.plan.att; i++) b.step();
    const p = planOut(b.view().plan!) as Record<string, { phase: string; main: number[]; startY: number | null }>;
    expect(Object.keys(p)).toEqual(['att']);
    expect(Object.keys(p.att).sort()).toEqual(['main', 'phase', 'startY']);
    expect(p.att.main.length).toBeGreaterThanOrEqual(1);
    expect(p.att.main.length).toBeLessThanOrEqual(2);
    expect(JSON.stringify(p)).not.toMatch(/bold|methodical|mass/);
    const gun = b.units.find((u) => u.side === 'att')!, tank = b.units.find((u) => u.kind === 'arm')!;
    gun.aim = p.att.main[0];
    expect(unitOut(gun, b.t).aim).toBe(p.att.main[0]);
    tank.aloneAt = b.t;
    expect(unitOut(tank, b.t).alone).toBe(true);
    expect(unitOut(tank, b.t + 2).alone).toBeUndefined();
  });
  it('sends the men inside the blocks (the squad brain): the player\'s own always, an enemy block only once he knows it', () => {
    const { g, TC } = rig();
    const o = offers(g).find((x) => attackOutlook(g, x.corps, x.target)?.exact === false) ?? offers(g)[0];
    const st = TC.battleStart(o.corps.map((c) => c.id), o.target.id)!;
    type M = { blocks: { id: number; ph: string; men: { k: number; r: string; x: number; y: number; p: string; v: number; c: string }[] }[] };
    const units = st.view.units as { id: number; side: string; known?: boolean }[];
    let men = TC.battleMen() as M;
    const mine = units.filter((u) => u.side === 'att').map((u) => u.id);
    for (const id of mine) expect(men.blocks.some((b) => b.id === id), `block ${id}`).toBe(true);
    for (const u of units.filter((x) => x.known === false)) expect(men.blocks.some((b) => b.id === u.id)).toBe(false);
    const foot = men.blocks.find((b) => b.men.some((m) => m.r === 'officer'))!;
    expect(foot.men.every((m) => typeof m.x === 'number' && typeof m.p === 'string')).toBe(true);
    // Sent in, his men go forward with their blocks.
    const foe = units.find((x) => x.side === 'def')!.id;
    expect(TC.battleOrder({ do: 'attack', units: mine, target: foe }).ok).toBe(true);
    TC.battleSpeed(4);
    for (let i = 0; i < 10; i++) TC.battleTick(1);
    men = TC.battleMen() as M;
    expect(men.blocks.filter((b) => mine.includes(b.id)).some((b) => b.men.some((m) => m.v > 0))).toBe(true);
  });
  it('the setup step (release 2): only when asked; the clock waits while his army is placed, then runs from Begin', () => {
    const { g, TC } = rig();
    // An attack of two corps, the second made guns for the test: the campaign's opening offers bring none.
    const o = offers(g).find((x) => x.corps.length >= 2)!;
    o.corps[1].kind = 'art';
    const ids = o.corps.map((c) => c.id);
    // Two arguments: as always, no setup step.
    const plain = TC.battleStart(ids, o.target.id)!;
    expect((plain as { setup?: unknown }).setup).toBeUndefined();
    expect((plain.view as { setup?: unknown }).setup).toBeUndefined();
    const st = TC.battleStart(ids, o.target.id, { setup: true })! as typeof plain & { setup: { room: number[] } };
    expect(st.setup.room).toHaveLength(4);
    expect((st.view as { setup?: string }).setup).toBe('att');
    // A speed or a tick while placing runs nothing. Every block of his side is placed, his foot as his guns (the
    // general's controls, release 2), and turned the way he drags it.
    expect(TC.battleTick(5).t).toBe(0);
    const units = st.view.units as { id: number; side: string; kind: string; x: number; y: number }[];
    const foot = units.find((u) => u.side === 'att' && u.kind !== 'art')!;
    expect(TC.battleCanPlace(foot.id, foot.x, foot.y - 30).why ?? '').not.toMatch(/Only guns/);
    const gun = units.find((u) => u.side === 'att' && u.kind === 'art')!;
    expect(gun).toBeDefined();
    {
      const [x0, , x1, y1] = st.setup.room;
      // The ghost asks first and moves nothing; the drop places it.
      let at = -1;
      for (let x = x0 + 80; x < x1 - 80 && at < 0; x += 30) if (TC.battleCanPlace(gun.id, x, y1 - 40).why === null) at = x;
      expect(at).toBeGreaterThan(0);
      expect((TC.battleView().units as { id: number; y: number }[]).find((u) => u.id === gun.id)!.y).not.toBeCloseTo(y1 - 40, 0);
      expect(TC.battleCanPlace(foot.id, foot.x, foot.y - 30).why ?? '').not.toMatch(/Only guns/);
      const theirs = units.find((u) => u.side === 'def')!;
      expect(TC.battleCanPlace(theirs.id, theirs.x, theirs.y).why).toMatch(/your own/);
      const put = TC.battlePlace(gun.id, at, y1 - 40);
      expect(put!.ok).toBe(true);
      expect(put!.why).toBeNull();
      const moved = (put!.view.units as { id: number; y: number }[]).find((u) => u.id === gun.id)!;
      expect(moved.y).toBeCloseTo(y1 - 40, 0);
    }
    const go = TC.battleBegin();
    expect((go.view as { setup?: unknown }).setup).toBeUndefined();
    expect(go.view.speed).toBe(1);
    expect(TC.battleTick(1).t).toBeGreaterThan(0);
    expect(TC.battlePlace(foot.id, foot.x, foot.y).ok).toBe(false);
  });
  it('direct command (release 2): his staff fights what he has not taken in hand, and his orders go by radio', () => {
    const { g, TC } = rig();
    const o = offers(g)[0];
    const st = TC.battleStart(o.corps.map((c) => c.id), o.target.id, { staff: true, radio: true })!;
    const me = (st.view as { me: string }).me;
    expect((st.view as { ai: Record<string, boolean> }).ai[me]).toBe(true);
    TC.battleSpeed(1);
    TC.battleTick(2);
    type U = { id: number; side: string; kind: string; x: number; y: number; taken?: boolean; orderIn?: number };
    const units = TC.battleView().units as U[];
    expect(units.some((u) => u.taken)).toBe(false);
    const mine = units.find((u) => u.side === me && u.kind !== 'art')!;
    expect(TC.battleOrder({ do: 'move', units: [mine.id], x: mine.x, y: mine.y + (me === 'att' ? 60 : -60), run: true }).ok).toBe(true);
    const sent = (TC.battleView().units as U[]).find((u) => u.id === mine.id)!;
    expect(sent.taken).toBe(true);
    expect(sent.orderIn).toBeGreaterThan(0);
    expect(sent.orderIn).toBeLessThanOrEqual(LAND.radio.delay * LAND.radio.cut);
    for (let i = 0; i < 16; i++) TC.battleTick(1);
    const there = (TC.battleView().units as U[]).find((u) => u.id === mine.id)!;
    expect(there.orderIn).toBeUndefined();
    expect(there.taken).toBe(true);
    // Handed back to the staff.
    expect(TC.battleOrder({ do: 'land:staff', units: [mine.id] }).ok).toBe(true);
    expect((TC.battleView().units as U[]).find((u) => u.id === mine.id)!.taken).toBeUndefined();
  });
  it('a support team is sent as a gun block with its team, so the 3D game draws its own models (release 2)', () => {
    const b = new LandBattle(withTeams(practiceTicket(PRACTICE_GROUNDS[0], { seed: 7, humanAtt: true })));
    const out = b.units.filter((u) => u.team).map((u) => unitOut(u) as { kind: string; team?: string; cal?: string });
    expect(out.map((u) => [u.kind, u.team, u.cal ?? null])).toEqual([['art', 'mg', null], ['art', 'mor', 'mortar'], ['art', 'mg', null], ['art', 'mor', 'mortar']]);
    expect(b.units.filter((u) => !u.team).every((u) => (unitOut(u) as { team?: string }).team === undefined)).toBe(true);
  });
});
