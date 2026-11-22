// Neighbours join the fight (release 2): the five findings of the independent review of 22 November, each held here.
//
// 1. An ally's corps in the player's army that joined a battle earlier in the month keeps its month spent when the
//    player's month opens (jointMonth reads joinedIn as startPhase does for his own corps).
// 2. A staff's two-wave plan keeps its main body out of the first wave's battle, and reckons the second wave without
//    those the first will have spent (the first wave itself, and whoever joined it on either side).
// 3. "This month" is the calendar's: a corps whose nation plays later in the month has its whole month in hand,
//    whatever it left unspent or fought last month; a battle fought earlier this month in another nation's phase
//    still counts after its own month has begun (foughtIn).
// 4. The player's hold-back order can be given in the game. Since Q3 was folded (the design director, 18:12Z) it is
//    the one garrison toggle on each of his corps' cards, and the attack card and the tray only show what it set.
// 5. A corps on foot that comes up into open desert against tanks and lorries is driven round, as the defenders on
//    foot outside their box are.
//
// And its two display nits:
// 6. The landing card names the enemy next door as the attack card does: where any of it is not identified, only
//    that armies next door may join.
// 7. The month card's men lost: his own corps that only came up from next door count, in a battle that is not his;
//    in his own battles, only his own nation's corps that came up count, not an ally's.
// And the design director's ruling of 08:16Z:
// 8. A storm of a town and a garrison's sortie keep taking nobody from next door, on either side, and their cards
//    say so in one line, with the switch on alone.
import { afterEach, beforeEach, describe, expect, it } from 'vitest';
import page from '../src/campaign/ui/main.ts?raw';
import { DESERT, addCorps, assess, createGame, declareWar, ensureSupply, limitAttackers, relocate, resolveBattle, startPhase, type Assessment } from '../src/campaign/game';
import { twoWaves } from '../src/campaign/ai';
import { JOIN, JOIN_ATTACK, JOIN_DEFENCE, JOIN_NONE, MAY_JOIN, SORTIE_ALONE, STORM_ALONE, battleThisMonth, weeksInHand } from '../src/campaign/join';
import { identify } from '../src/campaign/intel';
import { monthCard, monthMark, type CardRow, type MonthCard, type MonthMark } from '../src/campaign/monthcard';
import { garrison, previewSally } from '../src/campaign/siege';
import { startOpen } from '../src/campaign/staff';
import { GARRISON, beginHuman, candidates, canOrder, endHuman, garrisonHover, garrisonNotice, humanUp, joinChoices, keptOutLine, nextDoorCount, orderAttack, orderHold, orderJoin, orderMove, previewLanding, stepAI, stormOutlook, ticketSeen } from '../src/campaign/turn';
import { type BattleReport, type Corps, type Game, type Region } from '../src/campaign/types';
import { type TicketLine } from '../src/core/ticket';

/** The player's first month in a seat: the computer's months before his are played. */
function seat(nat: string, seed = 3): Game {
  const g = startOpen(createGame(seed, nat));
  for (let k = 0; !humanUp(g) && k < 200; k++) stepAI(g);
  beginHuman(g);
  return g;
}
const corps = (g: Game, nat: string, name: string): Corps => g.corps.find((c) => c.nat === nat && c.name === name && !c.dead)!;
const ids = (a: Assessment, side: 'att' | 'def'): number[] => (a.join?.[side] ?? []).map((j) => j.id);
/**
 * The staffs' front rule, screen (ai.ts advance; the design director's ruling 3): two corps stay to screen any place
 * that faces the enemy, so a computer staff's corps that stand one or two to such a province never come up from it.
 * Where a test is about something else, two test corps of that side, of the kind of the first corps there, are raised
 * in each province next door to the battle where the side stands. They have had a battle this month, so they never
 * come up themselves: they stand in the screen, and the real corps there may come up.
 */
function screenFor(g: Game, target: Region, nat: string): void {
  for (const j of target.nb) {
    const r = g.regions[j], first = r.units.find((u) => u.nat === nat && u.kind !== 'fort');
    if (!first) continue;
    for (const k of [1, 2]) {
      const c = addCorps(g, nat, `Test Screen ${k} (${r.name})`, first.kind, 1, r.id, {});
      c.fought = true; c.foughtIn = g.turn;
    }
  }
}

beforeEach(() => { JOIN.on = true; });
afterEach(() => { JOIN.on = false; });

describe("1. an ally's corps in the player's army", () => {
  it('that joined a battle earlier in the month has it spent when his month opens, and the next month is its own again', () => {
    // France's seat: London District has crossed to Lorraine and marches with the French army there.
    const g = seat('FRA');
    const uk = corps(g, 'UK', 'London District'), lorraine = g.byName['Lorraine'], alsace = g.byName['Alsace'], saar = g.byName['Saar'];
    relocate(g, uk, lorraine);
    expect(orderJoin(g, uk)).toBe(true);
    expect(uk.joint).toBe('FRA');
    // (It came under Britain's hold from the setup, the one mark: a corps in his army is his to free with its toggle.)
    expect(uk.hold).toBe(true);
    expect(orderHold(g, uk, false)).toBe(true);
    // The month ends; the next opens with Germany's phase, before France's.
    endHuman(g);
    for (let k = 0; g.turn === 1 && k < 200; k++) stepAI(g);
    expect(g.turn).toBe(2);
    expect(g.order[g.phase]).toBe('GER');
    // A German corps from the Saar falls on Alsace: London District comes up from Lorraine next door, in defence.
    const german = saar.units.find((c) => c.nat === 'GER' && c.kind !== 'fort')!;
    expect(german).toBeDefined();
    ensureSupply(g);
    const a = assess(g, [german], alsace);
    expect(ids(a, 'def')).toContain(uk.id);
    resolveBattle(g, [german], alsace);
    expect(uk.joinedIn).toBe(2);
    expect(uk.ap).toBe(0);
    // France's month opens: the corps that marches in the player's army has spent this month already.
    g.phase = g.order.indexOf('FRA');
    beginHuman(g);
    expect(canOrder(g, uk)).toBe(true);
    expect(uk.ap).toBe(0);
    expect(candidates(g, saar)).not.toContain(uk);
    expect(orderAttack(g, [uk], saar)).toBeNull();
    // A French corps that joined nothing has its month as ever.
    const french = g.corps.find((c) => c.nat === 'FRA' && !c.dead && c.kind !== 'fort' && c.town === undefined && c.joinedIn === undefined && c.leave === undefined)!;
    expect(french.ap).toBe(french.apMax);
    // The next month it is the army's again.
    g.turn += 1;
    startPhase(g, 'UK');
    startPhase(g, 'FRA');
    expect(uk.joint).toBe('FRA');
    expect(uk.ap).toBe(uk.apMax);
  });
});

describe("2. a staff's two waves", () => {
  /** Germany at Lodz: IV Corps the first wave, the six corps the roads carry behind it the main body; Poland's corps next door spent but the last to come up. */
  function waves(): { g: Game; avail: Corps[]; pool: Corps[]; pole: Corps } {
    const g = seat('GER');
    const target = g.byName['Lodz'];
    screenFor(g, target, 'POL');
    const avail = g.corps.filter((c) => c.nat === 'GER' && !c.dead && c.ap > 0 && c.kind !== 'fort' && target.nb.includes(c.reg));
    const pool = limitAttackers(g, avail.filter((c) => c.org >= 0.35 && c.name !== 'IV Corps'), target);
    expect(pool).toHaveLength(6);
    // (One Polish corps next door can still come up, the one that comes up last; the others have had a battle this month.)
    const def = assess(g, [corps(g, 'GER', 'IV Corps')], target).join!.def;
    const pole = def[def.length - 1].corps;
    for (const j of target.nb) for (const c of g.regions[j].units) if (c.nat === 'POL' && c !== pole) { c.fought = true; c.foughtIn = g.turn; }
    // (And it is worn down to a quarter of its men, so that IV Corps alone still comes at the odds a first wave needs.)
    pole.str = 0.25;
    return { g, avail, pool, pole };
  }

  it('keep the main body out of the first battle, and reckon the second without those the first spends', () => {
    const { g, avail, pool, pole } = waves();
    const target = g.byName['Lodz'];
    const two = twoWaves(g, avail, pool, target, 0.01)!;
    expect(two).not.toBeNull();
    expect(two.corps.map((c) => c.name)).toEqual(['IV Corps']);
    expect(two.keep).toEqual(pool);
    // The first wave's odds: none of the main body come up in it; the Pole next door does.
    const a1 = assess(g, two.corps, target, { spare: two.keep });
    expect(ids(a1, 'att').some((id) => pool.some((c) => c.id === id))).toBe(false);
    expect(ids(a1, 'def')).toEqual([pole.id]);
    // The main body's odds count neither the first wave nor the Pole as joiners: as the plan used to, they read higher.
    const asWas = assess(g, pool, target, { after: { blows: a1.weight, wear: a1.wear } });
    expect(ids(asWas, 'att')).toContain(two.corps[0].id);
    expect(ids(asWas, 'def')).toContain(pole.id);
    const plan = assess(g, pool, target, { after: { blows: a1.weight, wear: a1.wear }, spare: [...two.corps, pole] });
    expect(plan.join).toBeUndefined();
    expect(two.then).toBe(plan.odds);
    // The first wave goes in as the staff sends it: the main body is left whole for the second.
    const rep = resolveBattle(g, two.corps, target, { spare: two.keep });
    expect(rep.attackers.filter((u) => u.joined !== undefined)).toHaveLength(0);
    for (const c of pool) {
      expect(c.ap).toBeGreaterThan(0);
      expect(c.fought).toBe(false);
      expect(c.joinedIn).toBeUndefined();
    }
    expect(pole.joinedIn).toBe(g.turn);
    // And the second battle has the joiners the plan reckoned: none.
    if (!rep.taken) expect(assess(g, pool, target).join).toBeUndefined();
  });

  it('off, plan as they always did', () => {
    const { g, avail, pool } = waves();
    JOIN.on = false;
    const target = g.byName['Lodz'];
    const two = twoWaves(g, avail, pool, target, 0.01)!;
    expect(two).not.toBeNull();
    expect('keep' in two).toBe(false);
    const a1 = assess(g, two.corps, target);
    expect(two.then).toBe(assess(g, pool, target, { after: { blows: a1.weight, wear: a1.wear } }).odds);
  });
});

describe("3. the month is the calendar's", () => {
  it('a corps whose nation plays later in the month has its whole month in hand, whatever it did last month', () => {
    const g = seat('GER');
    const target = g.byName['Lodz'], iv = corps(g, 'GER', 'IV Corps');
    screenFor(g, target, 'POL');
    const krakow = corps(g, 'POL', 'Army Krakow (Silesia)');
    ensureSupply(g);
    expect(ids(assess(g, [iv], target), 'def')).toContain(krakow.id);
    // Last month it marched its weeks away and fought a battle (Poland plays after Germany).
    krakow.ap = 0; krakow.fought = true; krakow.foughtIn = g.turn - 1;
    expect(g.order.indexOf('POL')).toBeGreaterThan(g.phase);
    expect(weeksInHand(g, krakow)).toBe(krakow.apMax);
    expect(battleThisMonth(g, krakow)).toBe(false);
    expect(ids(assess(g, [iv], target), 'def')).toContain(krakow.id);
    // Had Poland's month begun, that would be this month's: no weeks left, and a battle since it began.
    const was = g.phase;
    g.phase = g.order.indexOf('POL');
    expect(weeksInHand(g, krakow)).toBe(0);
    expect(battleThisMonth(g, krakow)).toBe(true);
    g.phase = was;
  });

  it('a battle fought in an earlier phase of the month still counts once its own month has begun, and no longer the next month', () => {
    const g = seat('GER');
    const target = g.byName['Lodz'], iv = corps(g, 'GER', 'IV Corps');
    const rep = orderAttack(g, [iv], target)!;
    // Everyone in the battle has fought this month, written with the switch on.
    for (const u of rep.defenders.filter((x) => !x.guard && x.joined === undefined)) {
      const c = corps(g, u.nat, u.name);
      if (c) expect(c.foughtIn).toBe(g.turn);
    }
    expect(iv.foughtIn).toBe(g.turn);
    const lodzArmy = corps(g, 'POL', 'Army Lodz');
    expect(lodzArmy.foughtIn).toBe(g.turn);
    // Poland's month opens: its 'fought' is cleared, the battle of Germany's phase is not forgotten.
    startPhase(g, 'POL');
    expect(lodzArmy.fought).toBe(false);
    expect(battleThisMonth(g, lodzArmy)).toBe(true);
    // The next month it is free of it.
    g.turn += 1;
    startPhase(g, 'POL');
    expect('foughtIn' in lodzArmy).toBe(false);
    expect(battleThisMonth(g, lodzArmy)).toBe(false);
  });
});

describe("4. the player's garrison toggle, in the game (Q3 folded: one mark, one toggle, worded Garrison)", () => {
  it('the attack card shows his corps next door: those that come up, and his garrisons there, which will not', () => {
    const g = seat('GER');
    const target = g.byName['Lodz'], iv = corps(g, 'GER', 'IV Corps'), xvi = corps(g, 'GER', 'XVI Corps');
    let a = assess(g, [iv], target);
    let next = joinChoices(g, a, target, [iv]);
    expect(next.coming.map((j) => j.corps.id)).toEqual(ids(a, 'att'));
    expect(next.coming.every((j) => j.word !== null && j.share > 0)).toBe(true);
    expect(next.held).toEqual([]);
    // His toggle on XVI Corps' card: it is a garrison, and the card names it among those that will not join.
    expect(orderHold(g, xvi, true)).toBe(true);
    a = assess(g, [iv], target);
    next = joinChoices(g, a, target, [iv]);
    expect(next.coming.map((j) => j.corps)).not.toContain(xvi);
    expect(next.held.map((j) => j.corps)).toEqual([xvi]);
    expect(keptOutLine(next.held.map((j) => j.corps))).toBe('Kept out, will not join: XVI Corps.');
    // Sent in itself, it is no part of the strip.
    expect(joinChoices(g, assess(g, [iv, xvi], target), target, [iv, xvi]).held).toEqual([]);
    // Not the enemy's, and nothing with the switch off.
    expect([...next.coming, ...next.held].every((j) => j.corps.nat === 'GER')).toBe(true);
    JOIN.on = false;
    expect(joinChoices(g, assess(g, [iv], target), target, [iv])).toEqual({ coming: [], held: [] });
  });

  it("the words are the UI director's: the tag, the toggle, the hovers, the notices, the strip's line and the tray's count", () => {
    const g = seat('GER');
    const xii = corps(g, 'GER', 'XII Corps'), xvi = corps(g, 'GER', 'XVI Corps');
    // A hold from the setup: its hover opens with the line for it, then the hold-back's own text.
    expect(GARRISON.tag).toBe('Garrison');
    expect(GARRISON.toggle).toBe('GARRISON');
    expect(garrisonHover(g, xii)).toBe(`Garrison from the start of the campaign. ${GARRISON.held}`);
    expect(GARRISON.held).toBe('Kept out of the battles next door: it fights only where you send it, and its month stays its own. Press again to let it come up.');
    expect(GARRISON.set).toBe('Make this corps the garrison here: it will not join a battle next door.');
    expect(GARRISON.lift).toBe('Free this corps from garrison duty: it can join a battle next door again.');
    orderHold(g, xvi, true);
    expect(garrisonHover(g, xvi)).toBe(GARRISON.held);
    expect(garrisonNotice(g, xvi)).toBe(`XVI Corps is now the garrison at ${g.regions[xvi.reg].name}: it will not join the battles next door.`);
    orderHold(g, xvi, false);
    expect(garrisonNotice(g, xvi)).toBe('XVI Corps is no longer a garrison: it can join the battles next door.');
    // Three names, then how many more.
    const five = g.corps.filter((c) => c.nat === 'GER' && !c.dead && c.kind !== 'fort').slice(0, 5);
    expect(keptOutLine(five)).toBe(`Kept out, will not join: ${five.slice(0, 3).map((c) => c.name).join(', ')} and 2 more.`);
    expect(keptOutLine(five.slice(0, 4))).toBe(`Kept out, will not join: ${five.slice(0, 3).map((c) => c.name).join(', ')} and one more.`);
    expect(keptOutLine(five.slice(0, 2))).toBe(`Kept out, will not join: ${five[0].name} and ${five[1].name}.`);
    // The tray's count, with no brackets.
    const seven = g.corps.filter((c) => c.nat === 'GER' && !c.dead && c.kind !== 'fort' && !c.hold).slice(0, 7);
    for (const c of seven.slice(4)) c.hold = true;
    expect(nextDoorCount(seven)).toBe('Battles next door: 4 join them, 3 stay out.');
    expect(nextDoorCount(seven.slice(0, 4))).toBe('Battles next door: 4 join them.');
    expect(nextDoorCount(seven.slice(3, 5))).toBe('Battles next door: 1 joins them, 1 stays out.');
  });

  it('the page gives it: one toggle under each of his corps cards in the tray, and no second control anywhere', () => {
    // The corps card in the tray: the Garrison tag on a held corps of his, and the toggle, lit while it is set.
    expect(page).toMatch(/joinRules\(\) && mine && c\.hold && c\.kind !== 'fort' \? `<span class="tag" title="\$\{esc\(garrisonHover\(g, c\)\)\}">\$\{GARRISON\.tag\}<\/span>`/);
    expect(page).toMatch(/if \(!\(mine && mayHold\(g, c\)\)\) return box;/);
    expect(page).toContain('<button class="gtog${c.hold ? \' on\' : \'\'}" data-act="hold" data-id="${c.id}" aria-pressed="${c.hold ? \'true\' : \'false\'}" title="${esc(c.hold ? GARRISON.lift : GARRISON.set)}">${GARRISON.toggle}</button>');
    // (Its own action, "hold": "garrison" is already the order into a town, and a second case of it would never run.)
    expect(page.match(/case 'hold':/g)).toHaveLength(1);
    expect(page).toMatch(/case 'hold': \{[\s\S]*?orderHold\(g, c, !c\.hold\)\) \{ ui\.msg = garrisonNotice\(g, c\); queueSave\(\); \}[\s\S]*?refresh\(\); break;/);
    // The corps row: his own held corps carry the word too, with the switch on (off, as release 1: another's only).
    expect(page).toContain("if (c.hold && (!mine || joinRules())) tags.push(`<span class=\"tag\">${GARRISON.tag}</span>`);");
    // The tray's row is a count, and the attack card's strip a plain line: nothing there is pressed.
    expect(page).toContain('${esc(nextDoorCount(able))}');
    expect(page).toContain('<span class="keptout">${esc(keptOutLine(next.held.map((j) => j.corps)))}</span>');
    expect(page).toMatch(/const nextDoor = joinChoices\(g, a, target, chosen\);/);
    expect(page).toContain('${joinStrip(nextDoor)}');
    for (const gone of ['data-act="joinnext"', 'data-act="joinhold"', 'orderHoldBack', 'holdsBack', 'Stays out', 'Stay out']) expect(page).not.toContain(gone);
    expect(page).toMatch(/note: why\(c\) \|\| joinNote\(c\)/);
  });
});

describe('5. open desert', () => {
  /** Italy against Britain in the Western Desert: the 7th Armoured holds Sidi Barrani, the 4th Indian (on foot) stands next door at Mersa Matruh. */
  function desert(mobile: boolean): { a: Assessment; off: Assessment; indian: Corps } {
    JOIN.on = true;
    const g = startOpen(createGame(3, null));
    declareWar(g, 'ITA', 'UK');
    const sidi = g.byName['Sidi Barrani'];
    relocate(g, corps(g, 'UK', '7th Armoured Division'), sidi);
    const indian = corps(g, 'UK', '4th Indian Division');
    expect(indian.reg).toBe(g.byName['Mersa Matruh'].id);
    const xxi = corps(g, 'ITA', 'XXI Corps, 10th Army');
    const att = mobile ? [xxi, addCorps(g, 'ITA', 'Test Motorised Corps', 'mot', 2, 'Bardia', {})] : [xxi];
    ensureSupply(g);
    const a = assess(g, att, sidi);
    JOIN.on = false;
    const off = assess(g, att, sidi);
    JOIN.on = true;
    return { a, off, indian };
  }

  it('foot that comes up against tanks and lorries counts at the open desert cut, and the card says so', () => {
    const moving = desert(true), plain = desert(false);
    for (const x of [moving, plain]) expect(ids(x.a, 'def')).toEqual([x.indian.id]);
    // The 4th Indian's part of the defence, with and without anything on wheels or tracks in the attack.
    const cut = (moving.a.D - moving.off.D) / (plain.a.D - plain.off.D);
    expect(cut).toBeCloseTo(DESERT.open, 9);
    // The armour that holds the ground is not driven round: off, there is no open desert line; on, the joiner brings it.
    expect(moving.off.lines.some((l) => l.label === 'Open desert')).toBe(false);
    expect(moving.a.lines.find((l) => l.label === 'Open desert')!.effect).toBe(`Foot infantry defends at ${Math.round(DESERT.open * 100)}%`);
    expect(plain.a.lines.some((l) => l.label === 'Open desert')).toBe(false);
  });
});

describe('6. the landing card', () => {
  it('names the enemy next door only where his staff has identified it, as the attack card does', () => {
    // Germany's seat: the Konigsberg garrison lands in the Danzig Corridor; Army Pomorze, not identified, stands next door.
    const g = seat('GER');
    const from = corps(g, 'GER', 'Konigsberg garrison'), beach = g.byName['Danzig Corridor'];
    screenFor(g, beach, 'POL');
    // (Late in Germany's month: every German corps on land but XIX Corps next door has had its battle and spent its
    // weeks, so the keeping sends XIX Corps to the landing and Army Pomorze to the Corridor, the design director's ruling 4.)
    const xix = corps(g, 'GER', 'XIX Corps');
    for (const c of g.corps) if (c.nat === 'GER' && c !== from && c !== xix) { c.fought = true; c.ap = 0; }
    const a = previewLanding(g, [from], beach.id).assess!;
    expect(a.join!.att.map((j) => j.name)).toEqual(['XIX Corps']);
    expect(a.join!.def.map((j) => j.name)).toEqual(['Army Pomorze']);
    expect(a.lines.find((l) => l.label === JOIN_DEFENCE)!.value).toMatch(/Army Pomorze/);
    const seen = ticketSeen(g, a);
    expect(seen.find((l) => l.label === JOIN_DEFENCE)).toEqual({ label: JOIN_DEFENCE, value: MAY_JOIN });
    expect(JSON.stringify(seen)).not.toContain('Army Pomorze');
    // Every other line as the ticket has it: his own side's joiners named.
    expect(seen.find((l) => l.label === JOIN_ATTACK)).toEqual(a.lines.find((l) => l.label === JOIN_ATTACK));
    expect(seen.filter((l) => l.label !== JOIN_DEFENCE)).toEqual(a.lines.filter((l) => l.label !== JOIN_DEFENCE));
    // Identified, named with their shares.
    identify(g, a.join!.def.map((j) => j.corps), 'GER');
    expect(ticketSeen(g, a)).toEqual(a.lines);
    // Off, nobody joins and the ticket is the assessment's own.
    JOIN.on = false;
    const off = previewLanding(g, [from], beach.id).assess!;
    expect(off.join).toBeUndefined();
    expect(ticketSeen(g, off)).toBe(off.lines);
    // The page's landing card reads its ticket through it.
    expect(page).toContain('<h3>Battle Ticket</h3><table class="ticket">${ticketSeen(g, a).map((l) =>');
    expect(page).not.toContain('<h3>Battle Ticket</h3><table class="ticket">${a.lines.map(');
  });
});

describe("7. the month card's men lost", () => {
  /** France's seat: the 3rd Army attacks the Saar from Lorraine, where London District now stands; it and the 5th Army in Alsace come up. */
  function saar(): { g: Game; marks: Record<'FRA' | 'UK', MonthMark>; rep: BattleReport } {
    const g = seat('FRA');
    const uk = corps(g, 'UK', 'London District');
    relocate(g, uk, g.byName['Lorraine']);
    // (Its hold lifted: a computer staff's corps under a hold stays where it is, the design director's ruling 3. And the
    // 5th Army's, a hold of his from the setup, lifted with its toggle: under a hold it would stay, as Q3 folded has it.
    // With the switch off the toggle does not exist and the setup hold stays, as in release 1.)
    uk.hold = false;
    expect(orderHold(g, corps(g, 'FRA', '5th Army'), false)).toBe(JOIN.on);
    const marks = { FRA: monthMark(g, 'FRA'), UK: monthMark(g, 'UK') };
    const rep = orderAttack(g, [corps(g, 'FRA', '3rd Army')], g.byName['Saar'])!;
    return { g, marks, rep };
  }
  const lostBox = (g: Game, mark: MonthMark, nat: string): string | undefined => monthCard(g, mark, nat).boxes.find((b) => b.key === 'men')?.figure;
  const num = (n: number): string => n.toLocaleString('en-US');

  it("count his own corps that only came up, and in his battles none of an ally's that came up", () => {
    const { g, marks, rep } = saar();
    const uk = rep.attackers.find((u) => u.nat === 'UK')!, fifth = rep.attackers.find((u) => u.name === '5th Army')!;
    expect(uk.joined).toBeDefined();
    expect(fifth.joined).toBeDefined();
    expect(uk.lost).toBeGreaterThan(0);
    expect(fifth.lost).toBeGreaterThan(0);
    // France's battle: his own corps, the 5th Army that came up among them; not London District.
    const french = rep.attackers.filter((u) => u.nat === 'FRA').reduce((n, u) => n + u.lost, 0);
    expect(french).toBe(rep.attLost - uk.lost);
    expect(lostBox(g, marks.FRA, 'FRA')).toBe(num(french));
    const rowsOf = (card: MonthCard): CardRow[] => [...(card.lead ? [card.lead] : []), ...card.rows];
    const row = rowsOf(monthCard(g, marks.FRA, 'FRA')).find((r) => r.report?.type === 'land' && r.report.id === rep.id)!;
    expect(row.figures[0]).toEqual({ n: num(french), label: 'lost' });
    // Britain's card: no row for a battle it only came up in, and the men London District lost.
    expect(rowsOf(monthCard(g, marks.UK, 'UK')).some((r) => r.report?.type === 'land' && r.report.id === rep.id)).toBe(false);
    expect(lostBox(g, marks.UK, 'UK')).toBe(num(uk.lost));
  });

  it('off, nobody comes up and the card counts the battle as release 1 did', () => {
    JOIN.on = false;
    const { g, marks, rep } = saar();
    expect([...rep.attackers, ...rep.defenders].some((u) => u.joined !== undefined)).toBe(false);
    expect(lostBox(g, marks.FRA, 'FRA')).toBe(num(rep.attLost));
    expect(lostBox(g, marks.UK, 'UK')).toBeUndefined();
  });
});

describe('8. storms and sorties', () => {
  it('take nobody from next door, and their cards say so in one line, with the switch on alone', () => {
    // Germany's seat: Lodz held against her by one Polish corps inside the town, the field cleared, one German corps round it.
    const g = seat('GER');
    const town = g.byName['Lodz'];
    const inside = town.units.find((u) => u.nat === 'POL' && u.kind === 'inf')!;
    expect(garrison(g, inside)).toBe(true);
    for (const u of [...town.units]) relocate(g, u, g.byName['Warsaw']);
    const ger = town.nb.flatMap((j) => g.regions[j].units.filter((u) => u.nat === 'GER' && u.kind !== 'fort' && u.kind !== 'eng' && u.ap > 0));
    for (const c of ger.slice(0, 2)) orderMove(g, c, town.id);
    // Corps of both sides still stand next door.
    expect(ger.length).toBeGreaterThan(2);
    expect(town.nb.some((j) => g.regions[j].units.some((u) => u.nat === 'POL' && u.kind !== 'fort'))).toBe(true);
    const read = (): { storm: TicketLine[]; stormOdds: number; sortie: TicketLine[]; sortieOdds: number } => {
      const st = stormOutlook(g, town.id)!, so = previewSally(g, town, 'POL')!;
      return { storm: st.lines, stormOdds: st.odds, sortie: so.lines, sortieOdds: so.odds };
    };
    const on = read();
    expect(on.storm.filter((l) => l.label === JOIN_NONE)).toEqual([{ label: JOIN_NONE, value: STORM_ALONE }]);
    expect(on.sortie.filter((l) => l.label === JOIN_NONE)).toEqual([{ label: JOIN_NONE, value: SORTIE_ALONE }]);
    for (const lines of [on.storm, on.sortie]) expect(lines.some((l) => l.label === JOIN_ATTACK || l.label === JOIN_DEFENCE)).toBe(false);
    // Off, no such line, and every other line and the odds as they are with it on.
    JOIN.on = false;
    const off = read();
    for (const lines of [off.storm, off.sortie]) expect(lines.some((l) => l.label === JOIN_NONE)).toBe(false);
    expect(on.storm.filter((l) => l.label !== JOIN_NONE)).toEqual(off.storm);
    expect(on.sortie.filter((l) => l.label !== JOIN_NONE)).toEqual(off.sortie);
    expect(on.stormOdds).toBe(off.stormOdds);
    expect(on.sortieOdds).toBe(off.sortieOdds);
  });
});
