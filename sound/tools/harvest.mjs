// Gathers the real recordings a batch of sounds needs, on a machine that the archives will talk to.
//
//   node sound/tools/harvest.mjs <work-folder> [--only id,id] [--no-search] [--gap 20]
//
// Reads sound/recordings/wanted.json. For each recording wanted it: fetches the file from Wikimedia Commons with its
// licence checked (fetch_recording.mjs), measures it (vet.mjs), and copies its sound into the repository. A film
// clip's sound track is lifted out untouched, so nothing is lost and the files stay small. It also runs the searches
// listed there and saves what they find. It rests --gap seconds between downloads, so as not to lean on the archive. Whole downloads stay in <work-folder>/raw. What goes into the repository:
// sound/recordings/sources.json (the licence log), audio/* (the sound), vet/*.json (the measurements),
// search/*.json and harvest-report.md. Run it again at any time: files already fetched are not fetched twice.

import fs from 'node:fs';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { FFMPEG } from '../kit/source.mjs';
import { search } from './find_commons.mjs';

const here = path.dirname(fileURLToPath(import.meta.url));
const rec = path.join(here, '../recordings');
const a = process.argv.slice(2), work = a[0];
if (!work) { console.error('usage: node sound/tools/harvest.mjs <work-folder> [--only id,id] [--no-search]'); process.exit(1); }
const only = a.includes('--only') ? a[a.indexOf('--only') + 1].split(',') : null;
const gap = a.includes('--gap') ? +a[a.indexOf('--gap') + 1] : 20;
const nap = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
let fetched = 0;
const raw = path.join(work, 'raw');
for (const d of [raw, path.join(rec, 'vet'), path.join(rec, 'audio'), path.join(rec, 'search')]) fs.mkdirSync(d, { recursive: true });
const wanted = JSON.parse(fs.readFileSync(path.join(rec, 'wanted.json'), 'utf8'));
const node = (script, args) => spawnSync(process.execPath, [path.join(here, script), ...args], { encoding: 'utf8', maxBuffer: 1 << 28, env: { ...process.env, SOUND_RAW: raw } });
const report = ['# Harvest report', '', `Run ${new Date().toISOString()}`, '', '| id | for | licence | format | seconds | events | flags | MB |', '|---|---|---|---|---|---|---|---|'];
const log = () => (fs.existsSync(path.join(rec, 'sources.json')) ? JSON.parse(fs.readFileSync(path.join(rec, 'sources.json'), 'utf8')) : []);

for (const w of wanted.recordings) {
  if (only && !only.includes(w.id)) continue;
  let row = log().find((r) => r.id === w.id);
  if (!row || !fs.existsSync(path.join(raw, row.file))) {
    if (fetched++ && gap > 0) nap(gap * 1000);
    const f = node('fetch_recording.mjs', [w.title, '--id', w.id, '--for', w.for, '--raw', raw, '--note', w.note ?? '']);
    if (f.stderr && f.status === 0) process.stdout.write(f.stderr);
    if (f.status !== 0) { console.log(`FAILED ${w.id}: ${(f.stderr || f.stdout).trim().split('\n').pop()}`); report.push(`| ${w.id} | ${w.for} | NOT FETCHED: ${(f.stderr || '').trim().split('\n').pop().replace(/\|/g, '/')} | | | | | |`); continue; }
    row = log().find((r) => r.id === w.id);
  }
  const file = path.join(raw, row.file), vetFile = path.join(rec, 'vet', w.id + '.json');
  const v = node('vet.mjs', [file, '--json', vetFile]);
  if (v.status !== 0 || !fs.existsSync(vetFile)) { console.log(`FAILED to measure ${w.id}: ${(v.stderr || '').trim().split('\n').pop()}`); continue; }
  const m = JSON.parse(fs.readFileSync(vetFile, 'utf8'))[0];
  m.file = row.file; fs.writeFileSync(vetFile, JSON.stringify([m], null, 1));
  // The sound itself goes into the repository, small because it is still in the form the archive held it in.
  const kept = path.join(rec, 'audio', row.file);
  fs.copyFileSync(file, kept);
  const mb = fs.statSync(kept).size / 1e6;
  console.log(`${w.id}: ${row.licence}, ${m.format}, ${m.seconds} s, ${m.events} events, ${mb.toFixed(1)} MB${m.flags.length ? ', flags: ' + m.flags.map((f) => f.split(':')[0]).join(' ') : ''}`);
  report.push(`| ${w.id} | ${w.for} | ${row.licence}${row.credit ? ' (credit: ' + row.credit.slice(0, 50).replace(/\|/g, '/') + ')' : ''} | ${m.format} | ${m.seconds} | ${m.events} | ${m.flags.map((f) => f.split(':')[0]).join(', ') || 'none'} | ${mb.toFixed(1)} |`);
}

if (!a.includes('--no-search') && !only) {
  report.push('', '## Searches', '');
  for (const s of wanted.searches ?? []) {
    try {
      const rows = search(s.words, { video: !!s.video, limit: s.limit ?? 50 });
      fs.writeFileSync(path.join(rec, 'search', s.slug + '.json'), JSON.stringify(rows.map((r) => ({ title: r.title, ok: r.ok, licence: r.licence, credit: r.credit, why: r.why, seconds: r.seconds, bytes: r.bytes, author: r.author, about: r.about, date: r.date, page: r.page })), null, 1));
      console.log(`search "${s.words}"${s.video ? ' (film)' : ''}: ${rows.length} found, ${rows.filter((r) => r.ok).length} usable`);
      report.push(`- "${s.words}"${s.video ? ' (film)' : ''}: ${rows.length} found, ${rows.filter((r) => r.ok).length} a game may use`);
    } catch (e) { console.log(`search "${s.words}" failed: ${e.message}`); report.push(`- "${s.words}": FAILED (${e.message})`); }
  }
}
fs.writeFileSync(path.join(rec, 'harvest-report.md'), report.join('\n') + '\n');
const bytes = fs.readdirSync(path.join(rec, 'audio')).reduce((n, f) => n + fs.statSync(path.join(rec, 'audio', f)).size, 0);
console.log(`done: sound/recordings/audio holds ${(bytes / 1e6).toFixed(1)} MB`);
