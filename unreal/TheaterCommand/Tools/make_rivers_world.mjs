// Finds the rivers of the whole world from its heights, for the ground beyond the map of Europe.
//
// The world's heights are NOAA's (Content/Data/Campaign/elevation-world.bin, made by make_elevation_world.mjs:
// one height for every two map units). Water runs downhill: every hollow in the heights is filled until it
// spills, each point of land is given the neighbour its water leaves by, and the rain that falls on every
// point is carried down from point to point to the sea. How much rain falls is taken from the art director's
// climate table (world-climate-boxes.json): a jungle sheds a great deal, a desert next to nothing, so the Sahara
// grows no rivers of its own and the Nile still crosses it. Where a great closed basin would have to fill to its
// brim before it spilled (Lake Chad, the Tarim, the Great Basin, the Altiplano), its rivers end in it.
//
// The output is every course that carries enough water, as a line of points in the world map's own frame (x east
// from the seam at 30.45 W, y south from 71.4 N, in map units; a course that crosses the seam keeps counting
// past it), each point with how much water passes it:
//
//   "TCR2", number of rivers (uint32), then for each: number of points (uint32), the river it joins (int32, its
//   place in this list, or -1 where it reaches the sea or ends in a basin), how much water that river carries
//   where it joins it (float32), and x, y, flow (float32) for each point
//
// Nothing is fetched and nothing is copied from a map: the courses are worked out from the heights alone.
//
//   node Tools/make_rivers_world.mjs <elevation-world.bin> <world-climate-boxes.json> <rivers-world.bin> [preview.png]

import fs from 'node:fs';
import zlib from 'node:zlib';

const [, , elevationPath, climatePath, outputPath, previewPath] = process.argv;
if (!elevationPath || !climatePath || !outputPath) {
  console.error('usage: node make_rivers_world.mjs <elevation-world.bin> <world-climate-boxes.json> <rivers-world.bin> [preview.png]');
  process.exit(1);
}

// ---- the heights -------------------------------------------------------------------------------
const raw = fs.readFileSync(elevationPath);
if (raw.toString('latin1', 0, 4) !== 'TCE2') throw new Error('not a world elevation file');
const fileWide = raw.readUInt32LE(4), high = raw.readUInt32LE(8), step = raw.readFloatLE(12);
const WORLD = 9256.14; // the world's width in map units
const wide = Math.round(WORLD / step); // the ring: the file's last column is its first again
const heights = new Int16Array(wide * high);
for (let j = 0; j < high; j++) for (let i = 0; i < wide; i++) heights[j * wide + i] = raw.readInt16LE(16 + 2 * (j * fileWide + i));
console.log(`heights: ${wide} by ${high}, every ${step} map units`);

// A few of the world's great rivers leave their basins by a gorge narrower than one of these heights is wide, and
// the heights show a wall there: filled to its brim, the basin spills over the wrong pass and the river reaches the
// wrong sea. For those a channel is dug along the gorge before anything else is done: each is a line of places
// (latitude, longitude, and the river's own height there in metres), and no height near the line is left above it.
const GATES = [
  // the Congo: down its channel through the plateau to its pool, and down the cataracts below the pool to the sea
  [[-1.2, 16.6, 292], [-2.2, 16.2, 288], [-3.3, 16.2, 280], [-4.3, 15.3, 272], [-4.9, 14.6, 200], [-5.5, 13.9, 90], [-5.9, 13.2, 12], [-6.05, 12.5, 1]],
];
const latitudeOf = (j) => 71.4 - (j * step) / 40;
const longitudeOf = (i) => { let l = -30.45 + (i * step) / 25.7115; while (l > 180) l -= 360; return l; };

{
  let dug = 0;
  for (const gate of GATES) {
    for (let k = 0; k + 1 < gate.length; k++) {
      const [la0, lo0, h0] = gate[k], [la1, lo1, h1] = gate[k + 1];
      const x0 = ((lo0 + 30.45 + 360) % 360) * 25.7115 / step, y0 = (71.4 - la0) * 40 / step, x1 = ((lo1 + 30.45 + 360) % 360) * 25.7115 / step, y1 = (71.4 - la1) * 40 / step;
      const n = Math.ceil(Math.hypot(x1 - x0, y1 - y0) * 2);
      for (let t = 0; t <= n; t++) {
        const x = Math.round(x0 + (x1 - x0) * t / n), y = Math.round(y0 + (y1 - y0) * t / n), level = Math.max(1, Math.round(h0 + (h1 - h0) * t / n));
        for (let dy = 0; dy <= 1; dy++) for (let dx = 0; dx <= 1; dx++) {
          const at = Math.min(high - 1, Math.max(0, y + dy)) * wide + ((x + dx) % wide + wide) % wide;
          if (heights[at] > level) { heights[at] = level; dug++; }
        }
      }
    }
  }
  console.log(`gorges dug: ${GATES.length}, ${dug} heights lowered`);
}

// ---- how much rain each look sheds -------------------------------------------------------------
const table = JSON.parse(fs.readFileSync(climatePath, 'utf8'));
const SHEDS = { temperate: 0.5, drysouth: 0.14, prairie: 0.16, steppe: 0.07, desert: 0.006, savanna: 0.26, jungle: 1.0, monsoon: 0.8, dryplain: 0.14, taiga: 0.34, tundra: 0.2, ice: 0.0, alpine: 0.12 };
const PER = 2, CW = 360 * PER, CH = 132 * PER, CSOUTH = -58;
const look = new Array(CW * CH).fill('temperate');
for (const box of table.boxes) {
  for (let j = 0; j < CH; j++) {
    const lat = CSOUTH + (j + 0.5) / PER;
    if (lat < box.south || lat > box.north) continue;
    for (let i = 0; i < CW; i++) {
      const lon = -180 + (i + 0.5) / PER;
      if ((lon >= box.west && lon <= box.east) || (lon + 360 >= box.west && lon + 360 <= box.east)) look[j * CW + i] = box.look;
    }
  }
}
const shedsAt = (lat, lon) => {
  const j = Math.min(CH - 1, Math.max(0, Math.floor((lat - CSOUTH) * PER))), i = ((Math.floor((lon + 180) * PER) % CW) + CW) % CW;
  return SHEDS[look[j * CW + i]] ?? 0.3;
};

// ---- fill the hollows and find which way the water leaves each point -----------------------------
const count = wide * high;
const DX = [1, 1, 0, -1, -1, -1, 0, 1], DY = [0, 1, 1, 1, 0, -1, -1, -1];
const down = new Int32Array(count).fill(-1); // the point the water goes to; -1 for the sea, -2 for a river's end
const filled = new Float32Array(count);
const seen = new Uint8Array(count);
const order = new Int32Array(count);
let ordered = 0;
// (a heap of points, lowest first; among equals, the one put in first: so that water crosses a flat by the shortest way)
const heapAt = new Int32Array(count + 1), heapKey = new Float64Array(count + 1);
let heapSize = 0, stamp = 0;
// (and on a dead flat, a marsh or a plain these heights show as level, each point is taken as a little higher or
// lower than it is, by under a metre and always the same for the same point: the water then finds a winding way
// across, as it does, and not a ruled diagonal)
const uneven = (at) => { let h = Math.imul(at ^ 0x9e3779b9, 0x85ebca6b); h ^= h >>> 13; h = Math.imul(h, 0xc2b2ae35); h ^= h >>> 16; return (h >>> 0) / 4294967296; };
const push = (at, level) => {
  const key = Math.round((level + 0.9 * uneven(at)) * 64) * 16777216 + stamp++;
  let k = ++heapSize;
  while (k > 1 && heapKey[k >> 1] > key) { heapAt[k] = heapAt[k >> 1]; heapKey[k] = heapKey[k >> 1]; k >>= 1; }
  heapAt[k] = at; heapKey[k] = key;
};
const pop = () => {
  const top = heapAt[1], at = heapAt[heapSize], key = heapKey[heapSize--];
  let k = 1;
  for (;;) {
    let c = k * 2;
    if (c > heapSize) break;
    if (c < heapSize && heapKey[c + 1] < heapKey[c]) c++;
    if (heapKey[c] >= key) break;
    heapAt[k] = heapAt[c]; heapKey[k] = heapKey[c]; k = c;
  }
  heapAt[k] = at; heapKey[k] = key;
  return top;
};
const isLand = (at) => heights[at] > 0;
let land = 0;
for (let j = 0; j < high; j++) {
  for (let i = 0; i < wide; i++) {
    const at = j * wide + i;
    if (!isLand(at)) { seen[at] = 1; continue; }
    land++;
    // (land on the shore, or on the top or bottom edge of the world, sheds straight into the sea)
    let shore = j === 0 || j === high - 1;
    for (let d = 0; d < 8 && !shore; d++) {
      const y = j + DY[d];
      if (y < 0 || y >= high) continue;
      if (!isLand(y * wide + (i + DX[d] + wide) % wide)) shore = true;
    }
    if (shore) { seen[at] = 1; filled[at] = heights[at]; push(at, heights[at]); }
  }
}
while (heapSize > 0) {
  const at = pop();
  order[ordered++] = at;
  const i = at % wide, j = (at - i) / wide;
  for (let d = 0; d < 8; d++) {
    const y = j + DY[d];
    if (y < 0 || y >= high) continue;
    const to = y * wide + (i + DX[d] + wide) % wide;
    if (seen[to]) continue;
    seen[to] = 1;
    down[to] = at;
    filled[to] = Math.max(heights[to], filled[at]);
    push(to, filled[to]);
  }
}
console.log(`land: ${land} points, ${ordered} drained`);

// ---- the great closed basins: where the filling is deep over a wide country, the rivers end ------
const DRY_BASIN = 0.22; // a basin sheds less than this, on average, to be a closed one
const DEEP = 35, WIDE_BASIN = 900; // metres of filling; points of it together (about 40,000 square kilometres)
const basin = new Int32Array(count).fill(-1);
const ends = new Uint8Array(count);
{
  const stack = [];
  let basins = 0, closed = 0;
  for (let start = 0; start < count; start++) {
    if (!isLand(start) || basin[start] >= 0 || filled[start] - heights[start] <= DEEP) continue;
    const members = [];
    stack.push(start); basin[start] = basins;
    while (stack.length) {
      const at = stack.pop();
      members.push(at);
      const i = at % wide, j = (at - i) / wide;
      for (let d = 0; d < 8; d++) {
        const y = j + DY[d];
        if (y < 0 || y >= high) continue;
        const to = y * wide + (i + DX[d] + wide) % wide;
        if (isLand(to) && basin[to] < 0 && filled[to] - heights[to] > DEEP) { basin[to] = basins; stack.push(to); }
      }
    }
    // (A wide, deep filling is one of two things. In dry country it is a true closed basin, whose rivers die in a
    // salt lake or a marsh. In wet country it is a great river's own basin behind a gorge too narrow for these
    // heights to show, the Congo's below its pool or the Yangtze's in its gorges: that one spills, and its river goes on.)
    if (members.length >= WIDE_BASIN) {
      let wet = 0;
      for (const at of members) { const i = at % wide, j = (at - i) / wide; wet += shedsAt(latitudeOf(j), longitudeOf(i)); }
      if (wet / members.length < DRY_BASIN) { closed++; for (const at of members) ends[at] = 1; }
    }
    basins++;
  }
  console.log(`filled hollows deeper than ${DEEP} m: ${basins}, of which ${closed} are great closed basins`);
}

// ---- carry the rain down ------------------------------------------------------------------------------------
const flow = new Float32Array(count);
for (let j = 0; j < high; j++) {
  const lat = latitudeOf(j), area = (step / 40) * 111.2 * (step / 25.7115) * 111.2 * Math.cos((lat * Math.PI) / 180);
  for (let i = 0; i < wide; i++) {
    const at = j * wide + i;
    if (isLand(at)) flow[at] = area * shedsAt(lat, longitudeOf(i));
  }
}
for (let k = ordered - 1; k >= 0; k--) {
  const at = order[k], to = down[at];
  if (to < 0 || ends[at]) continue; // (water that has reached a closed basin stays in it)
  flow[to] += flow[at];
}

// ---- the courses ----------------------------------------------------------------------------------------------
const LEAST = Number(process.env.LEAST) || 3500; // the least water a course carries to be drawn (rain-weighted square kilometres)
// (Across a filled hollow, which is a lake or a marsh or a flat too level for these heights, the water of every
// stream that enters goes straight for the outlet, and drawn they are a comb of ruled lines. Only the greatest
// of them is drawn across it: the rest end at its edge.)
// (A wide one only. Every valley floor has small hollows a few points long, where these heights are too coarse to
// show the river's own bed: across those a stream is drawn as anywhere.)
const FLAT = 3, WIDE_FLAT = 70, ACROSS = 14, REACH_ACROSS = 9;
const level = new Uint8Array(count);
{
  const mark = new Uint8Array(count), stack = [];
  for (let start = 0; start < count; start++) {
    if (!isLand(start) || mark[start] || filled[start] - heights[start] <= FLAT) continue;
    const members = [];
    stack.push(start); mark[start] = 1;
    while (stack.length) {
      const at = stack.pop();
      members.push(at);
      const i = at % wide, j = (at - i) / wide;
      for (let d = 0; d < 8; d++) {
        const y = j + DY[d];
        if (y < 0 || y >= high) continue;
        const to = y * wide + (i + DX[d] + wide) % wide;
        if (isLand(to) && !mark[to] && filled[to] - heights[to] > FLAT) { mark[to] = 1; stack.push(to); }
      }
    }
    if (members.length >= WIDE_FLAT) for (const at of members) level[at] = 1;
  }
}
const stream = (at) => at >= 0 && isLand(at) && !ends[at] && flow[at] >= (level[at] ? LEAST * ACROSS : LEAST);
// each point of a stream's biggest feeder
// (At a fork the river is the longer branch, as it is on any map, so long as it carries at least half the water of the other:
// the Niger and not the Benue, the Missouri and not the upper Mississippi, the White Nile. The other is its tributary.)
const feeder = new Int32Array(count).fill(-1);
const above = new Int32Array(count); // how many points of stream lie above each point, by its longest branch
const most = new Float32Array(count); // the most water any one branch brings to each point
for (let k = ordered - 1; k >= 0; k--) {
  const at = order[k];
  if (!stream(at)) continue;
  if (above[at] < 1) above[at] = 1;
  const to = down[at];
  if (!stream(to)) continue;
  if (above[to] < above[at] + 1) above[to] = above[at] + 1;
  if (flow[at] > most[to]) most[to] = flow[at];
}
for (let at = 0; at < count; at++) {
  if (!stream(at)) continue;
  const to = down[at];
  if (!stream(to) || flow[at] < 0.5 * most[to]) continue;
  if (feeder[to] < 0 || above[at] > above[feeder[to]] || (above[at] === above[feeder[to]] && flow[at] > flow[feeder[to]])) feeder[to] = at;
}
const rivers = [];
const trace = (mouth, joinsAt) => {
  // from where it ends, up its biggest feeder at every fork, to its source; then written from source to mouth
  const points = [];
  if (joinsAt >= 0) points.push(joinsAt);
  for (let at = mouth; at >= 0; at = feeder[at]) points.push(at);
  points.reverse();
  return points;
};
for (let at = 0; at < count; at++) {
  if (!stream(at)) continue;
  const to = down[at];
  if (!stream(to)) {
    // It reaches the sea, a closed basin, or the edge of a wide flat where only the greatest stream is drawn. Where
    // that flat is narrow and a river runs just beyond, the course is run through to join it: a creek does not stop
    // a little short of the river it feeds.
    const course = trace(at, -1);
    let c = to, run = [];
    while (c >= 0 && isLand(c) && !ends[c] && !stream(c) && run.length < REACH_ACROSS) { run.push(c); c = down[c]; }
    let joins = -1;
    if (run.length > 0 && run.length < REACH_ACROSS && (c < 0 || !isLand(c) || stream(c))) { for (const r of run) course.push(r); if (c >= 0 && stream(c)) { course.push(c); joins = c; } }
    course.joins = joins;
    rivers.push(course);
  }
  else if (feeder[to] !== at) { const course = trace(at, to); course.joins = to; rivers.push(course); } // it joins the river
}
// A great river's mouth is often a long drowned valley, which these heights give as sea: the Amazon below Gurupa,
// the Plate. From high up the game's coarse land closes such a narrow water, and the river stopped in the middle of
// the jungle a long way short of the coast. A great river is therefore carried on down its estuary to open water:
// from its last point of land, by the shortest way through the narrow sea and its delta to water with no land near it.
{
  const NEAR = 7; // points: water with land nearer than this is narrow water
  const LOW = 10; // metres: land no higher than this is delta
  const toLand = new Uint8Array(count).fill(255);
  let edge = [];
  for (let at = 0; at < count; at++) if (isLand(at)) { toLand[at] = 0; edge.push(at); }
  for (let d = 1; d <= NEAR && edge.length; d++) {
    const next = [];
    for (const at of edge) {
      const i = at % wide, j = (at - i) / wide;
      for (let k = 0; k < 8; k++) {
        const y = j + DY[k];
        if (y < 0 || y >= high) continue;
        const to = y * wide + (i + DX[k] + wide) % wide;
        if (toLand[to] === 255) { toLand[to] = d; next.push(to); }
      }
    }
    edge = next;
  }
  let carried = 0;
  for (const course of rivers) {
    if (course.joins >= 0 || course.length < 3) continue;
    const mouth = course[course.length - 1];
    if (flow[mouth] < 200000 || ends[mouth]) continue;
    // (breadth first from the mouth through narrow water, as far as 70 points, to the first open water)
    const from = new Map([[mouth, -1]]);
    let ring = [mouth], found = -1;
    for (let d = 0; d < 70 && ring.length && found < 0; d++) {
      const next = [];
      for (const at of ring) {
        const i = at % wide, j = (at - i) / wide;
        for (let k = 0; k < 8 && found < 0; k++) {
          const y = j + DY[k];
          if (y < 0 || y >= high) continue;
          const to = y * wide + (i + DX[k] + wide) % wide;
          // (through the water, and over the mud banks and islands of a delta, which stand a few metres out of it)
          if (heights[to] > LOW || from.has(to)) continue;
          from.set(to, at);
          if (toLand[to] === 255) found = to; else next.push(to);
        }
      }
      ring = next;
    }
    if (found < 0) continue;
    const way = [];
    for (let at = found; at !== mouth && at >= 0; at = from.get(at)) way.push(at);
    way.reverse();
    // (down the estuary it carries what it carried at its last point of land)
    if (way.length > NEAR + 2) { course.estuaryFrom = course.length; course.estuaryWater = flow[mouth]; for (const at of way.slice(0, way.length - NEAR)) course.push(at); carried++; }
  }
  console.log(`great rivers carried down their estuaries: ${carried}`);
}

// Which course each point of stream belongs to, so that a tributary knows the river it joins.
const courseOf = new Int32Array(count).fill(-1);
rivers.forEach((course, index) => { const own = course.joins >= 0 ? course.length - 1 : course.length; for (let k = 0; k < own; k++) courseOf[course[k]] = index; });

// Each course as a smooth line: the points' own places, with the corners of the grid cut off twice.
const out = [];
let totalPoints = 0;
for (const [index, course] of rivers.entries()) {
  if (course.length < 3) continue;
  let line = [];
  let turn = 0, lastI = null;
  for (const at of course) {
    const i = at % wide, j = (at - i) / wide;
    if (lastI !== null) { if (i - lastI > wide / 2) turn--; else if (lastI - i > wide / 2) turn++; }
    lastI = i;
    line.push([(i + turn * wide) * step, j * step, course.estuaryFrom !== undefined && line.length >= course.estuaryFrom ? course.estuaryWater : flow[at]]);
  }
  // (where it joins a bigger river its last point is that river's: it comes in at its own breadth, not the bigger one's)
  if (line.length >= 2 && line[line.length - 1][2] > line[line.length - 2][2] * 1.5) line[line.length - 1][2] = line[line.length - 2][2];
  for (let pass = 0; pass < 2; pass++) {
    const next = [line[0]];
    for (let k = 0; k + 1 < line.length; k++) {
      const a = line[k], b = line[k + 1];
      next.push([a[0] * 0.75 + b[0] * 0.25, a[1] * 0.75 + b[1] * 0.25, a[2] * 0.75 + b[2] * 0.25]);
      next.push([a[0] * 0.25 + b[0] * 0.75, a[1] * 0.25 + b[1] * 0.75, a[2] * 0.25 + b[2] * 0.75]);
    }
    next.push(line[line.length - 1]);
    line = next;
  }
  // (thinned again to about one point for every map unit and a half: the cutting made four of every one)
  const thin = [line[0]];
  for (let k = 1; k + 1 < line.length; k++) {
    const last = thin[thin.length - 1];
    if (Math.hypot(line[k][0] - last[0], line[k][1] - last[1]) >= 1.4) thin.push(line[k]);
  }
  thin.push(line[line.length - 1]);
  // (the river it joins, and how much water that river carries where it joins it)
  thin.was = index;
  thin.joined = course.joins >= 0 ? courseOf[course.joins] : -1;
  thin.joinedWater = course.joins >= 0 ? flow[course.joins] : 0;
  out.push(thin);
  totalPoints += thin.length;
}
out.sort((a, b) => b[b.length - 1][2] - a[a.length - 1][2]);
const placeOf = new Map(out.map((line, place) => [line.was, place]));
const bytes = Buffer.alloc(8 + out.length * 12 + totalPoints * 12);
bytes.write('TCR2', 0, 'latin1');
bytes.writeUInt32LE(out.length, 4);
let at = 8;
for (const line of out) {
  bytes.writeUInt32LE(line.length, at); at += 4;
  bytes.writeInt32LE(placeOf.has(line.joined) ? placeOf.get(line.joined) : -1, at); at += 4;
  bytes.writeFloatLE(line.joinedWater, at); at += 4;
  for (const p of line) { bytes.writeFloatLE(p[0], at); bytes.writeFloatLE(p[1], at + 4); bytes.writeFloatLE(p[2], at + 8); at += 12; }
}
fs.writeFileSync(outputPath, bytes);
console.log(`rivers: ${out.length} courses, ${totalPoints} points, ${bytes.length} bytes`);
for (const line of out.slice(0, 14)) {
  const m = line[line.length - 1], s = line[0];
  const where = (p) => `${(71.4 - p[1] / 40).toFixed(1)}N ${(((-30.45 + p[0] / 25.7115 + 540) % 360) - 180).toFixed(1)}E`;
  console.log(`  ${Math.round(m[2]).toLocaleString('en-US').padStart(10)}  from ${where(s)} to ${where(m)}, ${line.length} points`);
}

// ---- a picture to look the courses over by -----------------------------------------------------------------------
if (previewPath) {
  const S = 2, pw = Math.floor(wide / S), ph = Math.floor(high / S);
  const rgb = Buffer.alloc(pw * ph * 3);
  for (let y = 0; y < ph; y++) for (let x = 0; x < pw; x++) {
    const h = heights[y * S * wide + x * S], k = (y * pw + x) * 3;
    if (h <= 0) { rgb[k] = 40; rgb[k + 1] = 60; rgb[k + 2] = 84; }
    else { const g = Math.min(235, 150 + h / 40); rgb[k] = g; rgb[k + 1] = g - 6; rgb[k + 2] = g - 30; if (ends[y * S * wide + x * S]) { rgb[k] -= 40; rgb[k + 2] += 10; } }
  }
  for (const line of out) for (let k = 0; k + 1 < line.length; k++) {
    const a = line[k], b = line[k + 1], n = Math.max(1, Math.ceil(Math.hypot(b[0] - a[0], b[1] - a[1]) / (step * S) * 2));
    const r = Math.min(2.2, Math.max(0, Math.log10(a[2] / LEAST)) * 0.7);
    for (let t = 0; t <= n; t++) {
      const x = (((a[0] + (b[0] - a[0]) * t / n) / (step * S)) % pw + pw) % pw, y = (a[1] + (b[1] - a[1]) * t / n) / (step * S);
      for (let dy = -Math.ceil(r); dy <= Math.ceil(r); dy++) for (let dx = -Math.ceil(r); dx <= Math.ceil(r); dx++) {
        if (dx * dx + dy * dy > r * r + 0.3) continue;
        const px = (Math.round(x) + dx + pw) % pw, py = Math.round(y) + dy;
        if (py < 0 || py >= ph) continue;
        const q = (py * pw + px) * 3; rgb[q] = 30; rgb[q + 1] = 70; rgb[q + 2] = 170;
      }
    }
  }
  const crc = (buf) => { let c, n = ~0; for (let i = 0; i < buf.length; i++) { c = (n ^ buf[i]) & 255; for (let k = 0; k < 8; k++) c = c & 1 ? (c >>> 1) ^ 0xedb88320 : c >>> 1; n = (n >>> 8) ^ c; } return ~n >>> 0; };
  const chunk = (type, data) => { const len = Buffer.alloc(4); len.writeUInt32BE(data.length); const body = Buffer.concat([Buffer.from(type, 'latin1'), data]); const sum = Buffer.alloc(4); sum.writeUInt32BE(crc(body)); return Buffer.concat([len, body, sum]); };
  const head = Buffer.alloc(13); head.writeUInt32BE(pw, 0); head.writeUInt32BE(ph, 4); head[8] = 8; head[9] = 2;
  const rows = Buffer.alloc((pw * 3 + 1) * ph);
  for (let y = 0; y < ph; y++) rgb.copy(rows, y * (pw * 3 + 1) + 1, y * pw * 3, (y + 1) * pw * 3);
  fs.writeFileSync(previewPath, Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk('IHDR', head), chunk('IDAT', zlib.deflateSync(rows)), chunk('IEND', Buffer.alloc(0))]));
  console.log(`preview: ${previewPath}, ${pw} by ${ph}`);
}
