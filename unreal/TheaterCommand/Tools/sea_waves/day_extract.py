#!/usr/bin/env python3
"""Today's light and haze, cut out of the game for the cloud (the release 2 ocean plan, step 3: the light core).

sea_day_test.cpp holds SeaDay::LookOf's seed mode to the scene's own numbers bit for bit. It is not held to copies: this
cuts the game's text out as it stands (each piece found exactly once, by its first line, a whole function to its closing
brace, comments and strings read as such; or a run of lines from one first line to another), and day_shim.h stands in
for the engine names it uses. Each piece carries a #line back to the game's file, so a compile error in it points at the
game's own line. One file is written into OUT, day_cut.inc, included inside the test's namespace Today:
  - SeaParts (Battle/SeaBattle.cpp): SunAt, HazeInk, SkyOnScreen, SkyColour, GradeOfTheFit, GradeNow, LightOfStill and
    SeaSkyLight;
  - OpenOnSun: OpenOn's hour of the day, from its seed or -TheaterSeaHour=, the sun at it and its 12 to 70 degree clamp;
  - RoomFog: TakeTheRoom's fog, from its 2 km start to BattleLook::AirHaze;
  - WantOf, SkyWantOf, KelvinAfter: LightTheBattle's key and fills, the sky light and the key's warmth;
  - BattleLook (Campaign/CampaignBattleLook.{h,cpp}): AirHaze, with its default 15 km at twice, and the switch reader it
    uses;
  - AirParts and AirFog (Battle/AirBattle.cpp): AirHazeInk, SeaHazeInk and the air scene's haze, from 30 km at 1.2 times
    over the sea;
  - AAirStage (Battle/AirStage.cpp): SetSeaWind and MovingSea's sky and haze parameters (SkyColour, SkyHigh, HazeReach,
    HazeTop);
  - SeaUnderParts and ReadSunDaylight (Battle/SeaUnder.cpp): KeyLux and ReadSun's day under the water.

Usage: python3 -I -B Tools/sea_waves/day_extract.py SRC OUT   (SRC: Source/TheaterCommand)
"""

import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from marks_extract import Missing, block, closing, line_start, once  # noqa: E402  (the same cutting rules)


def single(text, head, where):
    """The one line whose text (after its indent) begins with head."""
    at = once(text, head, where)
    start = line_start(text, at)
    end = text.find("\n", at)
    return text[start:end], text.count("\n", 0, start) + 1


def run(text, first, last, where):
    """The lines from the one beginning first to the one beginning last, both whole."""
    a = once(text, first, where)
    b = once(text, last, where)
    if b < a:
        raise Missing("%s: %r comes before %r" % (where, last, first))
    start = line_start(text, a)
    end = text.find("\n", b)
    return text[start:end], text.count("\n", 0, start) + 1


class Out:
    def __init__(self, path):
        self.path = path
        self.lines = []

    def own(self, text):
        """The test's own lines (a #line back to this file first)."""
        self.lines.append('#line %d "%s"' % (len(self.lines) + 2, self.path))
        self.lines.extend(text.split("\n"))

    def cut(self, piece, origin):
        text, first = piece
        self.lines.append('#line %d "%s"' % (first, origin))
        self.lines.extend(text.split("\n"))

    def write(self):
        with open(self.path, "w", encoding="utf-8") as f:
            f.write("\n".join(self.lines) + "\n")


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    src, out_dir = sys.argv[1], sys.argv[2]
    os.makedirs(out_dir, exist_ok=True)
    files = {
        "sea": os.path.join(src, "Battle", "SeaBattle.cpp"),
        "air": os.path.join(src, "Battle", "AirBattle.cpp"),
        "stage": os.path.join(src, "Battle", "AirStage.cpp"),
        "under": os.path.join(src, "Battle", "SeaUnder.cpp"),
        "look": os.path.join(src, "Campaign", "CampaignBattleLook.cpp"),
        "look_h": os.path.join(src, "Campaign", "CampaignBattleLook.h"),
    }
    files = {k: os.path.abspath(v) for k, v in files.items()}
    text = {k: open(v, encoding="utf-8").read() for k, v in files.items()}
    out = Out(os.path.abspath(os.path.join(out_dir, "day_cut.inc")))
    cut = []

    def take(kind, key, *heads, tail=""):
        where = os.path.basename(files[key])
        if kind == "block":
            piece = block(text[key], heads[0], where, tail)
        elif kind == "single":
            piece = single(text[key], heads[0], where)
        else:
            piece = run(text[key], heads[0], heads[1], where)
        cut.append((where, heads[0], piece))
        out.cut(piece, files[key])

    try:
        out.lines.append("// Cut out of the game's source by Tools/sea_waves/day_extract.py: the game's own text, not a copy to keep.")
        # (SeaBattle.cpp's SeaParts)
        out.own("namespace SeaParts\n{")
        take("block", "sea", "static void SunAt(double Latitude, int32 Month0, double Hour, double& OutAzimuth, double& OutElevation)")
        take("single", "sea", "static const FLinearColor HazeInk(")
        take("single", "sea", "static const FLinearColor SkyOnScreen(")
        take("block", "sea", "static FLinearColor SkyColour()")
        take("single", "sea", "static const FVector2D GradeOfTheFit(")
        take("block", "sea", "static FVector2D GradeNow()")
        take("block", "sea", "static FLinearColor LightOfStill(const FLinearColor& Still, const FVector2D& Grade)")
        take("block", "sea", "static FLinearColor SeaSkyLight()")
        out.own("}")
        # (CampaignBattleLook's AirHaze and its switch reader)
        take("block", "look", "void Asked(const TCHAR* Switch, TArray<double*> Kept)")
        out.own("namespace BattleLook\n{")
        take("single", "look_h", "void AirHaze(UExponentialHeightFogComponent& Haze, double UnitsPerMetre, double Start = ")
        out.own("}")
        take("block", "look", "void BattleLook::AirHaze(UExponentialHeightFogComponent& Haze, double UnitsPerMetre, double Start, double Thicker)")
        # (AirBattle.cpp's AirParts)
        out.own("namespace AirParts\n{")
        take("single", "air", "static const FLinearColor AirHazeInk(")
        take("block", "air", "static FLinearColor SeaHazeInk()")
        out.own("}")
        # (SeaUnder.cpp's SeaUnderParts)
        out.own("namespace SeaUnderParts\n{")
        take("single", "under", "static constexpr double KeyLux = ")
        out.own("}")
        # OpenOn's sun
        out.own("struct FOpenOnRec\n{\n\tstruct FRec\n\t{\n\t\tstruct FBoard\n\t\t{\n\t\t\tdouble Lat = 45.0;\n\t\t\tint32 Month = 4;\n\t\t} Board;\n\t} Rec;\n};\n"
                "inline void OpenOnSun(const FOpenOnRec& S, uint64 Seed, double& OutHour, double& OutFrom, double& OutUp)\n{\n\tusing namespace SeaParts;")
        take("run", "sea", "double Hour = 8.0 + static_cast<double>((Seed * 2654435761ull) % 7000ull) / 1000.0;",
             "SunElevation = FMath::Clamp(SunElevation, 12.0, 70.0);")
        out.own("\tOutHour = Hour;\n\tOutFrom = SunAzimuth;\n\tOutUp = SunElevation;\n}")
        # TakeTheRoom's fog
        out.own("inline void RoomFog(UExponentialHeightFogComponent* Fog, double UnitsPerMetre)\n{")
        take("run", "sea", "Fog->SetStartDistance(2000.0f * static_cast<float>(UnitsPerMetre));", "BattleLook::AirHaze(*Fog, UnitsPerMetre);")
        out.own("}")
        # LightTheBattle's lamps
        out.own("inline float WantOf(bool bMain, const FUnderStandIn* Under)\n{")
        take("single", "sea", "const float Want = (bMain ? 7.0f : 0.75f)")
        out.own("\treturn Want;\n}\ninline float SkyWantOf(const FUnderStandIn* Under)\n{")
        take("single", "sea", "const float SkyWant = 0.72f")
        out.own("\treturn SkyWant;\n}\ninline float KelvinAfter(bool bMain, FLampStandIn* Light)\n{")
        take("block", "sea", "if (bMain && !FMath::IsNearlyEqual(Light->Temperature, 4900.0f, 1.0f))")
        out.own("\treturn Light->Temperature;\n}")
        # The air scene's haze over the sea
        out.own("inline void AirFog(UExponentialHeightFogComponent* Fog, double UnitsPerMetre, const FAirStateStandIn& State)\n{")
        take("run", "air", "Fog->SetStartDistance(2000.0f * static_cast<float>(UnitsPerMetre));",
             "BattleLook::AirHaze(*Fog, UnitsPerMetre, bOverLand ? 7500.0 : 30000.0, bOverLand ? 2.6 : 1.2);")
        out.own("}")
        # The stage's sea material
        out.own("struct AAirStage\n{\n\tdouble SeaDownwind = 0.0, SeaWind = 0.0;\n\tFLinearColor SeaSky;\n\tFVector2D SeaGrade;\n"
                "\tdouble UnitsPerMetre = 100.0;\n\tFVector Scale = FVector(1.0, 1.0, 1.0);\n\tFVector GetActorScale3D() const { return Scale; }\n"
                "\tvoid SetSeaWind(double Downwind, double Speed, const FLinearColor& SkyAtHorizon, const FVector2D& GradeRedBlue);\n"
                "\tvoid MovingSeaParams(FMaterialStandIn* Moving)\n\t{\n\t\tfloat Reflect = 1.0f;")
        take("run", "stage", "const FLinearColor Sky = SeaSky.A > 0.0f ? SeaSky : FLinearColor(",
             'Moving->SetScalarParameterValue(TEXT("HazeTop"), FMath::Clamp(Haze, 0.0f, 1.5f));')
        out.own("\t}\n};")
        take("block", "stage", "void AAirStage::SetSeaWind(double Downwind, double Speed, const FLinearColor& SkyAtHorizon, const FVector2D& GradeRedBlue)")
        # The day under the water
        out.own("struct FKeyStandIn\n{\n\tfloat Intensity = 7.0f;\n};\ninline double ReadSunDaylight(double SunUp, const FKeyStandIn* Key)\n{\n"
                "\tdouble Share = 1.0, Daylight = 0.0;\n\tif (Key)\n\t{")
        take("single", "under", "Share = FMath::Clamp(Key->Intensity / SeaUnderParts::KeyLux")
        out.own("\t}")
        take("single", "under", "Daylight = FMath::Clamp(SeaUnder::DaylightOf(SunUp) * Share")
        out.own("\treturn Daylight;\n}")
    except Missing as e:
        print("day_extract: the game's source no longer reads as this cuts it: %s" % e)
        return 1
    out.write()
    for where, head, (piece, first) in cut:
        print("  cut  %s: %s: lines %d-%d" % (where, head[:70], first, first + piece.count("\n")))
    print("day_extract: %d pieces of the game's source cut out into %s" % (len(cut), out.path))
    return 0


if __name__ == "__main__":
    sys.exit(main())
