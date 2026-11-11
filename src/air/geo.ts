// The air map: south-east England, the Channel and the Pas-de-Calais, in metres.
import geo from '../generated/geo-air.json';

export interface LandPoly {
  rings: [number, number][][];
  /** 1 = England, 2 = the continent. */
  cls: 1 | 2;
  minX: number; maxX: number; minY: number; maxY: number;
}

export const GEO = geo as unknown as {
  lat0: number; lon0: number; kx: number; ky: number;
  bounds: { minX: number; maxX: number; minY: number; maxY: number };
  land: [number, number][][][];
};

export function ll(lat: number, lon: number): { x: number; y: number } {
  return { x: (lon - GEO.lon0) * GEO.kx, y: (lat - GEO.lat0) * GEO.ky };
}

function inRing(ring: [number, number][], x: number, y: number): boolean {
  let inside = false;
  for (let i = 0, j = ring.length - 1; i < ring.length; j = i++) {
    const xi = ring[i][0], yi = ring[i][1], xj = ring[j][0], yj = ring[j][1];
    if (yi > y !== yj > y && x < ((xj - xi) * (y - yi)) / (yj - yi) + xi) inside = !inside;
  }
  return inside;
}

const london = ll(51.507, -0.128);

export const LAND: LandPoly[] = GEO.land.map((rings) => {
  let minX = 1e12, maxX = -1e12, minY = 1e12, maxY = -1e12, cx = 0, cy = 0;
  for (const [x, y] of rings[0]) {
    if (x < minX) minX = x; if (x > maxX) maxX = x; if (y < minY) minY = y; if (y > maxY) maxY = y;
    cx += x; cy += y;
  }
  cx /= rings[0].length; cy /= rings[0].length;
  const p: LandPoly = { rings, cls: 2, minX, maxX, minY, maxY };
  // England is the polygon London sits in, plus the small islands off its coast.
  if (inRing(rings[0], london.x, london.y)) p.cls = 1;
  else if (maxX - minX < 40000 && cx < 75000 && cy > -45000) p.cls = 1;
  return p;
});

/** 0 = sea, 1 = England, 2 = the continent. */
export function landClass(x: number, y: number): 0 | 1 | 2 {
  for (const p of LAND) {
    if (x < p.minX || x > p.maxX || y < p.minY || y > p.maxY) continue;
    if (!inRing(p.rings[0], x, y)) continue;
    let hole = false;
    for (let i = 1; i < p.rings.length; i++) if (inRing(p.rings[i], x, y)) { hole = true; break; }
    if (!hole) return p.cls;
  }
  return 0;
}
