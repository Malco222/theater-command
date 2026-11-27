// The bomber crews' page (release 2; Brian's yes of 27 November 2026 05:23Z to "bomber crews with a tour"): a bomber
// group kept by name has a page in the 3D game's rooms, 'crews:<group id>', with its bombers one by one, the missions
// each has flown, how near each crew is to the end of its tour, the crews gone home and the bombers lost, and the
// player's standing order for finished crews. Its own switch ('crewrollrule'), on by the release 2 ship list. It reads
// the records (bombers.ts) and changes none: reading it leaves the save as it was.
import { describe, expect, it } from 'vitest';
import { airAct, airPage, crewRollOn, setCrewRoll } from '../src/campaign/airpages';
import { flownMonth, muster, tourOf } from '../src/campaign/bombers';
import { createGame } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import { loadGame, saveGame } from '../src/campaign/save';
import type { AirFleet, Game } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

type Call = (...a: unknown[]) => unknown;
const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;
/** The grand campaign, in the player's own phase of the month (an order is his only then). */
const war = (seed: number, human: string | null): Game => {
  const g = startOpen(createGame(seed, human));
  if (human) g.phase = g.order.indexOf(human);
  return g;
};
const command = (g: Game, nat = 'USA'): AirFleet => g.airFleets.find((f) => f.nat === nat && f.bmr >= 40 && f.ship === undefined && f.ftr === 0)!;
type P = NonNullable<ReturnType<typeof airPage>>;

/** A group that has flown some months: every bomber has missions, and one crew is a mission from home. */
function flown(g: Game): AirFleet {
  const f = command(g);
  muster(g, f);
  // (three months: at most 15 missions each, so only the old hand below can finish a tour of 25 in the next)
  for (let i = 0; i < 3; i++) flownMonth(g, f, 0);
  const last = f.bombers!.on[0];
  last[2] = last[5] = tourOf(f.nat) - 1;
  return f;
}

describe('the crews page (release 2)', () => {
  it("says the bomber's own count only where it adds something: a veteran with a new crew, or fighters shot down", () => {
    const g = war(3, 'USA');
    const f = flown(g);
    setCrewRoll(g, true);
    const b = f.bombers!.on[0];
    b[2] = 40; b[5] = 24; b[3] = 0;
    let row = (airPage(g, `crews:${f.id}`) as P).sections[1].rows[0];
    expect(row.lines).toEqual([expect.stringMatching(/: 24 of their 25, one to go\.$/), 'The bomber has flown 40 missions.']);
    b[3] = 3;
    row = (airPage(g, `crews:${f.id}`) as P).sections[1].rows[0];
    expect(row.lines[1]).toBe('The bomber has flown 40 missions, with 3 fighters shot down.');
    b[2] = 24; b[3] = 1;
    row = (airPage(g, `crews:${f.id}`) as P).sections[1].rows[0];
    expect(row.lines[1]).toBe('A fighter shot down.');
  });

  it('is off until its switch is on: no page and no button, and the war is as it was', () => {
    const g = war(3, 'USA');
    const f = flown(g);
    expect(crewRollOn(g)).toBe(false);
    expect(airPage(g, `crews:${f.id}`)).toBeNull();
    expect(airPage(g, `group:${f.id}`)!.top.some((b) => b.label === 'THE CREWS')).toBe(false);
  });

  it('shows the bombers old hands first, their missions and each crew against its tour, and the door from the group', () => {
    const g = war(3, 'USA');
    const f = flown(g);
    setCrewRoll(g, true);
    const door = airPage(g, `group:${f.id}`)!.top.find((b) => b.label === 'THE CREWS');
    expect(door?.act).toBe(`open:crews:${f.id}`);
    const p = airPage(g, `crews:${f.id}`) as P;
    expect(p.id).toBe(`crews:${f.id}`);
    expect(p.title).toContain(f.name.toUpperCase());
    expect(p.headline).toMatch(new RegExp(`^${f.bombers!.on.length.toLocaleString('en-US')} bombers · no crew home yet · none lost$`));
    const rows = p.sections[1].rows;
    expect(rows.length).toBe(Math.min(12, f.bombers!.on.length));
    // The old hand a mission from home comes first, says so, and has a name (America names her bombers).
    expect(rows[0].title).not.toBe('No name yet');
    expect(rows[0].lines[0]).toMatch(/: 24 of their 25, one to go\.$/);
    // (The UI director, 16:12Z: no second line that only says the crew's count again.)
    expect(rows[0].lines.length).toBe(1);
    // Nearest home first: no crew below has flown more of its tour than the one above it.
    const crewOf = (r: (typeof rows)[number]): number => Number(/: (\d+) of their/.exec(r.lines[0])![1]);
    expect(rows.every((r, i) => i === 0 || crewOf(rows[i - 1]) >= crewOf(r))).toBe(true);
    // The standing order: three buttons, the order it has now shut and saying so.
    const order = p.sections[0].rows[0];
    expect(order.title).toBe('Crews go home after 25 missions');
    expect(order.buttons.map((b) => b.act)).toEqual([`crews:${f.id}:school`, `crews:${f.id}:bonds`, `crews:${f.id}:split`]);
    expect(order.buttons[2]).toMatchObject({ ok: false, tip: 'This is your standing order now.' });
    expect(p.sections[2].text).toMatch(/^No crew has finished its tour yet/);
  });

  it('reading it changes nothing in the war, and its one button gives the standing order', () => {
    const g = war(4, 'USA');
    const f = flown(g);
    setCrewRoll(g, true);
    const before = saveGame(g);
    airPage(g, `crews:${f.id}`);
    expect(saveGame(g)).toBe(before);
    const r = airAct(g, `crews:${f.id}:school`)!;
    expect(r.ok).toBe(true);
    expect(r.open).toBe(`crews:${f.id}`);
    expect(g.stories.crews!.USA.order).toBe('school');
    const order = (airPage(g, `crews:${f.id}`) as P).sections[0].rows[0];
    expect(order.buttons[0]).toMatchObject({ ok: false, tip: 'This is your standing order now.' });
    expect(order.lines[0]).toBe("The group's readiness rises each time. Then they train new crews.");
    expect(airAct(g, `crews:${f.id}:nowhere`)).toBeNull();
  });

  it('a crew that finishes its tour is on the roll of those gone home', () => {
    const g = war(5, 'USA');
    const f = flown(g);
    setCrewRoll(g, true);
    flownMonth(g, f, 0);
    const p = airPage(g, `crews:${f.id}`) as P;
    expect(p.headline).toMatch(/· 1 crew gone home ·/);
    expect(p.sections[2].text).toMatch(/'s crew went home after 25 missions\.|A bomber's crew went home after 25 missions\./);
  });

  it("is the player's own: a computer's group, and a carrier wing, have none", () => {
    const g = war(3, 'GER');
    setCrewRoll(g, true);
    const usa = command(g);
    muster(g, usa);
    expect(airPage(g, `crews:${usa.id}`)).toBeNull();
    const wing = g.airFleets.find((f) => f.ship !== undefined);
    if (wing) expect(airPage(g, `crews:${wing.id}`)).toBeNull();
  });

  it('the 3D ship list turns it on once, and a switch the player turns off stays off', () => {
    TC().start({ view: 'grand', human: 'USA', seed: 11 });
    const read = (): Game => loadGame((TC().save() as { text: string }).text)!;
    expect(crewRollOn(read())).toBe(false);
    expect(TC().r2Ship(['crewrollrule'])).not.toBeNull();
    expect(crewRollOn(read())).toBe(true);
    expect(read().flags['r2:crewrollrule']).toBe(1);
    TC().act('crewrollrule:off');
    expect(TC().r2Ship(['crewrollrule'])).toBeNull();
    expect(crewRollOn(read())).toBe(false);
    TC().act('crewrollrule:on');
    expect(crewRollOn(read())).toBe(true);
  });
});
