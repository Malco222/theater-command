// Cuts the cities of data/campaign/cities.mjs out of the provinces round them and writes the chart with the cities
// on it. Called by build-geo-far.mjs, which hands over the chart it has just laid and the grids it laid it on.
//
// Nothing of that chart is touched: world-next.json is written as it was handed over, and the chart with the cities
// goes beside it as world-cities.json, in the same form and holding all of it. A city's row carries 'of', the number
// of the province it was cut from. A province that was cut keeps its number, name, seed and owner and the borders it
// had with the world outside it; one that was not cut is written as it stands, but for a neighbour that is now a
// city.
import fs from 'node:fs';
import polygonClipping from 'polygon-clipping';
import { ringArea } from './geo-common.mjs';
import { CELL } from './geo-seas.mjs';
import { CITY_GROUPS, CITY_SHORES, CITY_LINKS, CITY_STRAITS, CITY_GIVE } from '../data/campaign/cities.mjs';
import { cutLakes } from './cut-lakes.mjs';
import { RULED_GROUND } from '../data/campaign/detail.mjs';

export function carveCities(chart) {
  if (!CITY_GROUPS.length) return;
  const { core, today, regions, first, tables, parentRows, nowRows, fromTable, tabledSoFar, warn, proj, W, gx0, COLS, ROWS, inPoly, boxOf, closed, r1, trim, fillRound, regionAt, zoneAt, STEPS, BORDER, joins, links, zones, index, stages, out, arg, quiet } = chart;
  const SMALL = 400;
  const base = [...today.regions, ...regions];
  const byId = new Map(base.map((r) => [r.id, r]));
  const baseByName = new Map(base.map((r) => [r.name, r]));
  const names = new Set([...core.regions, ...base].map((r) => r.name));
  const rows = new Map(base.map((r) => [r.id, { ...r, flags: { ...r.flags }, nb: [...r.nb], seas: [...r.seas] }]));
  const kids = new Map(), parentOf = new Map(), latLon = new Map();
  const cities = [];
  const tabledBefore = tabledSoFar();
  let parentTabled = 0;
  // A province of the far side whose row the clerk has changed since the chart was handed over reads the row as
  // it is now (never its owner): the chart as handed over stands, this one follows the tables.
  const changedSince = [];
  for (const r of regions) if (nowRows.has(r.name)) {
    const row = rows.get(r.id), t = fromTable(r.name, row.owner, row.terrain, row.vp, row.fac, row.flags, nowRows);
    Object.assign(row, { terrain: t.terrain, vp: t.vp, fac: t.fac, flags: t.flags });
    changedSince.push(r.name);
  }
  for (const [parentName, list, change] of CITY_GROUPS) {
    const p = baseByName.get(parentName);
    if (!p) { warn(`! no province called ${parentName} to cut a city from`); continue; }
    const row = rows.get(p.id);
    if (change && p.id >= first) warn(`! ${parentName}: the tables say what a province of the far side is worth, not 'change'`);
    else if (change) for (const [k, v] of Object.entries(change)) { if (k === 'vp' || k === 'fac' || k === 'terrain') row[k] = v; else if (v) row.flags[k] = v; else delete row.flags[k]; }
    if (p.id < first && tables.has(p.name)) Object.assign(row, fromTable(p.name, row.owner, row.terrain, row.vp, row.fac, row.flags));
    // A province of the far side as it stands once it is cut: the clerk's row in cities-parents.csv (never its owner).
    if (p.id >= first && parentRows.has(p.name)) { const t = fromTable(p.name, row.owner, row.terrain, row.vp, row.fac, row.flags, parentRows); Object.assign(row, { terrain: t.terrain, vp: t.vp, fac: t.fac, flags: t.flags }); parentTabled++; }
    // A province of today's chart has no row in the tables: the clerk's row for it gives only what it is worth
    // once cut (victory points and factories; a blank stands for the map as it is).
    if (p.id < first && parentRows.has(p.name)) { const r = parentRows.get(p.name); if (r.vp !== '' && r.vp !== undefined) row.vp = Number(r.vp); if (r.factories !== '' && r.factories !== undefined) row.fac = Number(r.factories); parentTabled++; }
    for (const [name, lat, lon, terrain0, vp0, fac0, flags0] of list) {
      if (names.has(name)) warn(`! two provinces are called ${name}`);
      names.add(name);
      const [x, y] = proj([lon, lat]);
      if (!p.polys.some((poly) => inPoly(poly, x, y) || inPoly(poly, x + W, y))) {
        const at = base.find((r) => r.polys.some((poly) => inPoly(poly, x, y)));
        warn(`! ${name} does not stand in ${parentName}${at ? ` but in ${at.name}` : ', nor in any province'}`);
      }
      const t = fromTable(name, p.owner, terrain0, vp0, fac0, flags0 ?? {});
      const city = { id: first + regions.length + cities.length, name, owner: t.owner, terrain: t.terrain, vp: t.vp, fac: t.fac, flags: t.flags, x: r1(x), y: r1(y), polys: [], nb: [], seas: [], of: p.id };
      cities.push(city);
      latLon.set(city.id, [lat, lon]);
      parentOf.set(city.id, p.id);
      (kids.get(p.id) ?? kids.set(p.id, []).get(p.id)).push(city);
    }
  }
  // A province of today's chart that was not cut may have such a row too: a change made by ruling, not by a cut.
  let worthOnly = 0;
  for (const r of base) if (r.id < first && !kids.has(r.id) && parentRows.has(r.name)) {
    const row = rows.get(r.id), t = parentRows.get(r.name);
    if (t.vp !== '' && t.vp !== undefined) row.vp = Number(t.vp);
    if (t.factories !== '' && t.factories !== undefined) row.fac = Number(t.factories);
    changedSince.push(r.name);
    worthOnly++;
  }
  const cityTabled = tabledSoFar() - tabledBefore;
  for (const c of cities) rows.set(c.id, c);
  // Ground laid by a ruling stands over the seed and the tables, on this chart only.
  const ruled = [];
  {
    const named = new Map([...rows.values()].map((r) => [r.name, r]));
    for (const [name, g] of Object.entries(RULED_GROUND)) {
      const row = named.get(name);
      if (!row) { warn(`! no province called ${name} to lay ruled ground on`); continue; }
      if (row.terrain !== g) { row.terrain = g; ruled.push(name); }
    }
  }
  const sitesOf = (pid) => [rows.get(pid), ...kids.get(pid)];
  const piecesOf = (id) => (kids.has(id) ? sitesOf(id).map((s) => s.id) : [id]);
  const isPiece = (id) => kids.has(id) || parentOf.has(id);
  // (The same two questions for the loops over the whole grid: 1 a cut province, 2 a city.)
  const kind = new Uint8Array(first + regions.length + cities.length);
  for (const pid of kids.keys()) kind[pid] = 1;
  for (const c of cities) kind[c.id] = 2;

  // The ground nearer one seed than any of the others, as a shape.
  const cellFor = (sites, i, box) => {
    let poly = [[box[0], box[1]], [box[2], box[1]], [box[2], box[3]], [box[0], box[3]]];
    const s = sites[i];
    for (let j = 0; j < sites.length && poly.length; j++) {
      if (j === i) continue;
      const t = sites[j];
      const nx = t.x - s.x, ny = t.y - s.y, c = ((t.x + s.x) / 2) * nx + ((t.y + s.y) / 2) * ny;
      const kept = [];
      for (let k = 0; k < poly.length; k++) {
        const a = poly[k], b = poly[(k + 1) % poly.length];
        const da = nx * a[0] + ny * a[1] - c, db = nx * b[0] + ny * b[1] - c;
        if (da <= 0) kept.push(a);
        if ((da < 0 && db > 0) || (da > 0 && db < 0)) { const u = da / (da - db); kept.push([a[0] + u * (b[0] - a[0]), a[1] + u * (b[1] - a[1])]); }
      }
      poly = kept;
    }
    return poly.length >= 3 ? [[...poly, poly[0]]] : null;
  };
  const sizeOf = (poly) => { const c = closed(poly); return Math.abs(ringArea(c[0])) - c.slice(1).reduce((u, h) => u + Math.abs(ringArea(h)), 0); };
  const scratch = new Uint8Array(COLS * ROWS);
  const cellsOf = (poly) => {
    fillRound(scratch, closed(poly), 1);
    const [a, b, c, d] = boxOf(poly[0]);
    const cells = [];
    for (let row = Math.max(0, Math.floor(b / CELL) - 1); row <= Math.min(ROWS - 1, Math.ceil(d / CELL) + 1); row++) {
      for (let col = Math.floor((a - gx0) / CELL) - 1; col <= Math.ceil((c - gx0) / CELL) + 1; col++) {
        const j = row * COLS + ((col % COLS) + COLS) % COLS;
        if (scratch[j]) { cells.push(j); scratch[j] = 0; }
      }
    }
    return cells;
  };

  // Each cut province is shared out among its own seed and its cities' seeds.
  const pieceAt = new Int16Array(COLS * ROWS).fill(-1);
  const moved2 = [], lost = [];
  for (const [pid, list] of kids) {
    const p = byId.get(pid), sites = sitesOf(pid);
    let box = [1e9, 1e9, -1e9, -1e9];
    for (const poly of p.polys) { const b = boxOf(poly[0]); box = [Math.min(box[0], b[0]), Math.min(box[1], b[1]), Math.max(box[2], b[2]), Math.max(box[3], b[3])]; }
    box = [box[0] - 50, box[1] - 50, box[2] + 50, box[3] + 50];
    const cells = sites.map((_, i) => cellFor(sites, i, box));
    const got = sites.map(() => []);
    for (const poly of p.polys) {
      if (sizeOf(poly) < SMALL) {
        // A small island goes whole to the nearest of them.
        const n = poly[0].length, cx = poly[0].reduce((t, q) => t + q[0], 0) / n, cy = poly[0].reduce((t, q) => t + q[1], 0) / n;
        let best = 0, bd = 1e18;
        sites.forEach((s, i) => { const d = (s.x - cx) ** 2 + (s.y - cy) ** 2; if (d < bd) { bd = d; best = i; } });
        got[best].push(poly);
        continue;
      }
      sites.forEach((s, i) => {
        if (!cells[i]) return;
        let parts = [];
        try { parts = polygonClipping.intersection(cells[i], closed(poly)); } catch (e) { warn(`! could not cut ${s.name} out of ${p.name}: ${e.message}`); }
        for (const part of parts) {
          const rings = part.filter((r) => Math.abs(ringArea(r)) > 1.5).map((r) => trim(r.slice(0, -1).map(([x, y]) => [r1(x), r1(y)]), 1));
          if (rings.length && rings[0].length >= 3) got[i].push(rings);
        }
      });
    }
    // A share can come out in two pieces where the parent's outline bends. A piece that does not touch its own
    // seed's ground goes to the sister it has the longest border with, as on the far side itself.
    const main = got.map((polys, i) => {
      let k = polys.findIndex((q) => inPoly(q, sites[i].x, sites[i].y));
      if (k < 0) polys.forEach((q, j) => { if (k < 0 || sizeOf(q) > sizeOf(polys[k])) k = j; });
      return k;
    });
    got.forEach((polys, i) => { if (main[i] >= 0) for (const j of cellsOf(polys[main[i]])) pieceAt[j] = sites[i].id; });
    const final = got.map((polys, i) => (main[i] >= 0 ? [polys[main[i]]] : []));
    got.forEach((polys, i) => {
      polys.forEach((q, k) => {
        if (k === main[i]) return;
        const mine = new Set(cellsOf(q));
        const touch3 = new Map();
        for (const j of mine) {
          const c = j % COLS;
          for (const n of [j - c + (c + 1) % COLS, j - c + (c + COLS - 1) % COLS, j + COLS, j - COLS]) {
            if (n < 0 || n >= pieceAt.length || mine.has(n)) continue;
            const o = pieceAt[n];
            if (o >= 0 && (o === pid || parentOf.get(o) === pid)) touch3.set(o, (touch3.get(o) ?? 0) + 1);
          }
        }
        const best = [...touch3].filter(([o]) => o !== sites[i].id).sort((a, b) => b[1] - a[1] || a[0] - b[0])[0];
        if (touch3.has(sites[i].id) || !best || best[1] < 2) { final[i].push(q); return; }
        final[sites.findIndex((s) => s.id === best[0])].push(q);
        moved2.push(`${Math.round(sizeOf(q))} of ${sites[i].name} to ${rows.get(best[0]).name}`);
      });
    });
    sites.forEach((s, i) => {
      s.polys = final[i];
      if (!final[i].length) lost.push(s.name);
      for (const q of final[i]) for (const j of cellsOf(q)) pieceAt[j] = s.id;
    });
  }

  // The grid with the cities on it. A square of a cut province that no piece's outline covers (the rounding of a
  // cut) goes to the nearest seed.
  const regionAt2 = Int16Array.from(regionAt);
  let strays = 0, cut2 = 0;
  (() => {
    for (let k = 0; k < regionAt2.length; k++) {
      const pid = regionAt[k];
      if (pid < 0 || kind[pid] !== 1) continue;
      cut2++;
      const v = pieceAt[k];
      if (v >= 0 && (v === pid || parentOf.get(v) === pid)) { regionAt2[k] = v; continue; }
      strays++;
      const c = k % COLS, r = (k - c) / COLS, x = gx0 + (c + 0.5) * CELL, y = (r + 0.5) * CELL;
      let bd = 1e18;
      for (const s of sitesOf(pid)) { let dx = Math.abs(s.x - x); if (dx > W / 2) dx = W - dx; const d = dx * dx + (s.y - y) ** 2; if (d < bd) { bd = d; regionAt2[k] = s.id; } }
    }
  })();

  // Borders. Two pieces of one province are neighbours where their ground meets, as everywhere. A border the
  // parent had with a province outside it passes to whichever of the pieces lie along it; a crossing over a strait
  // passes to the piece nearest the other shore.
  const key = (a, b) => (a < b ? a * 10000 + b : b * 10000 + a);
  const meet2 = new Map();
  const tally2 = (a, b) => { if (b < 0 || a === b || !(kind[a] || kind[b])) return; const k = key(a, b); meet2.set(k, (meet2.get(k) ?? 0) + 1); };
  (() => {
    for (let r = 0; r < ROWS; r++) {
      for (let c = 0; c < COLS; c++) {
        const a = regionAt2[r * COLS + c];
        if (a < 0) continue;
        tally2(a, regionAt2[r * COLS + (c + 1) % COLS]);
        if (r + 1 < ROWS) tally2(a, regionAt2[(r + 1) * COLS + c]);
      }
    }
  })();
  const coreById = new Map(core.regions.map((r) => [r.id, r]));
  const shapeOf = (id) => rows.get(id) ?? coreById.get(id);
  const gap = (a, b) => {
    let best = 1e18;
    for (const pa of shapeOf(a).polys) for (const [x, y] of pa[0]) for (const pb of shapeOf(b).polys) for (const [u, v] of pb[0]) { const d = (x - u) ** 2 + (y - v) ** 2; if (d < best) best = d; }
    return best;
  };
  const oldNb = (id) => { const s = new Set(byId.get(id).nb); for (const [a, b] of joins) { if (a === id) s.add(b); else if (b === id) s.add(a); } return s; };
  const allLinks = [...today.links, ...links];
  const isLink = (a, b) => allLinks.some(([x, y]) => (x === a && y === b) || (x === b && y === a));
  const cityByName = new Map([...rows.values()].map((r) => [r.name, r]));
  const adj = new Set(), done = new Set(), linkNow = new Map();
  let passed = 0, thin = 0;
  for (const pid of kids.keys()) {
    const mine = piecesOf(pid);
    for (let i = 0; i < mine.length; i++) for (let j = i + 1; j < mine.length; j++) if ((meet2.get(key(mine[i], mine[j])) ?? 0) >= BORDER) adj.add(key(mine[i], mine[j]));
    for (const q of oldNb(pid)) {
      const k0 = key(pid, q);
      if (done.has(k0)) continue;
      done.add(k0);
      const pairs = [];
      for (const a of mine) for (const b of piecesOf(q)) pairs.push([a, b, meet2.get(key(a, b)) ?? 0]);
      let take = isLink(pid, q) ? [] : pairs.filter((x) => x[2] >= BORDER);
      if (!take.length) {
        const named = CITY_LINKS.map(([a, b]) => [cityByName.get(a)?.id, cityByName.get(b)?.id]).find(([a, b]) => pairs.some((x) => (x[0] === a && x[1] === b) || (x[0] === b && x[1] === a)));
        if (named) take = [pairs.find((x) => (x[0] === named[0] && x[1] === named[1]) || (x[0] === named[1] && x[1] === named[0]))];
        else if (!isLink(pid, q) && pairs.some((x) => x[2] > 0)) { take = [pairs.slice().sort((x, y) => y[2] - x[2] || x[0] - y[0] || x[1] - y[1])[0]]; thin++; }
        else take = [pairs.map((x) => [x[0], x[1], gap(x[0], x[1])]).sort((x, y) => x[2] - y[2] || x[0] - y[0] || x[1] - y[1])[0]];
      }
      if (isLink(pid, q)) linkNow.set(k0, [take[0][0], take[0][1]]);
      for (const [a, b] of take) adj.add(key(a, b));
      passed++;
    }
  }
  const family = (id) => (id < today.first ? 0 : (parentOf.get(id) ?? id) < first ? 1 : 2);
  for (const pid of kids.keys()) {
    const row = rows.get(pid);
    for (const q of row.nb) { const other = rows.get(q); other.nb = other.nb.filter((i) => i !== pid); }
    row.nb = [];
  }
  const joins2 = joins.filter(([a, b]) => !kids.has(a) && !kids.has(b));
  for (const k of [...adj].sort((p, q) => p - q)) {
    const a = Math.floor(k / 10000), b = k % 10000, fa = family(a), fb = family(b);
    if (fa === fb) { rows.get(a).nb.push(b); rows.get(b).nb.push(a); } else joins2.push(fa < fb ? [a, b] : [b, a]);
  }
  joins2.sort((p, q) => p[0] - q[0] || p[1] - q[1]);
  for (const r of rows.values()) r.nb.sort((p, q) => p - q);
  const links2 = allLinks.map(([a, b]) => {
    const now = linkNow.get(key(a, b));
    if (!now) return [a, b];
    const fa = now.find((id) => id === a || parentOf.get(id) === a), fb = now.find((id) => id === b || parentOf.get(id) === b);
    return [fa, fb];
  });
  // Straits laid on this chart: the two become neighbours across the water, and the border is a strait.
  const laidStraits = [];
  {
    const named = new Map([...rows.values()].map((r) => [r.name, r]));
    for (const [an, bn] of CITY_STRAITS) {
      const a = named.get(an), b = named.get(bn);
      if (!a || !b) { warn(`! no province called ${!a ? an : bn} for a strait`); continue; }
      if (a.nb.includes(b.id) || links2.some(([p, q]) => (p === a.id && q === b.id) || (p === b.id && q === a.id))) { warn(`! ${an} and ${bn} touch already: no strait laid`); continue; }
      if (family(a.id) === family(b.id)) { a.nb.push(b.id); b.nb.push(a.id); a.nb.sort((p, q) => p - q); b.nb.sort((p, q) => p - q); }
      else joins2.push(family(a.id) < family(b.id) ? [a.id, b.id] : [b.id, a.id]);
      links2.push([a.id, b.id]);
      laidStraits.push(`${an} - ${bn}`);
    }
  }

  // Coasts. A piece stands on a sea its parent stood on where that sea's water laps against it, by the same count;
  // every coast the parent had stays with one piece at least.
  const touch2 = new Map();
  (() => {
    for (let r = 0; r < ROWS; r++) {
      for (let c = 0; c < COLS; c++) {
        const zone = zoneAt[r * COLS + c];
        if (zone < 0) continue;
        for (let k = 0; k < STEPS.length; k++) {
          const rr = r + STEPS[k][1];
          if (rr < 0 || rr >= ROWS) continue;
          const id = regionAt2[rr * COLS + (c + STEPS[k][0] + COLS) % COLS];
          if (id < 0 || !kind[id]) continue;
          let m = touch2.get(id);
          if (!m) touch2.set(id, (m = new Map()));
          m.set(zone, (m.get(zone) ?? 0) + 1);
        }
      }
    }
  })();
  const zones2 = zones.map(({ i, ...z }) => ({ ...z, adj: [...z.adj], coast: z.coast.filter((id) => !kids.has(id)) }));
  const dry = [], portsPassed = [];
  for (const pid of kids.keys()) {
    const p = byId.get(pid), own = new Set(p.seas);
    const allowed = new Set(zones.filter((z) => z.coast.includes(pid)).map((z) => z.i));
    const pieces = sitesOf(pid), seasOf = new Map();
    for (const x of pieces) {
      const m = touch2.get(x.id) ?? new Map();
      const ranked = [...m].filter(([zone]) => allowed.has(zone)).sort((a, b) => b[1] - a[1] || a[0] - b[0]);
      const most = ranked.length ? ranked[0][1] : 0;
      let seas = [];
      const given2 = CITY_SHORES[x.name];
      if (given2) { for (const k of given2) { const zone = index.get(k); if (zone === undefined || !m.has(zone) || !allowed.has(zone)) warn(`! ${x.name} is given a shore on ${k}, which does not lap against it`); else seas.push(zone); } }
      else {
        seas = ranked.filter(([, n]) => n >= 8 && n >= most * 0.2).map(([zone]) => zone);
        if (!seas.length && (x.flags.port || x.flags.depot || x.flags.islet) && ranked.length) seas = [ranked[0][0]];
      }
      seasOf.set(x.id, seas);
    }
    for (const zone of allowed) {
      if (pieces.some((x) => seasOf.get(x.id).includes(zone))) continue;
      const best = pieces.filter((x) => !CITY_SHORES[x.name]).map((x) => [x, touch2.get(x.id)?.get(zone) ?? 0]).sort((a, b) => b[1] - a[1] || a[0].id - b[0].id)[0];
      if (best) seasOf.get(best[0].id).push(zone);
    }
    for (const x of pieces) {
      const seas = seasOf.get(x.id).sort((a, b) => a - b);
      x.seas = pid < first ? seas.filter((zone) => own.has(zone)) : seas;
      for (const zone of seas) zones2[zone].coast.push(x.id);
      if (x.flags.port && !seas.length) dry.push(x.name);
    }
    // A province of the far side whose whole coast is now a city's cannot be a port: the port passes to the city
    // that holds the coast and is a port itself. (Today's provinces say so themselves, by 'change'.)
    const self = rows.get(pid);
    if (pid >= first && self.flags.port && !self.seas.length) {
      const heir = kids.get(pid).find((c) => c.flags.port && c.seas.length);
      if (heir) { delete self.flags.port; dry.splice(dry.indexOf(self.name), 1); portsPassed.push(`${self.name} to ${heir.name}`); }
    }
  }
  for (const z of zones2) z.coast.sort((a, b) => a - b);
  for (const name of dry) warn(`! ${name} is a port with no sea`);
  if (portsPassed.length) console.log(`ports passed to the city on the coast: ${portsPassed.join(', ')}`);

  // The checks.
  const checks2 = [];
  const check2 = (ok, text) => { checks2.push(`${ok ? 'ok  ' : 'FAIL'} ${text}`); if (!ok) process.exitCode = 1; };
  {
    let worst = 0, where = '';
    for (const pid of kids.keys()) {
      const before = byId.get(pid).polys.reduce((t, q) => t + sizeOf(q), 0), after = sitesOf(pid).reduce((t, s) => t + s.polys.reduce((u, q) => u + sizeOf(q), 0), 0);
      const off = Math.abs(before - after) / Math.max(1, before);
      if (off > worst) { worst = off; where = byId.get(pid).name; }
    }
    check2(worst < 0.002 && strays <= cut2 * 0.002 && !lost.length, `the cities and what is left of their provinces tile the ground those provinces had: ${cut2} squares in ${kids.size} cut provinces, ${strays} set by the nearest seed, the widest difference in area ${(worst * 100).toFixed(3)}% (${where})${lost.length ? `; no ground left for ${lost.join(', ')}` : ''}`);
  }
  {
    // (A province whose row the clerk has changed since reads its ground, worth and flags from the row.)
    const since = new Set([...changedSince, ...ruled]);
    const worth = (r) => (since.has(r.name) ? [] : [r.terrain, r.vp, r.fac, r.flags]);
    const same = base.filter((r) => !kids.has(r.id)).every((r) => { const now = rows.get(r.id); return JSON.stringify([r.name, r.owner, worth(r), r.x, r.y, r.polys, r.seas]) === JSON.stringify([now.name, now.owner, worth(now), now.x, now.y, now.polys, now.seas]) && r.nb.every((j) => now.nb.includes(j) || kids.has(j)); });
    check2(same, `the ${base.length - kids.size} provinces that were not cut are written as they stand, but for neighbours that are now cities${since.size ? ` and ${since.size} whose row in the tables has changed since or whose ground a ruling has laid` : ''}`);
    const kept = [...kids.keys()].every((pid) => { const r = byId.get(pid), now = rows.get(pid); return r.name === now.name && r.owner === now.owner && r.x === now.x && r.y === now.y; });
    check2(kept, `the ${kids.size} provinces that were cut keep their numbers, names, seeds and owners`);
  }
  check2([...rows.values()].every((r) => r.nb.every((j) => rows.get(j)?.nb.includes(r.id) && family(j) === family(r.id))), 'every border is written on both sides');
  check2(names.size === core.regions.length + rows.size, 'no two provinces share a name');
  {
    const all2 = new Set();
    for (const r of rows.values()) for (const j of r.nb) all2.add(key(r.id, j));
    for (const [a, b] of joins2) all2.add(key(a, b));
    let gone = 0;
    const old2 = [];
    for (const r of base) for (const j of r.nb) if (r.id < j) old2.push([r.id, j]);
    for (const [a, b] of joins) old2.push([a, b]);
    for (const [a, b] of old2) if (!piecesOf(a).some((x) => piecesOf(b).some((y) => all2.has(key(x, y))))) gone++;
    check2(gone === 0, `every border of the chart as handed over is still a border: ${old2.length} of them, ${passed} passed on to pieces (${thin} by the longest stretch where none was a fair one)`);
  }
  check2([...kids.keys()].every((pid) => zones.every((z) => !z.coast.includes(pid) || sitesOf(pid).some((s) => zones2[z.i].coast.includes(s.id)))), 'every coast a cut province had is still a coast of one of its pieces');

  // Ground given by ruling (CITY_GIVE), after the checks above, which hold the chart as cut: the part of the strip
  // that lies in one province goes to the other, and the two become neighbours.
  const given = [], givenAfter = [];
  {
    const named = new Map([...rows.values()].map((r) => [r.name, r]));
    // The grid is moved with the ground, so that borders and coasts the gift makes or ends can be read off it after:
    // two that now meet where they did not become neighbours (Juneau and Ketchikan, whose new ground meets on the
    // mainland), and a province that gave all its shore away no longer stands on that sea.
    const touched = new Set(CITY_GIVE.flatMap(([f, t]) => [named.get(f)?.id, named.get(t)?.id]).filter((id) => id !== undefined));
    const meets = () => {
      const m = new Map();
      for (let r = 0; r < ROWS; r++) for (let c = 0; c < COLS; c++) {
        const a = regionAt2[r * COLS + c];
        if (a < 0) continue;
        for (const b of [regionAt2[r * COLS + (c + 1) % COLS], r + 1 < ROWS ? regionAt2[(r + 1) * COLS + c] : -1]) {
          if (b < 0 || a === b || !(touched.has(a) || touched.has(b))) continue;
          const k = key(a, b);
          m.set(k, (m.get(k) ?? 0) + 1);
        }
      }
      return m;
    };
    const shores = () => {
      const m = new Map();
      for (let r = 0; r < ROWS; r++) for (let c = 0; c < COLS; c++) {
        const zone = zoneAt[r * COLS + c];
        if (zone < 0) continue;
        for (const [dx, dy] of STEPS) {
          const rr = r + dy;
          if (rr < 0 || rr >= ROWS) continue;
          const id = regionAt2[rr * COLS + (c + dx + COLS) % COLS];
          if (!touched.has(id)) continue;
          const k = `${id}:${zone}`;
          m.set(k, (m.get(k) ?? 0) + 1);
        }
      }
      return m;
    };
    const meetBefore = meets(), shoreBefore = shores();
    for (const [fromName, toName, shape] of CITY_GIVE) {
      const a = named.get(fromName), b = named.get(toName);
      if (!a || !b) { warn(`! no province called ${!a ? fromName : toName} to give ground`); continue; }
      const strip = [[...shape.map(([lat, lon]) => proj([lon, lat])), proj([shape[0][1], shape[0][0]])]];
      const clean = (parts) => parts.map((part) => part.filter((r) => Math.abs(ringArea(r)) > 0.5).map((r) => trim(r.slice(0, -1).map(([x, y]) => [r1(x), r1(y)]), 1))).filter((q) => q.length && q[0].length >= 3);
      let take = [], left = [], grown = [];
      try {
        take = clean(polygonClipping.intersection(a.polys.map(closed), strip));
        left = clean(polygonClipping.difference(a.polys.map(closed), strip));
        grown = take.length ? clean(polygonClipping.union(b.polys.map(closed), take.map(closed))) : b.polys;
      } catch (e) { warn(`! could not give ground from ${fromName} to ${toName}: ${e.message}`); continue; }
      // The two shapes together keep their area, within a unit or (for a long fjord coast, whose points are rounded
      // and trimmed again) a twentieth of one per cent.
      const area = take.reduce((t, q) => t + sizeOf(q), 0);
      const before = a.polys.reduce((t, q) => t + sizeOf(q), 0) + b.polys.reduce((t, q) => t + sizeOf(q), 0);
      const after = left.reduce((t, q) => t + sizeOf(q), 0) + grown.reduce((t, q) => t + sizeOf(q), 0);
      const seeded = left.some((q) => inPoly(q, a.x, a.y)) && grown.some((q) => inPoly(q, b.x, b.y));
      if (!take.length || !seeded || Math.abs(before - after) > Math.max(1, before * 0.0005)) { warn(`! ground from ${fromName} to ${toName} not given: ${Math.round(area)} square units, seeds ${seeded ? 'kept' : 'lost'}, area ${before.toFixed(1)} to ${after.toFixed(1)}`); continue; }
      a.polys = left; b.polys = grown;
      for (const q of take) for (const j of cellsOf(q)) if (regionAt2[j] === a.id) regionAt2[j] = b.id;
      if (!a.nb.includes(b.id) && !joins2.some(([p, q]) => (p === a.id && q === b.id) || (p === b.id && q === a.id))) {
        if (family(a.id) === family(b.id)) { a.nb.push(b.id); b.nb.push(a.id); a.nb.sort((p, q) => p - q); b.nb.sort((p, q) => p - q); }
        else { joins2.push(family(a.id) < family(b.id) ? [a.id, b.id] : [b.id, a.id]); joins2.sort((p, q) => p[0] - q[0] || p[1] - q[1]); }
      }
      given.push(`${Math.round(area)} square units of ${fromName} to ${toName}`);
    }
    const isNb = (a, b) => rows.get(a)?.nb.includes(b) || joins2.some(([p, q]) => (p === a && q === b) || (p === b && q === a));
    const meetAfter = meets();
    for (const [k, n] of [...meetAfter].sort((p, q) => p[0] - q[0])) {
      const a = Math.floor(k / 10000), b = k % 10000;
      if (n < BORDER || (meetBefore.get(k) ?? 0) >= BORDER || !rows.has(a) || !rows.has(b) || isNb(a, b)) continue;
      if (family(a) === family(b)) { rows.get(a).nb.push(b); rows.get(b).nb.push(a); rows.get(a).nb.sort((p, q) => p - q); rows.get(b).nb.sort((p, q) => p - q); }
      else { joins2.push(family(a) < family(b) ? [a, b] : [b, a]); joins2.sort((p, q) => p[0] - q[0] || p[1] - q[1]); }
      givenAfter.push(`${rows.get(a).name} and ${rows.get(b).name} now meet`);
    }
    const shoreAfter = shores();
    for (const id of touched) {
      const r = rows.get(id);
      for (const zone of [...r.seas]) {
        if ((shoreAfter.get(`${id}:${zone}`) ?? 0) > 0) continue;
        r.seas = r.seas.filter((z) => z !== zone);
        zones2[zone].coast = zones2[zone].coast.filter((x) => x !== id);
        givenAfter.push(`${r.name} no longer on the ${zones2[zone].name}`);
        if (r.flags.port && !r.seas.length) warn(`! ${r.name} is a port with no sea after the ground it gave`);
      }
      for (const [k2, n] of shoreAfter) {
        const [who, zone] = k2.split(':').map(Number);
        if (who !== id || n < 8 || (shoreBefore.get(k2) ?? 0) >= 8 || r.seas.includes(zone)) continue;
        r.seas = [...r.seas, zone].sort((p, q) => p - q);
        zones2[zone].coast = [...zones2[zone].coast, id].sort((p, q) => p - q);
        givenAfter.push(`${r.name} now on the ${zones2[zone].name}`);
      }
    }
    check2([...rows.values()].every((r) => r.nb.every((j) => rows.get(j)?.nb.includes(r.id))), `ground given by ruling (${given.length}): ${given.join(', ') || 'none'}${givenAfter.length ? `; ${givenAfter.join(', ')}` : ''}; every border still written on both sides`);
  }

  const stages2 = [...stages, stages[stages.length - 1] + cities.length];
  const out2 = { ...out, stages: stages2, regions: [...base.map((r) => rows.get(r.id)), ...cities], links: links2, joins: joins2, seas: { ...out.seas, zones: zones2 } };
  // The lakes the chart was not handed over with are cut out of the ground last of all.
  const lakeNotes = cutLakes({ out: out2, core, proj, warn });
  fs.writeFileSync(new URL(`../src/generated/${arg('cities-out') ?? 'world-cities'}.json`, import.meta.url), JSON.stringify(out2));
  console.log(`\ncities: ${cities.length} new provinces (${first + regions.length} to ${first + regions.length + cities.length - 1}) cut out of ${kids.size}; ${out2.regions.length + core.regions.length} provinces in the world; ${(JSON.stringify(out2).length / 1024).toFixed(0)} KB`);
  console.log(`${cityTabled} of ${cities.length} cities have a row in the tables`);
  if (parentTabled) console.log(`${parentTabled} cut provinces have a row in cities-parents.csv${worthOnly ? `, and ${worthOnly} of today's chart that were not cut` : ''}`);
  if (changedSince.length) console.log(`${changedSince.length} provinces read a row the clerk has changed since the chart was handed over: ${changedSince.join(', ')}`);
  if (laidStraits.length) console.log(`straits laid on this chart (${laidStraits.length}): ${laidStraits.join(', ')}`);
  if (ruled.length) console.log(`ground laid by ruling on ${ruled.length} provinces whose seed or row says otherwise (${Object.keys(RULED_GROUND).length} ruled): ${ruled.join(', ')}`);
  if (!quiet && moved2.length) console.log(`pieces passed to a sister: ${moved2.join(', ')}`);
  const small = [...kids.keys()].flatMap((pid) => sitesOf(pid)).map((s) => [s.name, Math.round(s.polys.reduce((t, q) => t + sizeOf(q), 0))]).filter(([, a]) => a < 700);
  if (small.length) console.log(`small (under 700 square units): ${small.map(([n, a]) => `${n} ${a}`).join(', ')}`);
  console.log(checks2.join('\n'));
  console.log(lakeNotes.notes.join('\n'));
  if (!quiet) console.log(lakeNotes.lines.join('\n'));

  // The list of the cities for whoever fills in their tables: --cities-csv=path.
  if (arg('cities-csv')) {
    const q = (v) => (/[",\n]/.test(String(v)) ? `"${String(v).replace(/"/g, '""')}"` : String(v));
    const list = [['id', 'name', 'cut_from', 'lat', 'lon', 'owner', 'terrain', 'vp', 'factories', 'port', 'x', 'y', 'area', 'seas', 'neighbours']];
    const nameOf = (id) => (rows.get(id) ?? coreById.get(id)).name;
    const around = (r) => [...r.nb, ...joins2.filter(([a, b]) => a === r.id || b === r.id).map(([a, b]) => (a === r.id ? b : a))].map(nameOf).join('; ');
    const line = (r, from) => [r.id, r.name, from, ...(latLon.get(r.id) ?? ['', '']), r.owner, r.terrain, r.vp, r.fac, r.flags.port ? 1 : 0, r.x, r.y, Math.round(r.polys.reduce((t, p) => t + sizeOf(p), 0)), zones2.filter((z) => z.coast.includes(r.id)).map((z) => z.name).join('; '), around(r)];
    for (const c of cities) list.push(line(c, byId.get(c.of).name));
    for (const pid of kids.keys()) list.push(line(rows.get(pid), '(itself, now smaller)'));
    fs.writeFileSync(arg('cities-csv'), list.map((row) => row.map(q).join(',')).join('\n') + '\n');
  }
}
