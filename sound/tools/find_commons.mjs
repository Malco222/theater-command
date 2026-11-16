// Searches Wikimedia Commons for real recordings and says, for each, whether its licence lets a game use it.
//
//   node sound/tools/find_commons.mjs "<words>" [--video] [--limit 50] [--json out.json] [--all]
//
// Audio files by default; --video searches film clips instead (many are US military clips in the public domain,
// with the real sound of the thing filmed). Only files a game may use are listed unless --all is given.
// The rule (design director, ruling 144): public domain or CC0, or a licence that allows a game that may one day be
// sold and that the game can honour with a credit. Never non-commercial, no-derivatives or share-alike.

import fs from 'node:fs';
import { spawnSync } from 'node:child_process';

export const AGENT = 'sound-research-client/1.0';

const pause = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
let lastCall = 0;

/**
 * Fetches a URL with curl (which knows the workspace's proxy) and returns the body as text. Calls are spaced out,
 * and when the archive says "too many requests" this waits as long as it asks before trying again.
 */
export function get(url, { gap = 3000, tries = 5 } = {}) {
  for (let t = 1; ; t++) {
    const wait = lastCall + gap - Date.now(); if (wait > 0) pause(wait);
    lastCall = Date.now();
    const r = spawnSync('curl', ['-sS', '-L', '-m', '60', '-A', AGENT, '-w', '\n%{http_code} %header{retry-after}', url], { encoding: 'utf8', maxBuffer: 1 << 28 });
    if (r.status !== 0) throw new Error(`could not fetch ${url}: ${(r.stderr || '').trim()}`);
    const cut = r.stdout.lastIndexOf('\n'), [code, after] = r.stdout.slice(cut + 1).trim().split(' ');
    if (code === '200') return r.stdout.slice(0, cut);
    if (code === '429' && t < tries) { pause((Math.min(120, +after || 30) + 2) * 1000); continue; }
    throw new Error(`could not fetch ${url}: the archive answered ${code}`);
  }
}

const strip = (html) => String(html ?? '').replace(/<[^>]*>/g, ' ').replace(/&amp;/g, '&').replace(/&quot;/g, '"').replace(/&#0?39;/g, "'").replace(/&nbsp;/g, ' ').replace(/\s+/g, ' ').trim();

/** Reads a licence name and says whether the game may use the file, and what it owes. */
export function judge(meta) {
  const short = strip(meta.LicenseShortName?.value), code = String(meta.License?.value ?? '').toLowerCase();
  const s = short.toLowerCase();
  const artist = strip(meta.Artist?.value), credit = strip(meta.Credit?.value);
  if (/\bnc\b|non-?commercial|\bnd\b|no-?deriv/.test(s) || /-nc|-nd/.test(code)) return { ok: false, licence: short, why: 'non-commercial or no-derivatives' };
  if (/\bsa\b|share-?alike|gfdl|gnu|\bgpl/.test(s) || /-sa|gfdl|gpl/.test(code)) return { ok: false, licence: short, why: 'share-alike: the game could not honour it' };
  if (/^cc0|cc-zero|cc0/.test(code) || /cc0/.test(s)) return { ok: true, licence: 'CC0', credit: '' };
  if (/public domain|^pd\b|pd-/.test(s) || /^pd/.test(code)) return { ok: true, licence: short || 'Public domain', credit: '' };
  if (/^cc by \d|^cc-by-\d|attribution/.test(s) || /^cc-by-\d/.test(code)) return { ok: true, licence: short, credit: artist || credit || 'credit owed: author not stated' };
  return { ok: false, licence: short || '(none stated)', why: 'licence not recognised: read the file page' };
}

/** One search of Commons. Returns rows: title, page, url, mime, bytes, seconds, licence verdict, author, description. */
export function search(words, { video = false, limit = 50 } = {}) {
  const q = `filetype:${video ? 'video' : 'audio'} ${words}`;
  const url = 'https://commons.wikimedia.org/w/api.php?action=query&format=json&generator=search&gsrnamespace=6'
    + `&gsrlimit=${limit}&gsrsearch=${encodeURIComponent(q)}`
    + '&prop=videoinfo&viprop=url%7Csize%7Cmime%7Cextmetadata';
  const j = JSON.parse(get(url));
  const rows = [];
  for (const p of Object.values(j.query?.pages ?? {})) {
    const v = p.videoinfo?.[0]; if (!v) continue;
    const m = v.extmetadata ?? {};
    rows.push({
      rank: p.index, title: p.title, page: v.descriptionurl, url: v.url, mime: v.mime, bytes: v.size, seconds: +(v.duration ?? 0),
      ...judge(m), author: strip(m.Artist?.value).slice(0, 80), about: strip(m.ImageDescription?.value).slice(0, 240),
      date: strip(m.DateTimeOriginal?.value).slice(0, 30),
    });
  }
  return rows.sort((a, b) => a.rank - b.rank);
}

if (import.meta.url === `file://${process.argv[1]}`) {
  const a = process.argv.slice(2);
  const words = a.find((x) => !x.startsWith('--') && a[a.indexOf(x) - 1] !== '--limit' && a[a.indexOf(x) - 1] !== '--json');
  const opt = (k) => (a.includes(k) ? a[a.indexOf(k) + 1] : null);
  if (!words) { console.error('usage: node sound/tools/find_commons.mjs "<words>" [--video] [--limit 50] [--json out.json] [--all]'); process.exit(1); }
  const rows = search(words, { video: a.includes('--video'), limit: +(opt('--limit') ?? 50) });
  const shown = a.includes('--all') ? rows : rows.filter((r) => r.ok);
  console.log(`"${words}"${a.includes('--video') ? ' (film clips)' : ''}: ${rows.length} found, ${rows.filter((r) => r.ok).length} a game may use`);
  for (const r of shown) {
    console.log(`${r.ok ? ' ' : 'x'} ${r.title}`);
    console.log(`    ${r.licence}${r.ok ? (r.credit ? ' (credit: ' + r.credit.slice(0, 60) + ')' : '') : ' [' + r.why + ']'} | ${r.seconds.toFixed(0)} s | ${(r.bytes / 1e6).toFixed(1)} MB | ${r.mime} | ${r.author}`);
    if (r.about) console.log(`    ${r.about}`);
  }
  if (opt('--json')) fs.writeFileSync(opt('--json'), JSON.stringify(rows, null, 1));
}
