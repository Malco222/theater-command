// air_eye_probe: the strategy camera the player sees over a raid, measured frame by frame with no engine (Brian, 26
// November 2026 04:11Z, watching a raid: "the bombing video camera is super choppy"). It plays a recording through the
// game's own clock (AirDesk::Clock), view reader (AirFeed), flight core (AirShow), flyover (AirDesk::Flyover) and, with
// the raid camera's row on, AirDesk::Overview, and runs AAirBattle::AimEye's strategy camera as the game runs it
// (re-written here line by line, for AimEye needs the engine): with the row off, the hard rings as 30899121 had them;
// on, the three places AimEye asks AirDesk::Overview (the start afresh after a cut, the crowd and framing, the look
// carried with its knot). run.sh builds it with -DROWS -DBOXROW -DMARKS and holds its strategy camera's frames
// (<prefix>-over.csv) over Bremen's B-17s, at 60 frames x1, to the byte, against 30899121's and 2464374bb's:
//   CAMROW_EYE=1 the raid camera's row on (the Battle rules card's flySteadyEye), CAMROW_BOX=1 the bombers' row on
//   (flyBoxKeep, AirShow::Show::SetBoxKeep); unset, each off.
// It was measured first as camera/choppy1126's cam_probe (the notes below are its own).
//
// cam_probe: the cameras the player sees in an air battle, as AAirBattle::Tick / Feed / AimEye run them, driven by the
// game's own clock (AirDesk::Clock), view reader (AirFeed), flight core (AirShow) and flyover (AirDesk::Flyover), with
// no engine. Built against the Play build's tree (cd095d63) or, with -DFIXLAW, the fix's (6e78163a: AirDesk::Overview).
//
//   cam_probe live <seed> <prefix> <x1|half|halfat:<real s>> [long-frames file]
//        the rules live in node (serve.cjs, the owner's spec), stepped as Feed steps them (one view back a call)
//   cam_probe rec <recording.json> <prefix> <x1|half|halfat:<real s>> [long-frames file]
//        a replay: each frame every recorded view up to BattleT + 1.0 (Feed's replay branch)
//
// Cameras: "over" (the strategy camera with no group in hand, at the player's own zoom 800 m, pitch -12, as AimEye
// 6700-7083), preceded by the flyover's shots and its glide (AimEye 6498-6520, 7063-7082); "close" (the automatic ride,
// AimEye 6526-6697) measured beside it from the glide's end. Show.Eye follows "over", as Tick 3690-3694 sets it.
// Writes <prefix>-over.csv, <prefix>-close.csv (one row a frame) and prints a summary by phase.
#include "Battle/AirDesk.h"
#include "Battle/AirFeed.h"
#include "Battle/AirGround.h"
#include "Battle/AirShow.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

#if defined(FIXLAW) || defined(MARKS) || defined(ROWS)
#define TAKE(F) Show.Take((F).Head.T, (F).Groups, (F).Events, (F).Ships, &(F).Marks)
#else
#define TAKE(F) Show.Take((F).Head.T, (F).Groups, (F).Events, (F).Ships)
#endif

#ifdef ROWS
// (camrow1126: the rows at run time: CAMROW_EYE=1 the raid camera row on (AirDesk::Overview), CAMROW_BOX=1 the
// bomber row on (AirShow::Show::SetBoxKeep))
static const bool gFix = std::getenv("CAMROW_EYE") && std::string(std::getenv("CAMROW_EYE")) == "1";
static const bool gBox = std::getenv("CAMROW_BOX") && std::string(std::getenv("CAMROW_BOX")) == "1";
#endif
namespace
{
	constexpr double Pi = 3.14159265358979323846;
	constexpr double ScreenW = 1440.0, ScreenH = 878.0;
	double Rad(double D) { return D * Pi / 180.0; }
	double Deg(double R) { return R * 180.0 / Pi; }
	struct V3
	{
		double X = 0, Y = 0, Z = 0;
		V3() = default;
		V3(double A, double B, double C) : X(A), Y(B), Z(C) {}
		V3 operator+(const V3& O) const { return V3(X + O.X, Y + O.Y, Z + O.Z); }
		V3 operator-(const V3& O) const { return V3(X - O.X, Y - O.Y, Z - O.Z); }
		V3 operator*(double S) const { return V3(X * S, Y * S, Z * S); }
		double Dot(const V3& O) const { return X * O.X + Y * O.Y + Z * O.Z; }
		double Size() const { return std::sqrt(X * X + Y * Y + Z * Z); }
		V3 Safe() const { const double S = Size(); return S < 1e-8 ? V3() : *this * (1.0 / S); }
	};
	V3 Cross(const V3& A, const V3& B) { return V3(A.Y * B.Z - A.Z * B.Y, A.Z * B.X - A.X * B.Z, A.X * B.Y - A.Y * B.X); }
	V3 World(const AirShow::Vec& P) { return V3(P.Y, P.X, P.Z); }
	AirShow::Vec Board(const V3& W) { return AirShow::Vec(W.Y, W.X, W.Z); }
	struct Rot { double P = 0, Y = 0, R = 0; };
	void Axes(const Rot& Ro, V3& F, V3& Ri, V3& Up)
	{
		const double SP = std::sin(Rad(Ro.P)), CP = std::cos(Rad(Ro.P));
		const double SY = std::sin(Rad(Ro.Y)), CY = std::cos(Rad(Ro.Y));
		const double SR = std::sin(Rad(Ro.R)), CR = std::cos(Rad(Ro.R));
		F = V3(CP * CY, CP * SY, SP);
		Ri = V3(SR * SP * CY - CR * SY, SR * SP * SY + CR * CY, -SR * CP);
		Up = V3(-(CR * SP * CY + SR * SY), CY * SR - CR * SP * SY, CR * CP);
	}
	V3 Forward(const Rot& R) { V3 F, A, B; Axes(R, F, A, B); return F; }
	Rot RotationOf(const V3& V) { Rot R; R.Y = Deg(std::atan2(V.Y, V.X)); R.P = Deg(std::atan2(V.Z, std::sqrt(V.X * V.X + V.Y * V.Y))); return R; }
	double Norm180(double A) { A = std::fmod(A, 360.0); if (A < 0) A += 360.0; if (A > 180.0) A -= 360.0; return A; }
	Rot RInterpTo(const Rot& Cur, const Rot& Tgt, double Dt, double Speed)
	{
		if (Dt == 0.0) return Cur;
		const double K = std::clamp(Speed * Dt, 0.0, 1.0);
		Rot D{ Norm180(Tgt.P - Cur.P), Norm180(Tgt.Y - Cur.Y), Norm180(Tgt.R - Cur.R) };
		if (std::fabs(D.P) < 1e-4 && std::fabs(D.Y) < 1e-4 && std::fabs(D.R) < 1e-4) return Tgt;
		return Rot{ Norm180(Cur.P + D.P * K), Norm180(Cur.Y + D.Y * K), Norm180(Cur.R + D.R * K) };
	}
	double Smooth(double Real, double Seconds) { return 1.0 - std::exp(-Real / std::max(0.01, Seconds)); }
	struct Cam { V3 Eye; Rot Face; double Fov = 55.0; bool bValid = false; };
	bool Project(const Cam& C, const V3& P, double& Sx, double& Sy)
	{
		V3 F, R, U; Axes(C.Face, F, R, U);
		const V3 D = P - C.Eye;
		const double X = D.Dot(F), Y = D.Dot(R), Z = D.Dot(U);
		if (X < 1.0) return false;
		const double Fpx = (ScreenW * 0.5) / std::tan(Rad(C.Fov * 0.5));
		Sx = ScreenW * 0.5 + Y / X * Fpx; Sy = ScreenH * 0.5 - Z / X * Fpx;
		return true;
	}
	// a vector slerp of two look ways (the glide's FQuat::Slerp with no roll)
	V3 Slerp(const V3& A, const V3& B, double T)
	{
		const double D = std::clamp(A.Safe().Dot(B.Safe()), -1.0, 1.0);
		const double W = std::acos(D);
		if (W < 1e-6) return B.Safe();
		return (A.Safe() * (std::sin((1 - T) * W) / std::sin(W)) + B.Safe() * (std::sin(T * W) / std::sin(W))).Safe();
	}

	FILE* ToNode = nullptr;
	FILE* FromNode = nullptr;
	std::string Line()
	{
		std::string S;
		int C;
		while ((C = std::fgetc(FromNode)) != EOF && C != '\n') S.push_back(static_cast<char>(C));
		return S;
	}
	struct Long { std::string Kind; double At = 0.0, Ms = 0.0; bool bDone = false; };

	struct Meter
	{
		double Real = 0; long Frames = 0;
		long J20 = 0, J50 = 0, JCam20 = 0, JWorld20 = 0, Hop20 = 0, Zoom2 = 0, Zoom10 = 0, Cuts = 0, PickCh = 0, AnchorCh = 0, PaceCh = 0, LongF = 0, LongJ = 0, Measured = 0;
		double JMax = 0, HopMax = 0, ZoomMax = 0;
		std::vector<double> Jall;
		// (choppy1126 extension) the eye's place, aim and lens: largest one-frame step beyond its own motion (second
		// difference), pans (look faster than 500 m a real second, counted once an episode), shape and shadow swaps of
		// aircraft on the picture (near <300 m / >450 m, light past 1500 m x lens, shadow within 1000 m, as Draw)
		double EyeMax = 0, AimMax = 0, FovMax = 0, LookSpdMax = 0, PanKmSum = 0;
		long Pans = 0, NearSw = 0, LightSw = 0, ShadowSw = 0, SwapFrameMax = 0, SwapFrames = 0, Eye5 = 0, Aim1 = 0;
		// (choppy1126 film) the other side's fighters on the picture: their own one-frame hop (second difference seen
		// through this frame's eye) over 20 px, counted per aircraft; every aircraft the same; the screen hop of each
		long GSeen = 0, GHop = 0, GHopFrames = 0, AHop = 0, ASeen = 0, GScr20 = 0; double GHopMax = 0, AHopMax = 0;
	};
	struct Looks { bool bNear = false, bLight = false, bShadow = false; };
	struct Camera
	{
		std::string Kind;
		Cam Now, Prev; bool bCutNow = false; double Away = 0, PrevAway = 0; int CutAgo = 99;
		V3 LookAt, LookWanted, Look1, Look2; bool bLookSet = false; int Anchor = -1;
		std::vector<int> PickedWas; int TightWas = -1; double TypeAwayWas = -1; bool bFilmBack = false;
		// close
		AirDesk::Subject Subject; V3 CloseEye, CloseAhead; Rot CloseFacing; bool bCloseCut = true;
		std::map<int, std::pair<double, double>> P1, P2; std::map<int, V3> W1, W2; double Dt1 = 0;
		std::ofstream Csv;
		std::map<std::string, Meter> M;
		V3 Eye1, Eye2, Aim1v, Aim2v; double Fov1 = -1; int Steady = 0; bool bPanning = false; double PanFrom = 0; V3 PanStart;
		std::map<int, Looks> Shapes;
#if defined(FIXLAW) || defined(ROWS)
		AirDesk::Overview Over;
#endif
	};
}

int main(int Argc, char** Argv)
{
	if (Argc < 5) { std::fprintf(stderr, "usage: cam_probe live <seed>|rec <file> <prefix> <x1|half|halfat:s> [long-frames]\n"); return 2; }
	const std::string Source = Argv[1], What = Argv[2], Prefix = Argv[3], Mode = Argv[4];
	const bool bLive = Source == "live";
	const bool bHalf = Mode == "half";
	const bool bTwo = Mode == "x2";
	// (choppy1126) LONGMIX=<period s>: from 15 real s on, every period a long frame of 100, 150, 220 ms in turn
	const double LongMix = std::getenv("LONGMIX") ? std::atof(std::getenv("LONGMIX")) : 0.0;
	double LongMixNext = 15.0; int LongMixK = 0;
	const double HalfAt = Mode.rfind("halfat:", 0) == 0 ? std::atof(Mode.c_str() + 7) : -1.0;
	std::vector<Long> Longs;
	if (Argc > 5)
	{
		std::ifstream In(Argv[5]);
		std::string K;
		while (In >> K) { Long L; L.Kind = K; if (K == "appear") In >> L.Ms; else In >> L.At >> L.Ms; Longs.push_back(L); }
	}
	AirFeed::Sky Sky;
	AirFeed::Frame First;
	AirFeed::Recording Rec;
	std::string Why;
	pid_t Child = -1;
	if (bLive)
	{
		int A[2], B[2];
		if (pipe(A) || pipe(B)) return 3;
		Child = fork();
		if (Child == 0)
		{
			dup2(A[0], 0); dup2(B[1], 1); close(A[1]); close(B[0]);
			execlp("node", "node", "--stack-size=8000", "serve.cjs", What.c_str(), static_cast<char*>(nullptr));
			_exit(127);
		}
		close(A[0]); close(B[1]);
		ToNode = fdopen(A[1], "w"); FromNode = fdopen(B[0], "r");
		const std::string Head = Line();
		if (Head.rfind("OPEN", 0) != 0) { std::fprintf(stderr, "no opening: %s\n", Head.c_str()); return 4; }
		const std::string Opening = Line();
		AirFeed::Json Opened;
		if (!AirFeed::Parse(Opening, Opened, Why) || !AirFeed::ReadSky(Opened["sky"], Sky) || !AirFeed::ReadView(Opened["view"], Sky, First)) { std::fprintf(stderr, "opening: %s\n", Why.c_str()); return 5; }
	}
	else
	{
		std::ifstream In(What, std::ios::binary);
		std::stringstream Buf; Buf << In.rdbuf();
		if (!AirFeed::ReadRecording(Buf.str(), Rec, Why)) { std::fprintf(stderr, "%s\n", Why.c_str()); return 2; }
		Sky = Rec.Board;
		First = Rec.Views.front();
	}
	AirShow::Show Show(bLive ? 238 : (Rec.Seed ? Rec.Seed : 238));
	AirFeed::Apply(Sky, Show);
	Show.SetSmooth(true);
#if defined(ROWS) && defined(BOXROW)
	Show.SetBoxKeep(gBox);
#endif
	Show.Eye = AirShow::Vec(Sky.TargetX, Sky.TargetY - 6000.0, 5000.0);
	const std::shared_ptr<const AirGround::BoardLand> BoardGround = Sky.StageKey.empty() ? std::make_shared<const AirGround::BoardLand>(Sky.Land) : std::shared_ptr<const AirGround::BoardLand>();
	if (BoardGround && !BoardGround->IsEmpty()) Show.Ground = [BoardGround](double X, double Y) { return BoardGround->At(X, Y); };
	auto GroundAt = [&](const V3& W) { return BoardGround && !BoardGround->IsEmpty() ? std::max(0.0, BoardGround->At(W.Y, W.X)) : 0.0; };
	AirDesk::Desk Desk;
	Desk.Begin(Sky, First.Head.Me);
	TAKE(First);
	Desk.Take(First);
	double BattleT = First.Head.T, FedT = First.Head.T;
	size_t RecAt = 1;
	if (bLive) { std::fprintf(ToNode, "speed 1\n"); std::fflush(ToNode); Line(); }
	const int Me = Desk.MySideIndex();
	const double Scale = std::max(0.1, Sky.Scale);

	AirDesk::Clock Clock;
	AirDesk::Flyover Fly;
	bool bFlying = false, bFlyTried = false;
	double FlyT = 0.0, GlideT = -1.0; const double GlideLong = 2.5;
	V3 GlideEye; Rot GlideFacing; int ShotWas = -2;
	double Yaw = 200.0; bool bYawSet = false;
	const double Distance = 800.0, Pitch = -12.0;

	std::vector<Camera> Cams(3);
	Cams[0].Kind = "over"; Cams[1].Kind = "close"; Cams[2].Kind = "film";
	// (choppy1126) the picture run as AAirBattle::TakeShots runs it on ccc39121 (-TheaterAirBattleShots, RideHold 0.5):
	// rides on at Met+20.5 (his side) and Met+68.5 (the other side), each off 0.5 s after its picture (Met+22.5,
	// Met+70.5); the chase-view ride at Met+93.5..95.5; the parachutes look from ChutesT+1 to ChutesT+3.5; SHOTMS=<ms>
	// a long frame at each picture (the screenshot's dip). FILMEND=<s after Met> stops the film there (the Mac's run: 81).
	double MetRealT = -1.0, ChutesRealT = -1.0; V3 ChutesW; bool bFilmRideWas = false, bFilmChutesWas = false, bRideOther = false;
	const double ShotMs = std::getenv("SHOTMS") ? std::atof(std::getenv("SHOTMS")) : 0.0;
	const double FilmEnd = std::getenv("FILMEND") ? std::atof(std::getenv("FILMEND")) : 1e9;
	std::vector<double> ShotTimes; bool bShotsSet = false; size_t ShotNext = 0; double ApproachShot = -1.0, FlyEndRealT = -1.0;
	std::FILE* FilmLog = std::fopen((Prefix + "-film-events.txt").c_str(), "w");
	for (Camera& C : Cams)
	{
		C.Csv.open(Prefix + "-" + C.Kind + ".csv");
		C.Csv << "frame,real,dt,delta_ms,phase,battle,run_rate,taken,steps,cut,jump,wjump,hop_px,away,zoomstep,picked,pick_ch,anchor,anchor_ch,tight,typeaway,ex,ey,ez,lx,ly,lz,yaw,pitch,fov,subject,planes,inframe\n";
	}
	double RealT = -1e-9; bool bHeldFrame = true; int Frame = 0; bool bDefSeen = false; int LongNext = -1; double OverRealT = -1.0;
	double RateWas = -1;
	double DefAppearBattle = -1, ContactBattle = -1;
	for (; Frame < 400000; ++Frame)
	{
		double Delta = bHeldFrame ? 0.0 : 1.0 / 60.0;
		bool bLongFrame = false;
		if (!bHeldFrame)
		{
			if (LongNext == Frame) for (Long& L : Longs) if (L.Kind == "appear" && !L.bDone) { Delta = L.Ms / 1000.0; L.bDone = true; bLongFrame = true; break; }
			for (Long& L : Longs)
			{
				if (L.bDone) continue;
				if ((L.Kind == "real" && RealT >= L.At) || (L.Kind == "battle" && BattleT >= L.At)) { Delta = L.Ms / 1000.0; L.bDone = true; bLongFrame = true; break; }
			}
			if (!bLongFrame && ShotMs > 0.0 && ShotNext < ShotTimes.size() && RealT >= ShotTimes[ShotNext]) { Delta = ShotMs / 1000.0; bLongFrame = true; ++ShotNext; }
			if (!bLongFrame && LongMix > 0.0 && RealT >= LongMixNext) { static const double Ms[3] = { 100.0, 150.0, 220.0 }; Delta = Ms[LongMixK++ % 3] / 1000.0; bLongFrame = true; LongMixNext += LongMix; }
		}
		// --- AAirBattle::Tick: the clock, Feed, the clamp
		const double Real = bHeldFrame ? 0.0 : std::clamp(Delta, 0.0, 0.1);
		RealT += Delta;
		Clock.bQuestion = false;
		Clock.bFlyover = bFlying;
		Clock.bStrike = Show.StrikeOn();
		if (bHalf && Desk.Head().Contact >= 0.0) Clock.Speed = 0.5;
		if (bTwo && Desk.Head().Contact >= 0.0) Clock.Speed = 2.0;
		if (HalfAt >= 0.0 && RealT >= HalfAt) Clock.Speed = 0.5;
		std::string Tag;
		const double Run = Clock.Step(Real, Desk.Head(), Scale, Tag);
		const double EndT = bLive ? 1e18 : Rec.Views.back().Head.T;
		BattleT = std::min(EndT, BattleT + Run);
		int Steps = 0; bool bTaken = false;
		if (bLive)
		{
			if (!Desk.Head().bOver)
			{
				const double O = BattleT + 1.0 - FedT;
				if (O > 1e-6)
				{
					char Ask[64]; std::snprintf(Ask, sizeof Ask, "%.4f", O);
					std::fprintf(ToNode, "tick %s\n", Ask); std::fflush(ToNode);
					const std::string H = Line(); const std::string Reply = Line();
					long Us = 0; size_t Len = 0; std::sscanf(H.c_str(), "%d %ld %zu", &Steps, &Us, &Len);
					AirFeed::Json Ticked; AirFeed::Frame View;
					if (!(AirFeed::Parse(Reply, Ticked, Why) && AirFeed::ReadView(Ticked, Sky, View))) { std::fprintf(stderr, "frame %d: %s\n", Frame, Why.c_str()); return 6; }
					if (View.Head.T > FedT + 1e-9) { TAKE(View); Desk.Take(View); FedT = View.Head.T; bTaken = true; }
				}
			}
			BattleT = std::min(BattleT, FedT);
		}
		else
		{
			while (RecAt < Rec.Views.size() && Rec.Views[RecAt].Head.T <= BattleT + 1.0 + 1e-9) { const AirFeed::Frame& F = Rec.Views[RecAt++]; TAKE(F); Desk.Take(F); ++Steps; bTaken = true; FedT = F.Head.T; }
		}
		if (Cams[0].Now.bValid) Show.Eye = Board(Cams[0].Now.Eye);
		Show.Advance(BattleT);
		if (!bFlyTried && !Show.Groups().empty())
		{
			bFlyTried = true;
			if (Fly.Begin(Show.Groups(), Sky.TargetX, Sky.TargetY, Sky.PlaceKind == "harbour")) { bFlying = true; FlyT = 0.0; GlideT = -1.0; std::printf("the flyover, %.1f s (raid %d, guard %d)\n", Fly.Length(), Fly.RaidGroup(), Fly.GuardGroup()); }
		}
		Desk.Reach(Show.Shown());
		if (!bYawSet) for (const auto& Pr : Show.Groups()) if (Pr.second.Side == Me && !Pr.second.bFighter) { Yaw = Deg(Pr.second.Face); bYawSet = true; break; }
		if (!bDefSeen) for (const auto& G : Show.Groups()) if (G.second.Side == 1 && G.second.bFighter) { bDefSeen = true; LongNext = Frame + 1; DefAppearBattle = BattleT; std::printf("defenders' fighters first in the show: frame %d real %.3f battle %.2f\n", Frame, RealT, BattleT); break; }
		if (ContactBattle < 0 && Desk.Head().Contact >= 0) ContactBattle = BattleT;
		if (MetRealT < 0 && Desk.Head().Contact >= 0) { MetRealT = RealT; std::fprintf(FilmLog, "met real %.2f battle %.2f\n", RealT, BattleT); }
		if (!bFlying && FlyEndRealT < 0 && bFlyTried) FlyEndRealT = RealT;
		if (ApproachShot < 0 && FlyEndRealT >= 0 && MetRealT < 0 && RealT >= std::max(8.0, FlyEndRealT + 4.0)) { ApproachShot = RealT; std::fprintf(FilmLog, "picture 01-approach real %.2f battle %.2f\n", RealT, BattleT); }
		if (ChutesRealT < 0)
		{
			std::vector<V3> Open; for (const auto& T : Show.Things()) if (T.Kind == AirShow::ThingKind::Chute && T.bOpen) Open.push_back(World(T.At));
			for (const V3& One : Open) { V3 Sm; int N = 0; for (const V3& O : Open) if ((O - One).Size() < 1500.0) { Sm = Sm + O; ++N; } if (N >= 3) { ChutesRealT = RealT; ChutesW = Sm * (1.0 / N); std::fprintf(FilmLog, "chutes: three hang together real %.2f battle %.2f\n", RealT, BattleT); break; } }
		}
		if (!bShotsSet && MetRealT >= 0) { bShotsSet = true; for (double A : { 6.0, 22.0, 45.0, 70.0 }) ShotTimes.push_back(MetRealT + A); }
		if (ChutesRealT >= 0 && ShotTimes.size() == 4) { ShotTimes.push_back(ChutesRealT + 3.0); std::sort(ShotTimes.begin() + ShotNext, ShotTimes.end()); }
		const double SinceMet = MetRealT >= 0 ? RealT - MetRealT : -1e9;
		const bool bFilmRide = !bFlying && MetRealT >= 0 && ((SinceMet >= 20.5 && SinceMet < 22.5) || (SinceMet >= 68.5 && SinceMet < 70.5) || (SinceMet >= 93.5 && SinceMet < 95.5));
		bool bFilmChutes = !bFilmRide && ChutesRealT >= 0 && RealT >= ChutesRealT + 1.0 && RealT < ChutesRealT + 3.5;
		if (bFilmRide && !bFilmRideWas) { bRideOther = SinceMet >= 68.5 && SinceMet < 70.5; Cams[2].Subject = AirDesk::Subject(); Cams[2].bCloseCut = true; std::fprintf(FilmLog, "ride on (%s) real %.2f met+%.2f\n", bRideOther ? "the other side" : "his side", RealT, SinceMet); }
		if (!bFilmRide && bFilmRideWas) { std::fprintf(FilmLog, "ride off real %.2f met+%.2f\n", RealT, SinceMet); Cams[2].bFilmBack = true;
#if defined(ROWS)
if (gFix) {
			Cams[2].bLookSet = false;
} else {
}
#elif defined(FIXLAW)
			Cams[2].bLookSet = false;
#endif
		}
		if (bFilmChutes != bFilmChutesWas) { Cams[2].bLookSet = false; Cams[2].bFilmBack = true; std::fprintf(FilmLog, "chutes look %s real %.2f met+%.2f\n", bFilmChutes ? "on" : "off", RealT, SinceMet); }
		bFilmRideWas = bFilmRide; bFilmChutesWas = bFilmChutes;
		const bool bFilmOn = RealT < (MetRealT >= 0 ? MetRealT + FilmEnd : 1e18);
		const auto& Planes = Show.Planes();
		const auto& Groups = Show.Groups();
		static const int TraceKey = std::getenv("TRACEKEY") ? std::atoi(std::getenv("TRACEKEY")) : -1;
		static const double TraceFrom = std::getenv("TRACEFROM") ? std::atof(std::getenv("TRACEFROM")) : 0.0;
		if (TraceKey >= 0 && RealT >= TraceFrom && RealT < TraceFrom + 1.0)
			for (const auto& P : Planes) if (P.Key == TraceKey) std::fprintf(FilmLog, "trace %d real %.3f dt %.3f battle %.3f taken %d at %.2f %.2f %.2f hdg %.1f pitch %.1f bank %.1f falling %d\n", P.Key, RealT, Real, BattleT, bTaken ? 1 : 0, P.At.X, P.At.Y, P.At.Z, P.Heading, P.Pitch, P.Bank, P.bFalling ? 1 : 0);
		auto Find = [&Planes](int Key) -> const AirShow::PlaneOut* { for (const auto& P : Planes) if (P.Key == Key) return &P; return nullptr; };

		// --- AimEye: the flyover's shots
		bool bFlyShot = false, bFlyCut = false;
		Cam FlyCam;
		if (bFlying && GlideT < 0.0)
		{
			FlyT += Real;
			if (FlyT < Fly.Length())
			{
				const AirDesk::Flyover::Shot Shot = Fly.At(FlyT, Groups, Planes);
				V3 E = World(Shot.Eye); const V3 L = World(Shot.Look);
				E.Z = std::max({ E.Z, GroundAt(E) + 60.0, 40.0 });
				GlideEye = E; GlideFacing = RotationOf(L - E);
				FlyCam.Eye = E; FlyCam.Face = GlideFacing; FlyCam.Fov = 50.0; FlyCam.bValid = true;
				bFlyShot = true; bFlyCut = Shot.Index != ShotWas; ShotWas = Shot.Index;
			}
			else { GlideT = 0.0; for (Camera& C : Cams) C.bLookSet = false; }
		}
		// the action set as AimEye picks it, no group in hand: the groups fighting, else his bombers
		std::vector<int> Picked; AirShow::Vec Sum; int Count = 0;
		for (const auto& Pr : Groups) { const AirShow::Group& G = Pr.second; if (G.Doing == AirShow::Act::Down || G.Doing == AirShow::Act::Idle) continue; if (G.Doing == AirShow::Act::Fight) { Sum = Sum + AirShow::Vec(G.X, G.Y, G.Alt); Picked.push_back(G.Id); ++Count; } }
		if (Count == 0) for (const auto& Pr : Groups) { const AirShow::Group& G = Pr.second; if (G.Side == Me && !G.bFighter && G.Doing != AirShow::Act::Down) { Sum = Sum + AirShow::Vec(G.X, G.Y, G.Alt); Picked.push_back(G.Id); ++Count; } }
		std::sort(Picked.begin(), Picked.end());
		const bool bGlideDone = !bFlying;
		// (choppy1126: "strike" whenever the strike is on after the flyover, before contact too: a raid's bomb run)
		std::string Phase = bHeldFrame ? "held" : (bFlying ? (GlideT >= 0.0 ? "glide" : "flyover") : Desk.Head().bOver ? "over" : Show.StrikeOn() ? "strike" : Desk.Head().Contact < 0.0 ? "runup" : Desk.Head().bWinding ? "home" : "fight");
		const double Rate = Real > 0 ? Run / Real : -1;
		const bool bPaceCh = RateWas > 0 && Rate > 0 && (Rate / RateWas > 2.0 || RateWas / Rate > 2.0) && !bFlying;
		if (Rate > 0) RateWas = Rate;

		for (Camera& C : Cams)
		{
			C.Prev = C.Now; C.PrevAway = C.Away; C.bCutNow = false;
			int TightN = -1; double TypeAwayNow = -1; bool bPickCh = false, bAnchorCh = false;
			if (C.Kind == "film" && !bFilmOn) { C.Now.bValid = false; continue; }
			std::string PhaseC = Phase;
			if (C.Kind == "film" && bFilmRide) PhaseC = "ride";
			if (C.Kind == "film" && bFilmChutes) PhaseC = "chutes";
			if (C.Kind == "over" || (C.Kind == "film" && !bFilmRide))
			{
				const int AnchorWas = C.Anchor;
				AirShow::Vec Focus(Sky.TargetX, Sky.TargetY, 3000.0), Crowd; double CrowdSpan = 0.0;
				double OverAway = -1.0;
#if defined(ROWS)
if (gFix) {
				const AirDesk::Overview::Aim A = C.Over.Step(Planes, Picked, Real, C.Anchor, !C.bLookSet);
				if (A.bFound) { Focus = A.Focus; CrowdSpan = A.Span; } else if (Count > 0) Focus = Sum * (1.0 / Count);
				OverAway = A.TypeAway;
} else {
				if (AirDesk::CrowdOf(Planes, Picked, 1200.0, C.Anchor, Crowd))
				{
					Focus = Crowd;
					std::vector<double> Spans; const AirShow::PlaneOut* Nearest = nullptr; double NF = 1e18; AirShow::Vec Tight; int TC = 0;
					for (const auto& P : Planes)
					{
						if (!P.bShown || P.bFalling || P.bOnGround || std::find(Picked.begin(), Picked.end(), P.Group) == Picked.end()) continue;
						const AirShow::Vec O = P.At - Crowd; const double F2 = O.Dot(O);
						if (F2 > 1200.0 * 1200.0) continue;
						Spans.push_back(P.Span > 1.0 ? P.Span : 12.0);
						if (F2 < NF) { NF = F2; Nearest = &P; }
						if (F2 < 350.0 * 350.0) { Tight = Tight + P.At; ++TC; }
					}
					if (!Spans.empty()) { std::nth_element(Spans.begin(), Spans.begin() + Spans.size() / 2, Spans.end()); CrowdSpan = Spans[Spans.size() / 2]; }
					if (TC >= 2) Focus = Tight * (1.0 / TC); else if (Nearest) Focus = Nearest->At;
					TightN = TC;
				}
				else if (Count > 0) Focus = Sum * (1.0 / Count);
}
#elif defined(FIXLAW)
				const AirDesk::Overview::Aim A = C.Over.Step(Planes, Picked, Real, C.Anchor, !C.bLookSet);
				if (A.bFound) { Focus = A.Focus; CrowdSpan = A.Span; } else if (Count > 0) Focus = Sum * (1.0 / Count);
				OverAway = A.TypeAway;
#else
				if (AirDesk::CrowdOf(Planes, Picked, 1200.0, C.Anchor, Crowd))
				{
					Focus = Crowd;
					std::vector<double> Spans; const AirShow::PlaneOut* Nearest = nullptr; double NF = 1e18; AirShow::Vec Tight; int TC = 0;
					for (const auto& P : Planes)
					{
						if (!P.bShown || P.bFalling || P.bOnGround || std::find(Picked.begin(), Picked.end(), P.Group) == Picked.end()) continue;
						const AirShow::Vec O = P.At - Crowd; const double F2 = O.Dot(O);
						if (F2 > 1200.0 * 1200.0) continue;
						Spans.push_back(P.Span > 1.0 ? P.Span : 12.0);
						if (F2 < NF) { NF = F2; Nearest = &P; }
						if (F2 < 350.0 * 350.0) { Tight = Tight + P.At; ++TC; }
					}
					if (!Spans.empty()) { std::nth_element(Spans.begin(), Spans.begin() + Spans.size() / 2, Spans.end()); CrowdSpan = Spans[Spans.size() / 2]; }
					if (TC >= 2) Focus = Tight * (1.0 / TC); else if (Nearest) Focus = Nearest->At;
					TightN = TC;
				}
				else if (Count > 0) Focus = Sum * (1.0 / Count);
#endif
				const double TypeAway = OverAway > 0.0 ? OverAway : CrowdSpan > 0.0 ? std::clamp(CrowdSpan * 1920.0 / ((CrowdSpan >= 25.0 ? 70.0 : 45.0) * 2.0 * std::tan(Rad(27.5))), 250.0, 1500.0) : 800.0;
				double Away = Distance * TypeAway / 800.0;
				TypeAwayNow = TypeAway;
				V3 Wanted = World(Focus);
				double PitchC = Pitch;
				const bool bOnChutes = C.Kind == "film" && bFilmChutes;
				if (bOnChutes)
				{
					V3 Sm; int N = 0; for (const auto& T : Show.Things()) { const V3 W = World(T.At); if (T.Kind == AirShow::ThingKind::Chute && T.bOpen && (W - ChutesW).Size() < 1500.0) { Sm = Sm + W; ++N; } }
					if (N > 0) ChutesW = Sm * (1.0 / N);
					Wanted = ChutesW; Away = 300.0; PitchC = 4.0;
				}
				const bool bCutBack = C.Kind == "film" && C.bFilmBack; C.bFilmBack = false; // (a change of the film's camera: a cut on the screen whatever the look does)
#if defined(ROWS)
if (gFix) {
				if (C.bLookSet && !bOnChutes) C.LookAt = World(C.Over.Look(Board(C.LookAt), Focus, Real));
				else C.LookAt = Wanted;
} else {
				if (C.bLookSet && !bOnChutes) { const V3 Moved = Wanted - C.LookWanted; if (Moved.Size() < 150.0) C.LookAt = C.LookAt + Moved; C.LookAt = C.LookAt + (Wanted - C.LookAt) * Smooth(Real, 1.2); }
				else C.LookAt = Wanted;
}
#elif defined(FIXLAW)
				if (C.bLookSet && !bOnChutes) C.LookAt = World(C.Over.Look(Board(C.LookAt), Focus, Real));
				else C.LookAt = Wanted;
#else
				if (C.bLookSet && !bOnChutes) { const V3 Moved = Wanted - C.LookWanted; if (Moved.Size() < 150.0) C.LookAt = C.LookAt + Moved; C.LookAt = C.LookAt + (Wanted - C.LookAt) * Smooth(Real, 1.2); }
				else C.LookAt = Wanted;
#endif
				C.LookWanted = Wanted; C.bLookSet = true;
				const Rot Facing{ PitchC, Yaw, 0.0 };
				V3 Eye = C.LookAt - Forward(Facing) * Away; Eye.Z = std::max({ Eye.Z, GroundAt(Eye) + 40.0, 40.0 });
				C.Now.Eye = Eye; C.Now.Face = RotationOf(C.LookAt - Eye); C.Now.Fov = 55.0; C.Now.bValid = true; C.Away = Away;
				bPickCh = !C.PickedWas.empty() && Picked != C.PickedWas; C.PickedWas = Picked;
				bAnchorCh = AnchorWas >= 0 && C.Anchor != AnchorWas;
				if (bCutBack) C.bCutNow = true;
				if (bFlying && GlideT >= 0.0)
				{
					if (C.Kind == "over") GlideT += Real;
					const double Into = std::clamp(GlideT / GlideLong, 0.0, 1.0);
					const double Eased = Into * Into * (3.0 - 2.0 * Into);
					if (Into >= 1.0) { if (C.Kind == "over") bFlying = false; }
					else
					{
						const V3 Way = Slerp(Forward(GlideFacing), C.LookAt - Eye, Eased);
						C.Now.Eye = GlideEye + (Eye - GlideEye) * Eased; C.Now.Face = RotationOf(Way); C.Now.Fov = 50.0 + 5.0 * Eased;
						C.Away = (C.LookAt - C.Now.Eye).Size();
					}
				}
				if (bFlyShot) { C.Now = FlyCam; C.bCutNow = bFlyCut; C.Away = 0; }
			}
			else
			{
				// the automatic ride (no group in hand, no squadron ridden), from the glide's end
				if (!bGlideDone || bFlyShot) { C.Now.bValid = false; C.P1.clear(); C.P2.clear(); C.W1.clear(); C.W2.clear(); continue; }
				const AirShow::PlaneOut* Rider = Find(C.Subject.Plane);
				C.Subject.Since += Real * Scale;
				const bool bLost = !Rider || !Rider->bShown || Rider->bFalling || Rider->bOnGround;
				const bool bStale = C.Subject.Since > 10.0;
				if (bLost || bStale)
				{
					std::vector<AirShow::PlaneOut> Ours;
					const bool bOther = C.Kind == "film" && bRideOther;
					for (const auto& P : Planes) if ((P.Side == Me) != bOther) Ours.push_back(P);
					const AirShow::Vec Near = C.Kind == "film" && C.Prev.bValid ? Board(C.Prev.Eye) : Show.Eye;
					AirDesk::Subject Next = AirDesk::ChooseRide(Ours, Planes, Near, bLost ? -1 : C.Subject.Plane);
					if (Next.Plane < 0 && !bLost && (Rider->Side == Me) != bOther) C.Subject.Since = 0.0;
					else if (Next.Plane < 0) Next = AirDesk::Choose(Planes, std::vector<int>(), Show.Eye, bLost ? -1 : C.Subject.Plane);
					if (Next.Plane >= 0) { bPickCh = Next.Plane != C.Subject.Plane; C.Subject = Next; Rider = Find(Next.Plane); C.bCloseCut = true; }
				}
				if (!Rider || !Rider->bShown) { C.Now.bValid = false; continue; }
				if (C.Kind == "film" && bPickCh) std::fprintf(FilmLog, "  rides key %d side %d %s real %.2f\n", Rider->Key, Rider->Side, Rider->bFighter ? "fighter" : "bomber", RealT);
				V3 Ahead, Rr, Ru; Axes(Rot{ Rider->Pitch, Rider->Heading, Rider->Bank }, Ahead, Rr, Ru);
				const V3 Body = World(Rider->At);
				const bool bCut = C.bCloseCut;
				C.CloseAhead = bCut || C.CloseAhead.Size() < 1e-6 ? Ahead : (C.CloseAhead + (Ahead - C.CloseAhead) * Smooth(Real, 0.33)).Safe();
				V3 Flat(C.CloseAhead.X, C.CloseAhead.Y, 0.0); Flat = Flat.Size() < 1e-6 ? V3(Ahead.X, Ahead.Y, 0.0).Safe() : Flat.Safe();
				const V3 UpV(0, 0, 1); const V3 FlatRight = Cross(UpV, Flat).Safe();
				V3 Want, Look;
				if (Rider->bFighter) { Want = Body - C.CloseAhead * 22.0 + UpV * 6.0; Look = Body + C.CloseAhead * 80.0; }
				else
				{
					const AirShow::PlaneOut* Foe = Find(Rider->Target >= 0 ? Rider->Target : C.Subject.Other);
					const V3 Toward = Foe ? (World(Foe->At) - Body).Safe() : FlatRight;
					const double Hand = Toward.Dot(FlatRight) >= 0.0 ? 1.0 : -1.0;
					const double Size = std::max(12.0, Rider->Span);
					Want = Body - C.CloseAhead * (Size * 1.8) + FlatRight * (Hand * Size * 0.55) + UpV * (Size * 0.45); Look = Body + C.CloseAhead * (Size * 1.2);
				}
				C.CloseEye = Want;
				Rot Facing = RotationOf(Look - C.CloseEye);
				Facing.R = std::clamp(Rider->Bank * 0.25, -15.0, 15.0);
				C.CloseFacing = bCut ? Facing : RInterpTo(C.CloseFacing, Facing, Real, 6.0);
				C.bCutNow = bCut; C.bCloseCut = false;
				C.Now.Eye = C.CloseEye; C.Now.Face = C.CloseFacing; C.Now.Fov = 62.0; C.Now.bValid = true; C.Away = (Body - C.CloseEye).Size();
				C.LookAt = Look;
			}
			if (bHeldFrame || !C.Now.bValid) continue;
			// --- the measures: the frame's jump (the median aircraft's move on the 1440 x 878 picture beyond its last
			// frame's own motion, time-scaled, as eye_probe), the same with every frame seen through this frame's eye
			// (the aircraft's own part), and the look point's hop (its own second difference, in pixels at its distance)
			Meter& M = C.M[PhaseC];
			Meter& MA = C.M["ALL"];
			if (C.bCutNow) C.CutAgo = 0; else ++C.CutAgo;
			std::map<int, std::pair<double, double>> P0; std::map<int, V3> W0; std::vector<double> E, EW; int InN = 0, PlN = 0;
			for (const auto& P : Planes)
			{
				if (!P.bShown || P.bFalling || P.bOnGround) continue;
				const V3 W = World(P.At);
				double X, Y;
				if (!Project(C.Now, W, X, Y) || X < -200 || X > ScreenW + 200 || Y < -200 || Y > ScreenH + 200) continue;
				++PlN; if (X >= 0 && X <= ScreenW && Y >= 0 && Y <= ScreenH) ++InN;
				P0[P.Key] = { X, Y }; W0[P.Key] = W;
				auto A1 = C.P1.find(P.Key), A2 = C.P2.find(P.Key);
				if (A1 != C.P1.end() && A2 != C.P2.end() && C.Dt1 > 0)
				{
					const double K = Real / C.Dt1;
					E.push_back(std::hypot(X - A1->second.first - (A1->second.first - A2->second.first) * K, Y - A1->second.second - (A1->second.second - A2->second.second) * K));
					double X1, Y1, X2, Y2;
					if (Project(C.Now, C.W1[P.Key], X1, Y1) && Project(C.Now, C.W2[P.Key], X2, Y2))
						EW.push_back(std::hypot(X - X1 - (X1 - X2) * K, Y - Y1 - (Y1 - Y2) * K));
				}
			}
			double Jump = -1, WJump = -1, Hop = -1, ZoomStep = -1;
			if (E.size() >= 3 && C.CutAgo >= 2)
			{
				std::nth_element(E.begin(), E.begin() + E.size() / 2, E.end()); Jump = E[E.size() / 2];
				if (EW.size() >= 3) { std::nth_element(EW.begin(), EW.begin() + EW.size() / 2, EW.end()); WJump = EW[EW.size() / 2]; }
			}
			if (C.CutAgo >= 2 && C.Dt1 > 0 && C.Away > 1)
			{
				const double K = Real / C.Dt1;
				const V3 H = C.LookAt - C.Look1 - (C.Look1 - C.Look2) * K;
				Hop = H.Size() * (ScreenW * 0.5) / std::tan(Rad(C.Now.Fov * 0.5)) / C.Away;
			}
			if (C.CutAgo >= 1 && C.PrevAway > 0 && C.Away > 0) ZoomStep = std::fabs(C.Away / C.PrevAway - 1.0);
			// (choppy1126) the eye's place, aim and lens, one frame against its own motion; the look's speed (pans)
			double EyeStep = -1, AimStep = -1, FovStep = -1, LookSpd = -1;
			const V3 AimNow = Forward(C.Now.Face);
			if (C.CutAgo >= 2 && C.Dt1 > 0 && Real > 0)
			{
				const double K = Real / C.Dt1;
				EyeStep = (C.Now.Eye - C.Eye1 - (C.Eye1 - C.Eye2) * K).Size();
				const V3 Pred = (C.Aim1v + (C.Aim1v - C.Aim2v) * K).Safe();
				AimStep = Deg(std::acos(std::clamp(AimNow.Dot(Pred), -1.0, 1.0)));
			}
			if (C.CutAgo >= 1 && C.Fov1 > 0) FovStep = std::fabs(C.Now.Fov - C.Fov1);
			if (C.CutAgo >= 1 && Real > 0 && Phase != "flyover" && Phase != "glide") LookSpd = (C.LookAt - C.Look1).Size() / Real;
			bool bPanStart = false; double PanKm = 0;
			if (LookSpd > 500.0 && !C.bPanning) { C.bPanning = true; bPanStart = true; C.PanStart = C.Look1; }
			else if (C.bPanning && LookSpd >= 0 && LookSpd < 250.0) { C.bPanning = false; PanKm = (C.LookAt - C.PanStart).Size() / 1000.0; }
			long NearSw = 0, LightSw = 0, ShadowSw = 0;
			if (C.Kind == "over")
			{
				const double Enlarged = std::tan(Rad(27.5)) / std::tan(Rad(std::clamp(C.Now.Fov, 5.0, 120.0) * 0.5));
				for (const auto& P : Planes)
				{
					if (!P.bShown) { C.Shapes.erase(P.Key); continue; }
					const V3 W = World(P.At);
					const double Away = (W - C.Now.Eye).Size();
					double X, Y;
					const bool bOn = Project(C.Now, W, X, Y) && X >= 0 && X <= ScreenW && Y >= 0 && Y <= ScreenH;
					auto It = C.Shapes.find(P.Key);
					const bool bHad = It != C.Shapes.end();
					Looks Was = bHad ? It->second : Looks();
					Looks Now;
					Now.bShadow = Away < (P.bOnGround ? 3000.0 : 1000.0) * (Was.bShadow ? 1.1 : 1.0);
					Now.bNear = Away < (Was.bNear ? 450.0 : 300.0);
					Now.bLight = !Now.bNear && Away * Enlarged > 1500.0 * (Was.bLight ? 0.9 : 1.0);
					if (bHad && bOn && !bHeldFrame)
					{
						if (Now.bNear != Was.bNear) ++NearSw;
						if (!Now.bNear && !Was.bNear && Now.bLight != Was.bLight) ++LightSw;
						if (Now.bShadow != Was.bShadow) ++ShadowSw;
					}
					C.Shapes[P.Key] = Now;
				}
			}
			Meter& MB = C.M["battle"];
			const bool bBattle = Phase != "flyover" && Phase != "glide" && Phase != "held";
			// (choppy1126 film) each aircraft's own hop through this frame's eye; the other side's fighters apart
			long GH = 0, GS = 0, AH = 0, AS = 0, GScr = 0; double GHM = 0, AHM = 0;
			if (C.CutAgo >= 2 && C.Dt1 > 0)
			{
				const double K = Real / C.Dt1;
				for (const auto& P : Planes)
				{
					if (!P.bShown || P.bFalling || P.bOnGround) continue;
					auto I0 = P0.find(P.Key); if (I0 == P0.end()) continue;
					if (I0->second.first < 0 || I0->second.first > ScreenW || I0->second.second < 0 || I0->second.second > ScreenH) continue;
					auto I1 = C.W1.find(P.Key), I2 = C.W2.find(P.Key); if (I1 == C.W1.end() || I2 == C.W2.end()) continue;
					double X1, Y1, X2, Y2;
					if (!Project(C.Now, I1->second, X1, Y1) || !Project(C.Now, I2->second, X2, Y2)) continue;
					const double H = std::hypot(I0->second.first - X1 - (X1 - X2) * K, I0->second.second - Y1 - (Y1 - Y2) * K);
					auto S1 = C.P1.find(P.Key), S2 = C.P2.find(P.Key);
					const double Hs = S1 != C.P1.end() && S2 != C.P2.end() ? std::hypot(I0->second.first - S1->second.first - (S1->second.first - S2->second.first) * K, I0->second.second - S1->second.second - (S1->second.second - S2->second.second) * K) : 0.0;
					++AS; if (H > 20.0) ++AH; AHM = std::max(AHM, H);
					if (P.Side != Me && P.bFighter) { ++GS; if (H > 20.0) { ++GH; if (C.Kind != "over") std::fprintf(FilmLog, "  %s own hop %.0f px key %d %.0f m from the eye real %.2f battle %.2f taken %d cutago %d\n", C.Kind.c_str(), H, P.Key, (World(P.At) - C.Now.Eye).Size(), RealT, BattleT, bTaken ? 1 : 0, C.CutAgo); } if (Hs > 20.0) ++GScr; GHM = std::max(GHM, H); }
				}
			}
			for (Meter* Mp : { &M, &MA, bBattle ? &MB : nullptr })
			{
				if (!Mp) continue;
				Mp->EyeMax = std::max(Mp->EyeMax, EyeStep); Mp->AimMax = std::max(Mp->AimMax, AimStep); Mp->FovMax = std::max(Mp->FovMax, FovStep);
				if (EyeStep > 5.0) Mp->Eye5++;
				if (AimStep > 1.0) Mp->Aim1++;
				Mp->LookSpdMax = std::max(Mp->LookSpdMax, LookSpd);
				if (bPanStart) Mp->Pans++;
				Mp->PanKmSum += PanKm;
				Mp->NearSw += NearSw; Mp->LightSw += LightSw; Mp->ShadowSw += ShadowSw;
				const long Sw = NearSw + LightSw + ShadowSw;
				if (Sw > 0) Mp->SwapFrames++;
				Mp->SwapFrameMax = std::max(Mp->SwapFrameMax, Sw);
			}
			for (Meter* Mp : { &M, &MA, bBattle ? &MB : nullptr })
			{
				if (!Mp) continue;
				Mp->Frames++; Mp->Real += Delta;
				Mp->GSeen += GS; Mp->GHop += GH; if (GH > 0) Mp->GHopFrames++; Mp->GHopMax = std::max(Mp->GHopMax, GHM); Mp->ASeen += AS; Mp->AHop += AH; Mp->AHopMax = std::max(Mp->AHopMax, AHM); Mp->GScr20 += GScr;
				if (C.bCutNow) Mp->Cuts++;
				if (bPickCh) Mp->PickCh++;
				if (bAnchorCh) Mp->AnchorCh++;
				if (bPaceCh) Mp->PaceCh++;
				if (bLongFrame) Mp->LongF++;
				if (Jump >= 0)
				{
					Mp->Measured++; Mp->Jall.push_back(Jump);
					if (Jump > 20) { Mp->J20++; if (WJump >= 0 && WJump <= 10) Mp->JCam20++; if (bLongFrame) Mp->LongJ++; }
					if (WJump > 20) Mp->JWorld20++;
					if (Jump > 50) Mp->J50++;
					Mp->JMax = std::max(Mp->JMax, Jump);
				}
				if (Hop > 20) Mp->Hop20++;
				Mp->HopMax = std::max(Mp->HopMax, Hop);
				if (ZoomStep > 0.02) Mp->Zoom2++;
				if (ZoomStep > 0.10) Mp->Zoom10++;
				Mp->ZoomMax = std::max(Mp->ZoomMax, ZoomStep);
			}
			std::string PickS; for (int G : Picked) PickS += std::to_string(G) + " ";
			C.Csv << Frame << ',' << RealT << ',' << Real << ',' << Delta * 1000 << ',' << PhaseC << (bLongFrame ? "+long" : "") << ',' << BattleT << ',' << Rate << ',' << (bTaken ? 1 : 0) << ',' << Steps << ',' << (C.bCutNow ? 1 : 0) << ','
				<< Jump << ',' << WJump << ',' << Hop << ',' << C.Away << ',' << ZoomStep << ',' << PickS << ',' << (bPickCh ? 1 : 0) << ',' << (C.Kind == "over" ? C.Anchor : C.Subject.Plane) << ',' << (bAnchorCh ? 1 : 0) << ',' << TightN << ',' << TypeAwayNow << ','
				<< C.Now.Eye.X << ',' << C.Now.Eye.Y << ',' << C.Now.Eye.Z << ',' << C.LookAt.X << ',' << C.LookAt.Y << ',' << C.LookAt.Z << ',' << C.Now.Face.Y << ',' << C.Now.Face.P << ',' << C.Now.Fov << ',' << C.Subject.Plane << ',' << PlN << ',' << InN << '\n';
			C.P2 = C.P1; C.P1 = P0; C.W2 = C.W1; C.W1 = W0; C.Dt1 = Real; C.Look2 = C.Look1; C.Look1 = C.LookAt;
			C.Eye2 = C.Eye1; C.Eye1 = C.Now.Eye; C.Aim2v = C.Aim1v; C.Aim1v = AimNow; C.Fov1 = C.Now.Fov;
			(void)TightN; (void)TypeAwayNow;
		}
		bHeldFrame = false;
		if (Desk.Head().bOver && Show.Shown() >= Desk.Head().T - 1e-6)
		{
			if (OverRealT < 0.0) { OverRealT = RealT; std::printf("over: frame %d real %.3f battle %.2f\n", Frame, RealT, BattleT); }
			if (RealT - OverRealT > 4.0) break;
		}
		if (!bLive && BattleT >= EndT - 1e-9 && OverRealT < 0 && RealT > 5000) break;
	}
	if (bLive) { std::fprintf(ToNode, "end\n"); std::fflush(ToNode); Line(); std::fprintf(ToNode, "quit\n"); std::fflush(ToNode); int St = 0; waitpid(Child, &St, 0); }
	std::printf("frames %d real %.1f; swedes/defenders appear battle %.2f; contact battle %.2f\n", Frame, RealT, DefAppearBattle, ContactBattle);
	auto Pct = [](std::vector<double> V, double Q) { if (V.empty()) return -1.0; std::sort(V.begin(), V.end()); return V[std::min(V.size() - 1, (size_t)(Q * V.size()))]; };
	for (Camera& C : Cams)
	{
		std::printf("== %s\n  %-8s %6s %6s | %7s %7s %7s %6s %6s | %6s %6s | %5s %5s %6s | %4s %5s %5s %4s %4s\n", C.Kind.c_str(), "phase", "real", "meas", "j20/min", "cam/min", "air/min", "j50", "jmax", "hop20", "hopmx", "zm2%", "zm10%", "zmax%", "cuts", "pick", "anch", "pace", "long");
		for (const char* Ph : { "flyover", "glide", "runup", "fight", "strike", "ride", "chutes", "home", "over", "battle", "ALL" })
		{
			auto It = C.M.find(Ph); if (It == C.M.end()) continue; const Meter& M = It->second; const double Min = std::max(M.Real / 60.0, 1e-9);
			std::printf("  X %-8s eyemax %.1f m (>5m %ld) aimmax %.2f deg (>1deg %ld) fovmax %.2f | lookspd max %.0f m/s pans %ld (%.1f km) | swaps/min near %.1f light %.1f shadow %.1f; frames with a swap %.1f/min, most in one frame %ld | j20 %ld zoom10 %ld cuts %ld pace %ld long %ld real %.1f\n",
				Ph, M.EyeMax, M.Eye5, M.AimMax, M.Aim1, M.FovMax, M.LookSpdMax, M.Pans, M.PanKmSum, M.NearSw / Min, M.LightSw / Min, M.ShadowSw / Min, M.SwapFrames / Min, M.SwapFrameMax, M.J20, M.Zoom10, M.Cuts, M.PaceCh, M.LongF, M.Real);
			std::printf("  G %-8s foe fighters seen %ld (%.1f a frame): own hops>20px %ld = %.1f/min, frames with one %.1f/min, max %.0f px; on-screen hops>20px %ld = %.1f/min | all aircraft own hops %ld of %ld = %.1f/min max %.0f\n", Ph, M.GSeen, M.Frames ? double(M.GSeen) / M.Frames : 0.0, M.GHop, M.GHop / Min, M.GHopFrames / Min, M.GHopMax, M.GScr20, M.GScr20 / Min, M.AHop, M.ASeen, M.AHop / Min, M.AHopMax);
			std::printf("  %-8s %6.1f %6ld | %7.2f %7.2f %7.2f %6ld %6.1f | %6ld %6.0f | %5ld %5ld %6.1f | %4ld %5ld %5ld %4ld %4ld/%ld  p99 %.1f\n", Ph, M.Real, M.Measured, M.J20 / Min, M.JCam20 / Min, M.JWorld20 / Min, M.J50, M.JMax, M.Hop20, M.HopMax, M.Zoom2, M.Zoom10, M.ZoomMax * 100, M.Cuts, M.PickCh, M.AnchorCh, M.PaceCh, M.LongJ, M.LongF, Pct(M.Jall, 0.99));
		}
	}
	return 0;
}
