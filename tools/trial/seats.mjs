// TRIAL (far-side seats rig, never for main). Lays seats files over the game's data files, and checks them.
//
// A seats file is keyed by the data file each block belongs in (with or without ".json"):
//
//   { "_about": ...,
//     "grand.json":      { "nations": [ { "id", "small": true, "years": { "1939": text } } ] },
//     "world.json":      { "nations": { code: row }, "order": [codes], "follow": { code: mother }, "idle": { code: n },
//                          "pairs": [[a, b]], "parts": { "names": { part: label }, "of": { code: part } },
//                          "corps": [rows], "fleets": [rows] },
//     "traits.json":     { "nations": { code: row } },
//     "eve.json":        { code: { title, advice, place } },
//     "relations.json":  { "groups": [...], "pairs": [...] },
//     "diplomacy.json":  { "home": {}, "bandwagon": {}, "resist": {}, "events": [] },
//     "government.json": { "nations": { code: form } },
//     "commanders.json": { "names": { code: { first, last } } },
//     "open.json":       { "temper": { code: row } },
//     "setup":           { "capitals": { code: province }, "drop": [[nat, corps name]], "wars": [[a, b]],
//                          "held": { province: nation } } }
//
// Each block is merged into data/campaign/<that name>.json: objects by key, lists appended (a row with an "id"
// replaces the row of the same id; a row already there is not added twice). A value under a nation's code (two or
// three capital letters) replaces what was there whole. "setup" is the rig's own and goes into
// data/campaign/trial-far.json. A top-level key that names no data file is ignored with a warning.
//
// The placeholder file (tools/trial/placeholder-seats.json) is laid down first, so that every owner code on the
// chart is a country; a seats file that brings a code (a nation row, a door entry or a corps row for it) takes the
// placeholder's rows for that code away.
//
// Nothing here writes to data/: the merged files exist only inside the bundle (tools/trial/build.mjs) or the test
// run (vitest.config.ts).
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

export const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
export const DATA = path.join(ROOT, 'data/campaign');
export const PLACEHOLDER = path.join(ROOT, 'tools/trial/placeholder-seats.json');
export const SEATS_HOME = '/mnt/project-files/far-side-seats';
const CODE = /^[A-Z]{2,3}$/;
const KINDS = ['inf', 'mot', 'arm', 'cav', 'mtn', 'fort', 'art', 'eng'];
const isObj = (v) => v !== null && typeof v === 'object' && !Array.isArray(v);
const read = (p) => JSON.parse(fs.readFileSync(p, 'utf8'));

/** Every *seats.json one folder down from the seats home: globe/globe-seats.json, pacific/far-seats.json and so on. */
export function findSeatFiles(home = SEATS_HOME) {
  if (!fs.existsSync(home)) return [];
  const out = [];
  for (const d of fs.readdirSync(home, { withFileTypes: true })) {
    if (!d.isDirectory()) continue;
    for (const f of fs.readdirSync(path.join(home, d.name))) if (f.endsWith('seats.json')) out.push(path.join(home, d.name, f));
  }
  return out.sort();
}

/** The data file a top-level key of a seats file names, or null: "grand" and "grand.json" are the same key; "setup" is the rig's own file. */
export function targetOf(key) {
  const k = key.replace(/\.json$/, '');
  if (k === 'setup') return 'trial-far';
  return fs.existsSync(path.join(DATA, `${k}.json`)) ? k : null;
}

/** Reads a seats file into { path, blocks: { target: block }, warnings }. */
export function loadSeats(p) {
  const raw = read(p), blocks = {}, warnings = [];
  for (const [key, block] of Object.entries(raw)) {
    if (key.startsWith('_')) continue;
    const t = targetOf(key);
    if (!t) { warnings.push(`${path.basename(p)}: top-level key "${key}" names no data file and is ignored`); continue; }
    if (!isObj(block)) { warnings.push(`${path.basename(p)}: "${key}" is not an object and is ignored`); continue; }
    blocks[t] = blocks[t] ? merge(blocks[t], block) : block;
  }
  return { path: p, blocks, warnings };
}

/** Objects by key, lists appended; a value under a nation's code replaces what was there. */
export function merge(base, add) {
  if (Array.isArray(base) && Array.isArray(add)) {
    const out = [...base];
    for (const row of add) {
      if (isObj(row) && 'id' in row) {
        const i = out.findIndex((x) => isObj(x) && x.id === row.id);
        if (i >= 0) out[i] = row; else out.push(row);
      } else if (!out.some((x) => JSON.stringify(x) === JSON.stringify(row))) out.push(row);
    }
    return out;
  }
  if (isObj(base) && isObj(add)) {
    const out = { ...base };
    for (const [k, v] of Object.entries(add)) out[k] = k in base && !CODE.test(k) ? merge(base[k], v) : v;
    return out;
  }
  return add;
}

/** The codes a seats file brings: a nation row, a door entry or a first-day corps row for it. */
export function claims(file) {
  const c = new Set();
  const w = file.blocks.world ?? {};
  for (const k of Object.keys(w.nations ?? {})) c.add(k);
  for (const row of w.corps ?? []) c.add(row[0]);
  for (const n of file.blocks.grand?.nations ?? []) c.add(n.id);
  return c;
}

/** A block without its rows for the given codes: keys that are one of them, list rows that begin with or name one. */
function strip(v, codes) {
  if (Array.isArray(v)) {
    return v.filter((row) => !(typeof row === 'string' && codes.has(row)) && !(Array.isArray(row) && codes.has(row[0])) && !(isObj(row) && (codes.has(row.id) || codes.has(row.nat)))).map((row) => (isObj(row) ? strip(row, codes) : row));
  }
  if (isObj(v)) return Object.fromEntries(Object.entries(v).filter(([k]) => !codes.has(k)).map(([k, x]) => [k, strip(x, codes)]));
  return v;
}

/**
 * The plan for one build: the placeholder (less what the seats files bring) and then each seats file in order.
 * Returns the layers by target, the seats (door entries the files bring, and the placeholder's that are left),
 * and the warnings met reading them.
 */
export function plan(paths, { placeholders = true } = {}) {
  const files = paths.map(loadSeats);
  const warnings = files.flatMap((f) => f.warnings);
  const claimed = new Set(files.flatMap((f) => [...claims(f)]));
  const layers = [];
  if (placeholders && fs.existsSync(PLACEHOLDER)) {
    const ph = loadSeats(PLACEHOLDER);
    // A capital the placeholder moves stays moved for a claimed code unless the seats file moves it itself.
    const keep = ph.blocks['trial-far']?.capitals;
    ph.blocks = strip(ph.blocks, claimed);
    if (keep) (ph.blocks['trial-far'] ??= {}).capitals = keep;
    layers.push({ ...ph, placeholder: true });
  }
  layers.push(...files);
  const seats = [];
  for (const l of layers) for (const n of l.blocks.grand?.nations ?? []) if (!seats.some((s) => s.id === n.id)) seats.push({ id: n.id, from: path.basename(l.path), placeholder: !!l.placeholder });
  return { layers, seats, claimed: [...claimed].sort(), warnings, files: files.map((f) => f.path) };
}

/** A data file with every layer's block for it laid over: `name` without ".json". */
export function overlay(name, base, layers) {
  let out = base;
  for (const l of layers) if (l.blocks[name]) out = merge(out, l.blocks[name]);
  return out;
}

/** Every data file the layers touch, merged: { name: object }. */
export function mergedData(layers, names = null) {
  const touched = new Set(layers.flatMap((l) => Object.keys(l.blocks)));
  for (const n of names ?? []) touched.add(n);
  const out = {};
  for (const n of touched) out[n] = overlay(n, read(path.join(DATA, `${n}.json`)), layers);
  return out;
}

/**
 * Checks the merged data against the chart. Errors stop the build: a row that the game would drop in silence, a
 * seat that could not be played. Warnings are printed and the build goes on.
 */
export function validate(p, chartFile = path.join(ROOT, 'src/generated/world-cities.json')) {
  const errors = [], warnings = [...p.warnings];
  const D = mergedData(p.layers, ['world', 'grand', 'traits', 'trial-far', 'nations', 'setup-1939', 'relations', 'diplomacy', 'government']);
  const W = D.world, T = D['trial-far'];
  const europe = read(path.join(ROOT, 'src/generated/campaign-map.json')), chart = read(chartFile);
  const regions = new Map([...europe.regions, ...chart.regions].map((r) => [r.name, r]));
  const seaKeys = new Set(chart.seas.zones.map((z) => z.id));
  const nations = new Set([...Object.keys(D.nations).filter((k) => !k.startsWith('_')), ...Object.keys(W.nations)]);
  const tied = (holder, nat) => holder === nat || W.follow[holder] === nat || W.follow[nat] === holder;
  const holder = (r) => (r.id >= chart.first ? T.held[r.name] ?? r.owner : r.owner);

  // Every owner on the chart is a country, and every country of the wider world takes its place in the order of play.
  for (const o of new Set(chart.regions.map((r) => r.owner))) if (!nations.has(o)) errors.push(`the chart has provinces owned by ${o}, which is no country: add a world.json nation row for it`);
  for (const id of Object.keys(W.nations)) {
    if (D.nations[id]) warnings.push(`${id} has a row in nations.json already: its world.json row is not read by attachWorld (the old row stands)`);
    else if (!W.order.includes(id)) errors.push(`${id} has a world.json nation row but is not in world.json "order": the country would never be created`);
  }
  for (const [id, of] of Object.entries(W.follow)) if (!nations.has(id) || !nations.has(of)) errors.push(`follow: ${id} follows ${of}, and one of them is no country`);
  for (const [id, part] of Object.entries(W.parts.of)) if (!W.parts.names[part]) errors.push(`parts: ${id} lies in "${part}", which has no name under parts.names`);

  // The rig's first-day table.
  for (const [id, name] of Object.entries(T.capitals)) {
    const r = regions.get(name);
    if (!r) errors.push(`setup.capitals: ${id}'s capital "${name}" is not a province`);
    else if (r.owner !== id) errors.push(`setup.capitals: "${name}" is ${r.owner}'s, not ${id}'s`);
  }
  for (const [name, by] of Object.entries(T.held)) {
    if (!regions.has(name)) errors.push(`setup.held: "${name}" is not a province`);
    else if (regions.get(name).id < chart.first) warnings.push(`setup.held: "${name}" is on the map of Europe, where the rig sets no holder`);
    if (!nations.has(by)) errors.push(`setup.held: "${name}" is held by ${by}, which is no country`);
  }
  for (const [a, b] of T.wars) for (const x of [a, b]) if (!nations.has(x)) errors.push(`setup.wars: ${x} is no country`);
  const firstDay = [...D['setup-1939'].corps.map((r) => ['setup-1939.json', r]), ...W.corps.map((r) => ['world.json', r])];
  for (const [nat, name] of T.drop) if (!firstDay.some(([, r]) => r[0] === nat && r[1] === name)) warnings.push(`setup.drop: no first-day row [${nat}, "${name}"] to drop`);
  const dropped = (r) => T.drop.some(([n, x]) => n === r[0] && x === r[1]);

  // First-day corps of the wider world: each must stand on ground its country holds that day, or the game leaves it out without a word.
  const placed = new Map();
  for (const [file, r] of firstDay) {
    if (dropped(r)) continue;
    const [nat, name, kind, divs, at] = r;
    if (file === 'world.json') {
      const where = `world.json corps [${nat}, "${name}"]`;
      if (!nations.has(nat)) { errors.push(`${where}: ${nat} is no country`); continue; }
      if (!KINDS.includes(kind)) errors.push(`${where}: kind "${kind}" is not one of ${KINDS.join(', ')}`);
      if (!(Number(divs) > 0)) errors.push(`${where}: ${divs} divisions`);
      const reg = regions.get(at);
      if (!reg) { errors.push(`${where}: "${at}" is not a province of the chart`); continue; }
      if (!tied(holder(reg), nat)) { errors.push(`${where}: "${at}" is held by ${holder(reg)} on the first day (owner ${reg.owner}), not by ${nat}: the game would leave the corps out`); continue; }
      const o = r[5];
      if (o && o.str !== undefined && !(o.str > 0 && o.str <= 1)) errors.push(`${where}: str ${o.str} is not between 0 and 1`);
    }
    placed.set(nat, (placed.get(nat) ?? 0) + 1);
  }
  const names = new Map();
  for (const [, r] of firstDay) { if (dropped(r)) continue; const k = `${r[0]}:${r[1]}`; names.set(k, (names.get(k) ?? 0) + 1); }
  for (const [k, n] of names) if (n > 1) warnings.push(`first-day corps: ${n} rows are named ${k}`);

  for (const f of W.fleets) {
    const where = `world.json fleets [${f.nat}, "${f.name}"]`;
    if (!nations.has(f.nat)) { errors.push(`${where}: ${f.nat} is no country`); continue; }
    const reg = regions.get(f.base);
    if (!reg) errors.push(`${where}: base "${f.base}" is not a province of the chart`);
    else {
      if (holder(reg) !== f.nat) errors.push(`${where}: base "${f.base}" is held by ${holder(reg)} on the first day, not by ${f.nat}: the game would leave the fleet out`);
      if (!reg.flags.port && !reg.flags.depot) warnings.push(`${where}: base "${f.base}" has no port`);
      const z = chart.seas.zones.findIndex((s) => s.id === f.home);
      if (z >= 0 && !(reg.seas ?? []).includes(z)) warnings.push(`${where}: base "${f.base}" does not stand on its home sea "${f.home}"`);
    }
    for (const k of ['home', 'zone']) if (!seaKeys.has(f[k])) errors.push(`${where}: ${k} "${f[k]}" is not a sea of the chart`);
  }

  // The seats: a door entry, a text that says what the player takes on, an army, a capital in its own hands.
  for (const s of p.seats) {
    const g = D.grand.nations.find((n) => n.id === s.id), where = `seat ${s.id} (${s.from})`;
    if (!nations.has(s.id)) { errors.push(`${where}: ${s.id} is no country`); continue; }
    const text = g?.years?.['1939'];
    if (typeof text !== 'string' || text.length < 41) errors.push(`${where}: the 1939 door text is ${typeof text === 'string' ? `${text.length} letters` : 'missing'}; it must be over 40`);
    if (Object.keys(g?.years ?? {}).some((y) => y !== '1939')) errors.push(`${where}: a small country is offered from September 1939 only (ruling 209(a)), and this entry has ${Object.keys(g.years).join(', ')}`);
    if (!g?.small) warnings.push(`${where}: the door entry is not marked small`);
    if (!placed.get(s.id)) errors.push(`${where}: no first-day corps: a country with no army has no seat (ruling 209(a))`);
    const cap = [...regions.values()].find((r) => r.owner === s.id && (T.capitals[s.id] ? r.name === T.capitals[s.id] : r.flags.cap));
    if (!cap) errors.push(`${where}: no capital: no province of ${s.id}'s carries the cap flag, and setup.capitals names none`);
    else if (holder(cap) !== s.id) errors.push(`${where}: its capital ${cap.name} is held by ${holder(cap)} on the first day`);
    const tr = D.traits.nations[s.id];
    if (s.placeholder) continue;
    // A seat may have no abilities row, or a row of one ability: the record decides (the historian struck those it
    // could not stand behind). Only a row that is not the game's shape is refused.
    if (tr && !Array.isArray(tr.traits)) errors.push(`${where}: its traits.json row has no list under "traits"`);
    if (!s.placeholder && !D.nations[s.id] && !W.parts.of[s.id]) warnings.push(`${where}: no part of the world under world.json parts.of: it would count as "${W.parts.new}" for the League's rule of neighbours`);
  }
  for (const gr of D.relations.groups) for (const n of gr.nations) if (!nations.has(n)) errors.push(`relations.json group "${gr.id}": ${n} is no country`);
  for (const [a, b] of D.relations.pairs) for (const x of [a, b]) if (!nations.has(x)) errors.push(`relations.json pairs: ${x} is no country`);
  for (const k of ['home', 'bandwagon', 'resist']) for (const id of Object.keys(D.diplomacy[k])) if (!id.startsWith('_') && !nations.has(id)) warnings.push(`diplomacy.json ${k}: ${id} is no country`);
  for (const [id, form] of Object.entries(D.government.nations)) if (!id.startsWith('_') && !D.government.forms[form]) errors.push(`government.json: ${id} has the form "${form}", which is not one of ${Object.keys(D.government.forms).join(', ')}`);
  return { errors, warnings, data: D };
}

/** The merged text of a data file for a loader: null when no layer touches it. */
export function mergedText(p, file) {
  const name = path.basename(file, '.json');
  if (path.dirname(path.resolve(file)) !== DATA) return null;
  if (!p.layers.some((l) => l.blocks[name])) return null;
  return JSON.stringify(overlay(name, read(file), p.layers));
}
