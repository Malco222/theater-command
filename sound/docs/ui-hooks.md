# The interface's sound hooks, and the sound that answers each

Sound director, 16 November 2026, with the UI director (`ui-director/ui-style-guide.md`, "Sound hooks").

**How the two lists fit.** The UI director names the moments the interface raises (`ui.press`). The sound set names the files (`click`). Neither is renamed: the builders fire the hook, and this table says which file plays. It is the one list both builders work from. Hook names are the UI director's to keep; file names are the sound director's.

There are thirty-six hooks: the UI director's first twenty, and sixteen moments the sound set already had sounds for, which the UI director adopted under the names suggested here.

## The first twenty

| Hook | Sound | Notes |
|---|---|---|
| `ui.press` | `click` | A bakelite switch: short, soft, the commonest sound in the game |
| `ui.primary` | `ui_primary` | The same click, heavier, with brass in it and a release |
| `ui.toggle` | `switch_on` when it turns something on, `switch_off` when off | The hook carries its direction. A segment with three or more choices fires `ui.tab` |
| `ui.tab` | `ui_tab` | A card flicked forward in an index |
| `ui.open.panel` | `screen_open` | A drawer run out and a book set down; the book lands at 160 ms, as the panel finishes rising |
| `ui.close.panel` | `screen_close` | The book shut, the drawer pushed home. Also the close stud and Esc on a page |
| `ui.open.card` | `card_open` | Paper slid onto the desk; it lands at 160 ms |
| `ui.close.card` | `card_close` | The same slide backwards, lighter. Also the close stud and Esc on a card |
| `ui.error` | `ui_error` | Two dull knocks on wood. Never a buzzer |
| `ui.coin` | `coin_run` | Four coins in under half a second, inside the 400 ms count |
| `ui.order.placed` | `stamp_light` | The stamp, lightly, as a clerk marks a chit. For something bought or ordered in the arsenal or a city, not a move on the map |
| `ui.research.start` | `pen_sign` | A nib across paper and a full stop |
| `ui.research.done` | `research_done` | Three small bells climbing |
| `ui.alert.in` | `alert_slip` | A telegraph sounder, three strokes. Once for a batch of slips, not once a slip |
| `ui.stamp.won` | `stamp` | Fired when the stamp lands (220 ms after it starts to fall), not when it starts. The hit is in the sound's first 10 ms |
| `ui.stamp.lost` | `stamp_low` | The same stamp, lower and heavier |
| `ui.band.war` | `drum_low` | One low drum stroke. When the moment has its own call (`war_declared`, `nation_falls`), the call plays and this is skipped |
| `ui.month.end` | `month_end` | The calendar page torn off, the day book shut, one stroke of the clock |
| `ui.month.ready` | `switch_on`, 6 dB down | A lamp switch, quiet |
| `ui.dial` | `ui_dial` | A detent's small sharp click |

## The sixteen adopted on 16 November

| Hook | Sound | When |
|---|---|---|
| `ui.back` | `ui_back` | Cancel, or a step back to the screen before: the click, lower, so that going back sounds like going back. Not the close stud or Esc |
| `ui.page` | `page_turn` | A page of the ledger, the guide or the roll of honour; one report to the next |
| `ui.select` | `select_region` | A province chosen on the map |
| `ui.piece.up` | `pickup` | An army or a fleet taken in hand |
| `ui.piece.down` | `putdown` | It is set down (PUT DOWN) |
| `ui.order.march` | `order_march` | A move pencilled on the map |
| `ui.order.attack` | `order_attack` | An attack pencilled: two hard strokes |
| `ui.order.cancel` | `order_cancel` | An order rubbed out |
| `ui.sign` | `pen_sign` | A treaty or a promise signed |
| `ui.month.ask` | `month_ask` | The end-of-month question rises. It takes the place of `ui.open.card` and of the button's own click |
| `ui.month.passing` | `month_passing_loop` | While the other countries move. The only hook that loops; it stops the moment `ui.month.yours` fires (a 0.3 s fade) |
| `ui.month.yours` | `month_yours` | The others have moved; the month is the player's |
| `ui.month.card` | `month_card_open`, then `month_card_good`, `month_card_bad` or `month_card_quiet` | The month card is laid down; the second sound follows 0.6 s later, by the card's news. It takes the place of `ui.open.card` |
| `ui.note.in` | `note_arrives` | First Command: a staff note arrives |
| `ui.note.done` | `note_done` | First Command: the note is stamped DONE. Fired when the stamp lands, like the other stamps; the hit is in the sound's first 10 ms and a small bell follows at 200 ms |
| `ui.note.closing` | `note_closing` | First Command: the closing note |

## The rules

All are the UI director's, adopted as they stand and written into the sound brief and both builders' plans.

1. One hook to an act, and the same sound never faster than eight a second.
2. Nothing fires on pointer-over.
3. `ui.error` always comes with the reason in words.
4. When a moment has a hook of its own, that one fires and the general one does not. The end-of-month question fires `ui.month.ask` and the month card fires `ui.month.card`, not `ui.open.card`. Cancel fires `ui.back`, PUT DOWN fires `ui.piece.down` and a signature fires `ui.sign`, not `ui.press` or `ui.primary`. Ending the month fires `ui.month.end`, not `ui.primary`.
5. The close stud and Esc on a page or card fire `ui.close.panel` or `ui.close.card`, not `ui.back`.
6. `ui.month.passing` is the only hook that loops.
7. The three stamps (`ui.stamp.won`, `ui.stamp.lost`, `ui.note.done`) fire when the stamp lands.

None of these fights the way a sound was cut: every sound that answers a landing has its hit at the very start, and every sound that takes the place of a click (`month_ask`, `month_end`, `putdown`, `pen_sign`) begins within 40 ms of being fired and needs no click before it.

**Timings the sounds were cut to.** A panel or a card lands at 160 ms (`screen_open`, `card_open`); a stamp lands at 220 ms (the stamps are single hits, fired at the landing); the treasury counts inside 400 ms (`coin_run` is four coins in under half a second). If a motion timing changes, the sound beside it must be cut again.

Everything else in the sound set (engines, guns, trains, the band's calls, the ambience) is raised by the game's events, not by the interface, and is listed by event in `implementation-3d.md` and `implementation-browser.md`.
