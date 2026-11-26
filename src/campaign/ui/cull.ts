// Drawing only what the view shows (the browser map, before the whole world goes on for release 1).
//
// With the whole world on, the map holds about 29,400 drawn things where the map of the Americas held 13,900, and the
// browser pays for every one of them at every redraw and at every step of a pan, on the screen or not: it reads their
// markup, works out their style, and lays every word on the map out again whenever the view moves (the Pacific
// planner's and the second builder's measures of 18 November). So a heavy layer of the map keeps its pieces, each
// with the box of the map it covers, and puts in the page only those in a window round what the screen shows, the
// stretch of the map that a copy beyond the seam shows among it. When the view leaves that window the layer takes in
// the pieces that come into it and lets go of those far behind, and leaves the rest standing; a redraw writes again
// only the pieces in the window. A piece out of the window is out of sight, so nothing on the screen changes.
//
// Like a stage set: the scenery is built where the audience can see, and a little beyond the wings; the rest of the
// town stays on the drawing board until the play walks there.

/** A box of the map, in map units. */
export interface Area { x0: number; y0: number; x1: number; y1: number }

/** One thing drawn on a layer of the map: its markup, and a box round all it draws (strokes and words with room to spare). */
export interface Piece extends Area {
  html: string;
  /** The nodes it was given in the page, while it is there. */
  nodes?: ChildNode[] | null;
}

/** The whole map: a piece with this box is drawn wherever the view is. */
export const EVERYWHERE: Area = { x0: -Infinity, y0: -Infinity, x1: Infinity, y1: Infinity };

/** A piece that covers this box, grown by `pad` on every side. */
export const piece = (html: string, b: Area, pad = 0): Piece => ({ html, x0: b.x0 - pad, y0: b.y0 - pad, x1: b.x1 + pad, y1: b.y1 + pad });
/** A piece round a point: `rx` to each side and `ry` up and down. */
export const pieceAt = (html: string, x: number, y: number, rx: number, ry = rx): Piece => ({ html, x0: x - rx, y0: y - ry, x1: x + rx, y1: y + ry });

/** True where two boxes meet (a touch counts). */
export const meets = (a: Area, b: Area): boolean => a.x1 >= b.x0 && a.x0 <= b.x1 && a.y1 >= b.y0 && a.y0 <= b.y1;
/** True where a box meets any of these. */
export const meetsAny = (a: Area, bs: Area[]): boolean => { for (const b of bs) if (meets(a, b)) return true; return false; };
/** True where the first box lies wholly within the second. */
export const within = (a: Area, b: Area): boolean => a.x0 >= b.x0 && a.x1 <= b.x1 && a.y0 >= b.y0 && a.y1 <= b.y1;

/** The box round a province's (or any) outline. */
export function boxOf(polys: [number, number][][][]): Area {
  let x0 = Infinity, y0 = Infinity, x1 = -Infinity, y1 = -Infinity;
  for (const poly of polys) for (const ring of poly) for (const [x, y] of ring) {
    if (x < x0) x0 = x; if (x > x1) x1 = x;
    if (y < y0) y0 = y; if (y > y1) y1 = y;
  }
  return { x0, y0, x1, y1 };
}
/** The box round a line of points. */
export function boxOfLine(pts: [number, number][]): Area {
  let x0 = Infinity, y0 = Infinity, x1 = -Infinity, y1 = -Infinity;
  for (const [x, y] of pts) {
    if (x < x0) x0 = x; if (x > x1) x1 = x;
    if (y < y0) y0 = y; if (y > y1) y1 = y;
  }
  return { x0, y0, x1, y1 };
}

const SVG_NS = 'http://www.w3.org/2000/svg';
/**
 * Between the markup of two pieces read at once: an element of no meaning to the browser, taken out as soon as the
 * markup is read. (Not one of the names, such as <b> or <i>, that would end the drawing's own markup.)
 */
const CUT = '<tc-cut/>';
const CUT_NAME = 'tc-cut';
let scratch: Element | null = null;

/** Reads the markup of several pieces at once, off the page, and gives back each one's nodes in order. */
function readPieces(html: string[]): ChildNode[][] {
  scratch ??= document.createElementNS(SVG_NS, 'g');
  scratch.innerHTML = html.join(CUT);
  const out: ChildNode[][] = [[]];
  for (let n = scratch.firstChild; n; n = n.nextSibling) {
    if (n.nodeName === CUT_NAME) out.push([]);
    else out[out.length - 1].push(n);
  }
  return out;
}

/** A layer of the map that holds in the page only the pieces in the window round the view. */
export class Sheet {
  pieces: Piece[] = [];
  /** `fresh` is told of each piece as it is put in the page, with its nodes (a province's colour, say). */
  constructor(readonly el: Element, readonly fresh?: (p: Piece, nodes: ChildNode[]) => void) {}

  /** Draws the layer afresh from these pieces: those in the window now, the rest once the view comes to them. */
  set(pieces: Piece[], win: Area[]): void {
    this.pieces = pieces;
    const shown: Piece[] = [];
    for (const p of pieces) { p.nodes = null; if (meetsAny(p, win)) shown.push(p); }
    const el = this.el;
    el.innerHTML = shown.map((p) => p.html).join(CUT);
    // Read straight into the page, then each piece is given its nodes and the cuts between them are taken out.
    let k = 0, cur: ChildNode[] = [];
    for (let n = el.firstChild; n;) {
      const next = n.nextSibling;
      if (n.nodeName === CUT_NAME) { shown[k++].nodes = cur; cur = []; el.removeChild(n); } else cur.push(n);
      n = next;
    }
    if (shown.length) shown[k].nodes = cur;
    if (this.fresh) for (const p of shown) this.fresh(p, p.nodes!);
  }

  /**
   * The view has left the window: the pieces in the new window that are not in the page are put in, each in its
   * place among the others (the order of the pieces is the order they are drawn in), and those drawn that lie
   * outside `keep` (a wider box, so that a pan back and forth does not put the same pieces in and out) are taken out.
   * Nothing else in the layer is touched.
   */
  recull(win: Area[], keep: Area[]): void {
    const enter: Piece[] = [];
    for (const p of this.pieces) {
      if (p.nodes) {
        if (!meetsAny(p, keep)) { for (const n of p.nodes) n.parentNode?.removeChild(n); p.nodes = null; }
      } else if (meetsAny(p, win)) enter.push(p);
    }
    if (!enter.length) return;
    const read = readPieces(enter.map((p) => p.html));
    const fresh = new Set<Piece>(enter);
    enter.forEach((p, i) => { p.nodes = read[i]; });
    // From the last piece back: each new one goes in before the first node of the next piece in the page.
    let next: ChildNode | null = null;
    const el = this.el;
    for (let i = this.pieces.length - 1; i >= 0; i--) {
      const p = this.pieces[i];
      if (!p.nodes || !p.nodes.length) continue;
      if (fresh.has(p)) for (const n of p.nodes) el.insertBefore(n, next);
      next = p.nodes[0];
    }
    scratch!.textContent = '';
    if (this.fresh) for (const p of enter) this.fresh(p, p.nodes!);
  }
}

/**
 * A layer drawn as a few long lines (the land, the railways): each line is one path in the page made of many short
 * stretches, each with its box, and the path holds only the stretches in the window. A stretch out of the window
 * draws nothing on the screen, and each stretch begins with its own move, so the dashes of the rest fall as they did.
 */
export class Lines {
  /** For each path: its markup before its stretches and after them, and the stretches. */
  paths: { head: string; tail: string; bits: Piece[] }[] = [];
  private drawn = '';
  constructor(readonly el: Element) {}

  /** The paths' markup with the stretches in `win`, and those already drawn that lie in `keep`. */
  private markup(win: Area[], keep: Area[] | null): string {
    let out = '';
    for (const p of this.paths) {
      let d = '';
      for (const b of p.bits) {
        if (meetsAny(b, win) || (keep && b.nodes && meetsAny(b, keep))) { d += b.html; b.nodes = []; } else b.nodes = null;
      }
      out += `${p.head}${d}${p.tail}`;
    }
    return out;
  }
  set(paths: { head: string; tail: string; bits: Piece[] }[], win: Area[]): void {
    this.paths = paths;
    this.drawn = this.markup(win, null);
    this.el.innerHTML = this.drawn;
  }
  recull(win: Area[], keep: Area[]): void {
    const out = this.markup(win, keep);
    if (out !== this.drawn) { this.drawn = out; this.el.innerHTML = out; }
  }
}
