// sea_lamps_test: the sea battle's detection of the effects kit's sea lamps (SeaParts::HasSeaLamps and AskSeaLamps,
// the release 2 ocean plan, step 11) on a mock kit of every setter shape, and its naval muzzle rows by calibre and kind,
// run on the game's own lines (lamps_cut.inc, cut by lamps_extract.py). A kit counts as having the lamps only when its
// SetLampRule takes the rule's own Sea(), by reference or by value, noexcept or not, whatever it returns; any other
// shape, and a kit with no setter (this branch's), reads as none, and the scene keeps today's rows.
// Usage: sea_lamps_test
#include <cstdio>
#include <string>
#include <type_traits>
using TCHAR = char;
#define TEXT(x) x
namespace TheaterFx { namespace Battle { struct LampRule { int N = 8; static LampRule Sea() { LampRule R; R.N = 3; return R; } }; } }
using R = TheaterFx::Battle::LampRule;
struct ByRef { int Asked = 0; void SetLampRule(const R& Rule) { Asked = Rule.N; } };
struct ByValue { int Asked = 0; void SetLampRule(R Rule) { Asked = Rule.N; } };
struct ByRefNoexcept { int Asked = 0; void SetLampRule(const R& Rule) noexcept { Asked = Rule.N; } };
struct ByValueNoexcept { int Asked = 0; void SetLampRule(R Rule) noexcept { Asked = Rule.N; } };
struct Returns { int Asked = 0; bool SetLampRule(const R& Rule) { Asked = Rule.N; return true; } };
struct ReturnsNoexcept { int Asked = 0; R& SetLampRule(const R& Rule) noexcept { static R Kept; Asked = Rule.N; return Kept; } };
struct ConstSetter { int Asked = 0; void SetLampRule(const R&) const {} };
struct NoSea { int Asked = 0; void SetLampRule(int N) { Asked = N; } };
struct Overloaded { int Asked = 0; void SetLampRule(const R& Rule) { Asked = Rule.N; } void SetLampRule(int) {} };
struct Old { int Asked = 0; void SetSun(int) {} };
namespace SeaParts
{
#include "lamps_cut.inc"

}
template <typename K> static int Try(const char* Name, bool bWant, int Want)
{
	K Kit;
	SeaParts::AskSeaLamps(&Kit);
	SeaParts::AskSeaLamps<K>(nullptr);
	const bool bOk = SeaParts::HasSeaLamps<K>() == bWant && Kit.Asked == Want;
	std::printf("  %s %-16s has %d asked %d\n", bOk ? "ok  " : "FAIL", Name, SeaParts::HasSeaLamps<K>() ? 1 : 0, Kit.Asked);
	return bOk ? 0 : 1;
}
int main()
{
	int Bad = 0;
	Bad += Try<ByRef>("by reference", true, 3);
	Bad += Try<ByValue>("by value", true, 3);
	Bad += Try<ByRefNoexcept>("ref noexcept", true, 3);
	Bad += Try<ByValueNoexcept>("value noexcept", true, 3);
	Bad += Try<Returns>("returns bool", true, 3);
	Bad += Try<ReturnsNoexcept>("returns, noexcept", true, 3);
	Bad += Try<ConstSetter>("const setter", false, 0);
	Bad += Try<NoSea>("takes an int", false, 0);
	Bad += Try<Overloaded>("overloaded", false, 0);
	Bad += Try<Old>("no setter", false, 0);
	struct Row { double Mm; const char* What; const char* Want; } Rows[] = { {406, "bb", "naval_heavy"}, {280, "bb", "naval_heavy"}, {203, "ca", "naval_medium"},
		{152, "dd", "naval_medium"}, {133, "ca", "naval_medium"}, {127, "ca", "naval_medium"}, {127, "dd", "naval_light"}, {127, "de", "naval_light"},
		{127, "cv", "naval_light"}, {140, "ss", "naval_light"}, {120, "ca", "naval_light"} };
	for (const Row& One : Rows)
	{
		const bool bOk = std::string(SeaParts::NavalRow(One.Mm, One.What)) == One.Want;
		Bad += bOk ? 0 : 1;
		std::printf("  %s %g mm %s: %s\n", bOk ? "ok  " : "FAIL", One.Mm, One.What, SeaParts::NavalRow(One.Mm, One.What));
	}
	std::printf("sea lamps detection: %d failed\n", Bad);
	return Bad;
}
