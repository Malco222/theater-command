// Shadowing (release 2, shadow.ts; Brian 24 November 2026 19:47Z, "yes to all"): a fleet of cruisers set on an enemy
// fleet follows it each month while it is in reach, keeps it identified, and reports where it is; it seeks no battle and
// mostly keeps its distance when found alone. The computer's admirals shadow by the same rule. Behind its own switch.
import { describe, expect, it } from 'vitest';
import stage from '../data/campaign/stage.json';
import { createGame } from '../src/campaign/game';
import { detach } from '../src/campaign/fleets';
import { setFog } from '../src/campaign/intel';
import { fleetReach, navalAI, seaBattles, sail } from '../src/campaign/navy';
import { loadGame, saveGame } from '../src/campaign/save';
import { SHADOW, aiShadows, setShadow, shadowLine, shadowOn, shadowsFollow, whyNotShadow } from '../src/campaign/shadow';
import { SHADOW_STAGE } from '../src/campaign/stage';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, fleetOptions, humanUp, orderFleet, orderShadowFleet, stepAI, stopShadowFleet } from '../src/campaign/turn';
import type { Fleet, Game } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

/** A grand campaign with the player's country at the table in its own month, the switch as given, the fog lifted. */
function seat(nat: string, seed: number, on: boolean): Game {
  const g = startOpen(createGame(seed, nat));
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  setShadow(g, on);
  setFog(g, false);
  beginHuman(g);
  return g;
}
const fleet = (g: Game, nat: string, name: string): Fleet => g.fleets.find((f) => f.nat === nat && f.name === name)!;
/** Two cruisers split off the Home Fleet, where they are, as a squadron of their own. */
const squadron = (g: Game): Fleet => detach(g, fleet(g, 'UK', 'Home Fleet'), { ca: 2 }, 'Cruiser Squadron')!;

describe('the switch', () => {
  it('ships off, and off nothing of it is offered or read', () => {
    expect((stage as { shadow?: boolean }).shadow).toBe(false);
    expect(SHADOW_STAGE).toBe(false);
    const g = seat('UK', 4, false);
    expect(shadowOn(g)).toBe(false);
    const sq = squadron(g), battle = fleet(g, 'GER', 'Battle Fleet');
    expect(whyNotShadow(g, sq.id, battle.id)).toMatch(/switched on/);
    expect(orderShadowFleet(g, sq.id, battle.id)).toBe(false);
    expect(fleetOptions(g, sq.id)!.shadow).toBeUndefined();
    const before = saveGame(g);
    shadowsFollow(g, 'UK');
    aiShadows(g, 'GER');
    expect(saveGame(g)).toBe(before);
  });
  it('switched off in a campaign under way, every shadow ends where it stands', () => {
    const g = seat('UK', 4, true);
    const sq = squadron(g), battle = fleet(g, 'GER', 'Battle Fleet');
    expect(orderShadowFleet(g, sq.id, battle.id)).toBe(true);
    const zone = sq.zone;
    setShadow(g, false);
    expect(sq.shadow).toBeUndefined();
    expect(sq.zone).toBe(zone);
  });
});

describe('the order', () => {
  it("is cruisers' work: a fleet with battleships is refused, a squadron split off is not", () => {
    const g = seat('UK', 4, true);
    const home = fleet(g, 'UK', 'Home Fleet'), battle = fleet(g, 'GER', 'Battle Fleet');
    expect(whyNotShadow(g, home.id, battle.id)).toMatch(/cruisers' work/);
    const sq = squadron(g);
    expect(fleetOptions(g, sq.id)!.shadow).toContain(battle.id);
    expect(whyNotShadow(g, sq.id, battle.id)).toBeNull();
    expect(orderShadowFleet(g, sq.id, battle.id)).toBe(true);
    expect(sq.shadow).toBe(battle.id);
    expect(sq.stance).toBe('patrol');
    expect(sq.zone).toBe(battle.stance === 'port' ? battle.home : battle.zone);
    expect(sq.ordered).toBe(true);
    expect(battle.seen?.UK).toBe(g.turn);
    expect(shadowLine(g, sq)).toMatch(/^Shadowing the Battle Fleet: it reports it /);
    expect(whyNotShadow(g, sq.id, battle.id)).toMatch(/already/);
  });

  it('follows its quarry, reports where it is, and loses it out of reach', () => {
    const g = seat('UK', 4, true);
    const sq = squadron(g), battle = fleet(g, 'GER', 'Battle Fleet');
    expect(orderShadowFleet(g, sq.id, battle.id)).toBe(true);
    // The Battle Fleet moves to another sea the squadron reaches: the squadron follows and reports it there.
    const reach = fleetReach(g, sq);
    const next = fleetReach(g, battle).find((z) => reach.includes(z) && z !== sq.zone)!;
    expect(next).toBeDefined();
    expect(sail(g, battle, next, 'patrol')).toBe(true);
    const news = g.headlines.length;
    shadowsFollow(g, 'UK');
    expect(sq.zone).toBe(next);
    expect(sq.shadow).toBe(battle.id);
    expect(g.headlines.slice(news).some((h) => /shadowing the Battle Fleet, reports it/.test(h.text))).toBe(true);
    // Out of its reach: contact is lost, and the squadron stays where it was.
    const far = g.seas.find((z) => !reach.includes(z.id))!;
    battle.zone = far.id; battle.stance = 'patrol';
    shadowsFollow(g, 'UK');
    expect(sq.shadow).toBeUndefined();
    expect(sq.zone).toBe(next);
    expect(g.headlines.slice(news).some((h) => /has lost contact with the Battle Fleet/.test(h.text))).toBe(true);
  });

  it('a new order of his ends it; so does STOP SHADOWING', () => {
    const g = seat('UK', 4, true);
    const sq = squadron(g), battle = fleet(g, 'GER', 'Battle Fleet');
    expect(orderShadowFleet(g, sq.id, battle.id)).toBe(true);
    expect(stopShadowFleet(g, sq.id)).toBe(true);
    expect(sq.shadow).toBeUndefined();
    expect(orderShadowFleet(g, sq.id, battle.id)).toBe(true);
    expect(orderFleet(g, sq.id, sq.home, 'port')).toBe(true);
    expect(sq.shadow).toBeUndefined();
  });
});

describe('at sea', () => {
  it('found alone by an enemy who would fight it, it keeps its distance most times; otherwise it is caught as any fleet is', () => {
    const g = seat('UK', 4, true);
    const sq = squadron(g), battle = fleet(g, 'GER', 'Battle Fleet');
    expect(orderShadowFleet(g, sq.id, battle.id)).toBe(true);
    // Only the squadron of the British is in that sea, and the Germans patrol it at the end of their month.
    for (const f of g.fleets) if (f.nat === 'UK' && f.id !== sq.id && f.zone === sq.zone && f.stance !== 'port') f.stance = 'port';
    battle.stance = 'patrol';
    const base = saveGame(g);
    let slipped = 0, fought = 0, ran = 0;
    for (let k = 0; k < 20; k++) {
      const h = loadGame(base)!;
      h.turn += k;
      const news = h.headlines.length;
      const reports = seaBattles(h, 'GER');
      if (h.headlines.slice(news).some((x) => /keeps its distance from/.test(x.text))) slipped++;
      if (reports.some((r) => r.def.some((d) => d.fleet === 'Cruiser Squadron'))) fought++;
      // (Not slipped away, a fleet outweighed as far as this may still decline battle and make for port: navy.json battle.evade.)
      if (h.headlines.slice(news).some((x) => /declines battle/.test(x.text))) ran++;
    }
    expect(slipped + fought + ran).toBe(20);
    expect(slipped).toBeGreaterThanOrEqual(Math.floor(20 * SHADOW.slip) - 5);
    expect(slipped).toBeLessThan(20);
  });
});

describe("the computer's admirals", () => {
  it('set a shadow on the strongest enemy fleet at sea they know of, and leave it to its work', () => {
    const g = seat('GER', 3, true);
    const battle = fleet(g, 'GER', 'Battle Fleet');
    const home = fleet(g, 'UK', 'Home Fleet');
    const sq = detach(g, home, { ca: 2 }, 'Cruiser Squadron')!;
    sq.ready = 1;
    // The Battle Fleet is out where the British cruisers reach it, and the British have it in contact.
    const z = fleetReach(g, sq).find((x) => fleetReach(g, battle).includes(x))!;
    expect(sail(g, battle, z, 'patrol')).toBe(true);
    (battle.seen ??= {}).UK = g.turn;
    aiShadows(g, 'UK');
    const shadows = g.fleets.filter((f) => f.nat === 'UK' && f.shadow !== undefined);
    expect(shadows.length).toBe(SHADOW.ai.most);
    expect(shadows[0].shadow).toBe(battle.id);
    const at = shadows[0].zone;
    navalAI(g, 'UK');
    expect(shadows[0].zone).toBe(at);
    expect(shadows[0].shadow).toBe(battle.id);
    // Worn, it comes home.
    shadows[0].ready = SHADOW.ai.home - 0.1;
    aiShadows(g, 'UK');
    expect(shadows[0].shadow).toBeUndefined();
    expect(shadows[0].stance).toBe('port');
  });
});

describe('through the bridge', () => {
  it('the fleet page offers SHADOW IT, and the order and STOP SHADOWING go through act', () => {
    const g = seat('UK', 4, true);
    const sq = squadron(g), battle = fleet(g, 'GER', 'Battle Fleet');
    type Call = (...a: unknown[]) => unknown;
    const TC = (globalThis as unknown as { TC: Record<string, Call> }).TC;
    TC.load(saveGame(g));
    expect((TC.view() as { shadow: boolean }).shadow).toBe(true);
    const page = TC.page(`fleet:${sq.id}`) as { sections: { heading: string; rows: { buttons: { label: string; act: string }[] }[] }[] };
    const rows = page.sections.find((x) => x.heading === 'ENEMY FLEETS TO SHADOW')!.rows;
    expect(rows.some((r) => r.buttons.some((b) => b.label === 'SHADOW IT' && b.act === `fleetshadow:${sq.id}:${battle.id}`))).toBe(true);
    const out = TC.act(`fleetshadow:${sq.id}:${battle.id}`) as { ok: boolean; note: string };
    expect(out.ok).toBe(true);
    expect(out.note).toMatch(/Shadowing the Battle Fleet/);
    const after = TC.page(`fleet:${sq.id}`) as { top: { label: string }[]; sections: { heading: string; text: string }[] };
    expect(after.top.some((b) => b.label === 'STOP SHADOWING')).toBe(true);
    expect(after.sections.find((x) => x.heading === 'SHADOWING')!.text).toMatch(/Battle Fleet/);
    expect((TC.act(`fleetshadow:${sq.id}:off`) as { ok: boolean }).ok).toBe(true);
    expect((TC.act('shadowrule:off') as { ok: boolean }).ok).toBe(true);
    expect((TC.view() as { shadow: boolean }).shadow).toBe(false);
  });
});
