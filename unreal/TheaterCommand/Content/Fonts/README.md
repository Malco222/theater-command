# Fonts

PT Serif (Regular, Italic, Bold), by ParaType, under the SIL Open Font License 1.1. The licence is in `OFL.txt`
beside this file and must stay with the fonts.

The files are unchanged copies of `PT_Serif-Web-Regular.ttf`, `PT_Serif-Web-Italic.ttf` and `PT_Serif-Web-Bold.ttf`
from the Google Fonts repository (github.com/google/fonts, folder `ofl/ptserif`), fetched on 2026-11-13. Only the
file names are shortened.

The game uses them for the lettering on the map and the town plates (`Serif()` in `CampaignHUD.cpp`). If they are
missing it falls back to the copy of PT Serif that ships with macOS.

Courier Prime (Regular, Bold), by Alan Dague-Greene for Quote-Unquote Apps, under the SIL Open Font License 1.1.
Its licence is in `CourierPrime-OFL.txt` beside this file and must stay with the fonts.

The files are unchanged copies of `CourierPrime-Regular.ttf` and `CourierPrime-Bold.ttf` from the Google Fonts
repository (github.com/google/fonts, folder `ofl/courierprime`), fetched on 2026-11-14.

The game uses them for everything typed: figures, orders, dispatches and the small print of the frame (`Typed()`
in `CampaignHUD.cpp`). If they are missing it falls back to the engine's own fixed-width face.
