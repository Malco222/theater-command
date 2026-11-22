// Brian's three air ideas of 22 November 2026, each behind a switch that is off as shipped (the design director's
// rulings R1 to R6, 22 November 17:44Z and 17:45Z): named aces (aces.ts), recon photos after a raid (photos.ts,
// photoprint.ts) and fog over the airfields (fog.ts).
import { afterEach, describe, expect, it } from 'vitest';
import { airFleetsOf, whyNotMission } from '../src/campaign/air';
import { ACES, aceEdge, aceGrief, aceLine, aceSeats, aceTitle, acesFought, acesOn, setAces } from '../src/campaign/aces';
import { groupRow } from '../src/campaign/airwar';
import { createGame } from '../src/campaign/game';
import { FOG, fogAt, fogBound, fogMonth, fogOdds, fogShipped, setFog } from '../src/campaign/fog';
import { PHOTOS, bombsIn, photoCaption, photosOn, raidPhoto, setPhotos } from '../src/campaign/photos';
import { photoPrint } from '../src/campaign/photoprint';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { storyEdge } from '../src/campaign/stories';
import { orderRaid, previewRaid, raidTargetsFor } from '../src/campaign/turn';
import { turnToDate } from '../src/core/ticket';
import type { AirFleet, AirSide, Game } from '../src/campaign/types';

const grand = (human: string | null = 'UK', seed = 7): Game => startOpen(createGame(seed, human));
const at = (g: Game, name: string): number => g.regions.find((r) => r.name === name)!.id;
const side = (f: AirFleet, ftr: number, lostFtr: number): AirSide => ({ nat: f.nat, fleet: f.name, id: f.id, base: f.base, leader: null, ftr, bmr: 0, lostFtr, lostBmr: 0 });
const fighterOf = (g: Game, nat: string): AirFleet => airFleetsOf(g, nat).find((f) => f.kind === 'ftr' && f.ftr > 0 && f.ship === undefined)!;

describe('the switches', () => {
  it('are off in the data but the photos (on since the art director passed them, 22 November 18:15Z), and fog cannot ship on until its table is sourced', () => {
    expect(ACES.on).toBe(false);
    expect(PHOTOS.on).toBe(true);
    expect(FOG.on).toBe(false);
    expect(FOG.sourced).toBe(false);
    expect(fogShipped()).toBe(false);
    const g = grand();
    expect(acesOn(g)).toBe(false);
    expect(photosOn(g)).toBe(false);
    expect(g.flags.aces ?? g.flags.photos ?? g.flags.fog).toBeUndefined();
  });

  it('do nothing while off: no ace, no photo, no fog, and no edge', () => {
    const g = grand();
    const f = fighterOf(g, 'UK');
    f.rec = { kills: 500 };
    acesFought(g, [side(f, 100, 100)], [f], [], [], at(g, 'Kent'), 'Kent');
    expect(f.aces).toBeUndefined();
    expect(aceEdge(g, f)).toBe(0);
    expect(aceGrief(g, f)).toBe(1);
    expect(raidPhoto(g, { aim: 'works', fac: 3, bombed: 100, through: 50, cut: 0.1, wrecked: false, rail: false, city: 0, night: false, sky: 'clear' })).toBeNull();
    for (const r of g.regions) expect(fogAt(g, r.id)).toBe(false);
  });

  it('are kept with a saved campaign', () => {
    const g = grand();
    setAces(g, true); setPhotos(g, true); setFog(g, true);
    const back = loadGame(saveGame(g))!;
    expect([back.flags.aces, back.flags.photos, back.flags.fog]).toEqual([1, 1, 1]);
  });
});

describe('named aces (R1, R2)', () => {
  it('names a group\'s ace once its leading pilot\'s share of what it has downed reaches five, an invented man of his nation', () => {
    const g = grand('UK');
    setAces(g, true);
    const f = fighterOf(g, 'UK');
    f.rec = { kills: 90, saved: 20 };
    const h0 = g.headlines.length;
    acesFought(g, [side(f, 100, 0)], [f], [], [], at(g, 'Kent'), 'Kent');
    expect(ACES.share).toBe(0.042);
    expect(f.aces?.now).toBeUndefined(); // 0.042 x 110 = 4.6: not yet five
    f.rec.kills = 100;
    acesFought(g, [side(f, 100, 0)], [f], [], [], at(g, 'Kent'), 'Kent');
    const a = f.aces!.now!;
    expect(a.kills).toBe(Math.round(ACES.share * 120));
    expect(a.since).toBe(g.turn);
    const [first, last] = [a.name.split(' ')[0], a.name.split(' ').slice(1).join(' ')];
    expect(ACES.names.uk.first).toContain(first);
    expect(ACES.names.uk.last).toContain(last);
    expect(ACES.guard).not.toContain(a.name);
    expect(aceTitle('UK', a)).toMatch(/^(Pilot Officer|Flying Officer|Flight Lieutenant|Squadron Leader|Wing Commander) /);
    expect(g.headlines.slice(h0).some((h) => h.text.includes(a.name) && /has an ace/.test(h.text))).toBe(true);
    expect(g.stories.mail.some((m) => m.m === 'ace' && m.x === aceTitle('UK', a))).toBe(true);
    expect(g.stories.aces?.[0].name).toBe(aceTitle('UK', a));
    expect(aceLine(g, f)).toBe(`Its ace: ${aceTitle('UK', a)}, ${a.kills} victories.`);
    expect(groupRow(g, f, 'UK').ace).toBe(aceLine(g, f));
    expect(aceSeats(g, [f])).toEqual({ [f.id]: { name: aceTitle('UK', a), kills: a.kills } });
  });

  it('gives a carrier group\'s ace his navy\'s ranks where his nation has them, and an air force\'s ace its own', () => {
    const base = { name: 'Tom Harland', rank: 0, kills: 6, from: 0, since: 0 };
    expect(aceTitle('USA', { ...base, navy: true })).toBe('Ensign Tom Harland');
    expect(aceTitle('USA', { ...base, rank: 1, navy: true })).toBe('Lieutenant (junior grade) Tom Harland');
    expect(aceTitle('USA', base)).toBe('Lieutenant Tom Harland');
    expect(aceTitle('UK', { ...base, navy: true })).toBe('Sub-Lieutenant Tom Harland');
    // A nation with one list keeps it at sea: Japan's is its navy's already.
    expect(aceTitle('JAP', { ...base, navy: true })).toBe(aceTitle('JAP', base));
    const g = grand('UK');
    setAces(g, true);
    const f = fighterOf(g, 'UK');
    f.ship = f.ship ?? 0;
    f.rec = { kills: 100, saved: 20 };
    acesFought(g, [side(f, 100, 0)], [f], [], [], at(g, 'Kent'), 'Kent');
    expect(f.aces!.now!.navy).toBe(true);
    expect(aceTitle('UK', f.aces!.now!)).toMatch(/^(Sub-Lieutenant|Lieutenant|Lieutenant Commander|Commander) /);
  });

  it('draws the same ace for the same campaign, and his tally grows with his group\'s', () => {
    const name = (): string => {
      const g = grand('UK', 11); setAces(g, true);
      const f = fighterOf(g, 'UK'); f.rec = { kills: 150 };
      acesFought(g, [side(f, 100, 0)], [f], [], [], -1, '');
      return f.aces!.now!.name;
    };
    expect(name()).toBe(name());
    const g = grand('UK'); setAces(g, true);
    const f = fighterOf(g, 'UK'); f.rec = { kills: 150 };
    acesFought(g, [side(f, 100, 0)], [f], [], [], -1, '');
    f.rec.kills = 300;
    acesFought(g, [side(f, 100, 0)], [f], [], [], -1, '');
    expect(f.aces!.now!.kills).toBe(Math.round(ACES.share * 300));
  });

  it('keeps his victories by the nation that put up the most aircraft against him, for his marks, and his lead unit carries him into the battle', () => {
    const g = grand('UK'); setAces(g, true);
    const f = fighterOf(g, 'UK'), ger = fighterOf(g, 'GER'), ita = fighterOf(g, 'ITA');
    f.rec = { kills: 150 };
    acesFought(g, [side(f, 100, 0)], [f], [side(ger, 60, 0), side(ita, 20, 0)], [ger, ita], -1, '');
    const a = f.aces!.now!;
    expect(a.by).toEqual({ GER: a.kills });
    f.rec.kills = 300;
    acesFought(g, [side(f, 100, 0)], [f], [side(ita, 50, 0)], [ita], -1, '');
    expect(a.by).toEqual({ GER: Math.round(ACES.share * 150), ITA: a.kills - Math.round(ACES.share * 150) });
    expect(Object.values(a.by!).reduce((x, y) => x + y, 0)).toBe(a.kills);
    expect(aceSeats(g, [f])![f.id]).toEqual({ name: aceTitle('UK', a), kills: a.kills, by: a.by });
  });

  it('may fall with his group\'s own loss share that day; then the next man\'s count begins', () => {
    const g = grand('UK'); setAces(g, true);
    const f = fighterOf(g, 'UK'); f.rec = { kills: 150 };
    acesFought(g, [side(f, 100, 0)], [f], [], [], at(g, 'Kent'), 'Kent');
    const a = f.aces!.now!;
    // No losses: he cannot fall.
    acesFought(g, [side(f, 100, 0)], [f], [], [], at(g, 'Kent'), 'Kent');
    expect(f.aces!.now).toBe(a);
    // The whole group lost: he falls (risk 1, ruling R2).
    const h0 = g.headlines.length;
    acesFought(g, [side(f, 100, 100)], [f], [], [], at(g, 'Kent'), 'Kent');
    expect(f.aces!.now).toBeUndefined();
    expect(f.aces!.fell).toBe(g.turn);
    expect(f.aces!.from).toBe(150);
    expect(g.headlines.slice(h0).some((h) => /has lost its ace: .*, was shot down over Kent\.$/.test(h.text) && h.text.includes(a.name))).toBe(true);
    expect(g.stories.mail.some((m) => m.m === 'acefell')).toBe(true);
    expect(g.stories.aces!.find((x) => x.since === a.since)!.fell).toBe(g.turn);
    // The next man: 5 more at 0.042 is 120 more downed (119 is 4.998).
    f.rec.kills = 150 + 119;
    acesFought(g, [side(f, 100, 0)], [f], [], [], -1, '');
    expect(f.aces!.now).toBeUndefined();
    f.rec.kills = 150 + 120;
    acesFought(g, [side(f, 100, 0)], [f], [], [], -1, '');
    expect(f.aces!.now).toBeDefined();
    expect(f.aces!.n).toBe(2);
  });

  it('names an enemy ace the player\'s fighters bring down, and changes no number for a computer-run nation', () => {
    const g = grand('UK'); setAces(g, true);
    const ger = fighterOf(g, 'GER'), uk = fighterOf(g, 'UK');
    ger.rec = { kills: 200 };
    acesFought(g, [side(ger, 100, 0)], [ger], [side(uk, 100, 0)], [uk], -1, '');
    expect(ger.aces!.now).toBeDefined();
    expect(g.stories.aces ?? []).toEqual([]); // the enemy's ace is not the player's news until he falls to the player's fighters
    expect(aceEdge(g, ger)).toBe(0);
    expect(storyEdge(g, ger)).toBe(1);
    const h0 = g.headlines.length;
    acesFought(g, [side(ger, 100, 100)], [ger], [side(uk, 100, 0)], [uk], at(g, 'Kent'), 'Kent');
    expect(g.headlines.slice(h0).some((h) => /^Over Kent, your fighters brought down the ace of /.test(h.text))).toBe(true);
    expect(g.stories.aces!.length).toBe(1);
  });

  it('adds the player\'s group its edge while he flies, inside the stories\' cap, and grief is held at 1', () => {
    const g = grand('UK'); setAces(g, true);
    const f = fighterOf(g, 'UK');
    const before = storyEdge(g, f);
    f.rec = { kills: 150 };
    acesFought(g, [side(f, 100, 0)], [f], [], [], -1, '');
    expect(aceEdge(g, f)).toBe(ACES.edge);
    expect(storyEdge(g, f)).toBeCloseTo(Math.min(1.12, before + ACES.edge), 10);
    acesFought(g, [side(f, 100, 100)], [f], [], [], -1, '');
    expect(ACES.grief).toBe(1);
    expect(aceGrief(g, f)).toBe(1);
    expect(storyEdge(g, f)).toBe(before);
  });
});

describe('recon photos (R3: shown, never counted)', () => {
  it('turns the count\'s accuracy into the share of bombs inside the fence, and words it as the interpreters did', () => {
    const g = grand('UK'); setPhotos(g, true);
    const p = raidPhoto(g, { aim: 'works', fac: 3, bombed: 200, through: 100, cut: 0.123, wrecked: true, rail: false, city: 1.25, night: false, sky: 'cloud' })!;
    expect(p).toEqual({ aim: 'works', bombed: 200, on: 0.15, cut: 0.123, wrecked: 1, city: 1.3, sky: 'cloud' });
    expect(raidPhoto(g, { aim: 'works', fac: 0, bombed: 10, through: 0, cut: 0, wrecked: false, rail: true, city: 0, night: true, sky: 'clear' })).toEqual({ aim: 'rail', bombed: 10, on: PHOTOS.least, cut: 0, rail: 1, night: 1 });
    expect(bombsIn(0.15)).toBe('about one bomb in seven');
    expect(bombsIn(0.04)).toBe('fewer than one bomb in ten');
    expect(bombsIn(0.5)).toBe('about half the bombs');
    expect(photoCaption(p, 'Schweinfurt')).toEqual([
      'Photographed the day after the raid, through breaks in the cloud.',
      'About one bomb in seven fell inside the works at Schweinfurt; the rest fell in the fields and the village round it.',
      'The works make 12% less for a month.',
      'One factory is wrecked: it makes nothing until engineers rebuild it.',
    ]);
  });

  it('lays the same print for the same raid, with the count\'s share of craters inside the fence', () => {
    const photo = { aim: 'works' as const, bombed: 200, on: 0.15, cut: 0.1 };
    const a = photoPrint('7:12', photo), b = photoPrint('7:12', photo);
    expect(a).toEqual(b);
    expect(a.craters.length).toBe(50);
    expect(a.craters.filter((c) => c[3] === 1).length).toBe(Math.round(50 * 0.15));
    const [fx, fy, fw, fh] = a.fence;
    for (const c of a.craters) expect(c[3] === 1).toBe(c[0] >= fx && c[0] <= fx + fw && c[1] >= fy && c[1] <= fy + fh);
    expect(photoPrint('7:13', photo)).not.toEqual(a);
  });

  it('is kept on the report of the player\'s own raid on works, and on no one else\'s', () => {
    const g = grand('GER'); setPhotos(g, true);
    let rep = null;
    for (const t of raidTargetsFor(g, 'bomb')) {
      const p = previewRaid(g, 'bomb', t.id);
      if (!p.picked.length) continue;
      rep = orderRaid(g, 'bomb', t.id, p.picked);
      if (rep) break;
    }
    expect(rep).not.toBeNull();
    expect(rep!.photo).toBeDefined();
    expect(rep!.photo!.bombed).toBeGreaterThan(0);
    expect(rep!.photo!.on).toBeGreaterThanOrEqual(PHOTOS.least);
    expect(rep!.photo!.on).toBeLessThanOrEqual(PHOTOS.most);
    expect(g.airReports.filter((r) => r.attNat !== 'GER').every((r) => !r.photo)).toBe(true);
  });
});

describe('fog over the airfields (R4, R5, R6)', () => {
  const kept: [string, number[]][] = [];
  afterEach(() => { while (kept.length) { const [k, o] = kept.pop()!; FOG.odds[k] = o; } });
  const force = (key: string, t: number): void => { kept.push([key, FOG.odds[key]]); FOG.odds[key] = Array.from({ length: 12 }, () => t); };

  it('is dealt from the seed by the field\'s zone and month, the same however often it is asked', () => {
    const g = grand('UK'); setFog(g, true);
    const kent = at(g, 'Kent');
    expect(fogOdds(g, kent)).toBe(FOG.odds.nwe[turnToDate(g.turn).month]);
    const first = g.regions.map((r) => fogAt(g, r.id));
    expect(g.regions.map((r) => fogAt(g, r.id))).toEqual(first);
    const again = grand('UK'); setFog(again, true);
    expect(again.regions.map((r) => fogAt(again, r.id))).toEqual(first);
  });

  it('closes the field for every aircraft based there, fighters too, but never a carrier\'s deck', () => {
    force('nwe', 10);
    const g = grand('UK'); setFog(g, true);
    const here = airFleetsOf(g, 'UK').filter((f) => f.ship === undefined && f.ftr + f.bmr > 0 && fogOdds(g, f.base) === 10);
    expect(here.some((f) => f.kind === 'ftr')).toBe(true);
    for (const f of here) {
      expect(fogBound(g, f)).toBe(true);
      expect(whyNotMission(g, f, 'bomb', at(g, 'Ruhr'))).toMatch(/^Fog has closed the airfield at /);
      expect(groupRow(g, f, 'UK').sky?.[0]).toBe(FOG.words.card);
    }
    for (const f of g.airFleets.filter((x) => x.ship !== undefined)) expect(fogBound(g, f)).toBe(false);
  });

  it('names the player\'s fogged fields in the month\'s news, and says so when the fog lifts (words only)', () => {
    const g = grand('UK'); setFog(g, true);
    const m = turnToDate(g.turn).month;
    force('nwe', 0);
    FOG.odds.nwe[m] = 10;
    const h0 = g.headlines.length;
    fogMonth(g);
    expect(g.headlines.slice(h0).some((h) => /^Fog closed your airfields? at [^()]+ this month: .* stayed on the ground\.$/.test(h.text))).toBe(true);
    g.turn++;
    const h1 = g.headlines.length;
    fogMonth(g);
    expect(g.headlines.slice(h1).some((h) => /^The fog has lifted: (one air group that sat it out is|\d+ air groups that sat it out are) fit to fly again\.$/.test(h.text))).toBe(true);
  });
});
