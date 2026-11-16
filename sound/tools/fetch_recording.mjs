// Fetches one real recording from Wikimedia Commons and writes it into the licence log.
//
//   node sound/tools/fetch_recording.mjs "File:Name.ogg" --id short_name --for bomber [--raw <folder>] [--note "..."]
//
// It asks Commons for the file's licence first and refuses anything the game may not use (see find_commons.mjs for
// the rule). A film clip is kept only as its sound track, lifted out untouched. Every file fetched gets a row in
// sound/recordings/sources.json: where it came from, its licence, the credit it is owed, and a fingerprint of the
// file, so that the same file can be fetched again and proved to be the same.

import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { get, judge, AGENT } from './find_commons.mjs';
import { probe, FFMPEG } from '../kit/source.mjs';

const here = path.dirname(fileURLToPath(import.meta.url));
const LOG = path.join(here, '../recordings/sources.json');
const a = process.argv.slice(2);
const opt = (k, d = null) => (a.includes(k) ? a[a.indexOf(k) + 1] : d);
const title = a[0];
const id = opt('--id'), forSound = opt('--for', ''), raw = opt('--raw', process.env.SOUND_RAW || path.join(here, '../recordings/raw')), note = opt('--note', '');
if (!title || !id) { console.error('usage: node sound/tools/fetch_recording.mjs "File:Name.ogg" --id short_name --for bomber [--raw folder]'); process.exit(1); }

const strip = (html) => String(html ?? '').replace(/<[^>]*>/g, ' ').replace(/&amp;/g, '&').replace(/&quot;/g, '"').replace(/&#0?39;/g, "'").replace(/&nbsp;/g, ' ').replace(/\s+/g, ' ').trim();
// The file's details: from a saved search (--survey file.json, which already holds what Commons said) or asked afresh.
let page, v, m, verdict;
const survey = opt('--survey');
if (survey) {
  const row = JSON.parse(fs.readFileSync(survey, 'utf8')).find((r) => r.title === title);
  if (!row) { console.error(`${title}: not in ${survey}`); process.exit(1); }
  page = { title: row.title }; v = { url: row.url, descriptionurl: row.page, size: row.bytes, sha1: null };
  m = { Artist: { value: row.author }, ImageDescription: { value: row.about }, DateTimeOriginal: { value: row.date } };
  verdict = { ok: row.ok, licence: row.licence, credit: row.credit, why: row.why };
} else {
  const j = JSON.parse(get('https://commons.wikimedia.org/w/api.php?action=query&format=json&prop=videoinfo&viprop=url%7Csize%7Cmime%7Cextmetadata%7Csha1&titles=' + encodeURIComponent(title)));
  page = Object.values(j.query?.pages ?? {})[0]; v = page?.videoinfo?.[0];
  if (!v) { console.error(`${title}: Commons has no such file`); process.exit(1); }
  m = v.extmetadata ?? {}; verdict = judge(m);
}
if (!verdict.ok) { console.error(`${title}: REFUSED. Licence "${verdict.licence}": ${verdict.why}`); process.exit(2); }

fs.mkdirSync(raw, { recursive: true });
const ext = path.extname(new URL(v.url).pathname).toLowerCase() || '.bin';
const whole = path.join(raw, id + '.download' + ext);
const r = spawnSync('curl', ['-sS', '--fail', '-L', '-m', '1800', '--retry', '3', '--retry-delay', '20', '-A', AGENT, '-o', whole, v.url], { encoding: 'utf8' });
if (r.status !== 0) { console.error(`${title}: download failed: ${(r.stderr || '').trim()}`); process.exit(1); }
const sha1 = crypto.createHash('sha1').update(fs.readFileSync(whole)).digest('hex');
if (v.sha1 && sha1 !== v.sha1) { console.error(`${title}: the file fetched is not the file Commons describes (fingerprints differ)`); process.exit(1); }

// Keep the sound, not the film: the sound track is lifted out exactly as it was encoded (nothing re-encoded, so
// nothing lost and the file stays small). Only if that cannot be done is it stored as FLAC.
const info = probe(whole);
let kept = path.join(raw, id + ext), form = 'as fetched';
if (info.hasVideo) {
  kept = path.join(raw, id + '.mka'); form = `sound track of a film clip, lifted out untouched (${info.codec})`;
  let f = spawnSync(FFMPEG, ['-v', 'error', '-y', '-nostdin', '-i', whole, '-vn', '-map', '0:a:0', '-c:a', 'copy', kept], { encoding: 'utf8' });
  if (f.status !== 0) {
    kept = path.join(raw, id + '.flac'); form = `sound track of a film clip (${info.codec}), kept as FLAC`;
    f = spawnSync(FFMPEG, ['-v', 'error', '-y', '-nostdin', '-i', whole, '-vn', '-map', '0:a:0', '-c:a', 'flac', kept], { encoding: 'utf8' });
  }
  if (f.status !== 0) { console.error(`${title}: could not take the sound track: ${(f.stderr || '').trim()}`); process.exit(1); }
  fs.unlinkSync(whole);
} else fs.renameSync(whole, kept);

const row = {
  id, for: forSound, file: path.basename(kept), title: page.title, page: v.descriptionurl, url: v.url,
  licence: verdict.licence, credit: verdict.credit || '', author: strip(m.Artist?.value).slice(0, 160),
  about: strip(m.ImageDescription?.value).slice(0, 400), recorded: strip(m.DateTimeOriginal?.value).slice(0, 40),
  sha1, bytes: v.size, form, format: `${info.codec} ${info.rate} Hz ${info.channels} ch`, seconds: +info.seconds.toFixed(1),
  fetched: new Date().toISOString().slice(0, 10), note,
};
const log = fs.existsSync(LOG) ? JSON.parse(fs.readFileSync(LOG, 'utf8')) : [];
const at = log.findIndex((x) => x.id === id);
if (at >= 0) log[at] = row; else log.push(row);
fs.mkdirSync(path.dirname(LOG), { recursive: true });
fs.writeFileSync(LOG, JSON.stringify(log, null, 1) + '\n');
console.log(`${id}: ${row.title}\n  ${row.licence}${row.credit ? ', credit: ' + row.credit : ''}\n  ${row.format}, ${row.seconds} s, ${(v.size / 1e6).toFixed(1)} MB, ${form}\n  kept at ${kept}`);
