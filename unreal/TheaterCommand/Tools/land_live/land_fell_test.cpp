// The ground battle's crash locator (Battle/LandFell), checked by crashing on purpose: a child process keeps a handler
// of its own (as the game keeps its crash report), arms the locator, goes through more parts than the ring holds, and
// reads address 0 two calls down. The parent then checks that the report names the signal, the address, the last part
// and the frames; that the ring file, kept in memory as it went, holds the last part; and that the child's own handler
// still ran after it (the game's crash report is not lost).
//
// Built and run by setup.sh:  g++ -std=c++20 -O1 -g -fno-omit-frame-pointer -I<Source/TheaterCommand> land_fell_test.cpp
//   <Battle>/LandFell.cpp -o land_fell_test && ./land_fell_test <scratch folder>
#include "Battle/LandFell.h"

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

namespace
{
	std::string Folder;
	int Failures = 0;

	void Check(bool bOk, const std::string& What)
	{
		if (!bOk)
		{
			++Failures;
			std::printf("FAIL: %s\n", What.c_str());
		}
	}

	std::string Read(const std::string& Path)
	{
		std::ifstream In(Path, std::ios::binary);
		std::stringstream S;
		S << In.rdbuf();
		return S.str();
	}

	void OwnHandler(int Sig)
	{
		const std::string Path = Folder + "/own-handler.txt";
		const int Fd = open(Path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
		const char Said[] = "the game's own crash report ran\n";
		if (Fd >= 0)
		{
			ssize_t Wrote = write(Fd, Said, sizeof(Said) - 1);
			(void)Wrote;
			close(Fd);
		}
		_exit(Sig == SIGSEGV ? 3 : 4);
	}

	__attribute__((noinline)) int Deeper(volatile int* At)
	{
		return *At + 1;
	}

	__attribute__((noinline)) int Deep(volatile int* At)
	{
		return Deeper(At) * 2;
	}
}

int main(int Argc, char** Argv)
{
	Folder = Argc > 1 ? Argv[1] : "/tmp/land-fell";
	const std::string Report = Folder + "/fell-over.txt", Ring = Folder + "/fell-over-ring.txt";
	std::remove(Report.c_str());
	std::remove(Ring.c_str());
	std::remove((Folder + "/own-handler.txt").c_str());
	const pid_t Child = fork();
	if (Child == 0)
	{
		std::signal(SIGSEGV, &OwnHandler);
		if (!LandFell::Arm(Report.c_str(), Ring.c_str()))
		{
			_exit(9);
		}
		static const char* const Parts[] = { "tick: live pump", "tick: men posed", "tick: general's view", "tick: ground shown", "men: skins" };
		for (int K = 0; K < 70; ++K)
		{
			LandFell::Phase(Parts[K % 5], K % 3 == 0 ? K : -1);
		}
		LandFell::Phase("tick: the walk", 1);
		LandFell::Phase("men: banked, the last part before the fall");
		volatile int* Nowhere = nullptr;
		const int Never = Deep(Nowhere);
		std::printf("not fallen (%d)\n", Never);
		_exit(0);
	}
	int Status = 0;
	waitpid(Child, &Status, 0);
	Check(WIFEXITED(Status) && WEXITSTATUS(Status) == 3, "the child's own handler ran after the locator and ended it (exit " +
		std::to_string(WIFEXITED(Status) ? WEXITSTATUS(Status) : -1) + ", signal " + std::to_string(WIFSIGNALED(Status) ? WTERMSIG(Status) : 0) + ")");
	Check(Read(Folder + "/own-handler.txt").find("own crash report ran") != std::string::npos, "the own handler's file was written");
	const std::string Said = Read(Report);
	std::printf("%s", Said.c_str());
	Check(Said.find("fell over: SIGSEGV at address 0x0 ===") != std::string::npos, "the report names the signal and address 0");
	const size_t Last = Said.find("men: banked, the last part before the fall");
	Check(Last != std::string::npos && Said.find("tick: the walk (1)") < Last, "the report's parts end with the last two, in order");
	Check(Said.find("tick: live pump (0)") == std::string::npos, "the ring keeps only its last 64");
	Check(Said.find("fell at") != std::string::npos && Said.find("land_fell_test") != std::string::npos && Said.find(" +0x") != std::string::npos, "the stack's frames are named by library and offset");
	int Frames = 0;
	for (size_t At = Said.find("  land_fell_test"); At != std::string::npos; At = Said.find("  land_fell_test", At + 1))
	{
		++Frames;
	}
	Check(Frames >= 3, "at least three of the test's own frames (Deeper, Deep, main): " + std::to_string(Frames));
	const std::string Lines = Read(Ring);
	Check(Lines.size() == 64 * 80, "the ring file is 64 lines of 80: " + std::to_string(Lines.size()));
	Check(Lines.find("000000071 men: banked, the last part before the fall") != std::string::npos, "the ring file holds the last part, numbered 71");
	Check(Lines.find("000000070 tick: the walk (1)") != std::string::npos, "and the one before it");
	Check(Lines.find("000000007 ") == std::string::npos, "and not the parts the ring has passed");
	std::printf(Failures ? "FAIL (%d)\n" : "the crash locator names where it fell\n", Failures);
	return Failures ? 1 : 0;
}
