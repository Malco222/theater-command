// The war correspondent's front page (release 2) as the browser test kitchen draws it: plain, for reading the words.
// The page's content is frontPage's (../frontpage.ts); its look belongs to the 3D game.
import type { FrontPage, FrontStory } from '../frontpage';

export function paperHtml(p: FrontPage, esc: (s: string) => string): string {
  const story = (s: FrontStory, big: boolean): string => `<div style="margin:0 0 12px">
    <div style="font:600 10px/1.2 sans-serif;letter-spacing:.12em;color:#6b5a3a">${esc(s.kicker)}</div>
    <div style="font:700 ${big ? 26 : 16}px/1.1 Georgia,serif;margin:2px 0 4px">${esc(s.head)}</div>
    ${s.decks.map((d) => `<div style="font:italic ${big ? 15 : 13}px/1.25 Georgia,serif;margin:0 0 3px">${esc(d)}</div>`).join('')}
    ${big ? `<div style="font:600 11px/1.3 sans-serif;margin:6px 0">${esc(p.byline)}</div>` : ''}
    ${s.body.map((t, i) => `<p style="font:13px/1.45 Georgia,serif;margin:0 0 6px">${i === 0 && s.dateline ? `<b>${esc(s.dateline)}</b> ` : ''}${esc(t)}</p>`).join('')}</div>`;
  return `<div style="background:#f3ecd9;color:#1d1a14;padding:18px 22px;max-width:980px">
    <div style="display:flex;justify-content:space-between;font:11px sans-serif;border-bottom:1px solid #1d1a14;padding-bottom:4px"><span>${esc(p.issue)}</span><span>${esc(p.edition)}</span><span>${esc(p.price)}</span></div>
    <div style="text-align:center;font:700 44px/1.1 Georgia,serif;margin:8px 0 2px">${esc(p.paper)}</div>
    <div style="text-align:center;font:italic 12px Georgia,serif">${esc(p.motto)}${p.translated ? ` · ${esc(p.translated)}` : ''}</div>
    <div style="display:flex;justify-content:space-between;font:12px sans-serif;border-top:3px double #1d1a14;border-bottom:1px solid #1d1a14;margin-top:6px;padding:3px 0"><span>${esc(p.city.toUpperCase())}</span><span>${esc(p.date)}</span></div>
    ${p.banner ? `<div style="text-align:center;font:900 34px/1.05 Georgia,serif;margin:10px 0 2px">${esc(p.banner.head)}</div>${p.banner.sub ? `<div style="text-align:center;font:italic 15px Georgia,serif;margin-bottom:8px">${esc(p.banner.sub)}</div>` : ''}` : ''}
    <div style="display:grid;grid-template-columns:2fr 1fr;gap:18px;margin-top:10px">
      <div>${p.lead ? story(p.lead, true) : `<p style="font:italic 15px Georgia,serif">${esc(p.quiet)}</p>`}
        ${p.picture ? `<div style="background:#cfc4a8;height:120px;display:flex;align-items:center;justify-content:center;font:11px sans-serif;color:#5b4f37">[photograph: ${esc(p.picture)}]</div><div style="font:italic 11px Georgia,serif;margin:3px 0 10px">${esc(p.caption)}</div>` : ''}
        ${p.letter ? `<div style="border-top:1px solid #1d1a14;padding-top:6px"><div style="font:700 13px Georgia,serif">${esc(p.letter.head)}</div><div style="font:italic 11px Georgia,serif">${esc(p.letter.intro)}</div><p style="font:13px/1.45 Georgia,serif">"${esc(p.letter.text)}" <i>${esc(p.letter.by)}</i></p></div>` : ''}
      </div>
      <div style="border-left:1px solid #1d1a14;padding-left:14px">${p.stories.map((s) => story(s, false)).join('')}
        ${p.briefs.length ? `<div style="border-top:1px solid #1d1a14;padding-top:6px"><div style="font:700 12px sans-serif;letter-spacing:.1em">THE WAR ELSEWHERE</div><ul style="font:12px/1.4 Georgia,serif;padding-left:16px;margin:4px 0">${p.briefs.map((b) => `<li>${esc(b)}</li>`).join('')}</ul></div>` : ''}
        ${p.figures.length ? `<div style="border-top:1px solid #1d1a14;padding-top:6px"><div style="font:700 12px sans-serif;letter-spacing:.1em">THE MONTH IN FIGURES</div>${p.figures.map((f) => `<div style="display:flex;justify-content:space-between;font:12px Georgia,serif"><span>${esc(f.label)}</span><b>${esc(f.n)}</b></div>`).join('')}</div>` : ''}
      </div>
    </div>
    <div style="display:flex;justify-content:space-between;align-items:center;border-top:1px solid #1d1a14;margin-top:8px;padding-top:6px;font:italic 11px Georgia,serif"><span>${esc(p.censor)}</span>
      <span><button class="mc-alt" data-act="backtomonth">THE MONTH'S REPORT</button> <button class="mc-fly" data-act="close">CONTINUE ▸</button></span></div>
  </div>`;
}
