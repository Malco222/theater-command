// The 3D game's THE SECRET SERVICE page (unreal/TheaterCommand/Rules/campaign-bridge.ts), release 2: the service's
// file shows the newest notes, where a turned agent's telltale report is (Brian, 22 November 2026, yes (3): a
// telltale sign so a false report is never unfair), and the note gives the chance by country only where there is
// more than one country, as the browser does.
import { describe, expect, it } from 'vitest';
import { createGame, setWar } from '../src/campaign/game';
import { enemyOrders, spyRoom, whyNotSend } from '../src/campaign/intel';
import { setSecretWar } from '../src/campaign/secretwar';
import { inOrAt } from '../src/campaign/towns';
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
    // (the saved note says "Ruhr"; the room gives it its article, as the news does)
    expect(file[0]).toBe(`${turnToDate(g.turn).label}: Agent Kestrel, the Ruhr: all quiet, and the German forces there are weaker than we feared.`);
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
    expect(note).toMatch(/ Enemy strike at home this month: \d+%, with no agents on guard\.$/);
    expect(note).not.toMatch(/\(/);
    setWar(g, 'UK', 'HUN');
    expect(spyRoom(g)!.threat.foes.length).toBeGreaterThan(1);
    // (the UI director, 22 November: the note stays at about 30 words, so the chance by country is in the last section)
    const page = show(g);
    expect(page.note).toMatch(/ Enemy strike at home this month: \d+%, with no agents on guard\.$/);
    // (the UI director, 22 November: a count in a sentence is a word, and the books are the enemy's)
    expect(page.note).toMatch(/^Agents abroad see through the fog of war, read the enemy's books, /);
    agentAt(g, 'London', 'guard');
    expect(show(g).note).toMatch(/, with one agent on guard\.$/);
    agentAt(g, 'Kent', 'guard');
    expect(show(g).note).toMatch(/, with two agents on guard\.$/);
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
    expect(words(note)).toBeLessThanOrEqual(34);
  });

  // (The UI director's ruling of 22 November, release 2: about 30 words, and in the services' quiet months nothing
  // that contradicts the help's "about one every three months".)
  it('is about 30 words, and in the quiet months says only that nothing works against him this month', () => {
    const g = startOpen(createGame(7, 'UK'));
    setWar(g, 'UK', 'GER');
    g.flags.spyfrom = g.turn;
    expect(spyRoom(g)!.threat.foes).toEqual([]);
    const note = show(g).note;
    expect(note).toMatch(/ No enemy service is working against you at home this month\.$/);
    expect(words(note)).toBeLessThanOrEqual(34);
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
    expect(line(ruhr)).toMatch(/^Four more need a capital, an oilfield or enemy aircraft on the field here\.$/);
    // A railway already cut has a railway: the line says it is cut.
    g.flags[`rail:${g.byName.Ruhr.id}`] = g.turn;
    expect(line(ruhr)).toMatch(/^Four more need .+ here\. The railway in the Ruhr is already cut\.$/);
    // Lying low, already said on the line above, is not said again.
    ruhr.rest = g.turn + 2;
    expect(room(g).sections[0].rows[0].lines[1]).toMatch(/Lying low for two more months/);
    expect(room(g).sections[0].rows[0].lines.filter((l) => /lying low/i.test(l)).length).toBe(1);
    // At peace: one sentence, the country named as the rest of the room names it.
    const stockholm = agentAt(g, 'Stockholm');
    expect(line(stockholm)).toBe('You are at peace with Sweden: sabotage would be an act of war.');
    const u = startOpen(createGame(7, 'USA'));
    const moscow = agentAt(u, 'Moscow');
    expect(room(u).sections[0].rows.find((r) => r.title.endsWith(moscow.name))!.lines.join(' ')).toMatch(/You are at peace with the Soviet Union: sabotage would be an act of war\./);
  });

  it('gives an agent on the road its work for where it is going, and one in training no line for the rest', () => {
    const g = startOpen(createGame(7, 'UK'));
    setWar(g, 'UK', 'GER');
    const a = agentAt(g, 'Berlin');
    a.ready = g.turn + 1;
    a.born = g.turn - 3; // (sent, not newly recruited)
    const row = room(g).sections.find((x) => x.heading === 'YOUR AGENTS')!.rows[0];
    expect(row.lines).toContain('At work in a month.');
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

  // (The UI director's mends of 22 November, read on the room as built.)
  it('names places and counts as a sentence would, and gives each agent one plain status line', () => {
    const g = startOpen(createGame(7, 'UK'));
    setWar(g, 'UK', 'GER');
    const rowOf = (a: Agent): Row => room(g).sections.find((x) => x.heading === 'YOUR AGENTS')!.rows.find((r) => r.title.endsWith(a.name))!;
    const berlin = agentAt(g, 'Berlin'), ruhr = agentAt(g, 'Ruhr'), home = agentAt(g, 'London', 'guard');
    expect(rowOf(berlin).lines[0]).toBe('Berlin, in Germany: watching and reporting.');
    expect(rowOf(ruhr).lines[0]).toBe('The Ruhr, in Germany: watching and reporting.');
    expect(rowOf(home).lines[0]).toBe('London, at home: on guard.');
    // An agent at home cannot be caught: no line for it.
    expect(rowOf(home).lines.some((l) => /caught/.test(l))).toBe(false);
    expect(rowOf(berlin).lines[1]).toMatch(/^Chance of being caught this month: \d+%\.$/);
    // A province named for a stretch of country is "in" it, with its article; one named for its town is "at" it.
    expect(rowOf(ruhr).lines.some((l) => l.startsWith('In sight in the Rhineland: '))).toBe(true);
    expect(rowOf(berlin).lines.some((l) => l.startsWith('In sight at Berlin: '))).toBe(true);
    expect(JSON.stringify(room(g))).not.toMatch(/ (at|to|from|through) (Ruhr|Rhineland)\b/);
    // On the road: what the agent will do there, and when, in words.
    const road = agentAt(g, 'Ruhr', 'factory');
    road.ready = g.turn + 2;
    expect(rowOf(road).lines[0]).toBe('The Ruhr, in Germany: on the road, and will wreck a factory there on arrival.');
    expect(rowOf(road).lines[1]).toBe('At work in two months.');
    const trainee = agentAt(g, 'London', 'guard');
    trainee.ready = g.turn + 1;
    trainee.born = trainee.ready - 1;
    expect(rowOf(trainee).lines.slice(0, 2)).toEqual(['London, at home: in training.', 'At work in a month.']);
  });

  // (Found by review of the mends, 22 November: the same words wherever the room, the card and the map say them.)
  it('says the places, the counts and the peace the same way on the card, the map and the room opened on a province', () => {
    const g = startOpen(createGame(7, 'UK'));
    setWar(g, 'UK', 'GER');
    g.flags.spyfrom = -99;
    const V = (): { agents: { id: number; doing: string; home: string; work: { id: string; why: string }[] }[]; agentMarks: { text: string }[] } => {
      expect(TC.load(saveGame(g))).not.toBeNull();
      return (TC as unknown as { view: () => ReturnType<typeof V> }).view();
    };
    // A province named for a stretch of country is "in" it; one named for its town "at" it.
    expect([g.byName.Ruhr, g.byName.Bavaria, g.byName.Kent, g.byName.Berlin].map(inOrAt)).toEqual(['in the Ruhr', 'in Bavaria', 'in Kent', 'at Berlin']);
    // (beyond the towns list, places.json's 'in' names the regions and islands: the wider world's provinces)
    if (g.byName.Colorado) expect([g.byName.Colorado, g.byName.Iceland, g.byName.Denver ?? g.byName.Berlin].map(inOrAt)).toEqual(['in Colorado', 'in Iceland', g.byName.Denver ? 'at Denver' : 'at Berlin']);
    expect(g.byName.Colorado).toBeDefined();
    const ruhr = agentAt(g, 'Ruhr');
    const road = agentAt(g, 'Ruhr', 'factory');
    road.ready = g.turn + 2;
    const v = V();
    expect(v.agents.find((x) => x.id === road.id)!.doing).toBe('On the road to the Ruhr: will wreck a factory there on arrival');
    expect(v.agents.find((x) => x.id === ruhr.id)!.doing).toBe('Watching in the Ruhr');
    expect(JSON.stringify(v.agents)).not.toMatch(/ (at|to|from|through) (Ruhr|Rhineland)\b/);
    expect(whyNotSend(g, 'UK', ruhr.id, g.byName.Ruhr.id)).toBe(`${ruhr.name} is already in the Ruhr.`);
    // The room opened with the Ruhr picked on the map.
    expect(TC.load(saveGame(g))).not.toBeNull();
    const opened = JSON.stringify((TC as unknown as { page: (id: string, r: number) => unknown }).page('spies', g.byName.Ruhr.id));
    expect(opened).toMatch(/or send one to the Ruhr from here\./);
    expect(opened).not.toMatch(/ (at|to|from|through) (Ruhr|Rhineland)\b/);
    // A province with no industry: the need is counted, not told.
    const sh = agentAt(g, 'Schleswig-Holstein');
    const line = room(g).sections[0].rows.find((r) => r.title.endsWith(sh.name))!.lines[2];
    expect(line).toBe('Five more need a capital, a factory, an oilfield or enemy aircraft on the field here.');
    // The card leaves out work no place can make possible, as the room's line does: Britain draws its own plans.
    expect(V().agents.find((x) => x.id === ruhr.id)!.work.some((w) => /Plans$/.test(w.id))).toBe(false);
    // Sabotage set: what the agent will do, plainly.
    // (given in the game as the player gives it: a saved game opens on the month, when set sabotage is tried)
    expect(TC.load(saveGame(g))).not.toBeNull();
    const T = TC as unknown as { act: (a: string, r: number) => unknown; page: (id: string) => Room };
    T.act(`spy:mission:${ruhr.id}:factory`, -1);
    expect(T.page('spies').sections[0].rows.find((r) => r.title.endsWith(ruhr.name))!.lines[0]).toBe('The Ruhr, in Germany: will try to wreck a factory next month.');
    // At home: no caught chance on the line or in the tips; the card's home line counts in words.
    const kent = agentAt(g, 'Kent', 'hunt');
    const kentRow = room(g).sections[0].rows.find((r) => r.title.endsWith(kent.name))!;
    expect(kentRow.lines[0]).toBe('Kent, at home: hunting enemy agents.');
    expect(JSON.stringify(kentRow)).not.toMatch(/caught/);
    expect(V().agents.find((x) => x.id === kent.id)!.home).toMatch(/, with no agents on guard\.$/);
    agentAt(g, 'London', 'guard');
    expect(V().agents.find((x) => x.id === kent.id)!.home).toMatch(/, with one agent on guard\.$/);
    // On an ally's ground: in the ally's country, not at home.
    const paris = agentAt(g, 'Paris', 'guard');
    expect(room(g).sections[0].rows.find((r) => r.title.endsWith(paris.name))!.lines[0]).toBe('Paris, in France: on guard.');
    // An uncovered enemy agent in a region.
    g.intel.foes = [{ id: 1, nat: 'GER', reg: g.byName.Kent.id, strike: g.turn + 2, found: g.turn, kind: 'factory' }];
    const sec = room(g).sections.find((x) => x.heading === 'ENEMY AGENTS UNCOVERED')!;
    expect(sec.rows[0].title).toBe('German agent in Kent');
    expect(sec.rows[0].lines[0]).toMatch(/^A German agent in Kent, uncovered this month\. /);
    // The map's mark for an agent caught this month gives the saved note its article.
    g.intel.notes.push({ turn: g.turn, agent: 'Kestrel', nat: 'UK', reg: g.byName.Ruhr.id, what: 'caught', text: 'Agent Kestrel is caught at Ruhr by German counter-intelligence. Nothing more is heard.', good: false });
    expect(V().agentMarks.map((m) => m.text)).toContain('Agent Kestrel is caught at the Ruhr by German counter-intelligence. Nothing more is heard.');
    // Stolen orders name their places with their articles.
    setSecretWar(g, 'orders', true);
    g.intel.orders = [{ nat: 'GER', field: g.byName.Ruhr.id, groups: [], target: g.byName.Midlands.id, mission: 'night', bombers: 120, turn: g.turn + 1, agent: 'Lapwing', by: 'UK' }];
    expect(enemyOrders(g)[0].text).toMatch(/^German bombers from the Ruhr against the Midlands next month, by night, about 120: read by Agent Lapwing\.$/);
  });

  it('says the peace once, the same on the card as in the room, and gives a country its article', () => {
    const g = startOpen(createGame(7, 'UK'));
    setWar(g, 'UK', 'GER');
    const st = agentAt(g, 'Stockholm');
    expect(TC.load(saveGame(g))).not.toBeNull();
    const work = (TC as unknown as { view: () => { agents: { work: { ok: boolean; now: boolean; why: string }[] }[] } }).view().agents[0].work;
    const whys = [...new Set(work.filter((w) => !w.ok && !w.now && /peace/.test(w.why)).map((w) => w.why))];
    expect(whys).toEqual(['You are at peace with Sweden: sabotage would be an act of war.']);
    expect(room(g).sections[0].rows.find((r) => r.title.endsWith(st.name))!.lines[2]).toBe('You are at peace with Sweden: sabotage would be an act of war.');
    // A double agent's line, with the article a country's name takes.
    const d = startOpen(createGame(7, 'GER'));
    setWar(d, 'GER', 'SOV');
    d.intel.doubles = [{ id: 9, name: 'Nightjar', of: 'SOV', by: 'GER', since: d.turn }];
    expect(spyRoom(d)!.doubles![0].text).toMatch(/, and open the Soviet Union's books\.$/);
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
