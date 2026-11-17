// Builds the listening board: one page on which every sound and scene can be played and stamped Good, Fix or Cut.
//
//   node sound/tools/make_board.mjs <sounds> <scenes> <mp3 of sounds> <mp3 of scenes> <browser-set> <out-folder>
//        [<sounds made from recordings> <their mp3s> [<battle pack> <its mp3s>]]
//
// With the next two, the board opens on the real recordings, each beside the old sound of the same name, and the
// first set is folded away below. With the last two, the battle pack follows them, grouped by who makes each sound.
//
// Writes <out>/listening-board.html (the page, with the list of sounds written into it), <out>/audio/*.mp3 (what it
// plays) and <out>/files.json (the audio paths, for publishing). The page is board/board.html with the data put in.

import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { isStereo, readWav } from '../kit/dsp.mjs';

const [src, scenesDir, mp3Dir, mp3Scenes, browserDir, out, realDir, realMp3, battleDir, battleMp3] = process.argv.slice(2);
if (!out) { console.error('usage: node sound/tools/make_board.mjs <sounds> <scenes> <mp3> <mp3-scenes> <browser-set> <out-folder>'); process.exit(1); }
const read = (f) => JSON.parse(fs.readFileSync(f, 'utf8'));
const list = read(path.join(src, 'sound-list.json')), scenes = read(path.join(scenesDir, 'scene-list.json'));
const mp3 = Object.fromEntries([...read(path.join(mp3Dir, 'mp3-list.json')), ...read(path.join(mp3Scenes, 'mp3-list.json'))].map((r) => [r.name, r]));
const inBrowser = new Set(read(path.join(browserDir, 'browser-sounds.json')).map((r) => r.name));

// A sound's shape: its loudest sample in each of n slices, against its own loudest, as one base-36 digit a slice.
function shape(file, n) {
  const x = readWav(file), a = isStereo(x) ? x.L : x, b = isStereo(x) ? x.R : null;
  const peaks = new Float32Array(n); let top = 1e-9;
  for (let i = 0; i < a.length; i++) { const v = Math.max(Math.abs(a[i]), b ? Math.abs(b[i]) : 0), k = Math.min(n - 1, Math.floor(i / a.length * n)); if (v > peaks[k]) peaks[k] = v; if (v > top) top = v; }
  return [...peaks].map((p) => Math.round(Math.sqrt(p / top) * 35).toString(36)).join('');
}

const FAMILIES = [
  ['air', 'Aircraft', 'Engines by kind, fly-pasts, a dive, a dogfight, a plane going down.'],
  ['flak', 'Flak and searchlights', 'The guns that defend a town, their shells bursting, the lamps.'],
  ['guns', 'Guns, bombs and battle', 'Artillery, shells, small arms, tank guns, bombs, the noise of a fight.'],
  ['ground', 'Armies', 'Tanks, lorries, half-tracks, boots, horses, digging, building, a factory.'],
  ['rail', 'Railways', 'A steam train from every side: going, stopping, turning round at the line’s end.'],
  ['sea', 'Ships', 'Sirens, engines, big guns, sonar, depth charges, torpedoes, a ship going down, landing craft.'],
  ['fire', 'Fire', 'One building alight, a town burning at night, a roof falling in.'],
  ['desk', 'The desk', 'Every button, card, order and page: the sounds of a staff officer’s desk.'],
  ['calls', 'Calls', 'The band’s short pieces for the great moments, and the sirens.'],
  ['beds', 'Ambience', 'Beds (long loops of pure texture) and sprinkles (single events dropped in at random).'],
  ['stings', 'Stings', 'Short mixes that are themselves sounds, for a card that reports something it cannot show.'],
];
const loopOf = (name) => (mp3[name]?.loop ? [mp3[name].loop.from, mp3[name].loop.length] : 0);
const sounds = list.map((s) => ({ n: s.name, f: s.family, s: s.seconds, lp: loopOf(s.name), st: s.channels === 2 ? 1 : 0, b: inBrowser.has(s.name) ? 1 : 0, a: s.about, w: shape(path.join(src, s.name + '.wav'), 56) }));
for (const s of scenes.filter((x) => x.sting)) sounds.push({ n: s.name, f: 'stings', s: s.seconds, lp: 0, st: 1, b: inBrowser.has(s.name) ? 1 : 0, a: s.about, w: shape(path.join(scenesDir, s.name + '.wav'), 56) });
const sceneRows = scenes.filter((x) => !x.sting).map((s) => ({ n: s.name, t: s.title, s: s.seconds, k: s.sounds.length, a: s.about, w: shape(path.join(scenesDir, s.name + '.wav'), 120) }));
const clip = (n) => ({ n, lp: loopOf(n) });
const tests = [
  { key: 'test_march_train', q: 'A march or a train?',
    a: { label: 'A march', hint: 'boots on a road', clips: ['marching', 'marching_loop'].map(clip) },
    b: { label: 'A train', hint: 'an army sent by rail', clips: ['rail_move', 'train_loop'].map(clip) } },
  { key: 'test_day_night', q: 'A raid by day or by night?',
    a: { label: 'A raid by day', hint: 'four engines or two', clips: ['raid_day_heavy', 'raid_day_medium'].map(clip) },
    b: { label: 'A raid by night', hint: 'four engines or two', clips: ['raid_night_medium', 'raid_night_heavy'].map(clip) } },
];
const have = new Set(sounds.map((s) => s.n));
for (const t of tests) for (const c of [...t.a.clips, ...t.b.clips]) if (!have.has(c.n)) throw new Error(`${c.n} is in a test but was not made`);

fs.mkdirSync(path.join(out, 'audio'), { recursive: true });
const files = [];
// The real recordings, each under its own name with "real_" before it, so the old sound of that name stays playable.
const real = [];
if (realDir) {
  const realList = read(path.join(realDir, 'sound-list.json')), realMp3List = Object.fromEntries(read(path.join(realMp3, 'mp3-list.json')).map((r) => [r.name, r]));
  for (const s of realList) {
    const lp = realMp3List[s.name]?.loop ? [realMp3List[s.name].loop.from, realMp3List[s.name].loop.length] : 0;
    const from = s.sources.map((x) => `${x.title.replace(/^File:/, '').replace(/\.[a-z0-9]+$/i, '')} (${x.licence}${x.credit ? ', credit ' + x.credit : ''})`).join('; ');
    real.push({ n: s.name, t: s.title ?? s.name, s: s.seconds, lp, a: s.about, src: from + '. Wikimedia Commons.', w: shape(path.join(realDir, s.name + '.wav'), 120), old: have.has(s.name) ? 1 : 0, oldlp: loopOf(s.name) });
    fs.copyFileSync(path.join(realMp3, s.name + '.mp3'), path.join(out, 'audio', `real_${s.name}.mp3`));
    files.push({ path: `audio/real_${s.name}.mp3` });
  }
}
// The battle pack, stamped as "battle_" and its name, grouped by who or what makes the sound.
const GROUPS = [['The rifleman and the machine gunner', /^(rifle_shot|lmg_burst|machine_gun)/], ['Explosions', /^(grenade|shell_|shells_|bomb_stick)/],
  ['The tank', /^tank_/], ['The sea', /^(ship_|sea_)/], ['The air', /^(fighter|plane_|dive|flak_light)/], ['Under a fight', /./]];
const battle = [];
if (battleDir) {
  const list = read(path.join(battleDir, 'sound-list.json')), mp3s = Object.fromEntries(read(path.join(battleMp3, 'mp3-list.json')).map((r) => [r.name, r]));
  for (const s of list) {
    const lp = mp3s[s.name]?.loop ? [mp3s[s.name].loop.from, mp3s[s.name].loop.length] : 0;
    const from = s.sources.map((x) => `${x.title.replace(/^File:/, '').replace(/\.[a-z0-9]+$/i, '')} (${x.licence}${x.credit ? ', credit ' + x.credit : ''})`).join('; ');
    const g = GROUPS.findIndex(([, re]) => re.test(s.name));
    battle.push({ n: s.name, t: s.title ?? s.name, s: s.seconds, lp, a: s.about, on: s.battle ?? '', g, src: from + '. Wikimedia Commons.', w: shape(path.join(battleDir, s.name + '.wav'), 120), old: have.has(s.name) ? 1 : 0, oldlp: loopOf(s.name) });
    fs.copyFileSync(path.join(battleMp3, s.name + '.mp3'), path.join(out, 'audio', `battle_${s.name}.mp3`));
    files.push({ path: `audio/battle_${s.name}.mp3` });
  }
  battle.sort((a, b) => a.g - b.g);
}
for (const name of [...sounds.map((s) => s.n), ...sceneRows.map((s) => s.n)]) {
  const from = [path.join(mp3Dir, name + '.mp3'), path.join(mp3Scenes, name + '.mp3')].find((f) => fs.existsSync(f));
  if (!from) throw new Error(`no MP3 for ${name}`);
  fs.copyFileSync(from, path.join(out, 'audio', name + '.mp3'));
  files.push({ path: `audio/${name}.mp3` });
}
const here = path.dirname(fileURLToPath(import.meta.url));
const data = JSON.stringify({ families: FAMILIES, sounds, scenes: sceneRows, tests, real, battle, groups: GROUPS.map(([t]) => t) }).replace(/</g, '\\u003c');
const page = fs.readFileSync(path.join(here, '../board/board.html'), 'utf8').replace('/*DATA*/null', () => data);
fs.writeFileSync(path.join(out, 'listening-board.html'), page);
fs.writeFileSync(path.join(out, 'files.json'), JSON.stringify(files));
const bytes = files.reduce((a, f) => a + fs.statSync(path.join(out, f.path)).size, 0);
console.log(`listening board: ${real.length} real recordings, ${battle.length} in the battle pack, ${sounds.length} sounds, ${sceneRows.length} scenes, ${files.length} audio files (${(bytes / 1048576).toFixed(1)} MB), page ${(page.length / 1024).toFixed(0)} KB`);
