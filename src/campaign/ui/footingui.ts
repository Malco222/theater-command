// The nation's footing in the browser test kitchen (ruling 172): the strip under the arsenal's rule and the one card
// it opens. Plain on purpose: the browser game holds the rules and their words, and the look is the 3D game's
// (Brian, 18 November 2026). Everything shown here comes from footingView (footing.ts), which both games read.

import { FOOTING, type FootingView } from '../footing';

const esc = (s: string): string => s.replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]!));
const pct = (x: number): string => `${Math.round(x * 100)}%`;
const months = (m: number): string => (m === 1 ? 'a month' : `${m} months`);

/** The strip on the buying page: the footing's name, the ledger's line word for word, the tip in its month, and the door. */
export function footingStrip(v: FootingView, tip: boolean): string {
  const worn = v.worn >= FOOTING.allout.amberAt - 1e-9 ? ` <span style="color:var(--amber, #c8902a)">${pct(v.worn)} of the will to fight worn away by all-out war.</span>` : '';
  const said = tip && v.tip ? `<br><small>${esc(FOOTING.tips[v.tip])}</small>` : '';
  return `<div class="note" id="footing"><b>${esc(v.name)}.</b> ${esc(v.line)}.${worn}${said} <button class="btn small" data-act="footing">Change the footing</button></div>`;
}

/** The card: the four footings as a tally with the one in force lit and the locks in one sentence, and the one move it asks about. */
export function footingCard(v: FootingView, cash: (pp: number, signed?: boolean) => string, capital: number): string {
  const rows = v.rows.map((r) => `<tr${r.state === 'on' ? ' style="font-weight:700"' : ''}><td>${r.state === 'on' ? '● ' : ''}${esc(r.name)}</td><td style="text-align:right">${r.index}</td><td><small>${r.state === 'on' ? 'In force' : r.why ? esc(r.why) : ''}</small></td></tr>`).join('');
  const m = v.move;
  let ask = '';
  if (m) {
    const lines: string[] = [];
    if (m.up) {
      lines.push(`Brings ${cash(m.brings, true)} a month once the climb is done, in ${months(Math.max(1, m.months))}.`);
      lines.push(`Costs ${m.cost} political capital to order (you have ${capital}), and ${m.hold} a month to hold. The month there is not enough it slips back a step.`);
      if (m.will) lines.push(`Rationing and empty shops wear the will to fight: ${pct(m.will.year)} in a year, ${pct(m.will.most)} at most. Stepping down mends it.`);
      if (m.trust !== null) lines.push(`The war cabinet's trust falls ${Math.abs(m.trust)} a month while it holds.`);
      lines.push(`It can be stepped down at once and free, but not raised again for ${months(m.again)} after that.`);
    } else {
      lines.push(m.brings > -1e-6 ? 'At once and free: it has brought nothing yet (the climb takes its first step as the month turns), and what it costs to hold ends now.' : `At once and free: what it brought, ${cash(-m.brings)} a month, ends now, and so does what it costs to hold.`);
      lines.push(`No footing can be raised again for ${months(m.again)}.`);
    }
    ask = `<h3>${m.up ? 'Go over to' : 'Step down to'} ${esc(m.name.toLowerCase())}?</h3><ul>${lines.map((l) => `<li>${esc(l)}</li>`).join('')}</ul>
      ${m.why ? `<p class="sub">${esc(m.why)}</p>` : ''}
      <div class="acts"><button class="btn small go" data-act="footingdo" data-to="${m.to}" ${m.why ? 'disabled' : ''}>${m.up ? `Order ${esc(m.name.toLowerCase())}` : `Step down to ${esc(m.name.toLowerCase())}`}</button><button class="btn small" data-act="close">Not now</button></div>`;
  }
  const worn = v.worn > 0.005 ? `<p class="sub"${v.amber ? ' style="color:var(--amber, #c8902a)"' : ''}>Worn away by all-out war: ${pct(v.worn)} of the will to fight (${pct(FOOTING.allout.wear * 12)} in a year, ${pct(FOOTING.allout.most)} at most).</p>` : '';
  return `<header><button class="x" data-act="close" aria-label="Close">×</button><div class="kick">The nation's footing</div><h2>${esc(v.name)}</h2></header>
    <div class="body">
      <p>${esc(v.line)}.</p>${worn}
      <table class="ltab"><tr><td><b>Footing</b></td><td style="text-align:right"><b>One shift = 100</b></td><td></td></tr>${rows}</table>
      ${ask}
    </div>`;
}
