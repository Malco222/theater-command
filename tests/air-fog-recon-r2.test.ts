// Fog and reconnaissance on air raids (release 2; Brian, 24 November 2026: "you should be able to bomb a german city but
// you dont know what your up against without doing recon. If a squadron does a recon mission it should lift the fog of
// war for that area for 2 turns"; the design director's ruling of 22:10Z). airfog.ts, behind g.flags.rfog, off as shipped.
import { describe, expect, it } from 'vitest';
import { AIRWAR, airAI, assessAir, fightersInReach, forecastAir } from '../src/campaign/air';
import { bandAt, knownDefence, rememberSkies, setRaidFog, skyKnown, staffReads, staffWeigh, wantsLook, weighKnown } from '../src/campaign/airfog';
import { START_LAID, setCityDefences } from '../src/campaign/citydefences';
import { airPage } from '../src/campaign/airpages';
import { offers } from '../src/campaign/arsenal';
import { allied, createGame, setWar } from '../src/campaign/game';
import { SIGHT, setFog, sightOf } from '../src/campaign/intel';
import { atSea } from '../src/campaign/navy';
import { loadGame, saveGame } from '../src/campaign/save';
import { characterOf, startOpen } from '../src/campaign/staff';
import { beginHuman, humanUp, orderRaid, orderScout, previewDayNight, previewRaid, raidTargetsFor, stepAI } from '../src/campaign/turn';
import type { AirFleet, Game } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

type Call = (...a: unknown[]) => unknown;
const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;
const F = AIRWAR.fog;

/** The player's Britain at war with Germany in September 1939, at the opening of his month. */
function britain(on: boolean): Game {
  const g = startOpen(createGame(7, 'UK'));
  setWar(g, 'UK', 'GER');
  if (on) setRaidFog(g, true);
  for (let k = 0; !humanUp(g) && k < 200; k++) stepAI(g);
  beginHuman(g);
  return g;
}
const id = (g: Game, n: string): number => g.byName[n].id;
const grp = (g: Game, name: string): AirFleet => g.airFleets.find((f) => f.nat === 'UK' && f.name === name)!;
const going = (g: Game, ids: number[]): AirFleet[] => g.airFleets.filter((f) => ids.includes(f.id));
const truth = (g: Game, ids: number[], t: number) => forecastAir(g, assessAir(g, going(g, ids), 'bomb', t), 'bomb', t);

describe('fog on raids: the switch off', () => {
  it('the card, the target list and the day and night forecast are the old ones, and nothing new is written', () => {
    const g = britain(false);
    const ruhr = id(g, 'Ruhr');
    const p = previewRaid(g, 'bomb', ruhr);
    expect(p.why).toBeNull();
    expect('fog' in p).toBe(false);
    expect(p.bomberLoss).toBe(truth(g, p.picked, ruhr).bomberLoss);
    const dn = previewDayNight(g, 'bomb', ruhr)!;
    expect('fog' in dn || 'gunsKnown' in dn || 'band' in dn.day || 'band' in dn.night).toBe(false);
    for (const t of raidTargetsFor(g, 'bomb')) expect(t.what).not.toMatch(/not known|when seen/);
    expect(orderScout(g, grp(g, 'No. 1 Group').id, ruhr).ok).toBe(true);
    airAI(g, 'GER');
    expect(g.intel.scoutBy).toBeUndefined();
    expect(g.intel.airSeen).toBeUndefined();
    expect(g.flags.rfog).toBeUndefined();
    // The bridge's raid card carries none of it.
    TC().load(saveGame(g));
    const q = previewRaid(g, 'bomb', id(g, 'Rhineland'));
    const card = (TC().page(`plan:bomb:${id(g, 'Rhineland')}:${q.picked.join('.')}`) as { card: Record<string, unknown> & { ways: Record<string, unknown> } }).card;
    expect('fog' in card).toBe(false);
    expect('fog' in card.ways).toBe(false);
    expect(card.ways.gunsFigure).not.toBe('Unknown');
  });
});

describe('fog on raids: the switch on', () => {
  it('a city never seen: fighters and guns unknown, and the forecast a band from the known defence to every fighter last seen near it rising', () => {
    const g = britain(true);
    const t = id(g, 'Rhineland');
    expect(sightOf(g, 'UK')[t]).not.toBe(SIGHT.full);
    const k = skyKnown(g, 'UK', t);
    expect(k.ago).toBe(-1);
    const p = previewRaid(g, 'bomb', t);
    expect(p.why).toBeNull();
    expect(p.fog!.fighters).toBe('Fighters: not known');
    expect(p.fog!.guns).toBe('Guns: not known');
    expect(p.fog!.band).toMatch(/^Losses forecast: \d+ to \d+% \(not scouted\)$/);
    // The low end: only the fighters known for sure (their fields in sight now), and no guns but the light flak.
    const lo = knownDefence(g, 'UK', t, 'low');
    expect(lo.extra).toBe(0);
    expect(lo.aa).toBe(0);
    const aLo = assessAir(g, going(g, p.picked), 'bomb', t, lo);
    expect(p.fog!.loss[0]).toBe(forecastAir(g, aLo, 'bomb', t).bomberLoss);
    // The high end: the heaviest guns, and the most fighters last seen in reach of a place near it, all rising.
    const hi = knownDefence(g, 'UK', t, 'high');
    expect(hi.aa).toBe(F.gunsUnknown);
    expect(k.near).toBeGreaterThan(0);
    const sure = fightersInReach(g, t, 'UK').filter(hi.sure).reduce((s, f) => s + f.ftr, 0);
    expect(hi.extra + sure).toBe(Math.max(k.near, sure));
    expect(p.fog!.loss[1]).toBe(forecastAir(g, assessAir(g, going(g, p.picked), 'bomb', t, hi), 'bomb', t).bomberLoss);
    expect(p.fog!.loss[1]).toBeGreaterThan(p.fog!.loss[0]);
    // The card's figures are the high end's: the defence the side must allow for.
    expect(p.bomberLoss).toBe(p.fog!.loss[1]);
    // The list of targets and the pages say the same.
    expect(raidTargetsFor(g, 'bomb').find((x) => x.id === t)!.what).toMatch(/built guns not known and 60 light, fighters not known/);
    const page = JSON.stringify(airPage(g, `plan:bomb:${t}:${p.picked.join('.')}`));
    expect(page).toContain('Fighters: not known');
    expect(page).toContain('(not scouted)');
    // And the 3D card, through the bridge, in the fields it already draws.
    TC().load(saveGame(g));
    const card = (TC().page(`plan:bomb:${t}:${p.picked.join('.')}`) as { card: { what: string; ticket: string; fog: { fighters: string }; ways: { foes: string; gunsFigure: string; day: { lost: string } } } }).card;
    expect(card.what).toMatch(/guns not known/);
    expect(card.ways.foes).toBe('Unknown');
    expect(card.ways.gunsFigure).toBe('Unknown');
    expect(card.ways.day.lost).toMatch(/^\d+ to \d+% · about /);
    expect(card.ticket).toContain('Losses forecast:');
    expect(card.fog.fighters).toBe('Fighters: not known');
  });

  it('a place seen before and out of sight now: the high end counts every fighter last seen in reach of it, all rising', () => {
    const g = britain(true);
    const t = id(g, 'Ruhr');
    expect(orderScout(g, grp(g, 'No. 1 Group').id, t).ok).toBe(true);
    const seen = skyKnown(g, 'UK', t);
    g.turn += 2;
    const k = skyKnown(g, 'UK', t);
    expect(k.live).toBe(false);
    expect(k.fighters).toBe(seen.fighters);
    expect(k.fighters).toBeGreaterThan(0);
    const hi = knownDefence(g, 'UK', t, 'high');
    const sure = fightersInReach(g, t, 'UK').filter(hi.sure).reduce((s, f) => s + f.ftr, 0);
    expect(hi.extra).toBe(Math.max(0, k.fighters - sure));
    expect(hi.aa).toBe(k.guns);
    const p = previewRaid(g, 'bomb', t);
    expect(p.fog!.fighters).toBe(`Fighters: about ${k.fighters.toLocaleString('en-US')} in reach, seen two months ago`);
    expect(p.fog!.loss[1]).toBe(forecastAir(g, assessAir(g, going(g, p.picked), 'bomb', t, hi), 'bomb', t).bomberLoss);
    expect(p.fog!.loss[1]).toBeGreaterThan(p.fog!.loss[0]);
  });

  it('a reconnaissance gives one figure, the truth, for the month flown and the next, and a band again in the third', () => {
    const g = britain(true);
    const t = id(g, 'Ruhr');
    expect(previewRaid(g, 'bomb', t).fog!.live).toBe(false);
    expect(orderScout(g, grp(g, 'No. 1 Group').id, t).ok).toBe(true);
    const month = g.turn;
    for (const turn of [month, month + 1]) {
      g.turn = turn;
      const S = sightOf(g, 'UK');
      expect(S[t]).toBe(SIGHT.full);
      // The ground beside it is seen too.
      for (const j of g.regions[t].nb) expect(S[j]).toBe(SIGHT.full);
      const p = previewRaid(g, 'bomb', t);
      expect(p.fog!.live && p.fog!.scouted).toBe(true);
      expect(p.fog!.band).toMatch(/^Losses forecast: \d+% \(scouted\)$/);
      expect(p.fog!.fighters).toMatch(/, scouted$/);
      expect(p.bomberLoss).toBe(truth(g, p.picked, t).bomberLoss);
      expect(p.fog!.loss[0]).toBe(p.fog!.loss[1]);
    }
    g.turn = month + 2;
    const p = previewRaid(g, 'bomb', t);
    expect(p.fog!.live).toBe(false);
    expect(p.fog!.band).toMatch(/^Losses forecast: \d+ to \d+% \(not scouted\)$/);
  });

  it('the memory ages: seen so many months ago, then forgotten', () => {
    const g = britain(true);
    const t = id(g, 'Ruhr');
    expect(orderScout(g, grp(g, 'No. 1 Group').id, t).ok).toBe(true);
    const month = g.turn;
    g.turn = month + 3;
    const p = previewRaid(g, 'bomb', t);
    expect(p.fog!.ago).toBe(3);
    expect(p.fog!.fighters).toMatch(/seen three months ago$/);
    expect(p.fog!.guns).toMatch(/^Guns: (none built|level [123]), seen three months ago$/);
    // A staff counts it stale from 'stale' months.
    expect(wantsLook(g, 'UK', t)).toBe(3 >= F.stale);
    g.turn = month + F.forget;
    rememberSkies(g, 'UK');
    expect(g.intel.airSeen!.UK[t]).toBeUndefined();
    expect(previewRaid(g, 'bomb', t).fog!.fighters).toBe('Fighters: not known');
  });

  it('a raid can be ordered on a city never seen, and it is fought and booked on what is really there', () => {
    const g = britain(true);
    const t = id(g, 'Rhineland');
    expect(skyKnown(g, 'UK', t).ago).toBe(-1);
    const p = previewRaid(g, 'bomb', t);
    expect(p.why).toBeNull();
    // The same month with the switch off: the raid flown is the same raid, to the aircraft.
    const twin = structuredClone(g);
    setRaidFog(twin, false);
    const a = orderRaid(g, 'bomb', t, p.picked);
    const b = orderRaid(twin, 'bomb', t, p.picked);
    expect(a).not.toBeNull();
    expect(JSON.stringify(a)).toBe(JSON.stringify(b));
    expect(JSON.stringify(g.airFleets)).toBe(JSON.stringify(twin.airFleets));
  });

  it('on the same band a careful staff declines a raid that a bold staff flies', () => {
    const g = britain(true);
    const R = AIRWAR.ai.raids;
    // The staff's own test of a raid (air.ts raidStaff): the bombers' losses within what it bears, and a fair chance.
    const flies = (fc: ReturnType<typeof forecastAir>): boolean => fc.bomberLoss <= R.loss && fc.success >= R.success;
    let found = 0;
    for (const x of raidTargetsFor(g, 'bomb')) {
      const p = previewRaid(g, 'bomb', x.id);
      if (p.why || !p.fog || p.fog.live) continue;
      const w = weighKnown(g, 'UK', going(g, p.picked), 'bomb', x.id);
      if (!w.lo.fc || !w.hi.fc) continue;
      const careful = bandAt(w.lo.fc, w.hi.fc, F.reads.cautious), bold = bandAt(w.lo.fc, w.hi.fc, F.reads.bold);
      if (!(flies(bold) && !flies(careful))) continue;
      found++;
      // Each staff reads its own point of the band, by its character, and the British staff is a careful one in this war.
      expect(characterOf(g, 'UK')).toBe('cautious');
      expect(staffReads(g, 'UK')).toBe(F.reads.cautious);
      expect(staffWeigh(g, 'UK', going(g, p.picked), 'bomb', x.id).fc).toEqual(careful);
    }
    expect(found).toBeGreaterThan(0);
    // The careful staff reads the high end, the bold the middle (the ruling).
    expect(F.reads.cautious).toBe(1);
    expect(F.reads.bold).toBe(0.5);
  });

  it('a staff photographs a target it has never seen before it weighs it, on its own table, and every nation alike', () => {
    const g = startOpen(createGame(7, 'UK'));
    setWar(g, 'UK', 'GER');
    setRaidFog(g, true);
    const before = sightOf(g, 'GER');
    airAI(g, 'GER');
    const by = g.intel.scoutBy?.GER ?? {};
    const scouted = Object.keys(by).map(Number);
    expect(scouted.length).toBeGreaterThan(0);
    expect(scouted.length).toBeLessThanOrEqual(F.scouts);
    const after = sightOf(g, 'GER');
    for (const t of scouted) {
      expect(by[t]).toBe(g.turn);
      expect(before[t]).not.toBe(SIGHT.full);
      expect(after[t]).toBe(SIGHT.full);
      // What it saw there is remembered.
      expect(g.intel.airSeen!.GER[t][0]).toBe(g.turn);
    }
    // The player's own photographs are his: the staff's flights are not in them.
    expect(Object.keys(g.intel.scout ?? {}).filter((t) => scouted.includes(+t))).toEqual([]);
  });

  it('out of sight the battle ticket says what is reckoned, not what is there: no guns stated as fact, no "no fighters" while unseen fighters are counted', () => {
    const g = britain(true);
    const t = id(g, 'Rhineland');
    const p = previewRaid(g, 'bomb', t);
    expect(p.fog!.guns).toBe('Guns: not known');
    expect(p.fog!.squadrons[1]).toBeGreaterThan(0);
    const line = (label: string) => p.lines.find((l) => l.label === label);
    // The guns are the high end's reckoning: the ticket may not say they ring the factories.
    for (const l of p.lines) expect(l.value).not.toMatch(/^Level \d guns ring /);
    expect(line('Anti-aircraft guns')!.value).toMatch(/^Not scouted, so reckoned at the worst: level \d guns round the factories of /);
    expect(line('Flak')!.value).toMatch(/^Not scouted, so reckoned at the worst: about [\d,]+ guns built round /);
    // The forecast counts the fighters last seen near it: the ticket may not say none are in range.
    expect(line('In range to intercept')!.value).not.toBe('No enemy fighters');
    expect(line('Forces')!.value).not.toMatch(/against no fighters$/);
    expect(line('Forces')!.value).toMatch(/last seen near it/);
    // With the switch off the ticket is the old one, word for word.
    const h = britain(false);
    const q = previewRaid(h, 'bomb', t);
    expect(q.lines).toEqual(assessAir(h, going(h, q.picked), 'bomb', t).lines);
    expect(q.lines.some((l) => /may ring|may stand|last seen near it|known for sure/.test(`${l.value} ${l.effect ?? ''}`))).toBe(false);
  });

  it('out of sight the plan page heads with the chance of success as a band, the same band its note gives', () => {
    const g = britain(true);
    const t = id(g, 'Rhineland');
    const p = previewRaid(g, 'bomb', t);
    expect(p.fog!.live).toBe(false);
    const page = airPage(g, `plan:bomb:${t}:${p.picked.join('.')}`)!;
    const lo = Math.round(p.fog!.success[0] * 100), hi = Math.round(p.fog!.success[1] * 100);
    expect(lo).not.toBe(hi);
    expect(page.note).toContain(`Chance of success ${lo} to ${hi}%`);
    expect(page.headline).toContain(`${lo} to ${hi}%`);
    // Scouted, it is one figure again, the verdict's.
    expect(orderScout(g, grp(g, 'No. 1 Group').id, t).ok).toBe(true);
    const q = previewRaid(g, 'bomb', t);
    expect(q.fog!.live).toBe(true);
    expect(airPage(g, `plan:bomb:${t}:${q.picked.join('.')}`)!.headline).toBe(`${q.verdict}: ${Math.round(q.success * 100)}%`);
  });

  it('a saved war keeps what each side has seen and photographed, and its card reads the same', () => {
    const g = britain(true);
    const t = id(g, 'Ruhr');
    expect(orderScout(g, grp(g, 'No. 1 Group').id, t).ok).toBe(true);
    g.turn += 2;
    const p = previewRaid(g, 'bomb', t);
    const h = loadGame(saveGame(g))!;
    expect(h.flags.rfog).toBe(1);
    expect(h.intel.airSeen).toEqual(g.intel.airSeen);
    expect(h.intel.scoutBy).toEqual(g.intel.scoutBy);
    expect(previewRaid(h, 'bomb', t).fog).toEqual(p.fog);
    const j = JSON.parse(JSON.stringify(g)) as Game;
    expect(previewRaid(j, 'bomb', t).fog).toEqual(p.fog);
  });
});

// The UI director's answers of 25 Nov 2026 22:29Z to the gun count's third pass: (1) the town raid's guns line names the
// town, in all three forms, in sight too, as the card's town line does; (2) the build offer's guns note is blank exactly
// where the raid card would not show the truth (out of the side's sight under raid fog, skyKnown's `live`), and tells the
// level on an ally's ground, in sight, or with no fog set; the why "That ground is not in your hands." stays either way.
describe('fog on raids: the town\'s guns and the offer\'s note', () => {
  /** Germany at war with Britain and France in September 1939, at the opening of the player's month. */
  function germany(on: boolean): Game {
    const g = startOpen(createGame(7, 'GER'));
    setWar(g, 'GER', 'UK');
    setWar(g, 'GER', 'FRA');
    if (on) setRaidFog(g, true);
    for (let k = 0; !humanUp(g) && k < 200; k++) stepAI(g);
    beginHuman(g);
    return g;
  }
  const guns = (g: Game, t: number, level: number): void => { if (level > 0) g.flags[`wk:aa:${t}`] = level; else delete g.flags[`wk:aa:${t}`]; };

  it('the town raid\'s Anti-aircraft guns line names the town, as its town line does: in sight, seen and never scouted', () => {
    const g = germany(true);
    const t = id(g, 'Alsace');
    guns(g, t, 2);
    const p = previewRaid(g, 'town', t);
    expect(p.why).toBeNull();
    expect(p.fog!.live).toBe(true);
    const line = (lines: { label: string; value: string }[], label: string) => lines.find((l) => l.label === label)!.value;
    expect(line(p.lines, 'The town')).toMatch(/^Troops inside Strasbourg fight /);
    expect(line(p.lines, 'Anti-aircraft guns')).toBe('Level 2 guns ring Strasbourg');
    // Weighed as the card weighs a place out of sight: never scouted, and seen seven months ago.
    const fleets = going(g, p.picked).filter((f) => f.nat === 'GER');
    const never = assessAir(g, fleets, 'town', t, { ...knownDefence(g, 'GER', t, 'high'), seen: -1, ago: 0, aa: 3 });
    expect(line(never.lines, 'Anti-aircraft guns')).toBe('Not scouted, so reckoned at the worst: level 3 guns round Strasbourg');
    const seen = assessAir(g, fleets, 'town', t, { ...knownDefence(g, 'GER', t, 'high'), seen: 2, ago: 7, aa: 2 });
    expect(line(seen.lines, 'Anti-aircraft guns')).toBe('Level 2 guns ringed Strasbourg when seen seven months ago');
    // The bridge's ticket carries the same line.
    TC().load(saveGame(g));
    const card = (TC().page(`plan:town:${t}:${p.picked.join('.')}`) as { card: { ticket: string } }).card;
    expect(card.ticket).toContain('Anti-aircraft guns: Level 2 guns ring Strasbourg (They stop 30% of the bombing and bring down more of the bombers)');
  });

  it('the offer\'s guns note is blank exactly where the raid card would not show the truth, and tells the level elsewhere', () => {
    const note = (g: Game, t: number) => offers(g, 'UK', t).find((x) => x.key === 'work:aa')!;
    const bridge = (g: Game, t: number): string => {
      TC().load(saveGame(g));
      return (TC().arsenal(t) as { offers: { key: string; note: string }[] }).offers.find((x) => x.key === 'work:aa')!.note;
    };
    for (const setting of ['on', 'off', 'on, no fog'] as const) {
      const g = britain(setting !== 'off');
      if (setting === 'on, no fog') expect(setFog(g, false)).toBe(true);
      const sight = sightOf(g, 'UK');
      const kent = id(g, 'Kent'), paris = id(g, 'Paris'), rh = id(g, 'Rhineland');
      const enemyIn = g.regions.find((r) => r.ctrl === 'GER' && sight[r.id] === SIGHT.full && skyKnown(g, 'UK', r.id).live)!;
      const neutral = g.regions.find((r) => r.ctrl === 'NED')!.id;
      expect(allied(g, 'UK', 'FRA')).toBe(true);
      expect(enemyIn).toBeDefined();
      // Seen two months ago at level 2: the card shows the memory, not the truth.
      (g.intel as unknown as { airSeen: Record<string, Record<number, number[]>> }).airSeen = { UK: { [rh]: [g.turn - 2, 120, 2, 2, 0] } };
      for (const [t, name] of [[kent, 'Kent'], [paris, 'Paris'], [enemyIn.id, enemyIn.name], [rh, 'Rhineland'], [id(g, 'Saxony'), 'Saxony'], [neutral, g.regions[neutral].name]] as const) {
        guns(g, t, 2);
        const hidden = setting === 'on' && t !== kent && t !== paris && t !== enemyIn.id;
        expect(skyKnown(g, 'UK', t).live, `${setting} ${name}`).toBe(!hidden);
        const o = note(g, t);
        expect(o.note, `${setting} ${name}`).toBe(hidden ? '' : `${name} has level 2.`);
        expect(o.why, `${setting} ${name}`).toBe(t === kent ? null : 'That ground is not in your hands.');
        expect(bridge(g, t), `${setting} ${name}`).toBe(o.note);
        guns(g, t, 0);
        expect(note(g, t).note, `${setting} ${name}`).toBe(hidden ? '' : `${name} has no guns built: only the 60 light guns every province has.`);
      }
    }
  });
});

// The UI director's answer of 26 Nov 2026 02:06Z, 1: under fog the Flak line names the place the guns line beside it
// names, so one card never reads as two sets of guns: a town raid round {Leipzig}, a harbour raid round {the harbour of
// Bremen}, the factories as today (round {Rhineland}). The same month or "Not scouted" hedge as before.
describe('fog on raids: the Flak line names the place its guns line names', () => {
  type Lines = { label: string; value: string }[];
  const line = (lines: Lines, label: string): string => lines.find((l) => l.label === label)!.value;
  const guns = (g: Game, t: number, level: number): void => { if (level > 0) g.flags[`wk:aa:${t}`] = level; else delete g.flags[`wk:aa:${t}`]; };
  /** The two fog forms, weighed as the card weighs a place out of sight: never scouted, and seen two months ago at level 2. */
  const fogForms = (g: Game, nat: string, fleets: AirFleet[], m: 'town' | 'harbour' | 'bomb', t: number): [Lines, Lines] => [
    assessAir(g, fleets, m, t, { ...knownDefence(g, nat, t, 'high'), seen: -1, ago: 0, aa: 3 }).lines,
    assessAir(g, fleets, m, t, { ...knownDefence(g, nat, t, 'high'), seen: 2, ago: 2, aa: 2 }).lines,
  ];

  it('a town card names its town on the Flak line and the guns line, the name its town line prints', () => {
    const g = britain(true);
    const t = id(g, 'Saxony');
    guns(g, t, 2);
    const p = previewRaid(g, 'town', t);
    expect(p.why).toBeNull();
    expect(p.fog!.live).toBe(false);
    const town = line(p.lines, 'The town').match(/^Troops inside (.+?) fight /)![1];
    expect(town).toBe('Leipzig');
    // The card as the player sees it out of sight: never scouted.
    expect(line(p.lines, 'Flak')).toBe('Not scouted, so reckoned at the worst: about 1,080 guns built round Leipzig, 720 light and 360 heavy, and the 60 light guns every target has.');
    expect(line(p.lines, 'Anti-aircraft guns')).toBe('Not scouted, so reckoned at the worst: level 3 guns round Leipzig');
    const [never, seen] = fogForms(g, 'UK', going(g, p.picked), 'town', t);
    expect(line(never, 'Flak')).toBe('Not scouted, so reckoned at the worst: about 1,080 guns built round Leipzig, 720 light and 360 heavy, and the 60 light guns every target has.');
    expect(line(never, 'Anti-aircraft guns')).toBe('Not scouted, so reckoned at the worst: level 3 guns round Leipzig');
    expect(line(seen, 'Flak')).toBe('Seen two months ago: about 720 guns built round Leipzig, 480 light and 240 heavy, and the 60 light guns every target has.');
    expect(line(seen, 'Anti-aircraft guns')).toBe('Level 2 guns ringed Leipzig when seen two months ago');
    for (const lines of [never, seen]) expect(line(lines, 'The town')).toMatch(/^Troops inside Leipzig fight /);
    // In sight the Flak line names no place, as before.
    expect(line(assessAir(g, going(g, p.picked), 'town', t).lines, 'Flak')).toBe('About 720 guns built here, 480 light and 240 heavy, and the 60 light guns every target has.');
  });

  it('a harbour card names "the harbour of Bremen" on the Flak line and the guns line', () => {
    const g = startOpen(createGame(7, 'UK'));
    setWar(g, 'UK', 'GER');
    setWar(g, 'UK', 'ITA');
    setRaidFog(g, true);
    const t = id(g, 'Bremen');
    guns(g, t, 2);
    const p = previewRaid(g, 'harbour', t);
    expect(p.why).toBeNull();
    expect(p.fog!.live).toBe(false);
    expect(line(p.lines, 'Flak')).toBe('Not scouted, so reckoned at the worst: about 1,080 guns built round the harbour of Bremen, 720 light and 360 heavy, and the 60 light guns every target has.');
    expect(line(p.lines, 'Anti-aircraft guns')).toBe('Not scouted, so reckoned at the worst: level 3 guns round the harbour of Bremen');
    const [never, seen] = fogForms(g, 'UK', going(g, p.picked), 'harbour', t);
    expect(line(never, 'Flak')).toBe('Not scouted, so reckoned at the worst: about 1,080 guns built round the harbour of Bremen, 720 light and 360 heavy, and the 60 light guns every target has.');
    expect(line(never, 'Anti-aircraft guns')).toBe('Not scouted, so reckoned at the worst: level 3 guns round the harbour of Bremen');
    expect(line(seen, 'Flak')).toBe('Seen two months ago: about 720 guns built round the harbour of Bremen, 480 light and 240 heavy, and the 60 light guns every target has.');
    expect(line(seen, 'Anti-aircraft guns')).toBe('Level 2 guns ringed the harbour of Bremen when seen two months ago');
  });

  it('a factory card\'s Flak line names the province, as today', () => {
    const g = britain(true);
    const t = id(g, 'Rhineland');
    guns(g, t, 2);
    const p = previewRaid(g, 'bomb', t);
    expect(p.why).toBeNull();
    expect(p.fog!.live).toBe(false);
    expect(line(p.lines, 'Flak')).toBe('Not scouted, so reckoned at the worst: about 1,080 guns built round the Rhineland, 720 light and 360 heavy, and the 60 light guns every target has.');
    const [never, seen] = fogForms(g, 'UK', going(g, p.picked), 'bomb', t);
    expect(line(never, 'Flak')).toBe('Not scouted, so reckoned at the worst: about 1,080 guns built round the Rhineland, 720 light and 360 heavy, and the 60 light guns every target has.');
    expect(line(never, 'Anti-aircraft guns')).toBe('Not scouted, so reckoned at the worst: level 3 guns round the factories of Rhineland');
    expect(line(seen, 'Flak')).toBe('Seen two months ago: about 720 guns built round the Rhineland, 480 light and 240 heavy, and the 60 light guns every target has.');
    expect(line(seen, 'Anti-aircraft guns')).toBe('Level 2 guns ringed the factories of Rhineland when seen two months ago');
  });
});

// The UI director's rule of 26 Nov 2026 04:32Z: the raid card's place names take the news's article wherever they
// print. The Flak line is new in release 2, so always, with no guard ("about 720 guns built round the Ruhr"); the
// harbour's guns line is shipped, so under city defences' switch, off as shipped. A place without one is unchanged.
describe('the raid card\'s place names take the news\'s article (the UI director, 26 Nov 2026 04:32Z)', () => {
  type Lines = { label: string; value: string }[];
  const flak = (lines: Lines): string => lines.find((l) => l.label === 'Flak')!.value;
  const guns = (lines: Lines): string[] => lines.filter((l) => l.label === 'Anti-aircraft guns').map((l) => l.value);
  /** A war of seed 7 a year in, city defences off or on, level 2 guns at the target, 300 bombers, a fleet in harbour. */
  const card = (place: string, m: 'bomb' | 'town' | 'harbour', att: string, def: string, cd: boolean) => {
    const g = startOpen(createGame(7, null));
    g.turn = 12;
    if (cd) g.flags[START_LAID] = 1;
    setCityDefences(g, cd);
    setWar(g, att, def);
    const f = g.airFleets.find((x) => x.nat === att && x.bmr > 0 && x.ship === undefined)!;
    f.bmr = 300;
    const t = g.byName[place].id;
    if (place === 'Crimea') f.base = g.byName[place].nb[0];
    if (m === 'harbour') g.fleets.find((x) => x.nat === def && !atSea(x))!.base = t;
    g.flags[`wk:aa:${t}`] = 2;
    const at = (k?: Parameters<typeof assessAir>[4]): Lines => { const a = assessAir(g, [f], m, t, k); expect(a.why).toBeNull(); return a.lines; };
    return {
      sight: at(),
      never: at({ ...knownDefence(g, att, t, 'high'), seen: -1, ago: 0, aa: 3 }),
      seen: at({ ...knownDefence(g, att, t, 'high'), seen: 2, ago: 2, aa: 2 }),
    };
  };

  it('the Flak line names the factories\' place with its article under fog, with no guard, switch off or on', () => {
    for (const cd of [false, true]) {
      const c = card('Ruhr', 'bomb', 'UK', 'GER', cd);
      expect(flak(c.never)).toBe('Not scouted, so reckoned at the worst: about 1,080 guns built round the Ruhr, 720 light and 360 heavy, and the 60 light guns every target has.');
      expect(flak(c.seen)).toBe('Seen two months ago: about 720 guns built round the Ruhr, 480 light and 240 heavy, and the 60 light guns every target has.');
      // In sight the line names no place, as before.
      expect(flak(c.sight)).toBe('About 720 guns built here, 480 light and 240 heavy, and the 60 light guns every target has.');
      // A town raid names its town as it is.
      const town = card('Ruhr', 'town', 'UK', 'GER', cd);
      expect(flak(town.seen)).toBe('Seen two months ago: about 720 guns built round Essen, 480 light and 240 heavy, and the 60 light guns every target has.');
    }
  });

  it('the Flak line names the harbour\'s place with its article under fog, with no guard, switch off or on', () => {
    for (const cd of [false, true]) {
      const c = card('Cotentin', 'harbour', 'UK', 'GER', cd);
      expect(flak(c.never)).toBe('Not scouted, so reckoned at the worst: about 1,080 guns built round the harbour of the Cotentin, 720 light and 360 heavy, and the 60 light guns every target has.');
      expect(flak(c.seen)).toBe('Seen two months ago: about 720 guns built round the harbour of the Cotentin, 480 light and 240 heavy, and the 60 light guns every target has.');
      // A place without an article is unchanged.
      const b = card('Bremen', 'harbour', 'UK', 'GER', cd);
      expect(flak(b.seen)).toBe('Seen two months ago: about 720 guns built round the harbour of Bremen, 480 light and 240 heavy, and the 60 light guns every target has.');
    }
  });

  it('a harbour card\'s own guns line, found by its words, takes the article only with city defences on', () => {
    for (const [place, att, def] of [['Cotentin', 'UK', 'GER'], ['Crimea', 'GER', 'SOV']] as const) {
      const off = card(place, 'harbour', att, def, false), on = card(place, 'harbour', att, def, true);
      // With city defences off the card has two guns lines (the harbour's and the factories'); on, the harbour's alone: each is read by its own words.
      for (const [c, n] of [[off, 2], [on, 1]] as const) expect(guns(c.sight)).toHaveLength(n);
      const harbour = (lines: Lines): string => guns(lines).find((v) => v.includes('the harbour of'))!;
      expect(harbour(off.sight)).toBe(`Level 2 guns ring the harbour of ${place}`);
      expect(harbour(off.never)).toBe(`Not scouted, so reckoned at the worst: level 3 guns round the harbour of ${place}`);
      expect(harbour(off.seen)).toBe(`Level 2 guns ringed the harbour of ${place} when seen two months ago`);
      expect(harbour(on.sight)).toBe(`Level 2 guns ring the harbour of the ${place}`);
      expect(harbour(on.never)).toBe(`Not scouted, so reckoned at the worst: level 3 guns round the harbour of the ${place}`);
      expect(harbour(on.seen)).toBe(`Level 2 guns ringed the harbour of the ${place} when seen two months ago`);
    }
    // A place without an article is unchanged with the switch on.
    expect(guns(card('Bremen', 'harbour', 'UK', 'GER', true).sight).find((v) => v.includes('the harbour of'))).toBe('Level 2 guns ring the harbour of Bremen');
  });
});

// The UI director, 26 Nov 2026 05:17Z, round eight, 6: "Not on a harbour card. Both lines read out the same guns: out.aa is
// the province's one works level (gunsAt). And the factories' effect, "They stop {N}% of the bombing", is false on a
// harbour raid ... The harbour card keeps the harbour's line alone. Take the factories' line out on harbour raids under
// the same switch as the harbour line's article (city defences); off, as shipped. No result changes."
describe('a harbour card keeps the harbour\'s guns line alone under city defences (the UI director, 26 Nov 2026 05:17Z, round eight, 6)', () => {
  type Lines = { label: string; value: string; effect?: string }[];
  const guns = (lines: Lines): Lines => lines.filter((l) => l.label === 'Anti-aircraft guns');
  const card = (place: string, m: 'bomb' | 'town' | 'harbour', att: string, def: string, cd: boolean) => {
    const g = startOpen(createGame(7, null));
    g.turn = 12;
    if (cd) g.flags[START_LAID] = 1;
    setCityDefences(g, cd);
    setWar(g, att, def);
    const f = g.airFleets.find((x) => x.nat === att && x.bmr > 0 && x.ship === undefined)!;
    f.bmr = 300;
    const t = g.byName[place].id;
    if (place === 'Crimea') f.base = g.byName[place].nb[0];
    if (m === 'harbour') g.fleets.find((x) => x.nat === def && !atSea(x))!.base = t;
    g.flags[`wk:aa:${t}`] = 2;
    const at = (k?: Parameters<typeof assessAir>[4]) => { const a = assessAir(g, [f], m, t, k); expect(a.why).toBeNull(); return a; };
    return { sight: at(), never: at({ ...knownDefence(g, att, t, 'high'), seen: -1, ago: 0, aa: 3 }), seen: at({ ...knownDefence(g, att, t, 'high'), seen: 2, ago: 2, aa: 2 }) };
  };

  it('city defences on: one guns line on the harbour card, the harbour\'s, with its article, in sight, never scouted and seen', () => {
    for (const [place, att, def] of [['Cotentin', 'UK', 'GER'], ['Crimea', 'GER', 'SOV']] as const) {
      const on = card(place, 'harbour', att, def, true);
      expect(guns(on.sight.lines).map((l) => l.value)).toEqual([`Level 2 guns ring the harbour of the ${place}`]);
      expect(guns(on.never.lines).map((l) => l.value)).toEqual([`Not scouted, so reckoned at the worst: level 3 guns round the harbour of the ${place}`]);
      expect(guns(on.seen.lines).map((l) => l.value)).toEqual([`Level 2 guns ringed the harbour of the ${place} when seen two months ago`]);
      expect(guns(on.sight.lines)[0].effect).toBe('They bring down more of the bombers');
      for (const c of [on.sight, on.never, on.seen]) expect(c.lines.some((l) => l.value.includes('the factories of'))).toBe(false);
    }
    expect(guns(card('Bremen', 'harbour', 'UK', 'GER', true).sight.lines).map((l) => l.value)).toEqual(['Level 2 guns ring the harbour of Bremen']);
  });

  it('city defences off: both lines, bare, as shipped', () => {
    const off = card('Cotentin', 'harbour', 'UK', 'GER', false);
    expect(guns(off.sight.lines).map((l) => l.value)).toEqual(['Level 2 guns ring the harbour of Cotentin', 'Level 2 guns ring the factories of Cotentin']);
    expect(guns(off.sight.lines)[1].effect).toMatch(/^They stop \d+% of the bombing and bring down more of the bombers$/);
    expect(guns(off.seen.lines).map((l) => l.value)).toEqual(['Level 2 guns ringed the harbour of Cotentin when seen two months ago', 'Level 2 guns ringed the factories of Cotentin when seen two months ago']);
  });

  it('only the harbour card: the factories\' card and the town\'s keep their one guns line, with city defences on', () => {
    expect(guns(card('Cotentin', 'bomb', 'UK', 'GER', true).sight.lines)).toHaveLength(1);
    expect(guns(card('Cotentin', 'bomb', 'UK', 'GER', true).sight.lines)[0].value.includes('the factories of')).toBe(true);
    expect(guns(card('Ruhr', 'town', 'UK', 'GER', true).sight.lines).map((l) => l.value)).toEqual(['Level 2 guns ring Essen']);
  });
});
