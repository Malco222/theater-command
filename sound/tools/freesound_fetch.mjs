// Fetches the public previews of chosen Freesound sounds, with each sound's title, author and licence read from its
// own page, on a machine Freesound will talk to (Brian's Mac; Freesound refuses the cloud workspace).
//
//   node sound/tools/freesound_fetch.mjs sound/recordings/freesound/wanted2.json [--gap 3]
//
// wanted2.json: { "sounds": [{ "id": 438733, "for": "torpedo_run_loop" }, ...],
//                 "packs":  [{ "user": "craigsmith", "pack": 24916, "for": "submarine", "keep": "dive|torpedo|..." }] }
// A pack is read page by page from its own listing; only sounds whose titles match `keep` are fetched.
// Only sound pages and pack pages are read, never /search/ (Freesound's robots.txt asks crawlers to keep off it), and
// robots.txt is read first and obeyed. One request at a time, --gap seconds apart, User-Agent sound-research-client/1.0.
// No sign-in: only the public high-quality preview (Ogg) is fetched, the same file Freesound's own player plays.
// Writes sound/recordings/freesound/<id>.ogg and adds a row a sound to found.json (same fields as before), and
// prints one line a sound.

import fs from 'node:fs';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
const dir = path.join(here, '../recordings/freesound');
const a = process.argv.slice(2);
if (!a[0]) { console.error('usage: node sound/tools/freesound_fetch.mjs <wanted.json> [--gap 3]'); process.exit(1); }
const gap = (a.includes('--gap') ? +a[a.indexOf('--gap') + 1] : 3) * 1000;
const AGENT = 'sound-research-client/1.0', SITE = 'https://freesound.org';
const nap = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
let last = 0;

function curl(url, out) {
  const wait = last + gap - Date.now(); if (wait > 0) nap(wait);
  last = Date.now();
  const args = ['-sS', '-L', '-m', '120', '-A', AGENT, '-w', '\n%{http_code} %{url_effective}', url];
  if (out) args.splice(0, 0, '-o', out);
  const r = spawnSync('curl', args, { encoding: 'utf8', maxBuffer: 1 << 27 });
  if (r.status !== 0) return { code: 0, body: '', why: (r.stderr || '').trim() };
  const cut = r.stdout.lastIndexOf('\n'), [code, eff] = r.stdout.slice(cut + 1).trim().split(' ');
  return { code: +code, body: r.stdout.slice(0, cut), url: eff };
}

// robots.txt, the part for every crawler.
const robots = curl(SITE + '/robots.txt');
if (robots.code !== 200) { console.log(`STOP: freesound.org answered ${robots.code || robots.why} for robots.txt`); process.exit(2); }
const banned = []; let mine = false;
for (const line of robots.body.split('\n')) {
  const [k, ...v] = line.split(':'), val = v.join(':').trim();
  if (/^user-agent$/i.test(k.trim())) mine = val === '*';
  else if (mine && /^disallow$/i.test(k.trim()) && val) banned.push(val);
}
const allowed = (p) => !banned.some((b) => p.startsWith(b));

const unescape = (s) => String(s ?? '').replace(/&amp;/g, '&').replace(/&quot;/g, '"').replace(/&#0?39;/g, "'").replace(/&lt;/g, '<').replace(/&gt;/g, '>');
const meta = (html, prop) => unescape((html.match(new RegExp(`<meta[^>]+(?:property|name)="${prop}"[^>]+content="([^"]*)"`, 'i')) || html.match(new RegExp(`<meta[^>]+content="([^"]*)"[^>]+(?:property|name)="${prop}"`, 'i')) || [])[1]);
function licenceOf(url) {
  const u = url.toLowerCase();
  if (u.includes('publicdomain/zero')) return 'CC0';
  if (u.includes('sampling+')) return 'Sampling+';
  const m = u.match(/licenses\/([a-z-]+)\/([\d.]+)/);
  return m ? 'CC ' + m[1].toUpperCase() + ' ' + m[2] : url;
}

const want = JSON.parse(fs.readFileSync(a[0], 'utf8'));
const jobs = [...(want.sounds ?? [])];

// Packs: read each page of the pack's own listing and keep the sounds whose titles match.
for (const p of want.packs ?? []) {
  const keep = new RegExp(p.keep, 'i');
  for (let page = 1; page <= 12; page++) {
    const rel = `/people/${p.user}/packs/${p.pack}/` + (page > 1 ? `?page=${page}` : '');
    if (!allowed(rel)) { console.log(`skipped ${rel}: robots.txt`); break; }
    const r = curl(SITE + rel);
    if (r.code !== 200) { if (page === 1) console.log(`pack ${p.pack}: answered ${r.code || r.why}`); break; }
    // Every link to one of the pack's sounds; its title from the link's text or title attribute when the page gives
    // one, else from the sound's own page later.
    const found = [];
    for (const m of r.body.matchAll(new RegExp(`<a[^>]*href="/people/${p.user}/sounds/(\\d+)/"([^>]*)>([^<]*)<`, 'g'))) {
      const id = +m[1], text = unescape(m[3]).trim() || unescape((m[2].match(/title="([^"]*)"/) || [])[1]).trim();
      const had = found.find((f) => f.id === id);
      if (!had) found.push({ id, title: text }); else if (!had.title && text) had.title = text;
    }
    const before = jobs.length;
    for (const f of found) {
      if (jobs.some((j) => j.id === f.id)) continue;
      if (f.title && !keep.test(f.title)) continue;
      jobs.push({ id: f.id, for: p.for, from_pack: p.pack, ...(f.title ? {} : { keep }) });
    }
    console.log(`pack ${p.pack} page ${page}: ${found.length} sounds, ${jobs.length - before} kept`);
    for (const f of found) console.log(`  ${f.id}\t${f.title || '(title on its own page)'}`);
    if (!new RegExp(`[?&]page=${page + 1}\\b`).test(r.body)) break;
  }
}

const foundFile = path.join(dir, 'found.json');
const rows = JSON.parse(fs.readFileSync(foundFile, 'utf8'));
let got = 0, bad = 0;
for (const j of jobs) {
  if (rows.some((r) => r.id === j.id) && fs.existsSync(path.join(dir, `${j.id}.ogg`))) { console.log(`${j.id}: already here`); continue; }
  const page = curl(`${SITE}/s/${j.id}/`);
  if (page.code !== 200) { console.log(`${j.id}: page answered ${page.code || page.why}`); bad++; continue; }
  const html = page.body;
  const title = meta(html, 'og:title') || (html.match(/<title>([^<]*)<\/title>/) || [])[1] || '';
  const audio = meta(html, 'og:audio') || (html.match(/https:\/\/cdn\.freesound\.org\/previews\/[^"']+?-(?:lq|hq)\.(?:mp3|ogg)/) || [])[0] || '';
  const licUrl = (html.match(/href="(https?:\/\/creativecommons\.org\/(?:licenses|publicdomain)\/[^"]+)"/) || [])[1] || '';
  const author = (page.url.match(/\/people\/([^/]+)\/sounds\//) || [])[1] || '';
  const about = meta(html, 'og:description') || meta(html, 'description');
  if (j.keep && !j.keep.test(title)) { console.log(`${j.id}: ${unescape(title)}: not wanted`); continue; }
  if (!audio || !licUrl) { console.log(`${j.id}: no preview or licence on its page`); bad++; continue; }
  const hq = audio.replace(/-lq\.mp3$/, '-hq.ogg').replace(/-hq\.mp3$/, '-hq.ogg');
  const file = path.join(dir, `${j.id}.ogg`);
  const f = curl(hq, file + '.part');
  if (f.code !== 200) { fs.rmSync(file + '.part', { force: true }); console.log(`${j.id}: preview answered ${f.code || f.why}`); bad++; continue; }
  fs.renameSync(file + '.part', file);
  const licence = licenceOf(licUrl);
  const probe = spawnSync('ffprobe', ['-v', 'error', '-show_entries', 'format=duration', '-of', 'csv=p=0', file], { encoding: 'utf8' });
  const row = {
    item: j.for, id: j.id, title: unescape(title).replace(/^Freesound - /, '').replace(/ by [^ ]+$/, ''), author: decodeURIComponent(author),
    licence, licence_url: licUrl, credit_owed: licence !== 'CC0', page: page.url, duration_s: +(+probe.stdout || 0).toFixed(2) || null,
    description: about.slice(0, 400), file: `${j.id}.ogg`, file_kind: 'hq preview (Ogg Vorbis)', bytes: fs.statSync(file).size,
    ...(j.from_pack ? { pack: j.from_pack } : {}),
  };
  rows.push(row); got++;
  fs.writeFileSync(foundFile, JSON.stringify(rows, null, 1));
  console.log(`${j.id}: ${row.title} | ${row.author} | ${licence} | ${row.duration_s ?? '?'} s`);
}
console.log(`DONE: ${got} fetched, ${bad} failed, ${jobs.length} asked for`);
