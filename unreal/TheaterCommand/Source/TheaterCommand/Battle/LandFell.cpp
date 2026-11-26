#include "Battle/LandFell.h"

#include <atomic>
#include <cstdint>
#include <cstring>
#include <type_traits>

#if defined(__APPLE__) || defined(__linux__)
#define LAND_FELL_SIGNALS 1
#ifdef THIRD_PARTY_INCLUDES_START
THIRD_PARTY_INCLUDES_START
#endif
#include <dlfcn.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/ucontext.h>
#include <unistd.h>
#ifdef THIRD_PARTY_INCLUDES_END
THIRD_PARTY_INCLUDES_END
#endif
#else
#define LAND_FELL_SIGNALS 0
#endif

namespace LandFell
{
	namespace
	{
		constexpr unsigned RingSize = 64;
		std::atomic<const char*> Ring[RingSize];
		std::atomic<int> RingDetail[RingSize];
		std::atomic<unsigned> RingNext{ 0 };
		/** The ring as text in a file mapped into memory, written as it goes: the page stays with the file however the
		 * game ends, even should no handler run (RingLine characters a part, numbered so they sort). */
		constexpr unsigned RingLine = 80;
		char* Mapped = nullptr;

		void PutLine(char* Line, unsigned Seq, const char* Where, int Detail)
		{
			unsigned N = 0;
			char Digits[12];
			int D = 0;
			unsigned V = Seq;
			for (int K = 0; K < 9; ++K)
			{
				Digits[D++] = static_cast<char>('0' + V % 10);
				V /= 10;
			}
			while (D > 0)
			{
				Line[N++] = Digits[--D];
			}
			Line[N++] = ' ';
			for (const char* C = Where; C && *C && N < RingLine - 14; ++C)
			{
				Line[N++] = *C;
			}
			if (Detail >= 0)
			{
				Line[N++] = ' ';
				Line[N++] = '(';
				unsigned Dv = static_cast<unsigned>(Detail);
				D = 0;
				do
				{
					Digits[D++] = static_cast<char>('0' + Dv % 10);
					Dv /= 10;
				} while (Dv && D < 10);
				while (D > 0 && N < RingLine - 2)
				{
					Line[N++] = Digits[--D];
				}
				Line[N++] = ')';
			}
			while (N < RingLine - 1)
			{
				Line[N++] = ' ';
			}
			Line[RingLine - 1] = '\n';
		}

#if LAND_FELL_SIGNALS
		// (not SIGABRT: abort() puts the default back once a handler returns, so the game's own report would be lost; the
		// pre-check, 22 November)
		constexpr int Signals[] = { SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGTRAP };
		constexpr int SignalCount = static_cast<int>(sizeof(Signals) / sizeof(Signals[0]));
		struct sigaction Before[SignalCount];
		bool bArmed = false;
		int Out = -1;
		int RingOut = -1;

		/** The report, built without the heap or printf (a crash may have come from inside either). */
		struct Text
		{
			char Buffer[16384];
			size_t Used = 0;

			void Put(const char* S)
			{
				while (S && *S && Used + 1 < sizeof(Buffer))
				{
					Buffer[Used++] = *S++;
				}
			}
			void Dec(long long V)
			{
				char Digits[24];
				int N = 0;
				const bool bMinus = V < 0;
				unsigned long long U = bMinus ? 0ull - static_cast<unsigned long long>(V) : static_cast<unsigned long long>(V);
				do
				{
					Digits[N++] = static_cast<char>('0' + U % 10);
					U /= 10;
				} while (U && N < 23);
				if (bMinus)
				{
					Put("-");
				}
				while (N > 0 && Used + 1 < sizeof(Buffer))
				{
					Buffer[Used++] = Digits[--N];
				}
			}
			void Hex(uintptr_t V)
			{
				char Digits[20];
				int N = 0;
				do
				{
					const unsigned D = static_cast<unsigned>(V & 15u);
					Digits[N++] = static_cast<char>(D < 10 ? '0' + D : 'a' + D - 10);
					V >>= 4;
				} while (V && N < 19);
				Put("0x");
				while (N > 0 && Used + 1 < sizeof(Buffer))
				{
					Buffer[Used++] = Digits[--N];
				}
			}
		};

		Text Report;

		const char* NameOfSignal(int Sig)
		{
			switch (Sig)
			{
			case SIGSEGV: return "SIGSEGV";
			case SIGBUS: return "SIGBUS";
			case SIGILL: return "SIGILL";
			case SIGFPE: return "SIGFPE";
			case SIGTRAP: return "SIGTRAP";
			default: return "a signal";
			}
		}

		/** A code address as its library and offset (what `atos -o <library> -l <base>` or `atos -o <library>` with the
		 * offset reads), and the nearest exported name when there is one. */
		void Frame(Text& T, const char* Label, uintptr_t At)
		{
			T.Put("  ");
			T.Put(Label);
			T.Put(" ");
			T.Hex(At);
			Dl_info Info;
			std::memset(&Info, 0, sizeof(Info));
			if (At && dladdr(reinterpret_cast<void*>(At), &Info) && Info.dli_fname)
			{
				const char* Name = Info.dli_fname;
				for (const char* C = Info.dli_fname; *C; ++C)
				{
					if (*C == '/')
					{
						Name = C + 1;
					}
				}
				T.Put("  ");
				T.Put(Name);
				T.Put(" +");
				T.Hex(At - reinterpret_cast<uintptr_t>(Info.dli_fbase));
				T.Put(" (loaded at ");
				T.Hex(reinterpret_cast<uintptr_t>(Info.dli_fbase));
				T.Put(")");
				if (Info.dli_sname)
				{
					T.Put("  near ");
					T.Put(Info.dli_sname);
				}
			}
			T.Put("\n");
		}

		/** A register as an address, whether the platform's headers give it as a number or as a pointer. */
		template <typename T>
		uintptr_t AsAddress(T Value)
		{
			if constexpr (std::is_pointer_v<T>)
			{
				return reinterpret_cast<uintptr_t>(Value);
			}
			else
			{
				return static_cast<uintptr_t>(Value);
			}
		}

		/** The registers the fault left: where it was, where it was called from, its frame and its stack. */
		bool Registers(void* Context, uintptr_t& Pc, uintptr_t& Lr, uintptr_t& Fp, uintptr_t& Sp)
		{
			Pc = Lr = Fp = Sp = 0;
			if (!Context)
			{
				return false;
			}
#if defined(__APPLE__) && (defined(__arm64__) || defined(__aarch64__))
			const ucontext_t* U = static_cast<const ucontext_t*>(Context);
			if (!U->uc_mcontext)
			{
				return false;
			}
#ifdef __darwin_arm_thread_state64_get_pc
			Pc = AsAddress(__darwin_arm_thread_state64_get_pc(U->uc_mcontext->__ss));
			Lr = AsAddress(__darwin_arm_thread_state64_get_lr(U->uc_mcontext->__ss));
			Fp = AsAddress(__darwin_arm_thread_state64_get_fp(U->uc_mcontext->__ss));
			Sp = AsAddress(__darwin_arm_thread_state64_get_sp(U->uc_mcontext->__ss));
#else
			Pc = AsAddress(U->uc_mcontext->__ss.__pc);
			Lr = AsAddress(U->uc_mcontext->__ss.__lr);
			Fp = AsAddress(U->uc_mcontext->__ss.__fp);
			Sp = AsAddress(U->uc_mcontext->__ss.__sp);
#endif
			return true;
#elif defined(__APPLE__) && defined(__x86_64__)
			const ucontext_t* U = static_cast<const ucontext_t*>(Context);
			if (!U->uc_mcontext)
			{
				return false;
			}
			Pc = AsAddress(U->uc_mcontext->__ss.__rip);
			Fp = AsAddress(U->uc_mcontext->__ss.__rbp);
			Sp = AsAddress(U->uc_mcontext->__ss.__rsp);
			return true;
#elif defined(__linux__) && defined(__x86_64__) && defined(REG_RIP)
			const ucontext_t* U = static_cast<const ucontext_t*>(Context);
			Pc = AsAddress(U->uc_mcontext.gregs[REG_RIP]);
			Fp = AsAddress(U->uc_mcontext.gregs[REG_RBP]);
			Sp = AsAddress(U->uc_mcontext.gregs[REG_RSP]);
			return true;
#elif defined(__linux__) && defined(__aarch64__)
			const ucontext_t* U = static_cast<const ucontext_t*>(Context);
			Pc = AsAddress(U->uc_mcontext.pc);
			Lr = AsAddress(U->uc_mcontext.regs[30]);
			Fp = AsAddress(U->uc_mcontext.regs[29]);
			Sp = AsAddress(U->uc_mcontext.sp);
			return true;
#else
			return false;
#endif
		}

		void OnFault(int Sig, siginfo_t* Info, void* Context)
		{
			// The game's own handlers back first: a fault in this one goes to them (SA_NODEFER), and on return the
			// fault comes again and they report it as they always have.
			for (int K = 0; K < SignalCount; ++K)
			{
				sigaction(Signals[K], &Before[K], nullptr);
			}
			bArmed = false;
			Text& T = Report;
			T.Used = 0;
			T.Put("\n=== The ground battle fell over: ");
			T.Put(NameOfSignal(Sig));
			T.Put(" at address ");
			T.Hex(Info ? reinterpret_cast<uintptr_t>(Info->si_addr) : 0);
			T.Put(" ===\nThe last parts of its frame, oldest first (the last is where it fell):\n");
			const unsigned Next = RingNext.load(std::memory_order_relaxed);
			const unsigned First = Next > RingSize ? Next - RingSize : 0;
			for (unsigned K = First; K < Next; ++K)
			{
				const char* Where = Ring[K % RingSize].load(std::memory_order_relaxed);
				if (!Where)
				{
					continue;
				}
				T.Put("  ");
				T.Put(Where);
				const int Detail = RingDetail[K % RingSize].load(std::memory_order_relaxed);
				if (Detail >= 0)
				{
					T.Put(" (");
					T.Dec(Detail);
					T.Put(")");
				}
				T.Put("\n");
			}
			uintptr_t Pc = 0, Lr = 0, Fp = 0, Sp = 0;
			if (Registers(Context, Pc, Lr, Fp, Sp))
			{
				T.Put("Its stack (each frame's library and offset in it):\n");
				Frame(T, "fell at   ", Pc);
				if (Lr)
				{
					Frame(T, "called by ", Lr);
				}
				// (the frames by their chain of frame pointers: each holds the one before and its return address; only
				// a frame on the stack above the last, and aligned, is followed)
				uintptr_t At = Fp;
				for (int Depth = 0; Depth < 32; ++Depth)
				{
					if (!At || (At & 7u) || At < Sp || At - Sp > (64u << 20))
					{
						break;
					}
					const uintptr_t* Pair = reinterpret_cast<const uintptr_t*>(At);
					const uintptr_t Up = Pair[0], Back = Pair[1];
					if (!Back)
					{
						break;
					}
					Frame(T, "from      ", Back);
					if (Up <= At)
					{
						break;
					}
					At = Up;
				}
			}
			T.Put("=== (the game's own crash report follows) ===\n");
			if (Out >= 0)
			{
				ssize_t Wrote = write(Out, T.Buffer, T.Used);
				(void)Wrote;
				fsync(Out);
			}
			ssize_t Told = write(2, T.Buffer, T.Used);
			(void)Told;
		}
#endif
	}

	void Phase(const char* Where, int Detail)
	{
		const unsigned K = RingNext.load(std::memory_order_relaxed);
		Ring[K % RingSize].store(Where, std::memory_order_relaxed);
		RingDetail[K % RingSize].store(Detail, std::memory_order_relaxed);
		RingNext.store(K + 1, std::memory_order_relaxed);
		if (char* Lines = Mapped)
		{
			PutLine(Lines + (K % RingSize) * RingLine, K, Where, Detail);
		}
	}

	bool Arm(const char* File, const char* RingFile)
	{
#if LAND_FELL_SIGNALS
		if (bArmed)
		{
			return true;
		}
		RingOut = RingFile && *RingFile ? open(RingFile, O_RDWR | O_CREAT | O_TRUNC, 0644) : -1;
		if (RingOut >= 0 && ftruncate(RingOut, static_cast<off_t>(RingSize * RingLine)) == 0)
		{
			void* Page = mmap(nullptr, RingSize * RingLine, PROT_READ | PROT_WRITE, MAP_SHARED, RingOut, 0);
			if (Page != MAP_FAILED)
			{
				std::memset(Page, ' ', RingSize * RingLine);
				for (unsigned K = 0; K < RingSize; ++K)
				{
					static_cast<char*>(Page)[K * RingLine + RingLine - 1] = '\n';
				}
				Mapped = static_cast<char*>(Page);
			}
		}
		Out = File && *File ? open(File, O_WRONLY | O_CREAT | O_TRUNC, 0644) : -1;
		if (Out >= 0)
		{
			static const char Head[] = "The ground battle's crash locator was on for this run; nothing below this line means it did not fall over.\n";
			ssize_t Wrote = write(Out, Head, sizeof(Head) - 1);
			(void)Wrote;
		}
		struct sigaction Mine;
		std::memset(&Mine, 0, sizeof(Mine));
		Mine.sa_sigaction = &OnFault;
		Mine.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_NODEFER;
		sigemptyset(&Mine.sa_mask);
		for (int K = 0; K < SignalCount; ++K)
		{
			sigaction(Signals[K], &Mine, &Before[K]);
		}
		bArmed = true;
		return true;
#else
		(void)File;
		(void)RingFile;
		return false;
#endif
	}

	void Disarm()
	{
#if LAND_FELL_SIGNALS
		if (!bArmed)
		{
			return;
		}
		for (int K = 0; K < SignalCount; ++K)
		{
			sigaction(Signals[K], &Before[K], nullptr);
		}
		bArmed = false;
		if (Out >= 0)
		{
			close(Out);
			Out = -1;
		}
		if (Mapped)
		{
			char* Was = Mapped;
			Mapped = nullptr;
			munmap(Was, RingSize * RingLine);
		}
		if (RingOut >= 0)
		{
			close(RingOut);
			RingOut = -1;
		}
#endif
	}

	bool Armed()
	{
#if LAND_FELL_SIGNALS
		return bArmed;
#else
		return false;
#endif
	}
}
