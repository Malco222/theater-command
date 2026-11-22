// The 3D game's THE SECRET SERVICE page (unreal/TheaterCommand/Rules/campaign-bridge.ts), release 2: the service's
// file shows the newest notes, where a turned agent's telltale report is (Brian, 22 November 2026, yes (3): a
// telltale sign so a false report is never unfair), and the note gives the chance by country only where there is
// more than one country, as the browser does.
import { describe, expect, it } from 'vitest';
import { createGame, setWar } from '../src/campaign/game';
import { spyRoom } from '../src/campaign/intel';
import { saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { turnToDate } from '../src/core/ticket';
import type { Agent, FoeAgent, Game, Mission } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

type Page = { note: string; sections: { heading: string; text: string }[] };
const TC = (globalThis as unknown as { TC: { load: (t: string) => unknown; page: (id: string) => Page } }).TC;
const show = (g: Game): Page => { expect(TC.load(saveGame(g))).not.toBeNull(); return TC.page('spies'); };

describe("THE SERVICE'S FILE in the 3D game", () => {
  it('shows the ten newest notes, newest first, so the tell is never pushed off the page', () => {
    const g = startOpen(createGame(7, 'UK'));
    setWar(g, 'UK', 'GER');
    const ruhr = g.byName.Ruhr.id;
    for (let k = 0; k < 15; k++) g.intel.notes.push({ turn: g.turn - 15 + k, agent: 'Kestrel', nat: 'UK', reg: ruhr, what: 'seen', text: `Report number ${k}.`, good: true });
    g.intel.notes.push({ turn: g.turn, agent: 'Kestrel', nat: 'UK', reg: ruhr, what: 'quiet', text: 'Agent Kestrel, Ruhr: all quiet, and the German forces there are weaker than we feared.', good: true });
    const file = show(g).sections.find((s) => s.heading === "THE SERVICE'S FILE")!.text.split('\n');
    expect(file.length).toBe(10);
    expect(file[0]).toBe(`${turnToDate(g.turn).label}: Agent Kestrel, Ruhr: all quiet, and the German forces there are weaker than we feared.`);
    expect(file[1]).toMatch(/Report number 14\.$/);
    expect(file[9]).toMatch(/Report number 6\.$/);
  });
});

describe("the page's note", () => {
  it('gives the chance by country only where more than one country spies on him', () => {
    const g = startOpen(createGame(7, 'UK'));
    setWar(g, 'UK', 'GER');
    g.flags.spyfrom = -99;
    expect(spyRoom(g)!.threat.foes).toEqual(['GER']);
    const note = show(g).note;
    expect(note).toMatch(/ Enemy strike at home this month: \d+%, with 0 agents on guard\.$/);
    expect(note).not.toMatch(/\(/);
    setWar(g, 'UK', 'HUN');
    expect(spyRoom(g)!.threat.foes.length).toBeGreaterThan(1);
    // (the UI director, 22 November: the note stays at about 30 words, so the chance by country is in the last section)
    const page = show(g);
    expect(page.note).toMatch(/ Enemy strike at home this month: \d+%, with 0 agents on guard\.$/);
    expect(page.sections.at(-1)!.heading).toBe('HOW THE ENEMY SPIES ON YOU');
    expect(page.sections.at(-1)!.text).toMatch(/^The chance of a strike at home each month, by country: .*Germany \d+%.*Hungary \d+%/);
  });

  it('is about 30 words in an ordinary month, however many countries spy on him', () => {
    const g = startOpen(createGame(7, 'SOV'));
    for (const f of ['GER', 'ITA', 'HUN', 'ROM', 'FIN']) setWar(g, 'SOV', f as never);
    g.flags.spyfrom = -99;
    expect(spyRoom(g)!.threat.foes.length).toBeGreaterThan(3);
    const note = show(g).note;
    expect(note).not.toMatch(/\(/);
    expect(words(note)).toBeLessThanOrEqual(32);
  });

  // (The UI director's ruling of 22 November, release 2: about 30 words, and in the services' quiet months nothing
  // that contradicts the help's "about one every 3 months".)
  it('is about 30 words, and in the quiet months says only that nothing works against him this month', () => {
    const g = startOpen(createGame(7, 'UK'));
    setWar(g, 'UK', 'GER');
    g.flags.spyfrom = g.turn;
    expect(spyRoom(g)!.threat.foes).toEqual([]);
    const note = show(g).note;
    expect(note).toMatch(/ No enemy service is working against you at home this month\.$/);
    expect(words(note)).toBeLessThanOrEqual(32);
  });
});

const words = (t: string): number => t.split(/\s+/).filter((w) => /[A-Za-z0-9]/.test(w)).length;
type Row = { title: string; lines: string[]; buttons: { label: string; act: string; ok: boolean; tip: string }[] };
type Room = { title: string; headline: string; note: string; top: { label: string }[]; sections: { heading: string; text: string; rows: Row[] }[] };
const room = (g: Game): Room => show(g) as unknown as Room;
const agentAt = (g: Game, at: string, mission: Mission = 'watch'): Agent => {
  const a: Agent = { id: g.intel.next++, nat: g.human!, name: `Test ${g.intel.next}`, xp: 0, reg: g.byName[at].id, ready: g.turn, mission, born: g.turn };
  g.intel.agents.push(a);
  return a;
};

// (The UI director's count of 22 November, release 2: the room opened at 328 words with no agents, 245 of them help
// on watchers and turned agents, and an agent abroad carried 15 buttons, 11 of them dim. Rulings 1 to 4 and the sweep.)
describe('the room as the UI director ruled it', () => {
  it('says one sentence under YOUR AGENTS and keeps how the enemy spies on him for a last section of its own', () => {
    const g = startOpen(createGame(7, 'UK'));
    setWar(g, 'UK', 'GER');
    const page = room(g);
    const mine = page.sections.find((x) => x.heading === 'YOUR AGENTS')!;
    expect(mine.text).toBe('The service has no agents yet: recruit one above, and the new agent stands on the map at home.');
    const last = page.sections.at(-1)!;
    expect(last.heading).toBe('HOW THE ENEMY SPIES ON YOU');
    expect(last.text).toMatch(/A watcher maps the defences/);
    expect(last.text).toMatch(/too good to be true/);
    // Everything above the help: the head, the buttons and the other sections.
    const above = words([page.title, page.headline, page.note, ...page.top.map((b) => b.label), ...page.sections.slice(0, -1).map((x) => `${x.heading} ${x.text}`)].join(' '));
    expect(above).toBeLessThanOrEqual(95);
  });

  it("puts on an agent's row only the work that can be done there, with one line for the rest", () => {
    const g = startOpen(createGame(7, 'UK'));
    setWar(g, 'UK', 'GER');
    agentAt(g, 'Berlin');
    const row = room(g).sections.find((x) => x.heading === 'YOUR AGENTS')!.rows[0];
    const offered = spyRoom(g)!.agents[0].missions;
    const shown = row.buttons.filter((b) => b.act.startsWith('spy:mission:'));
    expect(shown.map((b) => b.act.split(':')[3])).toEqual(offered.filter((m) => m.ok || m.now).map((m) => m.id));
    expect(shown.length).toBeLessThan(offered.length);
    expect(row.buttons.some((b) => b.act.startsWith('spy:send:'))).toBe(true);
    expect(row.buttons.some((b) => b.act.startsWith('spy:retire:'))).toBe(true);
    // (a great power draws its own plans, so no theft is counted; Berlin lacks only an oilfield)
    expect(row.lines).toContain('There is no oilfield at Berlin.');
    expect(row.lines.join(' ')).not.toMatch(/plans|more need/);
    // (whole divisions, as the card counts them)
    expect(row.lines.filter((l) => l.startsWith('In sight at ')).length).toBeGreaterThan(0);
    for (const l of row.lines.filter((x) => x.startsWith('In sight at '))) expect(l).toMatch(/, \d+ divisions?\.$/);
  });

  // (Found by review, 22 November: each need is read from the rules' reason, never from the work's name.)
  it("says what the rest needs from the rules' own reasons, each cause once", () => {
    const g = startOpen(createGame(7, 'UK'));
    setWar(g, 'UK', 'GER');
    const line = (a: Agent): string => room(g).sections.find((x) => x.heading === 'YOUR AGENTS')!.rows.find((r) => r.title.endsWith(a.name))!.lines[2];
    const ruhr = agentAt(g, 'Ruhr');
    expect(line(ruhr)).toMatch(/^4 more need a capital, an oilfield or enemy aircraft on the field here\.$/);
    // A railway already cut has a railway: the line says it is cut.
    g.flags[`rail:${g.byName.Ruhr.id}`] = g.turn;
    expect(line(ruhr)).toMatch(/^4 more need .+ here\. The railway at Ruhr is already cut\.$/);
    // Lying low, already said on the line above, is not said again.
    ruhr.rest = g.turn + 2;
    expect(room(g).sections[0].rows[0].lines[1]).toMatch(/Lying low for 2 more months/);
    expect(room(g).sections[0].rows[0].lines.filter((l) => /lying low/i.test(l)).length).toBe(1);
    // At peace: one sentence, the country named as the rest of the room names it.
    const stockholm = agentAt(g, 'Stockholm');
    expect(line(stockholm)).toBe('You are at peace with Sweden: sabotage is an act of war, and its air staff has no orders against you.');
    const u = startOpen(createGame(7, 'USA'));
    const moscow = agentAt(u, 'Moscow');
    expect(room(u).sections[0].rows.find((r) => r.title.endsWith(moscow.name))!.lines.join(' ')).toMatch(/You are at peace with the Soviet Union: sabotage is an act of war/);
  });

  it('gives an agent on the road its work for where it is going, and one in training no line for the rest', () => {
    const g = startOpen(createGame(7, 'UK'));
    setWar(g, 'UK', 'GER');
    const a = agentAt(g, 'Berlin');
    a.ready = g.turn + 1;
    a.born = g.turn - 3; // (sent, not newly recruited)
    const row = room(g).sections.find((x) => x.heading === 'YOUR AGENTS')!.rows[0];
    expect(row.lines).toContain('At work in 1 month.');
    // (the design director, 22 November: work may be chosen on the road, for where the agent is going)
    expect(row.buttons.some((b) => b.act.endsWith(':factory') && b.ok)).toBe(true);
    expect(row.lines).toContain('There is no oilfield at Berlin.');
    const trainee = agentAt(g, 'London', 'guard');
    trainee.ready = g.turn + 1;
    trainee.born = trainee.ready - 1;
    const rows = room(g).sections.find((x) => x.heading === 'YOUR AGENTS')!.rows;
    const t = rows.find((r) => r.title.endsWith(trainee.name))!;
    expect(t.lines.some((l) => / more need |no oilfield|friendly ground/.test(l) || /not yet at work/.test(l))).toBe(false);
  });

  it("lists on the agent's card the work in force first, then the work that can be done there, then the rest", () => {
    const g = startOpen(createGame(7, 'UK'));
    setWar(g, 'UK', 'GER');
    agentAt(g, 'Berlin');
    expect(TC.load(saveGame(g))).not.toBeNull();
    const work = (TC as unknown as { view: () => { agents: { work: { id: string; now: boolean; why: string }[] }[] } }).view().agents[0].work;
    const rank = work.map((w) => (w.now ? 0 : w.why === '' ? 1 : 2));
    expect(rank[0]).toBe(0);
    expect([...rank].sort((x, y) => x - y)).toEqual(rank);
    expect(rank.filter((k) => k === 1).length).toBeGreaterThan(0);
  });

  it('calls an agent "the agent" or "they", never "it"', () => {
    const g = startOpen(createGame(7, 'UK'));
    setWar(g, 'UK', 'GER');
    g.flags.spyfrom = -99;
    const hunter = agentAt(g, 'London', 'hunt');
    // (a saboteur: a watcher on ground with no front slips away to watch elsewhere)
    const foe: FoeAgent = { id: 1, nat: 'GER', reg: hunter.reg, strike: g.turn + 2, found: g.turn, kind: 'factory' };
    g.intel.foes = [foe];
    // (a double agent of his, and the file's notes on one blown and one watcher gone: every place a person is named)
    g.intel.doubles = [{ id: 9, name: 'Nightjar', of: 'GER', by: 'UK', since: g.turn }];
    g.intel.notes.push({ turn: g.turn, agent: '', nat: 'UK', reg: hunter.reg, what: 'gone', text: 'The German agent uncovered at London has left. Nothing more is seen of the agent.', good: false });
    const all = JSON.stringify(show(g));
    expect(all).toMatch(/ENEMY AGENTS UNCOVERED/);
    expect(all).toMatch(/DOUBLE AGENTS/);
    expect(all).toMatch(/The agent strikes unless they are arrested\./);
    expect(all).not.toMatch(/\bit (strikes|is arrested|arrives|will soon be found|maps your|is ended|goes back|is found out|warns)\b/i);
    expect(all).not.toMatch(/Arrest it\b|its own service|trusting it\b|seen of it\b|\bIt is of no more use|\bIts service\b/);
  });
});

describe('an airfield wrecked by saboteurs on the 3D sea', () => {
  it('is never drawn as a ship wreck (wreck:<region> is a field, wreck:<raid>:<n> a ship)', () => {
    const g = startOpen(createGame(7, 'UK'));
    setWar(g, 'UK', 'GER');
    // A field wrecked this month: the mark's value is the month, which as a ship wreck would decode to a sea zone.
    for (let id = 0; id < 40; id++) g.flags[`wreck:${id}`] = g.turn;
    expect(TC.load(saveGame(g))).not.toBeNull();
    const v = (TC as unknown as { view: () => { wrecks: unknown[] } }).view();
    expect(v.wrecks).toEqual([]);
  });
});
