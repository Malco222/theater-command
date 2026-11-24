// The shadow relay (release 2, shadow.ts; Brian 24 November 2026 20:38Z, "yes to all"): a shadow's fuel lasts
// SHADOW.relay.months on station; then it makes for harbour, and a fresh cruiser fleet from harbour takes over without
// losing contact. With none, contact is lost. The same for every country. Its own switch, read only with shadowing on.
import { describe, expect, it } from 'vitest';
import stage from '../data/campaign/stage.json';
import { createGame } from '../src/campaign/game';
import { detach } from '../src/campaign/fleets';
import { setFog } from '../src/campaign/intel';
import { fleetReach, sail } from '../src/campaign/navy';
import { loadGame, saveGame } from '../src/campaign/save';
import { SHADOW, aiShadows, setShadow, setShadowRelay, shadowLine, shadowRelayOn, shadowsFollow } from '../src/campaign/shadow';
import { SHADOWRELAY_STAGE } from '../src/campaign/stage';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, humanUp, orderShadowFleet, stepAI } from '../src/campaign/turn';
import type { Fleet, Game } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

function seat(nat: string, seed: number, relay: boolean): Game {
  const g = startOpen(createGame(seed, nat));
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  setShadow(g, true);
  setShadowRelay(g, relay);
  setFog(g, false);
  beginHuman(g);
  return g;
}
const fleet = (g: Game, nat: string, name: string): Fleet => g.fleets.find((f) => f.nat === nat && f.name === name)!;
const byId = (g: Game, id: number): Fleet => g.fleets.find((f) => f.id === id)!;
/** A squadron of two cruisers off the Home Fleet, and (with `spare`) a second one resting in harbour, fit for sea. */
function squadrons(g: Game, spare: boolean): { sq: Fleet; rest?: Fleet } {
  const home = fleet(g, 'UK', 'Home Fleet');
  const sq = detach(g, home, { ca: 2 }, '1st Cruiser Squadron')!;
  if (!spare) return { sq };
  const rest = detach(g, home, { ca: 2 }, '2nd Cruiser Squadron')!;
  expect(sail(g, rest, rest.home, 'port')).toBe(true);
  rest.ready = 1;
  return { sq, rest };
}
/** The same campaign `months` later, at the start of Britain's month. */
const later = (g: Game, months: number): Game => { const h = loadGame(saveGame(g))!; h.turn += months; return h; };

describe('the switch', () => {
  it('ships off; off, a shadow stays on station as long as it keeps contact, and no fuel is counted', () => {
    expect((stage as { shadowRelay?: boolean }).shadowRelay).toBe(false);
    expect(SHADOWRELAY_STAGE).toBe(false);
    const g = seat('UK', 4, false);
    expect(shadowRelayOn(g)).toBe(false);
    const { sq } = squadrons(g, true);
    const battle = fleet(g, 'GER', 'Battle Fleet');
    expect(orderShadowFleet(g, sq.id, battle.id)).toBe(true);
    expect(sq.shadowAt).toBeUndefined();
    const h = later(g, SHADOW.relay.months + 3);
    shadowsFollow(h, 'UK');
    expect(byId(h, sq.id).shadow).toBe(battle.id);
    expect(byId(h, sq.id).shadowAt).toBeUndefined();
    expect(shadowLine(h, byId(h, sq.id))).not.toMatch(/fuel/);
  });
  it('is read only with shadowing on', () => {
    const g = seat('UK', 4, true);
    setShadow(g, false);
    expect(g.flags.shadowRelay).toBe(1);
    expect(shadowRelayOn(g)).toBe(false);
  });
});

describe('the relay', () => {
  it('out of fuel, the shadow makes for harbour and a fresh squadron from harbour takes over: contact is kept', () => {
    const g = seat('UK', 4, true);
    const { sq, rest } = squadrons(g, true);
    const battle = fleet(g, 'GER', 'Battle Fleet');
    expect(orderShadowFleet(g, sq.id, battle.id)).toBe(true);
    expect(sq.shadowAt).toBe(g.turn);
    expect(shadowLine(g, sq)).toMatch(/Its fuel lasts 3 more months on station: 2nd Cruiser Squadron, in harbour at Scapa Flow, would take over\./);
    // A month short of the end, it stays.
    const early = later(g, SHADOW.relay.months - 1);
    shadowsFollow(early, 'UK');
    expect(byId(early, sq.id).shadow).toBe(battle.id);
    expect(shadowLine(early, byId(early, sq.id))).toMatch(/Its fuel runs out at the start of next month/);
    const h = later(g, SHADOW.relay.months);
    const n = h.headlines.length;
    shadowsFollow(h, 'UK');
    const old = byId(h, sq.id), fresh = byId(h, rest!.id);
    expect(old.shadow).toBeUndefined();
    expect(old.stance).toBe('port');
    expect(fresh.shadow).toBe(battle.id);
    expect(fresh.shadowAt).toBe(h.turn);
    expect(fresh.stance).toBe('patrol');
    expect(fresh.zone).toBe(fleet(h, 'GER', 'Battle Fleet').zone);
    expect(fresh.ordered).toBe(true);
    expect(fleet(h, 'GER', 'Battle Fleet').seen?.UK).toBe(h.turn);
    const news = h.headlines.slice(n).map((x) => x.text);
    expect(news.some((t) => /2nd Cruiser Squadron takes over shadowing the Battle Fleet from 1st Cruiser Squadron, which makes for Scapa Flow to refuel\. Contact is kept/.test(t))).toBe(true);
    // One line for the hand-over: the fresh squadron does not report again in the same opening.
    expect(news.filter((t) => /Battle Fleet/.test(t)).length).toBe(1);
  });

  it('with no fresh squadron in harbour, the shadow makes for harbour and contact is lost', () => {
    const g = seat('UK', 4, true);
    const { sq } = squadrons(g, false);
    const battle = fleet(g, 'GER', 'Battle Fleet');
    expect(orderShadowFleet(g, sq.id, battle.id)).toBe(true);
    expect(shadowLine(g, sq)).toMatch(/no fresh cruiser fleet lies in harbour to take over, so contact would be lost/);
    const h = later(g, SHADOW.relay.months);
    const n = h.headlines.length;
    shadowsFollow(h, 'UK');
    expect(byId(h, sq.id).shadow).toBeUndefined();
    expect(byId(h, sq.id).stance).toBe('port');
    expect(h.fleets.some((f) => f.shadow !== undefined)).toBe(false);
    expect(h.headlines.slice(n).some((x) => /must make for .* to refuel after 3 months shadowing the Battle Fleet, and no fresh cruiser fleet lies in harbour to take over: contact is lost/.test(x.text))).toBe(true);
  });

  it("the computer's admirals' shadows are relieved by the same rule", () => {
    const g = seat('GER', 3, true);
    const battle = fleet(g, 'GER', 'Battle Fleet');
    const home = fleet(g, 'UK', 'Home Fleet');
    const a = detach(g, home, { ca: 2 }, '1st Cruiser Squadron')!;
    const b = detach(g, home, { ca: 2 }, '2nd Cruiser Squadron')!;
    a.ready = 1; b.ready = 0.7;
    // The Battle Fleet is out where the British cruisers reach it, and the British have it in contact.
    const z = fleetReach(g, a).find((x) => fleetReach(g, battle).includes(x))!;
    expect(sail(g, battle, z, 'patrol')).toBe(true);
    (battle.seen ??= {}).UK = g.turn;
    aiShadows(g, 'UK');
    const on = g.fleets.find((f) => f.nat === 'UK' && f.shadow === battle.id)!;
    expect(on).toBeDefined();
    expect(on.shadowAt).toBe(g.turn);
    const spare = on.id === a.id ? b : a;
    expect(sail(g, spare, spare.home, 'port')).toBe(true);
    spare.ready = 1;
    const h = later(g, SHADOW.relay.months);
    shadowsFollow(h, 'UK');
    expect(byId(h, on.id).shadow).toBeUndefined();
    expect(byId(h, spare.id).shadow).toBe(battle.id);
  });
});

describe('through the bridge', () => {
  it('the view carries the switch, and act switches it', () => {
    const g = seat('UK', 4, false);
    type Call = (...a: unknown[]) => unknown;
    const TC = (globalThis as unknown as { TC: Record<string, Call> }).TC;
    TC.load(saveGame(g));
    expect((TC.view() as { shadowRelay: boolean }).shadowRelay).toBe(false);
    const out = TC.act('shadowrelayrule:on') as { ok: boolean; note: string };
    expect(out.ok).toBe(true);
    expect(out.note).toMatch(/fresh cruiser fleet from harbour takes over/);
    expect((TC.view() as { shadowRelay: boolean }).shadowRelay).toBe(true);
  });
});
