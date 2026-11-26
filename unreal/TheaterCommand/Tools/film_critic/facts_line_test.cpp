// film critic: a plain test of the game's facts-line writer (Source/TheaterCommand/Campaign/FilmCriticLine.h), with no
// Unreal: each line it writes is compared, character for character, with the line expected.
//
//   g++ -std=c++17 -Wall -Wextra -Wshadow -Werror -I Source/TheaterCommand/Campaign Tools/film_critic/facts_line_test.cpp -o facts_line_test
//   ./facts_line_test             (prints the lines written, one a line, and "facts_line_test: N passed" on stderr)
//
// Its output can be fed to any JSON reader line by line (the night's check reads the facts file that way).

#include "FilmCriticLine.h"

#include <cmath>
#include <cstdio>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace
{
	int Failures = 0;
	int Passes = 0;

	void Expect(const std::string& Got, const std::string& Want, const char* What)
	{
		if (Got == Want)
		{
			++Passes;
			std::printf("%s", Got.c_str());
			if (Got.empty() || Got.back() != '\n')
			{
				std::printf("\n");
			}
			return;
		}
		++Failures;
		std::fprintf(stderr, "FAIL %s\n  got:  %s\n  want: %s\n", What, Got.c_str(), Want.c_str());
	}

	std::string Number(double Value, int Places)
	{
		std::string Out;
		FilmCriticLine::AppendNumber(Out, Value, Places);
		return Out;
	}

	std::string Quoted(const std::string& Text)
	{
		std::string Out;
		FilmCriticLine::AppendQuoted(Out, Text);
		return Out;
	}
}

int main()
{
	// Numbers: fixed decimals, rounded half away from zero, no "-0", not finite as null.
	Expect("{\"n\":" + Number(12.5, 3) + "}", "{\"n\":12.500}", "fixed decimals");
	Expect("{\"n\":" + Number(-0.25, 3) + "}", "{\"n\":-0.250}", "a small negative keeps its sign");
	Expect("{\"n\":" + Number(-0.0004, 3) + "}", "{\"n\":0.000}", "no -0");
	Expect("{\"n\":" + Number(-0.0, 2) + "}", "{\"n\":0.00}", "negative zero");
	Expect("{\"n\":" + Number(2.0625, 3) + "}", "{\"n\":2.063}", "rounding half up at the last decimal");
	Expect("{\"n\":" + Number(1.5, 0) + "}", "{\"n\":2}", "no decimals rounds half away");
	Expect("{\"n\":" + Number(-1.5, 0) + "}", "{\"n\":-2}", "no decimals rounds half away, negative");
	Expect("{\"n\":" + Number(1600.0, 0) + "}", "{\"n\":1600}", "a whole number");
	Expect("{\"n\":" + Number(0.000123456, 9) + "}", "{\"n\":0.000123456}", "nine decimals");
	Expect("{\"n\":" + Number(3.25, 12) + "}", "{\"n\":3.250000000}", "places held to nine");
	Expect("{\"n\":" + Number(7.0, -2) + "}", "{\"n\":7}", "negative places as none");
	Expect("{\"n\":" + Number(std::numeric_limits<double>::quiet_NaN(), 3) + "}", "{\"n\":null}", "NaN");
	Expect("{\"n\":" + Number(std::numeric_limits<double>::infinity(), 3) + "}", "{\"n\":null}", "infinity");
	Expect("{\"n\":" + Number(-std::numeric_limits<double>::infinity(), 1) + "}", "{\"n\":null}", "minus infinity");
	Expect("{\"n\":" + Number(1.0e300, 3) + "}", "{\"n\":null}", "too big to write");
	Expect("{\"n\":" + Number(123456789.125, 3) + "}", "{\"n\":123456789.125}", "a large value");
	Expect("{\"n\":" + Number(-98765.4321, 2) + "}", "{\"n\":-98765.43}", "a large negative");

	// Strings: quote, backslash and controls escaped; UTF-8 passed through.
	Expect("{\"s\":" + Quoted("plain") + "}", "{\"s\":\"plain\"}", "a plain string");
	Expect("{\"s\":" + Quoted("say \"go\"") + "}", "{\"s\":\"say \\\"go\\\"\"}", "quotes");
	Expect("{\"s\":" + Quoted("C:\\Shots\\x") + "}", "{\"s\":\"C:\\\\Shots\\\\x\"}", "backslashes");
	Expect("{\"s\":" + Quoted("a\nb\tc\rd") + "}", "{\"s\":\"a\\nb\\tc\\rd\"}", "newline, tab, return");
	Expect("{\"s\":" + Quoted(std::string("x\x01y\x1fz", 5)) + "}", "{\"s\":\"x\\u0001y\\u001fz\"}", "control characters");
	Expect("{\"s\":" + Quoted(std::string("nul\0end", 7)) + "}", "{\"s\":\"nul\\u0000end\"}", "a nul inside");
	Expect("{\"s\":" + Quoted("Panzerkampfwagen \xc3\xbc \xe2\x80\x94 Kur\xc3\xa9") + "}",
		"{\"s\":\"Panzerkampfwagen \xc3\xbc \xe2\x80\x94 Kur\xc3\xa9\"}", "UTF-8 passes through");
	Expect("{\"s\":" + Quoted("") + "}", "{\"s\":\"\"}", "an empty string");

	// Whole lines, as the game writes them.
	{
		FilmCriticLine::Line L(12.3456, "cam");
		L.Whole("f", 1234567).Num("bt", 301.256, 2).Str("scene", "air").Vec3("eye", 1.0, -2.5, 300.04).Vec3("rot", -12.5, 359.9999, 0.0, 2)
			.Num("fov", 62.0, 2).Num("dt", 16.6667, 2).Flag("cut", false).Str("why", "close").NamedAt("subject", "plane 12", 10.0, 20.0, 3000.5);
		Expect(L.Done(), "{\"t\":12.346,\"k\":\"cam\",\"f\":1234567,\"bt\":301.26,\"scene\":\"air\",\"eye\":[1.000,-2.500,300.040],"
			"\"rot\":[-12.50,360.00,0.00],\"fov\":62.00,\"dt\":16.67,\"cut\":false,\"why\":\"close\",\"subject\":{\"name\":\"plane 12\",\"at\":[10.000,20.000,3000.500]}}\n",
			"a cam line");
	}
	{
		FilmCriticLine::Line L(0.0, "run");
		L.Whole("f", 0).Str("scene", "map").StrOrNull("key", "").Vec2("window", 1600, 1000, 0).Nothing("strip").Flag("steps", true).Whole("v", 1);
		Expect(L.Done(), "{\"t\":0.000,\"k\":\"run\",\"f\":0,\"scene\":\"map\",\"key\":null,\"window\":[1600,1000],\"strip\":null,\"steps\":true,\"v\":1}\n", "a run line");
	}
	{
		const double None = std::numeric_limits<double>::quiet_NaN();
		const std::vector<std::pair<std::string, double>> InFrame = { { "attack", 3.0 }, { "defend", 0.0 } };
		const std::vector<std::pair<std::string, double>> Nearest = { { "attack", 812.25 }, { "defend", None } };
		FilmCriticLine::Line L(40.5, "planes");
		L.Whole("f", 99).Num("bt", 12.0, 2).Str("shot", "flyover-raid").Pairs("inframe", InFrame, 0).Pairs("nearest_m", Nearest, 1);
		Expect(L.Done(), "{\"t\":40.500,\"k\":\"planes\",\"f\":99,\"bt\":12.00,\"shot\":\"flyover-raid\",\"inframe\":{\"attack\":3,\"defend\":0},"
			"\"nearest_m\":{\"attack\":812.3,\"defend\":null}}\n", "a planes line");
	}
	{
		FilmCriticLine::Line L(1.0, "men");
		L.Num("run_p10", std::numeric_limits<double>::quiet_NaN(), 2).Strs("clips", { "run", "walk \"slow\"" }).Strs("none", {});
		Expect(L.Done(), "{\"t\":1.000,\"k\":\"men\",\"run_p10\":null,\"clips\":[\"run\",\"walk \\\"slow\\\"\"],\"none\":[]}\n", "lists and nulls");
	}
	{
		FilmCriticLine::Line L(2.0, "fleet");
		L.Whole("id", -1).Vec2("course", 0.6, -0.8).Vec2("bowpx", std::nan(""), 10.0, 1);
		Expect(L.Done(), "{\"t\":2.000,\"k\":\"fleet\",\"id\":-1,\"course\":[0.600,-0.800],\"bowpx\":[null,10.0]}\n", "vectors with a null");
	}

	std::fprintf(stderr, "facts_line_test: %d passed, %d failed\n", Passes, Failures);
	return Failures == 0 ? 0 : 1;
}
