// The gore falls' cloud test (Effects, release 2, behind -TheaterLandGore; Battle/LandMen's Crowd::AddGore and SetGore).
// Plays a small battle of its own through the crowd: 160 men, an American and a German line, killed at 3 s by a field
// gun's shell, a mortar's bomb or a rifle, each burst 0 to 13 m from the man it kills. Checks:
//   off   twins added but the switch off: every man falls exactly as with no twins (his fall, his skin, byte for byte)
//   on    only shell kills near their burst are gored, the share falling away with the distance; each gored man takes
//         one of his twin's gore falls, keeps his clips, a limb leaves its joint, his limb_off events come; no whole
//         joint of his opens on its caps (the twin's joined skin); no man's skin holds a NaN. Notes: a limb resting
//         under 1 m from its joint, or flying over 3 m high (the art director's asks: the clips' own to mend)
//   sides each gored man read by a side (1.5 m or more from the burst; or within 1 m, where his stance's gore falls hold
//         none from under he may take, by the side of the gun that fired it) falls in a fall whose gore_from is where
//         the burst (or the gun) was against him (front or behind within 45 degrees of his nose or his back, left or
//         right between: the test's own reading, from where he stood and his heading by his track at the round less his
//         lag) wherever his stance's gore falls hold one from there he may take that moves every limb half a metre or
//         more away from it (the test's own reading of the falls, from their first frame and their last, along the line
//         from it to the limb's joint), and every man standing or running has one from his side; none falls so that his
//         limbs move less far away than another fall he may take would move them, up to half a metre (so where none from
//         his side does, the best of all he may take but those from the side opposite and those from under); none falls
//         in one from the side opposite (front and behind, left and right) where his stance's gore falls hold one from
//         his side he may take (the verifier of f6f8b75c: it throws him toward the burst). Each man the burst was under
//         (within 1 m, whatever his stance: the deaths thread, 24 Nov) whose stance's gore falls hold one from under he
//         may take falls in one, or, by a field gun's or a howitzer's shell, in the throw his gun's side gives him (from
//         that side, every limb as far from the gun as that side's falls set); a fall from under never given a man the
//         burst was not under; his gun known, none in a fall from under that moves a limb toward the gun (the test's
//         reading, from the falls' first frames and last) where another from under he may take does not, nor where all
//         do in one that moves it more than the least of them (the deaths thread's mirrors of 25 Nov, die_leg_off_f and
//         die_leg_off_g: before, with one fall from under each stance, about half moved his lower leg toward the gun).
//         Printed: the falls from under read against his gun as played, and the men such a shell offered both a fall
//         from under and a throw, and how many of them were thrown (the throw weighs 8 to its 3; the Fates rule, never
//         the last fall again, sways a choice of two too far for a check of the weight). No limb off a man standing or
//         running moves toward the burst (or the gun) as played, from
//         his pose as struck to rest (a note under half a metre: his pose as struck is not the fall's first frame), nor
//         off a man of any stance whose gore falls hold one from each side that is not a throw (the twins of 24 Nov:
//         kneeling, in cover and lying too; those before, a note), nor the body (pelvis) of a man standing or running
//         more than 0.1 m where another fall he may take moves it away (kneeling, in cover or lying a note); no man
//         thrown has a limb or his body move toward it; only a field gun's or a howitzer's shell (w "gun", its block's
//         cal not "light") throws him (die_blast_*, kind thrown); a man lying thrown only from under him (die_blast_d),
//         by such a shell bursting on him; counted by side and by stance. Here, in a second battle of 384 men standing,
//         running, kneeling and lying, killed by a gun, a tank or a mortar 0.5 to 3.2 m from the burst all round (where
//         the twins' prone falls hold falls by side, some lying gored in them: the deaths thread's lying falls of 24
//         Nov), in a third of 240 so killed by a shell bursting 0.6 m from them, fired by a gun of their own 400 m off
//         all round (a howitzer's, a light gun's, a tank's or a mortar's; where the twins hold falls from under, some in
//         them and some thrown from their gun's side), and in the band's battle: every twin in the twins folder with its
//         own file beside it, 210 men each, standing, running or kneeling, killed by a howitzer's shell, a light gun's,
//         a tank's, a mortar's or a grenade with the burst 1.6 to 2.4 m off, or the gun where it burst on him, 31 to 49
//         degrees round on his left or his right of his nose (the verifier of 35ffc951: there the falls from the front
//         flung a limb toward it). The off lines and the men not gored print digests of their falls and skins, to be
//         byte-identical before and after a change to the gore falls that does not change who is gored.
//   sweep every twin's gore falls as the crowd gives a gored man them (LandMen::GoreFalls, ChooseFall's own rule; and
//         LandMen::GoreSide, the side they are read by): standing, running, kneeling, in cover and lying; a field gun's
//         or a howitzer's shell, or a light gun's, a tank's, a mortar's or a grenade's (the falls given differ only so);
//         the burst 1, 1.5, 2, 2.5 or 3 m off, or his gun 25 or 400 m off (read by its side), every 3 degrees all round;
//         and the burst under him, his gun 25 or 400 m off every 3 degrees all round or not known. Each fall given one
//         of his stance's he may take; only from his side wherever one from his side moves every limb half a metre away;
//         none moving a limb less far away than another he may take moves every limb, up to half a metre (where one from
//         his side, none from the side opposite weighed); none from the side opposite where one from his side he may
//         take; no limb off a man standing or running moving toward the burst or the gun (the least printed), nor off a
//         man of a stance whose falls hold one from each side; no fall given a man standing or running moving his body
//         toward it more than 0.01 m where another he may take moves it away (kneeling, in cover or lying a note); the
//         burst under him, the falls from under the rule gives and else only such a shell's throw from his gun's side,
//         none of the throws moving a limb or his body toward the gun. Of the falls from under (a man standing, running,
//         kneeling or in cover): his gun known, none given moving his limb toward it where one he may take does not; where
//         every one does (a gap, the falls' own, printed with where his gun was, not a failure), those that move it least
//         so; his gun not known, every one (both legs, with the mirrors); any stance with one from under (the twins of
//         24 Nov; lying, the prone throw) it, whatever his gun's side. A table prints, for each stance on his feet or a
//         knee and each side his gun may be on (or not known), which of them are given and how far each moves his limb
//         along the line toward the gun.
//   same  a second crowd gores the same men with the same falls
//   back  a replay from the start gives every man his own file again; played on, the same men are gored again
// With an out folder it also paints gore_falls.ppm: six gored men from the side, each from the hit to rest in six steps.
// Usage: land_gore_test <soldiers folder> <twins folder> <armies.json> [out folder]
#define main land_men_picture_main
#include "land_men_picture.cpp"
#undef main

#include <array>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <set>
#include <tuple>

namespace
{
	const char* const Cuts[] = { "upperarm_l", "upperarm_r", "lowerarm_l", "lowerarm_r", "thigh_l", "thigh_r", "calf_l", "calf_r" };
	constexpr int MenEach = 80;
	constexpr double Hit = 3.0;

	struct Who
	{
		int B = 1, K = 1;
		std::string W;
		double Burst = 0.0;
		double BX = 0.0, BY = 0.0; // the burst's place (the round's x, y)
		bool bBy = false;          // the round's gun known (by, k: a man of the battle's, standing at SX, SY)
		double SX = 0.0, SY = 0.0;
		bool bLight = false;       // a gun's shell from a block of cal "light"
		double X = 0.0, Y = 0.0, F = 0.0, Go = 0.0; // where he passes at the hit, his heading and his pace (m a second)
	};

	/** A number as the recording writes it (a stream's default six figures), so the test reads the burst where the crowd
	 * does: a burst written 1 m off is no nearer than 1 m. */
	double AsWritten(double V)
	{
		std::ostringstream O;
		O << V;
		return std::stod(O.str());
	}

	/** The half quarter round (45 degrees), where the gore falls' sides part (Crowd's BlastOf). */
	constexpr double Eighth = 0.785398163397448;

	std::string Recording(std::vector<Who>& Men)
	{
		std::ostringstream R, Rounds;
		R << "{\"seed\": 23, \"date\": \"July 1944\", \"sky\": {\"latLon\": [49.12, -1.09]},"
		  << "\"blocks\": [{\"id\": 1, \"army\": \"USA\", \"nat\": \"USA\", \"standIn\": \"POL\"}, {\"id\": 2, \"army\": \"GER\", \"nat\": \"GER\"}],"
		  << "\"men\": [";
		Rounds << "\"rounds\": [";
		for (int I = 0; I < 2 * MenEach; ++I)
		{
			Who Wh;
			Wh.B = I % 2 ? 2 : 1;
			Wh.K = I / 2 + 1;
			const int Kind = (I / 2) % 8;
			Wh.W = Kind < 6 ? "gun" : Kind == 6 ? "mortar" : "rifle";
			Wh.Burst = Wh.W == "rifle" ? 0.0 : static_cast<double>((I / 16) % 14);
			const double X = 30.0 * (I / 2), Y = Wh.B == 1 ? 0.0 : 60.0, F = Wh.B == 1 ? 0.0 : 3.14159265358979;
			Wh.X = X, Wh.Y = Y, Wh.F = F;
			const char* Stance = (I / 2) % 5 == 4 ? "wait" : "fire_stand";
			if (I) { R << ","; Rounds << ","; }
			R << "{\"b\": " << Wh.B << ", \"k\": " << Wh.K << ", \"r\": \"rifle\", \"at\": [[0, " << X << ", " << Y << ", " << F << "], [10, " << X << ", " << Y << ", " << F
			  << "]], \"ch\": [[0, \"" << Stance << "\", \"none\", 0, 0, 0, 0]], \"d\": " << Hit << "}";
			// (the burst that far from him, off to his side: x, y the round's, as the drill writes a shell's)
			const double A = 0.7 * I;
			Rounds << "{\"t\": " << Hit << ", \"w\": \"" << Wh.W << "\", \"on\": " << Wh.B << ", \"ok\": " << Wh.K << ", \"x\": " << X + Wh.Burst * std::cos(A)
			       << ", \"y\": " << Y + Wh.Burst * std::sin(A) << ", \"r\": \"kill\"}";
			Wh.BX = AsWritten(X + Wh.Burst * std::cos(A));
			Wh.BY = AsWritten(Y + Wh.Burst * std::sin(A));
			Men.push_back(Wh);
		}
		R << "], " << Rounds.str() << "]}";
		return R.str();
	}

	/**
	 * The sides' battle: 384 men, an American and a German line, each killed at 3 s by a field gun's shell, a tank's or
	 * a mortar's, standing (fire_stand, wait), running (advance, 2.5 m a second), kneeling or lying (fire_kneel,
	 * fire_prone), the burst 0.5 to 3.2 m from where he is struck, all round him, his heading a little each way.
	 */
	std::string SidesRecording(std::vector<Who>& Men)
	{
		std::ostringstream R, Rounds;
		R.precision(10);
		Rounds.precision(10);
		R << "{\"seed\": 29, \"date\": \"July 1944\", \"sky\": {\"latLon\": [49.12, -1.09]},"
		  << "\"blocks\": [{\"id\": 1, \"army\": \"USA\", \"nat\": \"USA\", \"standIn\": \"POL\"}, {\"id\": 2, \"army\": \"GER\", \"nat\": \"GER\"}],"
		  << "\"men\": [";
		Rounds << "\"rounds\": [";
		const char* const Guns[] = { "gun", "tank", "mortar" };
		const char* const Stances[] = { "fire_stand", "wait", "advance", "fire_kneel", "fire_prone" };
		const double Fars[] = { 0.5, 1.6, 2.0, 2.6, 3.2 }, Pace = 2.5;
		for (int I = 0; I < 384; ++I)
		{
			const int N = I / 2;
			Who Wh;
			Wh.B = I % 2 ? 2 : 1;
			Wh.K = N + 1;
			Wh.W = Guns[N % 3];
			Wh.Burst = Fars[(N / 15) % 5];
			const char* Stance = Stances[(N / 3) % 5];
			const double X = 30.0 * N, Y = Wh.B == 1 ? 0.0 : 60.0, F = (Wh.B == 1 ? 0.0 : 3.14159265358979) + 0.25 * (N % 5 - 2);
			const double A = 0.5 + 0.9 * I;
			// (a running man passes his spot at the hit; the others stand on it)
			const double Go = std::strcmp(Stance, "advance") == 0 ? Pace : 0.0;
			Wh.X = X, Wh.Y = Y, Wh.F = F, Wh.Go = Go;
			if (I) { R << ","; Rounds << ","; }
			R << "{\"b\": " << Wh.B << ", \"k\": " << Wh.K << ", \"r\": \"rifle\", \"at\": [[0, " << X - std::sin(F) * Go * Hit << ", " << Y - std::cos(F) * Go * Hit << ", " << F
			  << "], [10, " << X + std::sin(F) * Go * (10.0 - Hit) << ", " << Y + std::cos(F) * Go * (10.0 - Hit) << ", " << F << "]], \"ch\": [[0, \"" << Stance
			  << "\", \"none\", 0, 0, 0, 0]], \"d\": " << Hit << "}";
			Wh.BX = X + Wh.Burst * std::cos(A);
			Wh.BY = Y + Wh.Burst * std::sin(A);
			Rounds << "{\"t\": " << Hit << ", \"w\": \"" << Wh.W << "\", \"on\": " << Wh.B << ", \"ok\": " << Wh.K << ", \"x\": " << Wh.BX << ", \"y\": " << Wh.BY
			       << ", \"r\": \"kill\"}";
			Men.push_back(Wh);
		}
		R << "], " << Rounds.str() << "]}";
		return R.str();
	}

	/**
	 * The guns' battle: 240 men, an American and a German line, each killed at 3 s by a shell that burst 0.6 m from
	 * him (as the rules lay most shell kills on the man), fired by a gun of his own 400 m off all round him (the round's
	 * by and k, a man standing there): a howitzer's (block 3, cal "howitzer"), a light gun's (block 4, cal "light"), a
	 * tank's or a mortar's (block 3); standing, running, kneeling or lying, his heading a little each way.
	 */
	std::string GunsRecording(std::vector<Who>& Men)
	{
		std::ostringstream R, Guns, Rounds;
		R.precision(10);
		Guns.precision(10);
		Rounds.precision(10);
		R << "{\"seed\": 31, \"date\": \"July 1944\", \"sky\": {\"latLon\": [49.12, -1.09]},"
		  << "\"blocks\": [{\"id\": 1, \"army\": \"USA\", \"nat\": \"USA\", \"standIn\": \"POL\"}, {\"id\": 2, \"army\": \"GER\", \"nat\": \"GER\"},"
		  << " {\"id\": 3, \"army\": \"GER\", \"nat\": \"GER\", \"cal\": \"howitzer\"}, {\"id\": 4, \"army\": \"GER\", \"nat\": \"GER\", \"cal\": \"light\"}],"
		  << "\"men\": [";
		Rounds << "\"rounds\": [";
		const char* const Weapons[] = { "gun", "gun", "tank", "mortar" };
		const char* const Stances[] = { "fire_stand", "wait", "advance", "fire_kneel", "fire_prone" };
		const double Pace = 2.5, Off = 400.0;
		for (int I = 0; I < 240; ++I)
		{
			const int N = I / 2;
			Who Wh;
			Wh.B = I % 2 ? 2 : 1;
			Wh.K = N + 1;
			Wh.W = Weapons[N % 4];
			Wh.bLight = N % 4 == 1;
			Wh.Burst = 0.6;
			const char* Stance = Stances[(N / 4) % 5];
			const double X = 30.0 * N, Y = Wh.B == 1 ? 0.0 : 60.0, F = (Wh.B == 1 ? 0.0 : 3.14159265358979) + 0.25 * (N % 5 - 2);
			const double Go = std::strcmp(Stance, "advance") == 0 ? Pace : 0.0;
			Wh.X = X, Wh.Y = Y, Wh.F = F, Wh.Go = Go;
			// (his gun all round him, by the golden angle; the burst any way from him)
			const double Gun = F + 0.37 + 2.39996322972865 * I, A = 0.9 * I;
			Wh.bBy = true;
			Wh.SX = X + Off * std::sin(Gun);
			Wh.SY = Y + Off * std::cos(Gun);
			Wh.BX = X + Wh.Burst * std::cos(A);
			Wh.BY = Y + Wh.Burst * std::sin(A);
			const int By = Wh.bLight ? 4 : 3;
			if (I) { R << ","; Rounds << ","; }
			R << "{\"b\": " << Wh.B << ", \"k\": " << Wh.K << ", \"r\": \"rifle\", \"at\": [[0, " << X - std::sin(F) * Go * Hit << ", " << Y - std::cos(F) * Go * Hit << ", " << F
			  << "], [10, " << X + std::sin(F) * Go * (10.0 - Hit) << ", " << Y + std::cos(F) * Go * (10.0 - Hit) << ", " << F << "]], \"ch\": [[0, \"" << Stance
			  << "\", \"none\", 0, 0, 0, 0]], \"d\": " << Hit << "}";
			Guns << ", {\"b\": " << By << ", \"k\": " << I + 1 << ", \"r\": \"rifle\", \"at\": [[0, " << Wh.SX << ", " << Wh.SY << ", " << Gun + 3.14159265358979 << "], [10, "
			     << Wh.SX << ", " << Wh.SY << ", " << Gun + 3.14159265358979 << "]], \"ch\": [[0, \"wait\", \"none\", 0, 0, 0, 0]]}";
			Rounds << "{\"t\": " << Hit << ", \"w\": \"" << Wh.W << "\", \"by\": " << By << ", \"k\": " << I + 1 << ", \"on\": " << Wh.B << ", \"ok\": " << Wh.K
			       << ", \"x\": " << Wh.BX << ", \"y\": " << Wh.BY << ", \"r\": \"kill\"}";
			Men.push_back(Wh);
		}
		R << Guns.str() << "], " << Rounds.str() << "]}";
		return R.str();
	}

	/**
	 * The band's battle (the verifier of 35ffc951: a man standing or running with the burst, or his gun where it burst
	 * on him, some 38 to 45 degrees ahead on his left flung a limb toward it): every twin's men, a block each (its army
	 * its file's name), each killed at 3 s with the burst, or the gun that fired it, 31 to 49 degrees round on his left
	 * or his right of his nose (where the falls from the front give way to the side's), by a howitzer's shell, a light
	 * gun's, a tank's, a mortar's or a grenade; standing, running or kneeling; the burst 1.6 or 2.4 m off (a grenade's 1.6
	 * or 1.8), or 0.6 m off with the gun 400 m away (a grenade's thrower 20 m: a track that is no man, its place alone).
	 */
	std::string BandRecording(const std::vector<std::string>& FileArmies, std::vector<Who>& Men)
	{
		std::ostringstream R, Guns, Rounds;
		R.precision(10);
		Guns.precision(10);
		Rounds.precision(10);
		R << "{\"seed\": 37, \"date\": \"July 1944\", \"sky\": {\"latLon\": [49.12, -1.09]}, \"blocks\": [";
		for (size_t T = 0; T < FileArmies.size(); ++T) R << (T ? ", " : "") << "{\"id\": " << 10 + T << ", \"army\": \"" << FileArmies[T] << "\"}";
		R << ", {\"id\": 3, \"army\": \"GER\", \"nat\": \"GER\", \"cal\": \"howitzer\"}, {\"id\": 4, \"army\": \"GER\", \"nat\": \"GER\", \"cal\": \"light\"}], \"men\": [";
		Rounds << "\"rounds\": [";
		const int Bearings[] = { -49, -46, -43, -40, -37, -34, -31, 31, 34, 37, 40, 43, 46, 49 };
		const char* const Weapons[] = { "gun", "gun", "tank", "mortar", "grenade" };
		const char* const Stances[] = { "fire_stand", "advance", "fire_kneel" };
		const double Pace = 2.5;
		int Shooter = 0;
		for (size_t T = 0; T < FileArmies.size(); ++T)
		{
			for (int N = 0; N < 210; ++N)
			{
				Who Wh;
				Wh.B = 10 + static_cast<int>(T);
				Wh.K = N + 1;
				const int Wi = (N / 14) % 5;
				Wh.W = Weapons[Wi];
				Wh.bLight = Wi == 1;
				const char* Stance = Stances[(N / 70) % 3];
				const int Place = (N + N / 14 + N / 70) % 3;
				const double X = 30.0 * N, Y = 60.0 * static_cast<double>(T), F = 0.3 * (N % 7 - 3) + (N % 2 ? 3.14159265358979 : 0.0);
				const double Go = std::strcmp(Stance, "advance") == 0 ? Pace : 0.0;
				Wh.X = X, Wh.Y = Y, Wh.F = F, Wh.Go = Go;
				// (the burst, or his gun, that far round from his nose: clockwise his right, as the field's headings run)
				const double Round = F + Bearings[N % 14] * 3.14159265358979 / 180.0;
				// (every round fired by a gun of its block, at the bearing: a light gun's block's cal "light"; a round with no
				// gun known is read as a howitzer's)
				const double Off = Wh.W == "grenade" ? 20.0 : 400.0;
				Wh.bBy = true;
				Wh.SX = X + Off * std::sin(Round);
				Wh.SY = Y + Off * std::cos(Round);
				if (Place < 2)
				{
					Wh.Burst = Wh.W == "grenade" ? (Place ? 1.8 : 1.6) : (Place ? 2.4 : 1.6);
					Wh.BX = X + Wh.Burst * std::sin(Round);
					Wh.BY = Y + Wh.Burst * std::cos(Round);
				}
				else
				{
					const double A = 0.9 * N;
					Wh.Burst = 0.6;
					Wh.BX = X + Wh.Burst * std::cos(A);
					Wh.BY = Y + Wh.Burst * std::sin(A);
				}
				if (T || N) { R << ","; Rounds << ","; }
				R << "{\"b\": " << Wh.B << ", \"k\": " << Wh.K << ", \"r\": \"rifle\", \"at\": [[0, " << X - std::sin(F) * Go * Hit << ", " << Y - std::cos(F) * Go * Hit << ", " << F
				  << "], [10, " << X + std::sin(F) * Go * (10.0 - Hit) << ", " << Y + std::cos(F) * Go * (10.0 - Hit) << ", " << F << "]], \"ch\": [[0, \"" << Stance
				  << "\", \"none\", 0, 0, 0, 0]], \"d\": " << Hit << "}";
				Rounds << "{\"t\": " << Hit << ", \"w\": \"" << Wh.W << "\", ";
				if (Wh.bBy)
				{
					const int By = Wh.bLight ? 4 : 3;
					++Shooter;
					Guns << ", {\"b\": " << By << ", \"k\": " << Shooter << ", \"r\": \"gun\", \"at\": [[0, " << Wh.SX << ", " << Wh.SY << ", " << Round + 3.14159265358979
					     << "], [10, " << Wh.SX << ", " << Wh.SY << ", " << Round + 3.14159265358979 << "]]}";
					Rounds << "\"by\": " << By << ", \"k\": " << Shooter << ", ";
				}
				Rounds << "\"on\": " << Wh.B << ", \"ok\": " << Wh.K << ", \"x\": " << Wh.BX << ", \"y\": " << Wh.BY << ", \"r\": \"kill\"}";
				Men.push_back(Wh);
			}
		}
		R << Guns.str() << "], " << Rounds.str() << "]}";
		return R.str();
	}

	uint64_t HashText(const std::string& T, uint64_t H)
	{
		for (const unsigned char Ch : T) H = (H ^ Ch) * 1099511628211ull;
		return (H ^ 0xFFu) * 1099511628211ull;
	}

	uint64_t HashOf(const std::vector<float>& V, uint64_t H)
	{
		for (const float F : V)
		{
			uint32_t U = 0;
			std::memcpy(&U, &F, sizeof(U));
			H = (H ^ U) * 1099511628211ull;
		}
		return H;
	}

	/** The widest any limb stands off its joint (metres): the cut bone's head against where its parent holds it. */
	double LimbGap(const Crowd& C, int Man)
	{
		const Soldier& S = *C.SoldierOf(Man);
		const Xform* Fr = C.FramesOf(Man);
		double Most = 0.0;
		for (const char* Name : Cuts)
		{
			const int B = S.BoneIndex(Name);
			if (B < 0) continue;
			const int P = S.Bones[static_cast<size_t>(B)].Parent;
			if (P < 0) continue;
			const Vec3 Head = S.Bones[static_cast<size_t>(B)].Head;
			const Vec3 A = Fr[B].Point(Head), Q = Fr[P].Point(Head);
			Most = std::max(Most, std::sqrt(static_cast<double>((A.X - Q.X) * (A.X - Q.X) + (A.Y - Q.Y) * (A.Y - Q.Y) + (A.Z - Q.Z) * (A.Z - Q.Z))));
		}
		return Most;
	}

	/** The highest any limb off its joint stands above the (flat) ground now: every bone below a cut bone that has left
	 * its joint by more than 0.1 m, at its head. 0 where none has. */
	double LimbHigh(const Crowd& C, int Man)
	{
		const Soldier& S = *C.SoldierOf(Man);
		const Xform* Fr = C.FramesOf(Man);
		std::vector<char> Off(S.Bones.size(), 0);
		for (const char* Name : Cuts)
		{
			const int B = S.BoneIndex(Name);
			if (B < 0 || S.Bones[static_cast<size_t>(B)].Parent < 0) continue;
			const Vec3 Head = S.Bones[static_cast<size_t>(B)].Head;
			const Vec3 A = Fr[B].Point(Head), Q = Fr[S.Bones[static_cast<size_t>(B)].Parent].Point(Head);
			if (std::sqrt(static_cast<double>((A.X - Q.X) * (A.X - Q.X) + (A.Y - Q.Y) * (A.Y - Q.Y) + (A.Z - Q.Z) * (A.Z - Q.Z))) > 0.1) Off[static_cast<size_t>(B)] = 1;
		}
		double Most = 0.0;
		for (size_t B = 0; B < S.Bones.size(); ++B)
		{
			bool bLimb = false;
			for (int P = static_cast<int>(B); P >= 0 && !bLimb; P = S.Bones[static_cast<size_t>(P)].Parent) bLimb = Off[static_cast<size_t>(P)] != 0;
			if (bLimb) Most = std::max(Most, static_cast<double>(Fr[B].Point(S.Bones[B].Head).Z));
		}
		return Most;
	}

	/**
	 * The widest a whole joint of a gored man stands open (metres; the art director, 18:34Z: no cap seen till his limb
	 * goes): his twin's seam copies (its near points after his own file's, before the caps) against the points they
	 * copy, at the same rest place, skinned; only at cuts still whole (a copy and its point skinned alike once joined).
	 * His own file can hold several points at one rest place, each side of his own seams, weighted a little apart (the
	 * German's right knee: four, w2 0.119 and 0.106): a copy carries the weights of the one it copies (gore.py), so it is
	 * read against the point it copies, found from the triangles (the twin keeps his own file's triangles first and in
	 * order, pointing each at a copy where a copy took its place); only where his file and the twin's triangles do not
	 * line up (a base file of another delivery) against the nearest of the points at its rest place, which cannot see a
	 * copy given the weights of another of them (bExact false). -1 where it cannot be read.
	 */
	double JointOpen(Crowd& C, int Man, const Soldier& Own, bool& bExact)
	{
		const Soldier& Tw = *C.SoldierOf(Man);
		const int N = Own.Meshes[Near].Points, All = Tw.Meshes[Near].Points;
		if (Tw.Cuts.empty()) return -1.0;
		// (his mesh as the crowd makes it: under a helmet finish the points only his own helmet uses left out)
		const Mesh& Full = Tw.Meshes[Near];
		std::vector<int> Ix(static_cast<size_t>(All), 0);
		int Kept = 0;
		{
			std::vector<uint8_t> Use(static_cast<size_t>(All), 2);
			if (C.Info(Man).Finish >= 0 && Tw.HelmetCount[Near] > 0)
			{
				std::fill(Use.begin(), Use.end(), 0);
				const size_t F = static_cast<size_t>(Tw.HelmetFirst[Near]), Fc = static_cast<size_t>(Tw.HelmetCount[Near]);
				for (size_t T = 0; T < Full.Triangles.size() / 3; ++T)
					for (int K = 0; K < 3; ++K) Use[Full.Triangles[T * 3 + static_cast<size_t>(K)]] |= (T >= F && T < F + Fc) ? 1 : 2;
			}
			for (size_t P = 0; P < Use.size(); ++P) Ix[P] = Use[P] != 1 ? Kept++ : -1;
		}
		if (C.PointsOf(Man, Near) < Kept) return -1.0;
		int Caps = All;
		for (const Soldier::Cut& K : Tw.Cuts) Caps = std::min({ Caps, K.Stump[Near][0], K.Limb[Near][0] });
		// (each copy's own point, from the triangles: Src[copy] = his point; any triangle that does not line up, or a copy
		// standing for two points, and the nearest is read instead)
		std::vector<int> Src(static_cast<size_t>(All), -1);
		bExact = true;
		{
			const std::vector<uint32_t>& Bt = Own.Meshes[Near].Triangles;
			const std::vector<uint32_t>& Tt = Tw.Meshes[Near].Triangles;
			if (Bt.size() > Tt.size()) bExact = false;
			for (size_t K = 0; bExact && K < Bt.size(); ++K)
			{
				const uint32_t B = Bt[K], T = Tt[K];
				if (T == B) continue;
				if (static_cast<int>(T) < N || static_cast<int>(T) >= All || static_cast<int>(B) >= N) { bExact = false; break; }
				int& Of = Src[static_cast<size_t>(T)];
				if (Of >= 0 && Of != static_cast<int>(B)) { bExact = false; break; }
				Of = static_cast<int>(B);
			}
			if (!bExact) std::fill(Src.begin(), Src.end(), -1);
		}
		std::multimap<std::tuple<float, float, float>, int> Orig;
		const std::vector<float>& R = Tw.Meshes[Near].At;
		for (int P = 0; P < N; ++P) Orig.emplace(std::make_tuple(R[static_cast<size_t>(P) * 3], R[static_cast<size_t>(P) * 3 + 1], R[static_cast<size_t>(P) * 3 + 2]), P);
		std::vector<float> At, Fa;
		Skinned(C, Man, At, Fa);
		// (the cuts whose bone has left its joint, as the crowd reads them, and the bones that went with each)
		const Xform* Fr = C.FramesOf(Man);
		uint32_t Off = 0;
		std::vector<char> Gone(Tw.Bones.size(), 0);
		for (size_t K = 0; K < Tw.Cuts.size(); ++K)
		{
			const int B = Tw.Cuts[K].BoneIx;
			const Vec3 Head = Tw.Bones[static_cast<size_t>(B)].Head;
			const Vec3 A = Fr[B].Point(Head), Q = Fr[Tw.Bones[static_cast<size_t>(B)].Parent].Point(Head);
			if ((A.X - Q.X) * (A.X - Q.X) + (A.Y - Q.Y) * (A.Y - Q.Y) + (A.Z - Q.Z) * (A.Z - Q.Z) <= 1e-4f) continue;
			Off |= 1u << K;
			for (size_t Bn = 0; Bn < Tw.Bones.size(); ++Bn)
				for (int P = static_cast<int>(Bn); P >= 0; P = Tw.Bones[static_cast<size_t>(P)].Parent) if (P == B) Gone[Bn] = 1;
		}
		std::map<int, uint16_t> Masks;
		const Soldier::Joined& Jn = Tw.Joins[Near];
		for (size_t K = 0; K < Jn.Point.size(); ++K) Masks[static_cast<int>(Jn.Point[K])] = Jn.Mask[K];
		// (a point at a cut that is off, or riding a limb that went: not a whole joint)
		auto AtOff = [&](int P) {
			const auto Mk = Masks.find(P);
			if (Mk != Masks.end()) return (Mk->second & Off) != 0u;
			return Gone[Full.Part[static_cast<size_t>(P)]] != 0 || Gone[Full.Part2[static_cast<size_t>(P)]] != 0;
		};
		double Most = 0.0;
		for (int P = N; P < Caps; ++P)
		{
			const size_t O = static_cast<size_t>(P) * 3;
			if (AtOff(P) || Ix[static_cast<size_t>(P)] < 0) continue;
			const size_t A = static_cast<size_t>(Ix[static_cast<size_t>(P)]) * 3;
			double Least = -1.0;
			const int Exact = bExact ? Src[static_cast<size_t>(P)] : -1;
			const auto Span = Orig.equal_range(std::make_tuple(R[O], R[O + 1], R[O + 2]));
			for (auto Found = Span.first; Found != Span.second; ++Found)
			{
				if (Exact >= 0 && Found->second != Exact) continue;
				if (AtOff(Found->second) || Ix[static_cast<size_t>(Found->second)] < 0) continue;
				const size_t Q = static_cast<size_t>(Ix[static_cast<size_t>(Found->second)]) * 3;
				const double G = std::sqrt(static_cast<double>((At[A] - At[Q]) * (At[A] - At[Q]) + (At[A + 1] - At[Q + 1]) * (At[A + 1] - At[Q + 1]) + (At[A + 2] - At[Q + 2]) * (At[A + 2] - At[Q + 2])));
				Least = Least < 0.0 ? G : std::min(Least, G);
			}
			if (Least >= 0.0) Most = std::max(Most, Least);
		}
		return Most;
	}

	/** A gore fall's side as the twin gives it (gore_from), or empty. */
	std::string GoreFromOf(const Soldier& S, const std::string& Clip)
	{
		const auto Found = S.GoreFrom.find(Clip);
		return Found == S.GoreFrom.end() ? std::string() : Found->second;
	}

	/** The big throw: a fall of kind thrown, or any die_blast_* (the art director's: a field gun's or a howitzer's). */
	bool Throws(const std::string& Kind, const std::string& Clip) { return Kind == "thrown" || Clip.rfind("die_blast_", 0) == 0; }

	/** On the field now: the head of each bone his fall takes off (its limb_off events'), then his pelvis's. */
	std::vector<Vec3> CutHeads(const Crowd& C, int Man)
	{
		std::vector<Vec3> Out;
		const Soldier& S = *C.SoldierOf(Man);
		const Xform* Fr = C.FramesOf(Man);
		const int Ix = S.ClipIndex(C.Info(Man).FallClip);
		if (Ix >= 0)
		{
			for (const ClipEvent& E : S.Clips[static_cast<size_t>(Ix)].Events)
			{
				const int B = E.Fx == "limb_off" ? S.BoneIndex(E.BoneName) : -1;
				if (B >= 0) Out.push_back(Fr[B].Point(S.Bones[static_cast<size_t>(B)].Head));
			}
		}
		const int P = S.BoneIndex("pelvis");
		if (P >= 0) Out.push_back(Fr[P].Point(S.Bones[static_cast<size_t>(P)].Head));
		return Out;
	}

	struct Result
	{
		std::vector<std::string> Clip;
		std::vector<int> Gored;
		std::vector<double> Gap, High;
		uint64_t Skin = 1469598103934665603ull;
		int LimbOff = 0, NaN = 0;
		double Ms = 0.0;
		std::vector<std::string> Fall;              // each man's fall: clip, kind, from, stance
		std::vector<uint64_t> SkinOf;               // each man's skin at the end, hashed
		std::vector<std::vector<Vec3>> Joint, End;  // a gored man's CutHeads on his fall's first frame, and at the end
	};

	/** The digest of some men's falls and skins: "falls <hash>, skins <hash>". */
	std::string Digest(const Result& R, const std::vector<char>& Take)
	{
		uint64_t Fa = 1469598103934665603ull, Sk = 1469598103934665603ull;
		for (size_t I = 0; I < R.Fall.size(); ++I)
		{
			if (!Take[I]) continue;
			Fa = HashText(R.Fall[I], Fa);
			Sk = (Sk ^ R.SkinOf[I]) * 1099511628211ull;
		}
		char Words[80];
		std::snprintf(Words, sizeof(Words), "falls %016llx, skins %016llx", static_cast<unsigned long long>(Fa), static_cast<unsigned long long>(Sk));
		return Words;
	}

	Result Play(Crowd& C, const std::vector<Who>& Men, double From, double To)
	{
		Result Out;
		const auto T0 = std::chrono::steady_clock::now();
		double Measured = 0.0;
		Out.High.assign(Men.size(), 0.0);
		Out.Joint.assign(Men.size(), {});
		Out.End.assign(Men.size(), {});
		std::vector<char> Struck(Men.size(), 0);
		for (double T = From; T <= To; T += 1.0 / 60.0)
		{
			C.Update(T);
			for (const Event& E : C.Events()) Out.LimbOff += E.Fx == "limb_off" ? 1 : 0;
			// (the limbs' arcs, every frame: the art director's 3 m cap, 18:34Z; not in the time; and a gored man's cut
			// bones where his fall begins, his joints as he was struck)
			const auto M0 = std::chrono::steady_clock::now();
			for (size_t I = 0; I < Men.size(); ++I)
			{
				const int Man = C.Find(Men[I].B, Men[I].K);
				Out.High[I] = std::max(Out.High[I], LimbHigh(C, Man));
				if (!Struck[I] && C.Info(Man).FallStarted >= 0.0)
				{
					Struck[I] = 1;
					if (!C.SoldierOf(Man)->DeathsGore.empty()) Out.Joint[I] = CutHeads(C, Man);
				}
			}
			Measured += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - M0).count();
		}
		Out.Ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - T0).count() - Measured;
		for (size_t I = 0; I < Men.size(); ++I)
		{
			const int Man = C.Find(Men[I].B, Men[I].K);
			const ManInfo& In = C.Info(Man);
			Out.Clip.push_back(In.FallClip);
			Out.Fall.push_back(In.FallClip + "/" + In.FallKind + "/" + In.FallFrom + "/" + In.FallStance);
			std::vector<float> At, Fa;
			Skinned(C, Man, At, Fa);
			for (const float F : At) Out.NaN += std::isfinite(F) ? 0 : 1;
			Out.Skin = HashOf(At, Out.Skin);
			Out.SkinOf.push_back(HashOf(At, 1469598103934665603ull));
			Out.Gap.push_back(LimbGap(C, Man));
			if (!Out.Joint[I].empty()) Out.End[I] = CutHeads(C, Man);
		}
		return Out;
	}

	/** A stance's gored men as the test reads them: their limbs off and their bodies (their pelvises), each moved away
	 * from the burst (or the gun), along the line from it to where it stood as he was struck (as played, ReadSides; or
	 * as the falls given would move them, from their first frames and last, the sweep). */
	struct Lot
	{
		int Men = 0, Limbs = 0, Toward = 0, Short = 0;  // men (cells, in the sweep); limbs off; of them moving toward; under 0.5 m away
		double Least = 1e9;                             // the least a limb moved away (m)
		std::string LeastAt;
		int Bodies = 0, BodyToward = 0, BodyWorse = 0;  // pelvises; moving toward; more than the slack toward where another fall he may take moves it away
		double BodyLeast = 1e9;
		std::string BodyAt;
	};

	/** The stances the gore falls are read for, in order. */
	const char* const GoreStances[] = { "stand", "run", "kneel", "cover", "prone" };

	/** What the gored men of a play show of their bursts' sides (the art director, 23 Nov 17:52Z: the limb flies away
	 * from the blast; die_blast_* a field gun's or a howitzer's; the deaths thread's gore_from, and of 24 Nov its falls
	 * from under and the lying men's from each side). */
	struct Sided
	{
		int Gored = 0, Read = 0;         // gored; of them read by a side (1.5 m or more from the burst, or by his gun's side where it burst under him and he has no fall from under he may take)
		int Unread = 0;                  // neither read nor under: 1 to 1.5 m from the burst, or under him with no fall from under he may take and no gun known
		int ByGun = 0;                   // of those read, by the gun's side
		int OffSide = 0;                 // of those, in a fall from another side where one from his he may take moves every limb half a metre away
		int Wide = 0, WideOff = 0;       // of those, none from his side he may take so; of them in a fall from another side
		int Worse = 0;                   // of those, in a fall whose limbs move less far away than another he may take, up to half a metre
		int NoneThere = 0, NoneUp = 0;   // of those, his stance's gore falls none from his side he may take (of them standing or running)
		int Opposite = 0;                // of those with a fall from their side he may take, in one from the side opposite
		double FallLeast = 1e9;          // the least the falls of those standing or running move a limb away, read from their first frames and last
		int NoneLimbs = 0;               // the limbs off of the men read with no fall from their side
		double NoneLeast = 1e9;
		std::map<std::string, Lot> Lots; // by his stance: the limbs and bodies of the men read and of those thrown from under (by the gun)
		int Every = 0, EveryToward = 0;  // the limbs off of men whose stance's gore falls hold one from each side that is not a throw; of them toward
		int ThrownRead = 0, ThrownToward = 0; // those men thrown; of them a limb or his body moving toward the burst (or the gun)
		int GunGored = 0, GunThrown = 0; // the gored men of a shell that may throw (a gun's not light), and of them thrown
		int OtherThrown = 0;             // thrown by another's (a light gun's, a tank's, a mortar's)
		int Lying = 0, LyingSide = 0;    // gored lying; of them in a fall that is not a throw
		int LyingWrong = 0;              // lying and thrown but not from under him by a field gun's or a howitzer's shell within 1 m
		int Under = 0, UnderFrom = 0;    // within 1 m of the burst, his stance's gore falls holding one from under he may take; of them in one
		int UnderThrown = 0;             // of them thrown from his gun's side (a field gun's or a howitzer's shell)
		int UnderWrong = 0;              // of them in neither, or in a throw his gun's side would not give him
		int UnderOffered = 0, UnderOfferedThrown = 0; // of them by such a shell offered both a fall from under and a throw (GoreFalls); of those thrown
		std::map<std::string, Lot> UnderGun; // by stance: of those in a fall from under, his gun known, their limbs and bodies against the gun
		int UnderLegs = 0, UnderToward = 0; // of those, his falls from under read against the gun; of them in one that moves a limb toward it where another he may take does not, or more than the least of them where all do
		std::map<std::string, std::array<int, 3>> By; // side: gored, thrown, with no fall from there in his stance's table
		std::map<std::string, int> None;              // "stance side": no fall from there
	};

	/** How far a point moved away from Burst along the line from it to where the point began (m; less than 0 toward it). */
	double Away(const Vec3& From, const Vec3& To, double BX, double BY)
	{
		const double OX = static_cast<double>(From.X) - BX, OY = static_cast<double>(From.Y) - BY;
		return ((static_cast<double>(To.X) - static_cast<double>(From.X)) * OX + (static_cast<double>(To.Y) - static_cast<double>(From.Y)) * OY) / std::max(1e-6, std::hypot(OX, OY));
	}

	/** A gore fall as the test reads it from the twin: its kind, its side, whether it is the big throw, and each limb it
	 * takes off (its limb_off events'): the limb's joint, its bone's head, in his own axes on the fall's first frame and
	 * on its last; and his body, his pelvis's head, so. */
	struct FallRead
	{
		std::string Kind, Clip, From;
		bool bThrow = false;
		std::vector<std::pair<Vec3, Vec3>> Limbs;
		bool bBody = false;
		std::pair<Vec3, Vec3> Body;
	};

	/** His stance's gore falls as the test reads them (none where the twin has no falls for that stance). */
	std::vector<FallRead> ReadFalls(const Soldier& Tw, const std::string& Stance)
	{
		std::vector<FallRead> Out;
		const auto St = Tw.DeathsGore.find(Stance);
		if (St == Tw.DeathsGore.end()) return Out;
		for (const auto& Kd : St->second)
		{
			for (const std::string& N : Kd.second)
			{
				FallRead F;
				F.Kind = Kd.first;
				F.Clip = N;
				F.From = GoreFromOf(Tw, N);
				F.bThrow = Throws(Kd.first, N);
				const int Ix = Tw.ClipIndex(N);
				if (Ix >= 0)
				{
					const Clip& Cl = Tw.Clips[static_cast<size_t>(Ix)];
					Pose P0, P1;
					Xform A0[MaxBones], A1[MaxBones];
					Sample(Cl, 0.f, P0);
					Sample(Cl, Cl.Long, P1);
					BoneFrames(Tw, P0, A0);
					BoneFrames(Tw, P1, A1);
					for (const ClipEvent& E : Cl.Events)
					{
						const int B = E.Fx == "limb_off" ? Tw.BoneIndex(E.BoneName) : -1;
						if (B < 0) continue;
						const Vec3 H = Tw.Bones[static_cast<size_t>(B)].Head;
						F.Limbs.push_back({ A0[B].Point(H), A1[B].Point(H) });
					}
					const int Pv = Tw.BoneIndex("pelvis");
					if (Pv >= 0)
					{
						const Vec3 H = Tw.Bones[static_cast<size_t>(Pv)].Head;
						F.bBody = true;
						F.Body = { A0[Pv].Point(H), A1[Pv].Point(H) };
					}
				}
				Out.push_back(F);
			}
		}
		return Out;
	}

	/** The least any limb of the fall moves away from a point at X, Y in his own axes (m; less than 0 toward it). */
	double LeastAway(const FallRead& F, double X, double Y)
	{
		double Least = 1e9;
		for (const auto& L : F.Limbs) Least = std::min(Least, Away(L.first, L.second, X, Y));
		return Least;
	}

	/** How far the fall moves his body (his pelvis) away from a point at X, Y in his own axes (m; less than 0 toward it). */
	double BodyAway(const FallRead& F, double X, double Y) { return F.bBody ? Away(F.Body.first, F.Body.second, X, Y) : 1e9; }

	/** Whether a gored man may take the fall (the test's own reading): any not a throw; a throw only from a field gun's or
	 * a howitzer's shell, and only from his side (the burst under him and his gun not known: only the prone throw, from
	 * under); none from under (gore_from under: a lower leg off, or the prone throw) but where the burst was under him
	 * (Side under). */
	bool MayTakeRead(const FallRead& F, bool bBig, const std::string& Side)
	{
		if (F.From == "under" && Side != "under") return false;
		return !F.bThrow || (bBig && F.From == Side);
	}

	/** Whether a fall's side (gore_from) is the one opposite Side: front and behind, left and right (under opposite none). */
	bool OppositeRead(const std::string& From, const std::string& Side)
	{
		return (Side == "front" && From == "behind") || (Side == "behind" && From == "front") || (Side == "left" && From == "right") || (Side == "right" && From == "left");
	}

	/** What his falls do at a point in his own axes: the farthest the falls he may take from his side move their every
	 * limb away from it (-1e9 where none), and the farthest any he may take does, but where he may take one from his side
	 * none from the side opposite (it throws him toward the point: the verifier of f6f8b75c). OutBody: the farthest any he
	 * may take, from whatever side, moves his body away from it. */
	void BestAway(const std::vector<FallRead>& Falls, bool bBig, const std::string& Side, double X, double Y, double& OutSide, double& OutAll, double* OutBody = nullptr)
	{
		OutSide = OutAll = -1e9;
		double Body = -1e9;
		bool bFromSide = false;
		for (const FallRead& F : Falls) bFromSide = bFromSide || (MayTakeRead(F, bBig, Side) && F.From == Side);
		for (const FallRead& F : Falls)
		{
			if (!MayTakeRead(F, bBig, Side)) continue;
			Body = std::max(Body, BodyAway(F, X, Y));
			if (bFromSide && OppositeRead(F.From, Side)) continue;
			const double G = LeastAway(F, X, Y);
			OutAll = std::max(OutAll, G);
			if (F.From == Side) OutSide = std::max(OutSide, G);
		}
		if (OutBody) *OutBody = Body;
	}

	/** The floor his falls from a side set where he is thrown away from a point (the test's reading): half a metre where
	 * one from that side he may take moves every limb so far away from it; else as far as the best of all he may take
	 * but those from the side opposite moves them, up to half a metre. */
	double FloorRead(const std::vector<FallRead>& Falls, bool bBig, const std::string& Side, double X, double Y)
	{
		double SideBest = -1e9, AllBest = -1e9;
		BestAway(Falls, bBig, Side, X, Y, SideBest, AllBest);
		return SideBest >= 0.5 ? 0.5 : std::min(0.5, AllBest);
	}

	/** Whether his stance's gore falls hold one from each side, front, behind, left and right, that is not a throw (the
	 * deaths thread, 24 Nov: every stance a fall from each side besides the thrown; before, the kneel falls none from the
	 * right and from the left only a throw, and the prone falls only the throw from under). */
	bool EverySide(const std::vector<FallRead>& Falls)
	{
		for (const char* Side : { "front", "behind", "left", "right" })
		{
			bool bHas = false;
			for (const FallRead& F : Falls) bHas = bHas || (!F.bThrow && F.From == Side);
			if (!bHas) return false;
		}
		return true;
	}

	/** Whether his stance's gore falls hold one from under he may take where the burst was under him (the prone throw
	 * only from a field gun's or a howitzer's shell). */
	bool UnderRead(const std::vector<FallRead>& Falls, bool bBig)
	{
		for (const FallRead& F : Falls)
		{
			if (F.From == "under" && MayTakeRead(F, bBig, "under")) return true;
		}
		return false;
	}

	/** The side the test reads his falls by: where the burst was against him (front or behind within 45 degrees of his
	 * nose or his back, left or right between; within 1 m, under), but a burst under him whose stance's gore falls hold
	 * none from under he may take by his gun's side (Gun), where known. */
	std::string ReadSide(const std::vector<FallRead>& Falls, bool bBig, const std::string& Burst, const std::string& Gun)
	{
		if (Burst != "under" || UnderRead(Falls, bBig) || Gun.empty()) return Burst;
		return Gun;
	}

	/** The test reads where he stood and faced from his track at the round as the crowd does, but in its own sums: its
	 * reading of how far a fall moves his limbs away is allowed this much either way (m). */
	constexpr double Slack = 0.01;

	/** How far a gored man's body (his pelvis) may move toward the burst (or his gun) where another fall he may take
	 * moves it away, before it fails (m): as played his pose as struck is not his fall's first frame. */
	constexpr double BodySlack = 0.1;

	/** Each gored man's side, fall and limbs as the test reads them (bNotes: a line for each limb under half a metre away
	 * that is no failure, and each body toward the burst; the failures' lines always). */
	Sided ReadSides(Crowd& C, const std::vector<Who>& Men, const Result& R, std::map<std::string, std::shared_ptr<Soldier>>& Files, bool bNotes = true)
	{
		Sided Out;
		for (const char* Side : { "front", "behind", "left", "right", "under" }) Out.By[Side] = { 0, 0, 0 };
		for (size_t I = 0; I < Men.size(); ++I)
		{
			const int Man = C.Find(Men[I].B, Men[I].K);
			const ManInfo& In = C.Info(Man);
			if (C.SoldierOf(Man) == Files[In.FileArmy].get()) continue;
			const Soldier& Tw = *C.SoldierOf(Man);
			++Out.Gored;
			// (the burst against him: from where he stood and his heading as it struck him, by his track (the recording's,
			// his own) at the round less his lag (ManInfo's Delay), clockwise from north; his right is clockwise of it, the
			// field's x east and y north. A burst within 1 m of him, whatever his stance: under; and his gun's side, where
			// it is known and stood 1 m or more from him, for a throw, or where his stance's gore falls hold none from
			// under he may take)
			const double Struck = In.Dead - In.Delay - Hit, F = Men[I].F;
			const double X = Men[I].X + std::sin(F) * Men[I].Go * Struck, Y = Men[I].Y + std::cos(F) * Men[I].Go * Struck;
			const double Far = std::hypot(Men[I].BX - X, Men[I].BY - Y);
			auto Round = [&](double PX, double PY) {
				double Off = std::atan2(PX - X, PY - Y) - F;
				while (Off > 3.14159265358979) Off -= 6.28318530717959;
				while (Off < -3.14159265358979) Off += 6.28318530717959;
				return Off;
			};
			auto SideAt = [](double Off) { return std::string(std::fabs(Off) < Eighth ? "front" : std::fabs(Off) > 3.0 * Eighth ? "behind" : Off > 0.0 ? "right" : "left"); };
			// (a point in his own axes, x forward and y right)
			auto Own = [&](double PX, double PY, double& OutX, double& OutY) {
				const double DX = PX - X, DY = PY - Y, Sf = std::sin(F), Cf = std::cos(F);
				OutX = DX * Sf + DY * Cf;
				OutY = DX * Cf - DY * Sf;
			};
			const bool bGunKnown = Men[I].bBy && std::hypot(Men[I].SX - X, Men[I].SY - Y) >= 1.0;
			const double GunOff = bGunKnown ? Round(Men[I].SX, Men[I].SY) : 0.0;
			const std::string Gun = bGunKnown ? SideAt(GunOff) : std::string();
			const bool bBig = Men[I].W == "gun" && !Men[I].bLight, bThrown = Throws(In.FallKind, In.FallClip);
			const std::vector<FallRead> Falls = ReadFalls(Tw, In.FallStance);
			const std::string Side = ReadSide(Falls, bBig, Far < 1.0 ? std::string("under") : SideAt(Round(Men[I].BX, Men[I].BY)), Gun);
			const bool bByGun = Far < 1.0 && Side != "under";
			double PX = bByGun ? Men[I].SX : Men[I].BX, PY = bByGun ? Men[I].SY : Men[I].BY;
			double Off = bByGun ? GunOff : Round(Men[I].BX, Men[I].BY);
			const std::string Gf = GoreFromOf(Tw, In.FallClip);
			// (what his stance's gore falls hold: one from his side he may take, a throw only from a gun not light)
			bool bThere = false;
			for (const FallRead& Fr : Falls) bThere = bThere || (MayTakeRead(Fr, bBig, Side) && Fr.From == Side);
			std::array<int, 3>& Row = Out.By[Side];
			++Row[0];
			Row[1] += bThrown ? 1 : 0;
			Row[2] += bThere ? 0 : 1;
			if (bBig) { ++Out.GunGored; Out.GunThrown += bThrown ? 1 : 0; }
			else if (bThrown) ++Out.OtherThrown;
			if (In.FallStance == "prone")
			{
				++Out.Lying;
				Out.LyingSide += bThrown ? 0 : 1;
				if (bThrown && !(bBig && Far < 1.0 && Gf == "under")) ++Out.LyingWrong;
			}
			// (a burst under him whose stance's gore falls hold one from under he may take: one of those, with his gun known
			// read against it below (UnderLegs, UnderToward: none that moves a limb toward the gun where another from under
			// he may take does not, and where all do none but the least so), not known any; or a field gun's or a howitzer's
			// shell's throw from his gun's side where that side's falls give it, every limb at least as far from the gun as
			// the floor they set; a throw so read against the gun below as any man thrown)
			const bool bUnder = Side == "under" && bThere;
			std::string By = Side;
			if (bUnder)
			{
				++Out.Under;
				// (a field gun's or a howitzer's shell: whether GoreFalls offers him both a fall from under, not a throw, and a
				// throw from his gun's side, and so whether he was thrown: the kind from under weighs 3 to the throw's 8)
				if (bBig && bGunKnown)
				{
					GoreStrike Strike;
					Strike.Stance = In.FallStance;
					Strike.Side = "under";
					Strike.Gun = Gun;
					Strike.bAway = true;
					Strike.bBig = true;
					Own(Men[I].SX, Men[I].SY, Strike.X, Strike.Y);
					bool bOfferUnder = false, bOfferThrow = false;
					for (const auto& Fall : GoreFalls(Tw, Strike))
					{
						const bool bTh = Throws(Fall.first, Fall.second);
						bOfferThrow = bOfferThrow || bTh;
						bOfferUnder = bOfferUnder || (!bTh && GoreFromOf(Tw, Fall.second) == "under");
					}
					if (bOfferUnder && bOfferThrow)
					{
						++Out.UnderOffered;
						Out.UnderOfferedThrown += bThrown ? 1 : 0;
					}
				}
				if (Gf == "under")
				{
					++Out.UnderFrom;
					// (his limbs and his body read against his gun, where known, as played, as b470998e read him; and his fall
					// read against it from its first frame and its last, the test's reading: none that moves a limb toward the
					// gun where another fall from under he may take does not, and where all do none but the least so)
					if (bGunKnown)
					{
						Lot& Ug = Out.UnderGun[In.FallStance];
						++Ug.Men;
						char At[200];
						std::snprintf(At, sizeof(At), "%s %s (%s, %s%s, his gun %s %.0f deg)", In.FileArmy.c_str(), In.FallClip.c_str(), In.FallStance.c_str(),
							Men[I].W.c_str(), Men[I].bLight ? " light" : "", Gun.c_str(), GunOff * 57.2957795);
						{
							double UX = 0.0, UY = 0.0, UBest = -1e9, UMine = 1e9;
							Own(Men[I].SX, Men[I].SY, UX, UY);
							for (const FallRead& Fr : Falls)
							{
								if (Fr.From != "under" || !MayTakeRead(Fr, bBig, "under")) continue;
								const double G = LeastAway(Fr, UX, UY);
								UBest = std::max(UBest, G);
								if (Fr.Clip == In.FallClip) UMine = std::min(UMine, G);
							}
							++Out.UnderLegs;
							if (UMine < std::min(0.0, UBest) - Slack)
							{
								++Out.UnderToward;
								std::printf("     under toward: %s: his fall from under moves a limb %.2f m away from his gun (less than 0 toward it), the best of his falls "
									"from under he may take %.2f m\n", At, UMine, UBest);
							}
						}
						const std::vector<Vec3>& U0 = R.Joint[I];
						const std::vector<Vec3>& U1 = R.End[I];
						for (size_t L = 0; L < U0.size() && L < U1.size(); ++L)
						{
							const double Gain = Away(U0[L], U1[L], Men[I].SX, Men[I].SY);
							if (L + 1 < U0.size())
							{
								++Ug.Limbs;
								Ug.Toward += Gain < 0.0 ? 1 : 0;
								if (Gain < Ug.Least) { Ug.Least = Gain; Ug.LeastAt = At; }
							}
							else
							{
								++Ug.Bodies;
								Ug.BodyToward += Gain < 0.0 ? 1 : 0;
								if (Gain < Ug.BodyLeast) { Ug.BodyLeast = Gain; Ug.BodyAt = At; }
							}
						}
					}
					continue;
				}
				double GX = 0.0, GY = 0.0, Taken = 1e9;
				Own(Men[I].SX, Men[I].SY, GX, GY);
				for (const FallRead& Fr : Falls) if (Fr.Clip == In.FallClip) Taken = std::min(Taken, LeastAway(Fr, GX, GY));
				if (!(bThrown && bBig && bGunKnown && Gf == Gun) || Taken < FloorRead(Falls, bBig, Gun, GX, GY) - Slack)
				{
					++Out.UnderWrong;
					std::printf("     under: %s %s (%s, %s%s, the burst %.2f m off, read by the crowd %s): the burst under him, neither a fall from under nor the throw "
						"his gun's side (%s) gives\n", In.FileArmy.c_str(), In.FallClip.c_str(), In.FallStance.c_str(), Men[I].W.c_str(), Men[I].bLight ? " light" : "", Far,
						In.GoreFrom.c_str(), bGunKnown ? Gun.c_str() : "not known");
					continue;
				}
				++Out.UnderThrown;
				PX = Men[I].SX, PY = Men[I].SY, Off = GunOff, By = Gun;
			}
			else
			{
				if (Far < 1.5 && !bByGun) { ++Out.Unread; continue; }
				++Out.Read;
				Out.ByGun += bByGun ? 1 : 0;
			}
			// (the burst, or his gun, in his own axes, x forward and y right; and how far his stance's gore falls would
			// move their limbs away from it, the test's own reading from their first frame and their last: those from his
			// side he may take at best, any he may take at best, and his own fall)
			double OwnX = 0.0, OwnY = 0.0;
			Own(PX, PY, OwnX, OwnY);
			double SideBest = -1e9, AllBest = -1e9, BodyBest = -1e9, Taken = 1e9;
			BestAway(Falls, bBig, By, OwnX, OwnY, SideBest, AllBest, &BodyBest);
			for (const FallRead& Fr : Falls) if (Fr.Clip == In.FallClip) Taken = std::min(Taken, LeastAway(Fr, OwnX, OwnY));
			const bool bUp = In.FallStance == "stand" || In.FallStance == "run";
			if (!bUnder)
			{
				const bool bOwnSide = Gf == Side;
				if (!bThere)
				{
					++Out.NoneThere;
					Out.NoneUp += bUp ? 1 : 0;
					++Out.None[In.FallStance + " " + Side];
				}
				else if (SideBest < 0.5)
				{
					++Out.Wide;
					Out.WideOff += bOwnSide ? 0 : 1;
				}
				if (bUp) Out.FallLeast = std::min(Out.FallLeast, Taken);
				if (bThere && !bOwnSide && SideBest >= 0.5 + Slack)
				{
					++Out.OffSide;
					std::printf("     off side: %s %s (%s, %s, %s): one from his side he may take moves every limb %.2f m away\n", In.FileArmy.c_str(), In.FallClip.c_str(),
						In.FallStance.c_str(), Men[I].W.c_str(), Side.c_str(), SideBest);
				}
				if (bThere && OppositeRead(Gf, Side))
				{
					++Out.Opposite;
					std::printf("     opposite: %s %s (%s, %s, %s): a fall from the side opposite, where his stance's gore falls hold one from his side he may take\n",
						In.FileArmy.c_str(), In.FallClip.c_str(), In.FallStance.c_str(), Men[I].W.c_str(), Side.c_str());
				}
				if (Taken < std::min(0.5, AllBest) - Slack)
				{
					++Out.Worse;
					std::printf("     worse: %s %s (%s, %s, %s) moves a limb %.2f m away where one he may take moves every limb %.2f m\n", In.FileArmy.c_str(),
						In.FallClip.c_str(), In.FallStance.c_str(), Men[I].W.c_str(), Side.c_str(), Taken, AllBest);
				}
			}
			// (each limb off, and his pelvis last: how far it moved away from the burst (or his gun), along the line from
			// it to where it stood as he was struck. A limb never toward it where his stance's gore falls hold one from each
			// side that is not a throw (standing or running always; kneeling, in cover or lying since the deaths thread's
			// falls of 24 Nov), a note where they hold none from some side; standing or running his body never toward it
			// more than BodySlack where another fall he may take moves it away, else a note; a throw never toward it)
			const std::vector<Vec3>& J0 = R.Joint[I];
			const std::vector<Vec3>& J1 = R.End[I];
			const bool bEvery = EverySide(Falls);
			Lot& Lt = Out.Lots[In.FallStance];
			++Lt.Men;
			bool bThrownToward = false;
			const char* What = bByGun || bUnder ? "his gun" : "the burst";
			char Where[200];
			std::snprintf(Where, sizeof(Where), "%s %s (%s, %s%s, %s %s %.0f deg%s)", In.FileArmy.c_str(), In.FallClip.c_str(), In.FallStance.c_str(), Men[I].W.c_str(),
				Men[I].bLight ? " light" : "", bByGun || bUnder ? "gun" : "burst", By.c_str(), Off * 57.2957795, bUnder ? ", it burst under him" : bThere ? "" : ", no fall from there");
			for (size_t L = 0; L < J0.size() && L < J1.size(); ++L)
			{
				const double Gain = Away(J0[L], J1[L], PX, PY);
				bThrownToward = bThrownToward || Gain <= 0.0;
				if (L + 1 < J0.size())
				{
					if (!bThere) { ++Out.NoneLimbs; Out.NoneLeast = std::min(Out.NoneLeast, Gain); }
					++Lt.Limbs;
					if (Gain < Lt.Least) { Lt.Least = Gain; Lt.LeastAt = Where; }
					if (bEvery) { ++Out.Every; Out.EveryToward += Gain < 0.0 ? 1 : 0; }
					if (Gain < 0.0)
					{
						++Lt.Toward;
						if (bUp || bEvery) std::printf("     limb toward: %s a limb moved %.2f m toward %s\n", Where, -Gain, What);
						else if (bNotes) std::printf("     note: %s a limb moved %.2f m toward %s (his stance's gore falls hold none from some side that is not a throw; at best %.2f m)\n",
							Where, -Gain, What, AllBest);
					}
					else if (Gain < 0.5)
					{
						++Lt.Short;
						if (bNotes) std::printf("     note: %s a limb moved %.2f m away from %s (his fall's own, from its first frame, %.2f m; at best %.2f m)\n", Where, Gain, What,
							Taken, AllBest);
					}
				}
				else
				{
					++Lt.Bodies;
					Lt.BodyToward += Gain > 0.0 ? 0 : 1;
					if (Gain < Lt.BodyLeast) { Lt.BodyLeast = Gain; Lt.BodyAt = Where; }
					const bool bWorse = Gain < -BodySlack && BodyBest >= 0.0;
					Lt.BodyWorse += bWorse ? 1 : 0;
					if (bUp && bWorse)
						std::printf("     body toward: %s his body moved %.2f m toward %s where another fall he may take moves it %.2f m away\n", Where, -Gain, What, BodyBest);
					else if (bNotes && Gain <= 0.0)
						std::printf("     note: %s his body moved %.2f m toward %s (the fall's own way%s)\n", Where, -Gain, What, bWorse ? "; another he may take moves it away" : "");
				}
			}
			if (bThrown)
			{
				++Out.ThrownRead;
				Out.ThrownToward += bThrownToward ? 1 : 0;
				if (bThrownToward) std::printf("     thrown toward: %s %s (%s, %s, %s)\n", In.FileArmy.c_str(), In.FallClip.c_str(), In.FallStance.c_str(), Men[I].W.c_str(), By.c_str());
			}
		}
		return Out;
	}

	/** A lot's line: its limbs and bodies. */
	void PrintLot(const char* Label, const char* What, const Lot& L, double Body)
	{
		std::printf("     %s: %s: %d men read, %d limbs off, %d toward, %d under 0.5 m away, the least %.2f m%s%s%s; %d bodies, %d toward, %d of them more than %.2f m where another "
			"fall he may take moves it away, the most %.2f m toward%s%s%s\n", Label, What, L.Men, L.Limbs, L.Toward, L.Short, L.Limbs ? L.Least : 0.0, L.Limbs ? " (" : "",
			L.Limbs ? L.LeastAt.c_str() : "", L.Limbs ? ")" : "", L.Bodies, L.BodyToward, L.BodyWorse, Body, L.BodyLeast < 0.0 ? -L.BodyLeast : 0.0,
			L.BodyLeast < 0.0 ? " (" : "", L.BodyLeast < 0.0 ? L.BodyAt.c_str() : "", L.BodyLeast < 0.0 ? ")" : "");
	}

	void PrintSides(const char* Label, const Sided& S)
	{
		std::printf("     %s: %d gored, %d of them read by a side (1.5 m or more from the burst, or %d by the gun's side where it burst under him and his stance's "
			"gore falls hold none from under he may take); %d the burst under him with a fall from under he may take (%d in one, %d thrown from his gun's side); %d "
			"neither (1 to 1.5 m from the burst, or under him with none from under and no gun known); by the side (gored, thrown, no fall from there):", Label, S.Gored,
			S.Read, S.ByGun, S.Under, S.UnderFrom, S.UnderThrown, S.Unread);
		for (const char* Side : { "front", "behind", "left", "right", "under" })
		{
			const auto Found = S.By.find(Side);
			if (Found != S.By.end()) std::printf(" %s %d/%d/%d", Side, Found->second[0], Found->second[1], Found->second[2]);
		}
		std::printf("\n     %s: a field gun's or a howitzer's gored men %d, thrown %d; lying %d, %d of them in a fall that is not a throw\n", Label, S.GunGored, S.GunThrown,
			S.Lying, S.LyingSide);
		if (S.UnderOffered)
			std::printf("     %s: the burst under him, by a field gun's or a howitzer's shell offered both a fall from under and a throw from his gun's side: %d, %d of them "
				"thrown (the throw weighs 8 to the fall from under's 3, before the Fates and the section rules)\n", Label, S.UnderOffered, S.UnderOfferedThrown);
		for (const char* St : GoreStances)
		{
			const auto Found = S.UnderGun.find(St);
			if (Found == S.UnderGun.end()) continue;
			const Lot& L = Found->second;
			std::printf("     %s: %s, the burst under him, in a fall from under, read against his gun as played (held only from the falls' first frames and last, "
				"none toward where one he may take is not): %d men, %d limbs off, %d toward it, the most %.2f m (%s); %d bodies, %d toward it, the most %.2f m (%s)\n", Label, St, L.Men, L.Limbs, L.Toward,
				L.Least < 0.0 ? -L.Least : 0.0, L.Least < 0.0 ? L.LeastAt.c_str() : "none", L.Bodies, L.BodyToward, L.BodyLeast < 0.0 ? -L.BodyLeast : 0.0,
				L.BodyLeast < 0.0 ? L.BodyAt.c_str() : "none");
		}
		for (const char* St : GoreStances)
		{
			const auto Found = S.Lots.find(St);
			if (Found != S.Lots.end()) PrintLot(Label, St, Found->second, BodySlack);
		}
		if (S.FallLeast < 1e8)
			std::printf("     %s: standing or running, their falls, from the first frame, move a limb %.2f m away at least (his pose as struck is not the fall's first)\n",
				Label, S.FallLeast);
		std::printf("     %s: of those read with a fall from their side, %d had none from there that moves every limb half a metre away (the test's reading), %d of them "
			"in a fall from another side\n", Label, S.Wide, S.WideOff);
		for (const auto& KV : S.None)
			std::printf("     note: %s: %d gored men %s of the burst: their stance's gore falls hold none from there he may take; those he may take, the limbs away\n",
				Label, KV.second, KV.first.c_str());
		if (S.NoneLimbs)
			std::printf("     %s: of those men's %d limbs off, the least moved away from the burst %.2f m\n", Label, S.NoneLimbs, S.NoneLeast);
	}
}

int main(int Argc, char** Argv)
{
	if (Argc < 4)
	{
		std::fprintf(stderr, "usage: land_gore_test <soldiers folder> <twins folder> <armies.json> [out folder]\n");
		return 2;
	}
	std::string Err;
	std::map<std::string, std::shared_ptr<Soldier>> Files;
	std::map<std::string, Soldier> Twins;
	for (const char* Army : { "GER", "POL", "USA" })
	{
		auto S = std::make_shared<Soldier>();
		if (!S->Load(ReadFile(std::string(Argv[1]) + "/soldier_" + Army + ".json"), Err)) { std::printf("soldier %s: %s\n", Army, Err.c_str()); return 1; }
		Files[Army] = S;
		const std::string Text = ReadFile(std::string(Argv[2]) + "/soldier_" + Army + "_gore.json");
		if (Text.empty()) continue;
		Soldier Tw;
		if (!Tw.Load(Text, Err)) { std::printf("twin %s: %s\n", Army, Err.c_str()); return 1; }
		Twins[Army] = std::move(Tw);
	}
	if (Twins.find("USA") == Twins.end() || Twins.find("GER") == Twins.end()) { std::printf("FAIL: no USA or GER twin in %s\n", Argv[2]); return 1; }
	// (every twin in the twins folder, for the band's battle and the sweep: each with its own file from the soldiers
	// folder, where there is one, for the battle)
	std::map<std::string, const Soldier*> TwinOf;
	std::map<std::string, Soldier> MoreTwins;
	std::map<std::string, std::shared_ptr<Soldier>> BandFiles;
	{
		std::vector<std::string> Names;
		std::error_code Ec;
		for (const auto& E : std::filesystem::directory_iterator(Argv[2], Ec))
		{
			const std::string N = E.path().filename().string();
			if (N.size() > 18 && N.rfind("soldier_", 0) == 0 && N.compare(N.size() - 10, 10, "_gore.json") == 0) Names.push_back(N.substr(8, N.size() - 18));
		}
		std::sort(Names.begin(), Names.end());
		for (const std::string& Nm : Names)
		{
			if (Twins.find(Nm) != Twins.end()) TwinOf[Nm] = &Twins[Nm];
			else
			{
				Soldier Tw;
				if (!Tw.Load(ReadFile(std::string(Argv[2]) + "/soldier_" + Nm + "_gore.json"), Err)) { std::printf("twin %s: %s\n", Nm.c_str(), Err.c_str()); return 1; }
				if (Tw.DeathsGore.empty()) continue;
				MoreTwins[Nm] = std::move(Tw);
				TwinOf[Nm] = &MoreTwins[Nm];
			}
			if (Files.find(Nm) != Files.end()) BandFiles[Nm] = Files[Nm];
			else
			{
				const std::string Text = ReadFile(std::string(Argv[1]) + "/soldier_" + Nm + ".json");
				auto S = std::make_shared<Soldier>();
				if (Text.empty() || !S->Load(Text, Err)) { std::printf("     note: no soldier_%s.json read beside its twin: swept, not in the band's battle\n", Nm.c_str()); continue; }
				BandFiles[Nm] = S;
			}
		}
	}
	const std::string Armies = ReadFile(Argv[3]);
	std::vector<Who> Men;
	const std::string Rec = Recording(Men);
	const GroundFn Flat = [](double, double) { return Ground(); };
	int Failed = 0;
	auto Check = [&Failed](bool bOk, const std::string& What) { std::printf("%s: %s\n", bOk ? "ok" : "FAIL", What.c_str()); Failed += bOk ? 0 : 1; };
	auto Make = [&](Crowd& C, bool bTwins, bool bOn) {
		for (const auto& KV : Files) C.AddSoldier(KV.first, KV.second);
		if (bTwins)
		{
			for (const auto& KV : Twins)
			{
				std::string Why;
				if (!C.AddGore(KV.first, KV.second, Why)) std::printf("twin %s not used: %s\n", KV.first.c_str(), Why.c_str());
			}
		}
		C.SetGore(bOn);
		if (!C.ReadArmies(Armies, Err) || !C.Build(Rec, Flat, Err)) { std::printf("build: %s\n", Err.c_str()); std::exit(1); }
		C.SetEye(-1000, -1000, 2);
	};
	const double Until = Hit + 5.0;
	// (what the battles' twins hold, so their checks ask only what the falls allow: a prone fall that is not a throw (the
	// deaths thread, 24 Nov: a man lying gored by any shell, by its side), a fall from under him on his feet that is not
	// a throw (a lower leg off, the burst within a metre), and any fall from under at all, in any stance (lying, the prone
	// throw))
	bool bLyingFalls = false, bUnderFalls = false, bAnyUnder = false;
	for (const auto& KV : Twins)
	{
		for (const FallRead& Fr : ReadFalls(KV.second, "prone")) bLyingFalls = bLyingFalls || !Fr.bThrow;
		for (const FallRead& Fr : ReadFalls(KV.second, "stand")) bUnderFalls = bUnderFalls || (!Fr.bThrow && Fr.From == "under");
		for (const char* St : { "stand", "run", "kneel", "cover", "prone" })
		{
			for (const FallRead& Fr : ReadFalls(KV.second, St)) bAnyUnder = bAnyUnder || Fr.From == "under";
		}
	}

	// ---- Off: the twins added, the switch off, against no twins at all.
	{
		Crowd A, B;
		Make(A, false, false);
		Make(B, true, false);
		const Result Ra = Play(A, Men, 0.0, Until), Rb = Play(B, Men, 0.0, Until);
		Check(Ra.Clip == Rb.Clip && Ra.Skin == Rb.Skin && B.Gored() == 0 && Rb.LimbOff == 0, "off: every fall and every skin as with no twins, none gored");
		std::printf("     (%.0f ms against %.0f ms for %d frames)\n", Rb.Ms, Ra.Ms, static_cast<int>(Until * 60.0));
		std::printf("     off: %d men, %s\n", static_cast<int>(Men.size()), Digest(Rb, std::vector<char>(Men.size(), 1)).c_str());
	}

	// ---- On.
	Crowd C;
	Make(C, true, true);
	const Result Rc = Play(C, Men, 0.0, Until);
	int Gored = 0, RifleGored = 0, FarGored = 0, BadClip = 0, NoLimb = 0, LimbOnWhole = 0, LostClips = 0, JointsRead = 0, JointsExact = 0;
	double MostOpen = 0.0;
	int ByD[14] = {}, ShellD[14] = {}, WrongSide = 0;
	std::vector<int> GoredMen;
	for (size_t I = 0; I < Men.size(); ++I)
	{
		const int Man = C.Find(Men[I].B, Men[I].K);
		const Soldier* Own = Files[C.Info(Man).FileArmy].get();
		const bool bGored = C.SoldierOf(Man) != Own;
		if (Men[I].W != "rifle") { ++ShellD[static_cast<int>(Men[I].Burst)]; if (bGored) ++ByD[static_cast<int>(Men[I].Burst)]; }
		if (!bGored)
		{
			if (Rc.Gap[I] > 0.05) ++LimbOnWhole;
			continue;
		}
		++Gored;
		GoredMen.push_back(static_cast<int>(I));
		const Soldier& S = *C.SoldierOf(Man);
		if (Men[I].W == "rifle") ++RifleGored;
		const double Reach = Men[I].W == "gun" ? 8.0 : 6.0;
		if (Men[I].Burst > Reach) ++FarGored;
		// (thrown away from the burst: his fall's side its, where it fell 1.5 m or more off; the test's men stand still)
		if (Men[I].Burst >= 1.5)
		{
			const double A = 0.7 * static_cast<double>(I), Face = Men[I].B == 1 ? 0.0 : 3.14159265358979;
			double Off = std::atan2(std::cos(A), std::sin(A)) - Face;
			while (Off > 3.14159265358979) Off -= 6.28318530717959;
			while (Off < -3.14159265358979) Off += 6.28318530717959;
			Off = std::fabs(Off);
			if (C.Info(Man).FallFrom != (Off < Eighth ? "front" : Off > 3.0 * Eighth ? "back" : "side")) ++WrongSide;
		}
		bool bIn = false;
		for (const auto& St : S.DeathsGore) for (const auto& Kd : St.second) for (const std::string& N : Kd.second) bIn = bIn || N == Rc.Clip[I];
		if (!bIn) ++BadClip;
		// (off its joint at all; the art director asks 1 m or more, an arm 1 to 4 m, a lower leg 1 to 3 m: a note, the
		// clips' own to mend)
		if (Rc.Gap[I] < 0.1) ++NoLimb;
		if (Rc.Gap[I] < 1.0) std::printf("     note: %s %s's limb rests %.2f m from its joint (the art director asks 1 m or more)\n", C.Info(Man).FileArmy.c_str(), Rc.Clip[I].c_str(), Rc.Gap[I]);
		if (Rc.High[I] > 3.0) std::printf("     note: %s %s's limb flies %.1f m high (the art director asks 3 m at most)\n", C.Info(Man).FileArmy.c_str(), Rc.Clip[I].c_str(), Rc.High[I]);
		bool bExact = false;
		const double Open = JointOpen(C, Man, *Own, bExact);
		if (Open >= 0.0) { ++JointsRead; JointsExact += bExact ? 1 : 0; MostOpen = std::max(MostOpen, Open); }
		for (const Clip& Cl : Own->Clips) LostClips += S.ClipIndex(Cl.Name) == Own->ClipIndex(Cl.Name) ? 0 : 1;
	}
	std::printf("     gored %d of %d men (%d twins read); by the burst's distance (shell kills):", Gored, static_cast<int>(Men.size()), static_cast<int>(Twins.size()));
	for (int D = 0; D < 14; ++D) std::printf(" %dm %d/%d", D, ByD[D], ShellD[D]);
	std::printf("\n");
	Check(Gored > 0 && Gored == C.Gored(), "on: men gored, and the crowd's count says so");
	Check(RifleGored == 0, "on: no man killed by a rifle gored");
	Check(FarGored == 0, "on: no man past the burst's reach gored");
	Check(ByD[1] + ByD[2] > ByD[4] + ByD[5] && ByD[4] + ByD[5] >= ByD[6] + ByD[7], "on: more gored near the burst than farther off");
	Check(ByD[0] * (ShellD[1] + ShellD[2]) < (ByD[1] + ByD[2]) * ShellD[0], "on: a man the burst was laid on gored less often than one within 2 m");
	Check(WrongSide == 0, "on: each gored man 1.5 m or more from the burst falls by its side (" + std::to_string(WrongSide) + " not)");
	Check(BadClip == 0, "on: each gored man in one of his twin's gore falls");
	Check(LostClips == 0, "on: each gored man keeps every clip number of his own file");
	Check(NoLimb == 0, "on: each gored man has a limb off its joint (more than 0.1 m) at rest");
	Check(LimbOnWhole == 0, "on: no other man's limb leaves its joint");
	Check(Rc.LimbOff >= Gored, "on: the limb_off events come (" + std::to_string(Rc.LimbOff) + ")");
	char OpenWords[160];
	std::snprintf(OpenWords, sizeof(OpenWords), "on: no whole joint of a gored man opens on its caps (widest %.1f mm, %d men read, %d each copy against the point it copies)", MostOpen * 1000.0, JointsRead, JointsExact);
	Check(JointsRead > 0 && MostOpen < 0.002, OpenWords);
	Check(Rc.NaN == 0, "on: no NaN in any skin");
	std::printf("     (%.0f ms for %d frames)\n", Rc.Ms, static_cast<int>(Until * 60.0));
	{
		std::vector<char> Whole(Men.size(), 0);
		int Kept = 0;
		for (size_t I = 0; I < Men.size(); ++I)
		{
			const int Man = C.Find(Men[I].B, Men[I].K);
			Whole[I] = C.SoldierOf(Man) == Files[C.Info(Man).FileArmy].get() ? 1 : 0;
			Kept += Whole[I];
		}
		std::printf("     not gored: %d men, %s\n", Kept, Digest(Rc, Whole).c_str());
	}

	// ---- Sides: the burst's side against each gored man, his limbs away from it, the big throw a gun's alone.
	auto SideChecks = [&](const char* Label, const Sided& Sd, bool bUnderMust) {
		PrintSides(Label, Sd);
		const std::string L = Label;
		Check(Sd.Read + Sd.Under > 0 && Sd.OffSide == 0 && Sd.NoneUp == 0, L + ": each gored man read by a side (1.5 m or more from the burst, or his gun's side "
			+ "where it burst under him and his stance's gore falls hold none from under he may take) falls in a fall from its side (gore_from) where his stance's "
			+ "gore falls hold one he may take that moves every limb half a metre away from it (the test's reading, from the falls' first frames and last), and every "
			+ "man standing or running has one from his side (" + std::to_string(Sd.Read) + " read, " + std::to_string(Sd.OffSide + Sd.NoneUp) + " not)");
		Check(Sd.Worse == 0, L + ": no gored man read falls so that his limbs move less far away than another fall he may take would move them, up to half a metre, "
			+ "but where his stance's gore falls hold one from his side none from the side opposite (the test's reading, from the falls' first frames and last; "
			+ std::to_string(Sd.Worse) + " so)");
		Check(Sd.Opposite == 0, L + ": no gored man read falls in a fall from the side opposite the burst's (front and behind, left and right) where his stance's "
			+ "gore falls hold one from its side he may take (" + std::to_string(Sd.Opposite) + " so)");
		Check(Sd.UnderWrong == 0, L + ": each gored man the burst was under (within 1 m, whatever his stance) whose stance's gore falls hold one from under he may "
			+ "take falls in one, or by a field gun's or a howitzer's shell in the throw his gun's side gives him (every limb at least as far from the gun as that "
			+ "side's falls set; " + std::to_string(Sd.Under) + " so struck, " + std::to_string(Sd.UnderFrom) + " from under, " + std::to_string(Sd.UnderThrown)
			+ " thrown, " + std::to_string(Sd.UnderWrong) + " not)");
		// (asked of the guns' battle wherever the twins hold any fall from under (bUnderMust: its men the shell burst under
		// all have their gun known; with 7caf9453's twins its two men lying are thrown from under), and there of at least
		// one man so read, so that a battle left with none fails rather than passing on none; of the others only with a
		// man in a fall from under, his gun known: the first battle's and the sides' rounds have no gun known (bBy), and
		// with 7caf9453's twins the band's men (none lying) none from under)
		if (bUnderMust || Sd.UnderLegs > 0)
			Check(Sd.UnderLegs > 0 && Sd.UnderToward == 0, L + ": no gored man the burst was under, his gun known, falls from under in a fall that moves a limb toward his gun where "
				+ "another fall from under he may take does not, nor where all do in one that moves it more than the least of them so (the test's reading, from the "
				+ "falls' first frames and last; " + std::to_string(Sd.UnderLegs) + " in a fall from under with his gun known, " + std::to_string(Sd.UnderToward) + " so)");
		else
			std::printf("     %s: none in a fall from under with his gun known: his falls from under not read against it\n", Label);
		Lot Up;
		for (const char* St : { "stand", "run" })
		{
			const auto Found = Sd.Lots.find(St);
			if (Found == Sd.Lots.end()) continue;
			Up.Limbs += Found->second.Limbs, Up.Toward += Found->second.Toward, Up.Short += Found->second.Short;
			Up.Bodies += Found->second.Bodies, Up.BodyWorse += Found->second.BodyWorse;
			Up.Least = std::min(Up.Least, Found->second.Least), Up.BodyLeast = std::min(Up.BodyLeast, Found->second.BodyLeast);
		}
		char Least[64];
		std::snprintf(Least, sizeof(Least), "%.2f", Up.Limbs ? Up.Least : 0.0);
		Check(Up.Limbs > 0 && Up.Toward == 0, L + ": no limb off those men standing or running moves toward the burst (or the gun), along the line from it to its joint ("
			+ std::to_string(Up.Limbs) + " limbs, " + std::to_string(Up.Toward) + " toward; the least moved " + Least + " m away, " + std::to_string(Up.Short)
			+ " under half a metre)");
		Check(Sd.EveryToward == 0, L + ": no limb off a man read moves toward the burst (or the gun) where his stance's gore falls hold one from each side that is not "
			+ "a throw (standing or running, and since the deaths thread's falls of 24 Nov kneeling, in cover and lying: " + std::to_string(Sd.Every) + " limbs, "
			+ std::to_string(Sd.EveryToward) + " toward)");
		{
			char Most[64];
			std::snprintf(Most, sizeof(Most), "%.2f", Up.Bodies && Up.BodyLeast < 0.0 ? -Up.BodyLeast : 0.0);
			char Tol[16];
			std::snprintf(Tol, sizeof(Tol), "%.1f", BodySlack);
			Check(Up.BodyWorse == 0, L + ": no body (pelvis) of those men standing or running moves toward the burst (or the gun) more than " + Tol + " m where "
				+ "another fall he may take moves it away (the test's reading of the falls; " + std::to_string(Up.Bodies) + " bodies, " + std::to_string(Up.BodyWorse)
				+ " so; the most any moved toward " + Most + " m)");
		}
		Check(Sd.ThrownToward == 0, L + ": no man of those thrown has a limb or his body move toward the burst (or the gun) (" + std::to_string(Sd.ThrownRead) + " thrown, "
			+ std::to_string(Sd.ThrownToward) + " so)");
		Check(Sd.OtherThrown == 0, L + ": only a field gun's or a howitzer's shell throws a gored man (die_blast_*; " + std::to_string(Sd.OtherThrown) + " by another)");
		Check(Sd.LyingWrong == 0, L + ": a man lying thrown only from under him, by such a shell bursting on him (die_blast_d; " + std::to_string(Sd.Lying) + " lying gored, "
			+ std::to_string(Sd.LyingSide) + " of them in a fall that is not a throw; " + std::to_string(Sd.LyingWrong) + " not)");
	};
	SideChecks("on", ReadSides(C, Men, Rc, Files), false);

	// ---- Same: another crowd, built alike.
	{
		Crowd D;
		Make(D, true, true);
		const Result Rd = Play(D, Men, 0.0, Until);
		Check(Rd.Clip == Rc.Clip && Rd.Skin == Rc.Skin, "same: a second crowd gores the same men with the same falls");
	}

	// ---- Back: a replay from the start, then on again.
	{
		std::vector<uint32_t> No;
		for (const int I : GoredMen) No.push_back(C.MeshNo(C.Find(Men[static_cast<size_t>(I)].B, Men[static_cast<size_t>(I)].K)));
		C.Update(0.5);
		bool bOwn = C.Gored() == 0;
		for (size_t K = 0; K < GoredMen.size(); ++K)
		{
			const Who& Wh = Men[static_cast<size_t>(GoredMen[K])];
			const int Man = C.Find(Wh.B, Wh.K);
			bOwn = bOwn && C.SoldierOf(Man) == Files[C.Info(Man).FileArmy].get() && C.MeshNo(Man) != No[K] && C.PointsOf(Man, Near) > 0;
		}
		Check(bOwn, "back: a replay from the start gives every gored man his own file and mesh again");
		const Result Re = Play(C, Men, 0.5, Until);
		Check(Re.Clip == Rc.Clip && Re.Skin == Rc.Skin, "back: played on, the same men are gored with the same falls");
	}

	// ---- The sides' battle: more men, all round their bursts, every stance.
	{
		std::vector<Who> Many;
		const std::string RecMany = SidesRecording(Many);
		auto MakeMany = [&](Crowd& M, bool bTwins, bool bOn) {
			for (const auto& KV : Files) M.AddSoldier(KV.first, KV.second);
			if (bTwins)
			{
				for (const auto& KV : Twins)
				{
					std::string Why;
					if (!M.AddGore(KV.first, KV.second, Why)) std::printf("twin %s not used: %s\n", KV.first.c_str(), Why.c_str());
				}
			}
			M.SetGore(bOn);
			if (!M.ReadArmies(Armies, Err) || !M.Build(RecMany, Flat, Err)) { std::printf("build: %s\n", Err.c_str()); std::exit(1); }
			M.SetEye(-1000, -1000, 2);
		};
		Crowd Ma, Mb, Mc;
		MakeMany(Ma, false, false);
		MakeMany(Mb, true, false);
		MakeMany(Mc, true, true);
		const Result Pa = Play(Ma, Many, 0.0, Until), Pb = Play(Mb, Many, 0.0, Until), Pc = Play(Mc, Many, 0.0, Until);
		Check(Pa.Clip == Pb.Clip && Pa.Skin == Pb.Skin && Mb.Gored() == 0, "sides' battle off: every fall and every skin as with no twins, none gored");
		std::printf("     sides' battle off: %d men, %s\n", static_cast<int>(Many.size()), Digest(Pb, std::vector<char>(Many.size(), 1)).c_str());
		std::vector<char> Whole(Many.size(), 0);
		int Kept = 0;
		for (size_t I = 0; I < Many.size(); ++I)
		{
			const int Man = Mc.Find(Many[I].B, Many[I].K);
			Whole[I] = Mc.SoldierOf(Man) == Files[Mc.Info(Man).FileArmy].get() ? 1 : 0;
			Kept += Whole[I];
		}
		std::printf("     sides' battle not gored: %d men, %s\n", Kept, Digest(Pc, Whole).c_str());
		Check(Pc.NaN == 0 && Mc.Gored() == static_cast<int>(Many.size()) - Kept, "sides' battle: the crowd's count of the gored, and no NaN in any skin");
		const Sided Ss = ReadSides(Mc, Many, Pc, Files);
		SideChecks("sides' battle", Ss, false);
		if (bLyingFalls)
			Check(Ss.LyingSide > 0, "sides' battle: men lying gored by a tank's or a mortar's shell, or one off to their side, in a prone fall that is not a throw (the "
				"deaths thread, 24 Nov; " + std::to_string(Ss.LyingSide) + " of " + std::to_string(Ss.Lying) + " lying gored)");
		else std::printf("     note: sides' battle: the twins' prone falls hold none that is not a throw: a man lying is gored only by a field gun's or a howitzer's shell "
			"bursting on him (%d lying gored)\n", Ss.Lying);
	}

	// ---- The guns' battle: the burst on each man, his gun known all round him; a light gun's among them.
	{
		std::vector<Who> Many;
		const std::string RecGuns = GunsRecording(Many);
		auto MakeGuns = [&](Crowd& M, bool bTwins, bool bOn) {
			for (const auto& KV : Files) M.AddSoldier(KV.first, KV.second);
			if (bTwins)
			{
				for (const auto& KV : Twins)
				{
					std::string Why;
					if (!M.AddGore(KV.first, KV.second, Why)) std::printf("twin %s not used: %s\n", KV.first.c_str(), Why.c_str());
				}
			}
			M.SetGore(bOn);
			if (!M.ReadArmies(Armies, Err) || !M.Build(RecGuns, Flat, Err)) { std::printf("build: %s\n", Err.c_str()); std::exit(1); }
			M.SetEye(-1000, -1000, 2);
		};
		Crowd Ga, Gb, Gc;
		MakeGuns(Ga, false, false);
		MakeGuns(Gb, true, false);
		MakeGuns(Gc, true, true);
		const Result Pa = Play(Ga, Many, 0.0, Until), Pb = Play(Gb, Many, 0.0, Until), Pc = Play(Gc, Many, 0.0, Until);
		Check(Pa.Clip == Pb.Clip && Pa.Skin == Pb.Skin && Gb.Gored() == 0, "guns' battle off: every fall and every skin as with no twins, none gored");
		std::printf("     guns' battle off: %d men, %s\n", static_cast<int>(Many.size()), Digest(Pb, std::vector<char>(Many.size(), 1)).c_str());
		std::vector<char> Whole(Many.size(), 0);
		int Kept = 0, Light = 0, LightGored = 0;
		for (size_t I = 0; I < Many.size(); ++I)
		{
			const int Man = Gc.Find(Many[I].B, Many[I].K);
			Whole[I] = Gc.SoldierOf(Man) == Files[Gc.Info(Man).FileArmy].get() ? 1 : 0;
			Kept += Whole[I];
			Light += Many[I].bLight ? 1 : 0;
			LightGored += Many[I].bLight && !Whole[I] ? 1 : 0;
		}
		std::printf("     guns' battle not gored: %d men, %s\n", Kept, Digest(Pc, Whole).c_str());
		Check(Pc.NaN == 0 && Gc.Gored() == static_cast<int>(Many.size()) - Kept, "guns' battle: the crowd's count of the gored, and no NaN in any skin");
		Check(LightGored > 0, "guns' battle: a light gun's shell gores men (" + std::to_string(LightGored) + " of " + std::to_string(Light) + ")");
		const Sided Sg = ReadSides(Gc, Many, Pc, Files);
		Check(Sg.Gored > 0 && Sg.ByGun == Sg.Read && Sg.Under + Sg.ByGun + Sg.Unread == Sg.Gored, "guns' battle: each gored man struck within 1 m of the burst the "
			"burst under him, his stance's gore falls holding one from under he may take, or read by his gun's side (" + std::to_string(Sg.Under) + " under, "
			+ std::to_string(Sg.ByGun) + " by his gun, of " + std::to_string(Sg.Gored) + " gored; " + std::to_string(Sg.Unread) + " struck 1 to 1.5 m from it, not read)");
		if (bUnderFalls)
			Check(Sg.UnderFrom > 0 && Sg.UnderThrown > 0, "guns' battle: men the shell burst under fall from under (" + std::to_string(Sg.UnderFrom) + "), and a field "
				"gun's or a howitzer's shell throws some from their gun's side (" + std::to_string(Sg.UnderThrown) + ")");
		else std::printf("     note: guns' battle: the twins' standing falls hold none from under that is not a throw: a man the shell burst under is read by his gun's "
			"side (%d)\n", Sg.ByGun);
		SideChecks("guns' battle", Sg, bAnyUnder);
	}

	// ---- The band's battle: every twin's men, the burst (or the gun) 31 to 49 degrees round ahead on his left or his
	// right, where the falls from the front give way to the side's (the verifier of 35ffc951).
	if (!BandFiles.empty())
	{
		std::vector<Who> Many;
		std::vector<std::string> Names;
		for (const auto& KV : BandFiles) Names.push_back(KV.first);
		const std::string RecBand = BandRecording(Names, Many);
		Crowd Bc;
		for (const auto& KV : BandFiles) Bc.AddSoldier(KV.first, KV.second);
		for (const auto& KV : BandFiles)
		{
			std::string Why;
			if (!Bc.AddGore(KV.first, *TwinOf[KV.first], Why)) std::printf("twin %s not used: %s\n", KV.first.c_str(), Why.c_str());
		}
		Bc.SetGore(true);
		if (!Bc.ReadArmies(Armies, Err) || !Bc.Build(RecBand, Flat, Err)) { std::printf("build: %s\n", Err.c_str()); return 1; }
		Bc.SetEye(-1000, -1000, 2);
		const Result Pb = Play(Bc, Many, 0.0, Until);
		std::vector<char> Whole(Many.size(), 0), Gone(Many.size(), 0);
		int Kept = 0;
		for (size_t I = 0; I < Many.size(); ++I)
		{
			const int Man = Bc.Find(Many[I].B, Many[I].K);
			Whole[I] = Bc.SoldierOf(Man) == BandFiles[Bc.Info(Man).FileArmy].get() ? 1 : 0;
			Gone[I] = Whole[I] ? 0 : 1;
			Kept += Whole[I];
		}
		std::printf("     band's battle: %d men of %d twins; not gored: %d men, %s; gored: %d men, %s\n", static_cast<int>(Many.size()), static_cast<int>(Names.size()), Kept,
			Digest(Pb, Whole).c_str(), static_cast<int>(Many.size()) - Kept, Digest(Pb, Gone).c_str());
		Check(Pb.NaN == 0 && Bc.Gored() == static_cast<int>(Many.size()) - Kept && Kept < static_cast<int>(Many.size()), "band's battle: men gored, the crowd's count of them, "
			"and no NaN in any skin");
		SideChecks("band's battle", ReadSides(Bc, Many, Pb, BandFiles, false), false);
	}

	// ---- The sweep: every twin's gore falls as the crowd gives them (GoreFalls, ChooseFall's own rule), for a burst, or
	// his gun where it burst under him, all round him, each read by the test from the falls' first frames and last.
	{
		int Given = 0, Cells = 0, NotMay = 0, NoneMay = 0, SideWrong = 0, OffSide = 0, Worse = 0, Wide = 0, WideOff = 0, BandWide = 0, Opposite = 0;
		int Every = 0, EveryToward = 0;
		Lot Band;
		std::map<std::string, Lot> ByStance;
		std::map<std::string, std::pair<Lot, Lot>> ByTwin;
		// (the burst under him: cells; of them with a fall from under he may take, and of those given anything but the falls
		// from under the rule gives (his gun known, those that move no limb toward it, or where all do the least so; else
		// every one) and such a shell's throw from his gun's side where that side's falls give it, and those thrown so (the
		// throws' limbs and bodies against the gun); with none from under he may take, his gun known, and of them given
		// other than his gun's side gives; with none and his gun not known, and of them given other than all he may take)
		int UnderCells = 0, UnderThere = 0, UnderWrong = 0, UnderThrew = 0, UnderBack = 0, UnderBackWrong = 0, UnderAll = 0, UnderAllWrong = 0;
		Lot UnderThrow;
		std::map<std::string, Lot> UnderFall; // by stance: the cells with a fall from under given, his gun known, read against it
		// (the burst under him, a man standing, running, kneeling or in cover, his stance's falls from under he may take
		// read against his gun: the cells where one moves no limb toward it, and of them those given one that does; the
		// cells where every one does (gaps, the falls' own: printed, not failures), and of them those given other than the
		// least so; his gun not known, the cells (with two or more from under) and of them those given other than every
		// one; any stance with one from under he may take (lying, the prone throw), the cells and of them those not given it)
		int LegAway = 0, LegToward = 0, LegGaps = 0, LegGapWrong = 0, LegBlind = 0, LegBlindTwo = 0, LegBlindWrong = 0, OneUnder = 0, OneUnderWrong = 0;
		// (whether any twin's falls for a man on his feet or a knee hold one from under, not a throw (a lower leg off: the
		// deaths thread's, 24 Nov): the checks of them ask for cells only so, 7caf9453's twins holding none)
		bool bLegFalls = false;
		for (const auto& KV : TwinOf)
			for (const char* St : GoreStances)
				if (std::strcmp(St, "prone") != 0)
					for (const FallRead& Fr : ReadFalls(*KV.second, St)) bLegFalls = bLegFalls || (!Fr.bThrow && Fr.From == "under");
		struct Gap
		{
			int Cells = 0;
			std::set<std::string> Twins;
			std::map<std::string, int> Given;
			double Least = 1e9, Most = -1e9; // the given fall's limb toward the gun (m), the least and the most of the cells
			std::string MostAt;
			std::set<int> Degs;              // where his gun was, degrees round from his nose (his right above 0)
		};
		// (a set of the sweep's degrees, 3 apart, as runs: "-177..-138 138..180")
		auto Runs = [](const std::set<int>& Degs) {
			std::string Out;
			int First = 0, Last = 0;
			bool bOpen = false;
			auto Close = [&]() { Out += (Out.empty() ? "" : " ") + std::to_string(First) + (Last != First ? ".." + std::to_string(Last) : std::string()); };
			for (const int D : Degs)
			{
				if (bOpen && D == Last + 3) { Last = D; continue; }
				if (bOpen) Close();
				First = Last = D;
				bOpen = true;
			}
			if (bOpen) Close();
			return Out;
		};
		std::map<std::string, Gap> Gaps; // by "stance side"
		std::map<std::string, int> LegCells;                        // by "stance side" (side none: his gun not known): cells
		std::map<std::string, std::map<std::string, int>> LegGiven; // of them, given each fall from under
		// (the burst 1 to 3 m off, by where it fell; his gun 25 or 400 m off, the burst under him and his stance's gore falls
		// holding none from under he may take, by his gun's side)
		const double Places[] = { 1.0, 1.5, 2.0, 2.5, 3.0, 25.0, 400.0 };
		auto Quarter = [](int Deg) { return std::string(std::abs(Deg) < 45 ? "front" : std::abs(Deg) > 135 ? "behind" : Deg > 0 ? "right" : "left"); };
		auto Has = [](const std::vector<std::pair<std::string, std::string>>& List, const FallRead& Fr) {
			return std::find(List.begin(), List.end(), std::make_pair(Fr.Kind, Fr.Clip)) != List.end();
		};
		for (const auto& KV : TwinOf)
		{
			const Soldier& Tw = *KV.second;
			for (const char* St : GoreStances)
			{
				const std::vector<FallRead> Falls = ReadFalls(Tw, St);
				if (Falls.empty()) { std::printf("     note: sweep: %s has no %s gore falls\n", KV.first.c_str(), St); continue; }
				const bool bUp = std::strcmp(St, "stand") == 0 || std::strcmp(St, "run") == 0, bEvery = EverySide(Falls);
				// (every fall given one of his stance's: the test's reading of it)
				auto Known = [&](const std::vector<std::pair<std::string, std::string>>& List) {
					for (const auto& Fall : List)
					{
						bool bIn = false;
						for (const FallRead& Fc : Falls) bIn = bIn || (Fc.Kind == Fall.first && Fc.Clip == Fall.second);
						if (!bIn) return false;
					}
					return true;
				};
				for (const bool bBig : { true, false })
				{
					for (const double Far : Places)
					{
						for (int Deg = -177; Deg <= 180; Deg += 3)
						{
							GoreStrike Strike;
							Strike.Stance = St;
							Strike.Side = Quarter(Deg);
							Strike.bAway = true;
							Strike.X = Far * std::cos(Deg * 3.14159265358979 / 180.0);
							Strike.Y = Far * std::sin(Deg * 3.14159265358979 / 180.0);
							Strike.bBig = bBig;
							// (none he may take from there (lying, where the prone falls are the throw from under alone): no gore
							// fall for him, GoreSide none)
							bool bMay = false;
							for (const FallRead& Fr : Falls) bMay = bMay || MayTakeRead(Fr, bBig, Strike.Side);
							SideWrong += GoreSide(Tw, Strike) == (bMay ? Strike.Side : std::string()) ? 0 : 1;
							if (!bMay) { ++NoneMay; continue; }
							const std::vector<std::pair<std::string, std::string>> Falls2 = GoreFalls(Tw, Strike);
							double SideBest = -1e9, AllBest = -1e9, BodyBest = -1e9;
							BestAway(Falls, bBig, Strike.Side, Strike.X, Strike.Y, SideBest, AllBest, &BodyBest);
							const bool bInBand = std::abs(Deg) >= 30 && std::abs(Deg) <= 50 && bUp;
							++Cells;
							if (SideBest > -1e9 && SideBest < 0.5) ++Wide;
							bool bOther = false, bBodyToward = false, bBodyWorse = false;
							double Least = 1e9, BodyLeast = 1e9;
							std::string LeastClip, BodyClip;
							if (Falls2.empty() || !Known(Falls2)) ++NotMay;
							for (const auto& Fall : Falls2)
							{
								++Given;
								const FallRead* Fr = nullptr;
								for (const FallRead& Fc : Falls) if (Fc.Kind == Fall.first && Fc.Clip == Fall.second) Fr = &Fc;
								if (!Fr || !MayTakeRead(*Fr, bBig, Strike.Side)) { ++NotMay; continue; }
								const double G = LeastAway(*Fr, Strike.X, Strike.Y);
								if (SideBest >= 0.5 && Fr->From != Strike.Side) ++OffSide;
								if (SideBest > -1e9 && OppositeRead(Fr->From, Strike.Side)) ++Opposite;
								if (G < std::min(0.5, AllBest) - 1e-9) ++Worse;
								const double Bd = BodyAway(*Fr, Strike.X, Strike.Y);
								if (Bd < BodyLeast) { BodyLeast = Bd; BodyClip = Fr->Clip; }
								bBodyToward = bBodyToward || Bd < 0.0;
								bBodyWorse = bBodyWorse || (Bd < -Slack && BodyBest >= 0.0);
								bOther = bOther || Fr->From != Strike.Side;
								if (G < Least) { Least = G; LeastClip = Fr->Clip; }
							}
							if (SideBest > -1e9 && SideBest < 0.5 && bOther) ++WideOff;
							if (bInBand && bOther) ++BandWide;
							if (bEvery) { ++Every; EveryToward += Least < 0.0 ? 1 : 0; }
							char Where[160], BodyWhere[160];
							std::snprintf(Where, sizeof(Where), "%s %s, %s, %s %.1f m %+d deg, %s", KV.first.c_str(), St, bBig ? "a field gun's or a howitzer's" : "another's",
								Far > 3.5 ? "his gun" : "the burst", Far, Deg, LeastClip.c_str());
							std::snprintf(BodyWhere, sizeof(BodyWhere), "%s %s, %s, %s %.1f m %+d deg, %s", KV.first.c_str(), St, bBig ? "a field gun's or a howitzer's" : "another's",
								Far > 3.5 ? "his gun" : "the burst", Far, Deg, BodyClip.c_str());
							std::vector<Lot*> Into = { &ByStance[St], bUp ? &ByTwin[KV.first].first : &ByTwin[KV.first].second };
							if (bInBand) Into.push_back(&Band);
							for (Lot* Ty : Into)
							{
								++Ty->Men;
								Ty->Toward += Least < 0.0 ? 1 : 0;
								Ty->Short += Least < 0.5 ? 1 : 0;
								if (Least < Ty->Least) { Ty->Least = Least; Ty->LeastAt = Where; }
								Ty->BodyToward += bBodyToward ? 1 : 0;
								Ty->BodyWorse += bBodyWorse ? 1 : 0;
								if (BodyLeast < Ty->BodyLeast) { Ty->BodyLeast = BodyLeast; Ty->BodyAt = BodyWhere; }
							}
						}
					}
					// (the burst under him, his gun 25 or 400 m off all round him, or not known: every fall from under he may
					// take, none measured, and from a field gun's or a howitzer's shell the throw from his gun's side where its
					// limbs move as far from the gun as that side's falls set (FloorRead); where none from under he may take,
					// as his gun's side gives him (the cells above), or where it is not known all he may take)
					for (const double Far : { 25.0, 400.0, -1.0 })
					{
						for (int Deg = -177; Deg <= 180; Deg += Far > 0.0 ? 3 : 360)
						{
							GoreStrike Strike;
							Strike.Stance = St;
							Strike.Side = "under";
							Strike.bBig = bBig;
							if (Far > 0.0)
							{
								Strike.Gun = Quarter(Deg);
								Strike.bAway = true;
								Strike.X = Far * std::cos(Deg * 3.14159265358979 / 180.0);
								Strike.Y = Far * std::sin(Deg * 3.14159265358979 / 180.0);
							}
							const std::string By = ReadSide(Falls, bBig, "under", Strike.Gun);
							bool bMay = false;
							for (const FallRead& Fr : Falls) bMay = bMay || MayTakeRead(Fr, bBig, By);
							SideWrong += GoreSide(Tw, Strike) == (bMay ? By : std::string()) ? 0 : 1;
							if (!bMay) { ++NoneMay; continue; }
							++UnderCells;
							const std::vector<std::pair<std::string, std::string>> Falls2 = GoreFalls(Tw, Strike);
							if (Falls2.empty() || !Known(Falls2)) ++NotMay;
							if (By != "under")
							{
								++UnderBack;
								GoreStrike There = Strike;
								There.Side = By;
								UnderBackWrong += GoreFalls(Tw, There) == Falls2 ? 0 : 1;
								continue;
							}
							if (!UnderRead(Falls, bBig))
							{
								++UnderAll;
								bool bWrong = false;
								for (const FallRead& Fr : Falls) bWrong = bWrong || Has(Falls2, Fr) != MayTakeRead(Fr, bBig, "under");
								UnderAllWrong += bWrong ? 1 : 0;
								continue;
							}
							++UnderThere;
							const double Floor = Strike.bAway ? FloorRead(Falls, bBig, Strike.Gun, Strike.X, Strike.Y) : 1e9;
							// (his falls from under he may take, and with his gun known the most any of them moves its every limb
							// away from it: the floor of those given, 0 where one moves none toward it, else that most)
							int Unders = 0;
							double UnderBest = -1e9;
							for (const FallRead& Fr : Falls)
							{
								if (Fr.From != "under" || !MayTakeRead(Fr, bBig, "under")) continue;
								++Unders;
								if (Strike.bAway) UnderBest = std::max(UnderBest, LeastAway(Fr, Strike.X, Strike.Y));
							}
							const double UnderFloor = Strike.bAway ? std::min(0.0, UnderBest) : -1e9;
							bool bWrong = false, bThrew = false, bFromUnder = false;
							double Least = 1e9, BodyLeast = 1e9, UnderLeast = 1e9, UnderBody = 1e9;
							std::vector<std::string> UnderGiven;
							for (const FallRead& Fr : Falls)
							{
								const bool bUnder = Fr.From == "under" && MayTakeRead(Fr, bBig, "under");
								const bool bDue = bUnder && (!Strike.bAway || LeastAway(Fr, Strike.X, Strike.Y) >= UnderFloor - 1e-9);
								const bool bGunThrow = !bUnder && Fr.bThrow && bBig && Strike.bAway && Fr.From == Strike.Gun && LeastAway(Fr, Strike.X, Strike.Y) >= Floor - 1e-9;
								const bool bGiven = Has(Falls2, Fr);
								bWrong = bWrong || bGiven != (bDue || bGunThrow);
								if (bGiven && bUnder) UnderGiven.push_back(Fr.Clip);
								if (bGiven && bUnder && Strike.bAway)
								{
									bFromUnder = true;
									UnderLeast = std::min(UnderLeast, LeastAway(Fr, Strike.X, Strike.Y));
									UnderBody = std::min(UnderBody, BodyAway(Fr, Strike.X, Strike.Y));
								}
								if (!bGiven || !bGunThrow) continue;
								bThrew = true;
								Least = std::min(Least, LeastAway(Fr, Strike.X, Strike.Y));
								BodyLeast = std::min(BodyLeast, BodyAway(Fr, Strike.X, Strike.Y));
							}
							UnderWrong += bWrong ? 1 : 0;
							// (the falls from under alone, by the rule's own words: with one, it whatever his gun's side; a man on
							// his feet or a knee with his gun known, none toward it where one is not, else the least so (a gap, the
							// falls' own); his gun not known, every one)
							const int Given1 = static_cast<int>(UnderGiven.size());
							if (Unders == 1)
							{
								++OneUnder;
								OneUnderWrong += Given1 == 1 ? 0 : 1;
							}
							if (std::strcmp(St, "prone") != 0)
							{
								const std::string Cell = std::string(St) + " " + (Strike.bAway ? Strike.Gun : std::string("none"));
								++LegCells[Cell];
								for (const std::string& N : UnderGiven) ++LegGiven[Cell][N];
								if (!Strike.bAway)
								{
									++LegBlind;
									LegBlindTwo += Unders > 1 ? 1 : 0;
									LegBlindWrong += Given1 == Unders ? 0 : 1;
								}
								else if (UnderBest >= 0.0)
								{
									++LegAway;
									LegToward += Given1 > 0 && UnderLeast >= 0.0 ? 0 : 1;
								}
								else
								{
									++LegGaps;
									LegGapWrong += Given1 > 0 && UnderLeast >= UnderBest - 1e-9 ? 0 : 1;
									Gap& Gp = Gaps[Cell];
									++Gp.Cells;
									Gp.Twins.insert(KV.first);
									std::string Names;
									for (const std::string& N : UnderGiven) Names += (Names.empty() ? "" : "+") + N;
									++Gp.Given[Names];
									Gp.Least = std::min(Gp.Least, -UnderBest);
									if (-UnderBest > Gp.Most)
									{
										Gp.Most = -UnderBest;
										char At[160];
										std::snprintf(At, sizeof(At), "%s, his gun %.0f m %+d deg, %s", KV.first.c_str(), Far, Deg, Names.c_str());
										Gp.MostAt = At;
									}
									Gp.Degs.insert(Deg);
								}
							}
							if (bFromUnder)
							{
								Lot& Uf = UnderFall[St];
								char At[160];
								std::snprintf(At, sizeof(At), "%s, %s, his gun %.0f m %+d deg", KV.first.c_str(), bBig ? "a field gun's or a howitzer's" : "another's", Far, Deg);
								++Uf.Men;
								Uf.Toward += UnderLeast < 0.0 ? 1 : 0;
								Uf.BodyToward += UnderBody < 0.0 ? 1 : 0;
								if (UnderLeast < Uf.Least) { Uf.Least = UnderLeast; Uf.LeastAt = At; }
								if (UnderBody < Uf.BodyLeast) { Uf.BodyLeast = UnderBody; Uf.BodyAt = At; }
							}
							if (!bThrew) continue;
							++UnderThrew;
							++UnderThrow.Men;
							UnderThrow.Toward += Least < 0.0 ? 1 : 0;
							UnderThrow.Short += Least < 0.5 ? 1 : 0;
							UnderThrow.BodyToward += BodyLeast < 0.0 ? 1 : 0;
							char Where[160];
							std::snprintf(Where, sizeof(Where), "%s %s, his gun %.0f m %+d deg", KV.first.c_str(), St, Far, Deg);
							if (Least < UnderThrow.Least) { UnderThrow.Least = Least; UnderThrow.LeastAt = Where; }
							if (BodyLeast < UnderThrow.BodyLeast) { UnderThrow.BodyLeast = BodyLeast; UnderThrow.BodyAt = Where; }
						}
					}
				}
			}
		}
		for (const auto& KV : ByTwin)
			std::printf("     sweep %s: standing or running, the least a limb moves away %.2f m, %d of %d cells under 0.5 m; kneeling, in cover or lying %.2f m, %d "
				"toward, %d under 0.5 m\n", KV.first.c_str(), KV.second.first.Least, KV.second.first.Short, KV.second.first.Men, KV.second.second.Least,
				KV.second.second.Toward, KV.second.second.Short);
		std::printf("     sweep: %d twins, stand, run, kneel, cover and prone, a field gun's or a howitzer's shell (may throw him) and a light gun's, a tank's, a mortar's "
			"or a grenade's (may not), the burst 1, 1.5, 2, 2.5 or 3 m off or his gun 25 or 400 m off, every 3 degrees all round him: %d cells (%d more where he may "
			"take none: no gore fall), %d falls given; in %d of them none from his side he may take moves every limb half a metre away, %d of those given one from "
			"another side\n", static_cast<int>(TwinOf.size()), Cells, NoneMay, Given, Wide, WideOff);
		for (const char* St : GoreStances)
		{
			const auto Found = ByStance.find(St);
			if (Found == ByStance.end()) continue;
			const Lot& L = Found->second;
			std::printf("     sweep: %s: %d cells, %d given a fall that moves a limb toward the burst or the gun, %d under 0.5 m away, the least %.2f m (%s); %d given one "
				"that moves his body toward it, %d of them more than %.2f m where another he may take moves it away, the least %.2f m (%s)\n", St, L.Men, L.Toward, L.Short,
				L.Least, L.LeastAt.c_str(), L.BodyToward, L.BodyWorse, Slack, L.BodyLeast, L.BodyAt.c_str());
		}
		std::printf("     sweep: the band (a man standing or running, the burst or his gun 30 to 50 degrees round ahead on his left or his right): %d cells, %d given "
			"a fall from another side; %d with a limb toward it, %d under 0.5 m away, the least %.2f m (%s)\n", Band.Men, BandWide, Band.Toward, Band.Short,
			Band.Least, Band.LeastAt.c_str());
		std::printf("     sweep: the burst under him, his gun 25 or 400 m off every 3 degrees all round him or not known: %d cells; %d with a fall from under he may take, "
			"%d of them thrown from his gun's side (a field gun's or a howitzer's shell; the least a limb of those throws moves away from the gun %.2f m (%s), %d "
			"cells under 0.5 m, %d with his body toward it); %d with none from under, his gun known (by its side); %d with none and his gun not known (all he may "
			"take)\n", UnderCells, UnderThere, UnderThrew, UnderThrow.Men ? UnderThrow.Least : 0.0, UnderThrow.LeastAt.c_str(), UnderThrow.Short,
			UnderThrow.BodyToward, UnderBack, UnderAll);
		for (const char* St : GoreStances)
		{
			const auto Found = UnderFall.find(St);
			if (Found == UnderFall.end()) continue;
			const Lot& L = Found->second;
			std::printf("     sweep: the burst under him, %s, the falls from under given read against his gun (a limb toward it only where every one he may take "
				"moves it so, the gaps below): %d cells, %d with a limb toward the gun, the most %.2f m (%s); %d with his body toward it, the most %.2f m (%s)\n", St, L.Men, L.Toward,
				L.Least < 0.0 ? -L.Least : 0.0, L.Least < 0.0 ? L.LeastAt.c_str() : "none", L.BodyToward, L.BodyLeast < 0.0 ? -L.BodyLeast : 0.0,
				L.BodyLeast < 0.0 ? L.BodyAt.c_str() : "none");
		}
		// (the table: each stance on his feet or a knee and each side his gun may be on, or not known, the burst under him: of
		// the sweep's cells (every twin, his gun 25 or 400 m off every 3 degrees of that quarter, either shell) how many give
		// each fall from under; and with his gun 400 m off in that side's middle, a light gun's shell, how many twins are given
		// each and how far it moves his limb along the line toward the gun, the least and the most of the twins (m; less than
		// 0 away from it))
		for (const char* St : { "stand", "run", "kneel", "cover" })
		{
			for (const char* Side : { "front", "behind", "left", "right", "none" })
			{
				struct Row
				{
					int Twins = 0, Given = 0;
					double Lo = 1e9, Hi = -1e9;
				};
				std::map<std::string, Row> Rows;
				std::vector<std::string> Order;
				const bool bKnown = std::strcmp(Side, "none") != 0;
				const int Deg = std::strcmp(Side, "front") == 0 ? 0 : std::strcmp(Side, "behind") == 0 ? 180 : std::strcmp(Side, "left") == 0 ? -90 : 90;
				for (const auto& KV : TwinOf)
				{
					GoreStrike Strike;
					Strike.Stance = St;
					Strike.Side = "under";
					if (bKnown)
					{
						Strike.Gun = Side;
						Strike.bAway = true;
						Strike.X = 400.0 * std::cos(Deg * 3.14159265358979 / 180.0);
						Strike.Y = 400.0 * std::sin(Deg * 3.14159265358979 / 180.0);
					}
					const std::vector<std::pair<std::string, std::string>> Given2 = GoreFalls(*KV.second, Strike);
					for (const FallRead& Fr : ReadFalls(*KV.second, St))
					{
						if (Fr.From != "under" || !MayTakeRead(Fr, false, "under")) continue;
						if (Rows.find(Fr.Clip) == Rows.end()) Order.push_back(Fr.Clip);
						Row& Rw = Rows[Fr.Clip];
						++Rw.Twins;
						Rw.Given += Has(Given2, Fr) ? 1 : 0;
						if (!bKnown) continue;
						const double Toward = -LeastAway(Fr, Strike.X, Strike.Y);
						Rw.Lo = std::min(Rw.Lo, Toward);
						Rw.Hi = std::max(Rw.Hi, Toward);
					}
				}
				const std::string Cell = std::string(St) + " " + Side;
				std::string Line;
				for (const std::string& Clip : Order)
				{
					const Row& Rw = Rows[Clip];
					char Buf[240];
					if (bKnown)
						std::snprintf(Buf, sizeof(Buf), "%s %s: in its middle given %d of %d twins, toward the gun %+.2f to %+.2f m; in the sweep given in %d of %d cells",
							Line.empty() ? "" : ";", Clip.c_str(), Rw.Given, Rw.Twins, Rw.Lo, Rw.Hi, LegGiven[Cell][Clip], LegCells[Cell]);
					else
						std::snprintf(Buf, sizeof(Buf), "%s %s: given %d of %d twins; in the sweep given in %d of %d cells", Line.empty() ? "" : ";", Clip.c_str(), Rw.Given,
							Rw.Twins, LegGiven[Cell][Clip], LegCells[Cell]);
					Line += Buf;
				}
				std::printf("     under table: %s, his gun %s%s:%s\n", St, bKnown ? Side : "not known", bKnown ? (Deg == 0 ? " (middle 0 deg)" : Deg == 180 ? " (middle 180 deg)"
					: Deg < 0 ? " (middle -90 deg)" : " (middle +90 deg)") : "", Line.empty() ? " none from under" : Line.c_str());
			}
		}
		for (const auto& KV : Gaps)
		{
			const Gap& Gp = KV.second;
			std::string TwinWords, GivenWords;
			for (const std::string& T : Gp.Twins) TwinWords += (TwinWords.empty() ? "" : " ") + T;
			for (const auto& G : Gp.Given) GivenWords += (GivenWords.empty() ? "" : ", ") + G.first + " " + std::to_string(G.second);
			std::printf("     gap: %s: %d cells where every fall from under he may take moves his limb toward the gun (his gun at %s deg; %d twins: %s), given the least so "
				"(%s), %.2f to %.2f m toward it (the most: %s)\n", KV.first.c_str(), Gp.Cells, Runs(Gp.Degs).c_str(), static_cast<int>(Gp.Twins.size()), TwinWords.c_str(),
				GivenWords.c_str(), Gp.Least, Gp.Most, Gp.MostAt.c_str());
		}
		Check(Cells > 0 && NotMay == 0, "sweep: every fall given one of his stance's he may take (no throw but a field gun's or a howitzer's, none from another side, "
			"none from under but where the burst was under him), never none (" + std::to_string(NotMay) + " not)");
		Check(SideWrong == 0, "sweep: GoreSide reads every strike's side as the test does (a burst under him, under where his stance's gore falls hold one from under he "
			"may take, else his gun's side), and none where he may take none (" + std::to_string(SideWrong) + " not)");
		Check(OffSide == 0, "sweep: given only falls from his side wherever one from his side he may take moves every limb half a metre away from the burst or the gun ("
			+ std::to_string(OffSide) + " not)");
		Check(Worse == 0, "sweep: no fall given moves a limb less far away than another he may take moves every limb, up to half a metre, but where one from his side "
			"he may take none from the side opposite (" + std::to_string(Worse) + " so)");
		Check(Opposite == 0, "sweep: never a fall given from the side opposite the burst's or the gun's (front and behind, left and right) where one from its side he "
			"may take (" + std::to_string(Opposite) + " so)");
		{
			Lot Up;
			for (const char* St : { "stand", "run" })
			{
				const Lot& L = ByStance[St];
				Up.Men += L.Men, Up.Toward += L.Toward, Up.Short += L.Short, Up.BodyWorse += L.BodyWorse;
				if (L.Least < Up.Least) { Up.Least = L.Least; Up.LeastAt = L.LeastAt; }
				if (L.BodyLeast < Up.BodyLeast) { Up.BodyLeast = L.BodyLeast; Up.BodyAt = L.BodyAt; }
			}
			char Least[256];
			std::snprintf(Least, sizeof(Least), "%.2f m (%s)", Up.Least, Up.LeastAt.c_str());
			Check(Up.Men > 0 && Up.Toward == 0, "sweep: no limb off a man standing or running moves toward the burst or the gun (" + std::to_string(Up.Men) + " cells, "
				+ std::to_string(Up.Toward) + " toward; the least a limb moves away " + Least + "; " + std::to_string(Up.Short) + " cells under 0.5 m)");
			Check(Every > 0 && EveryToward == 0, "sweep: no limb off a man moves toward the burst or the gun where his stance's gore falls hold one from each side that "
				"is not a throw (standing or running, and since the deaths thread's falls of 24 Nov kneeling, in cover and lying: " + std::to_string(Every) + " cells, "
				+ std::to_string(EveryToward) + " toward)");
			std::snprintf(Least, sizeof(Least), "%.2f m (%s)", Up.BodyLeast, Up.BodyAt.c_str());
			Check(Up.BodyWorse == 0, "sweep: no fall given a man standing or running moves his body (his pelvis) toward the burst or the gun more than 0.01 m where "
				"another he may take moves it away (" + std::to_string(Up.BodyWorse) + " cells so; the least his body moves away " + Least + ")");
		}
		Check(UnderWrong == 0 && UnderBackWrong == 0 && UnderAllWrong == 0, "sweep: the burst under him, given the falls from under he may take (his gun known, "
			"those that move no limb toward it, or where every one does the least so; not known, every one) and else only such a shell's throw from his gun's side where its limbs move as far from the gun as that side's falls set; with none from under he may take, what his gun's "
			"side gives, or where it is not known all he may take (" + std::to_string(UnderCells) + " cells, " + std::to_string(UnderWrong + UnderBackWrong + UnderAllWrong)
			+ " not)");
		Check(UnderThrow.Toward == 0 && UnderThrow.BodyToward == 0, "sweep: the burst under him, no throw from his gun's side moves a limb or his body toward the gun ("
			+ std::to_string(UnderThrew) + " cells thrown, " + std::to_string(UnderThrow.Toward) + " with a limb toward, " + std::to_string(UnderThrow.BodyToward)
			+ " with his body)");
		const std::string NoLegFalls = bLegFalls ? "" : "; no twin's falls on his feet or a knee hold one from under";
		Check((LegAway > 0 || !bLegFalls) && LegToward == 0, "sweep: the burst under a man standing, running, kneeling or in cover, his gun known, no fall from "
			"under given moves his limb toward the gun where one he may take does not (" + std::to_string(LegAway) + " cells, " + std::to_string(LegToward) + " so"
			+ NoLegFalls + ")");
		Check(LegGapWrong == 0, "sweep: the burst under him so, his gun known, where every fall from under he may take moves his limb toward the gun, given those that "
			"move it least so (" + std::to_string(LegGaps) + " such cells, the gaps above, " + std::to_string(LegGapWrong) + " not)");
		Check((LegBlind > 0 || !bLegFalls) && LegBlindWrong == 0, "sweep: the burst under him so, his gun not known, given every fall from under he may take ("
			+ std::to_string(LegBlind) + " cells, " + std::to_string(LegBlindTwo) + " of them with two or more, " + std::to_string(LegBlindWrong) + " not"
			+ NoLegFalls + ")");
		Check(OneUnderWrong == 0, "sweep: the burst under him, any stance with one fall from under he may take (twins of 24 Nov; lying, the prone throw) given it "
			"whatever his gun's side (" + std::to_string(OneUnder) + " cells, " + std::to_string(OneUnderWrong) + " not)");
	}

	// ---- The picture: six gored men, each in his own fall, from the side.
	if (Argc > 4 && !GoredMen.empty())
	{
		std::vector<int> Pick;
		std::set<std::string> Seen;
		for (const int I : GoredMen) if (Pick.size() < 6 && Seen.insert(Rc.Clip[static_cast<size_t>(I)]).second) Pick.push_back(I);
		// (then each army's own falls: no clip twice for the same army)
		for (const int I : GoredMen)
			if (Pick.size() < 6 && std::find(Pick.begin(), Pick.end(), I) == Pick.end()
				&& Seen.insert(C.Info(C.Find(Men[static_cast<size_t>(I)].B, Men[static_cast<size_t>(I)].K)).FileArmy + "/" + Rc.Clip[static_cast<size_t>(I)]).second)
				Pick.push_back(I);
		for (const int I : GoredMen) if (Pick.size() < 6 && std::find(Pick.begin(), Pick.end(), I) == Pick.end()) Pick.push_back(I);
		Crowd P;
		Make(P, true, true);
		const int Cols = 6, Rows = static_cast<int>(Pick.size()), SheetW = 1600, CellH = 210;
		struct Step { std::vector<float> At, Fa; };
		std::vector<std::vector<Step>> Steps(static_cast<size_t>(Rows), std::vector<Step>(Cols));
		std::vector<std::pair<double, std::pair<int, int>>> Due;
		for (int Row = 0; Row < Rows; ++Row)
		{
			const Who& Wh = Men[static_cast<size_t>(Pick[static_cast<size_t>(Row)])];
			const Soldier& Tw = *C.SoldierOf(C.Find(Wh.B, Wh.K));
			const int Ix = Tw.ClipIndex(Rc.Clip[static_cast<size_t>(Pick[static_cast<size_t>(Row)])]);
			const double Long = Ix >= 0 ? Tw.Clips[static_cast<size_t>(Ix)].Long : 2.0;
			for (int Col = 0; Col < Cols; ++Col) Due.push_back({ Hit + 0.02 + (Col < Cols - 1 ? Long * Col / (Cols - 1) : Long + 0.3), { Row, Col } });
		}
		std::stable_sort(Due.begin(), Due.end(), [](const auto& A, const auto& B) { return A.first < B.first; });
		double Now = 0.0;
		for (const auto& Dn : Due)
		{
			while (Now + 1.0 / 60.0 < Dn.first) { Now += 1.0 / 60.0; P.Update(Now); }
			Now = Dn.first;
			P.Update(Now);
			const Who& Wh = Men[static_cast<size_t>(Pick[static_cast<size_t>(Dn.second.first)])];
			Step& St = Steps[static_cast<size_t>(Dn.second.first)][static_cast<size_t>(Dn.second.second)];
			Skinned(P, P.Find(Wh.B, Wh.K), St.At, St.Fa);
		}
		std::vector<uint8_t> Sheet(static_cast<size_t>(SheetW) * static_cast<size_t>(CellH * Rows) * 3, 236);
		Light Lt = Daylight(215.0, 13.0);
		Lt.EarthA = Lin(118, 98, 76);
		Lt.EarthB = Lin(96, 80, 62);
		Lt.HazeK = 0.0;
		for (int Row = 0; Row < Rows; ++Row)
		{
			const Who& Wh = Men[static_cast<size_t>(Pick[static_cast<size_t>(Row)])];
			const int Man = P.Find(Wh.B, Wh.K);
			std::printf("     picture row %d: %s, %s %s, burst %.0f m off, %s (%s)\n", Row, P.Info(Man).FileArmy.c_str(), Wh.W.c_str(), "kill", Wh.Burst,
				P.Info(Man).FallClip.c_str(), P.Info(Man).FallKind.c_str());
			double XLo = 1e9, XHi = -1e9, YLo = 1e9, YHi = -1e9;
			for (const Step& St : Steps[static_cast<size_t>(Row)])
			{
				for (size_t K = 0; K < St.At.size(); K += 3)
				{
					XLo = std::min(XLo, static_cast<double>(St.At[K])); XHi = std::max(XHi, static_cast<double>(St.At[K]));
					YLo = std::min(YLo, static_cast<double>(St.At[K + 1])); YHi = std::max(YHi, static_cast<double>(St.At[K + 1]));
				}
			}
			const double XMid = 0.5 * (XLo + XHi), YMid = 0.5 * (YLo + YHi);
			const double Back = std::max(5.0, (YHi - YLo) * 1.4 + 3.0);
			for (int Col = 0; Col < Cols; ++Col)
			{
				const int OX = (SheetW * Col) / Cols, OX1 = (SheetW * (Col + 1)) / Cols;
				const int CW = OX1 - OX - 4, CH = CellH - 4;
				Scene Sc;
				const Step& St = Steps[static_cast<size_t>(Row)][static_cast<size_t>(Col)];
				Sc.AddMan(P.LookOf(Man, Near), St.At, St.Fa);
				Sc.AddGround(XMid - 10.0, YMid - 10.0, XMid + 10.0, YMid + 12.0, 0.5, Flat);
				const double Span = std::max(XHi - XLo + 0.8, 2.4 * CW / CH);
				const double Fov = 2.0 * std::atan(Span * 0.5 / Back) * 180.0 / 3.14159265358979;
				const Camera Cam({ XMid, YMid - Back, 1.45 }, { XMid, YMid, 0.6 }, Fov, CW * 2, CH * 2);
				ShadowMap Sh;
				Sh.Build(Sc, Lt.ToSun, { XMid - 4.0, YMid - 4.0, -0.1 }, { XMid + 4.0, YMid + 4.0, 2.2 }, 1536);
				const Picture Pic = Paint(Sc, Cam, Lt, Sh);
				Put(Pic, 1.0, Sheet, SheetW, OX + 2, Row * CellH + 2);
			}
		}
		const std::string OutPath = std::string(Argv[4]) + "/gore_falls.ppm";
		if (!WritePpm(OutPath, Sheet, SheetW, CellH * Rows)) { std::printf("cannot write %s\n", OutPath.c_str()); return 1; }
		std::printf("wrote %s (%dx%d)\n", OutPath.c_str(), SheetW, CellH * Rows);
	}
	std::printf(Failed ? "land_gore_test: %d FAILED\n" : "land_gore_test: all passed\n", Failed);
	return Failed ? 1 : 0;
}
