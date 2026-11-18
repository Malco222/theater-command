#include "Rules/RulesHost.h"

#include "TheaterCommand.h"
#include "Misc/FileHelper.h"
#include "Misc/ScopeLock.h"

#if PLATFORM_MAC

// Apple's own headers have a type called FVector, as Unreal does. Unreal's usual remedy is to rename
// Apple's for the length of the include.
THIRD_PARTY_INCLUDES_START
#define FVector FVectorWorkaround
#include <JavaScriptCore/JavaScript.h>
#undef FVector
#include <crt_externs.h>
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
THIRD_PARTY_INCLUDES_END

#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

namespace
{
	FString ToFString(JSStringRef String)
	{
		if (!String)
		{
			return FString();
		}
		const size_t Length = JSStringGetLength(String);
		const JSChar* Chars = JSStringGetCharactersPtr(String);
		return FString(static_cast<int32>(Length), reinterpret_cast<const UTF16CHAR*>(Chars));
	}

	JSStringRef ToJSString(const FString& String)
	{
		const FTCHARToUTF16 Utf16(*String, String.Len());
		return JSStringCreateWithCharacters(reinterpret_cast<const JSChar*>(Utf16.Get()), Utf16.Length());
	}

	FString ValueToString(JSContextRef Context, JSValueRef Value)
	{
		if (!Value)
		{
			return FString();
		}
		JSStringRef String = JSValueToStringCopy(Context, Value, nullptr);
		const FString Out = ToFString(String);
		if (String)
		{
			JSStringRelease(String);
		}
		return Out;
	}

	/** Turns a thrown JavaScript error into one readable line, with where it happened when the engine knows. */
	FString DescribeException(JSContextRef Context, JSValueRef Exception)
	{
		FString Text = ValueToString(Context, Exception);
		if (JSValueIsObject(Context, Exception))
		{
			JSObjectRef Object = JSValueToObject(Context, Exception, nullptr);
			auto Property = [&](const char* Name)
			{
				JSStringRef Key = JSStringCreateWithUTF8CString(Name);
				JSValueRef Value = Object ? JSObjectGetProperty(Context, Object, Key, nullptr) : nullptr;
				JSStringRelease(Key);
				return (Value && !JSValueIsUndefined(Context, Value)) ? ValueToString(Context, Value) : FString();
			};
			const FString Line = Property("line");
			const FString Stack = Property("stack");
			if (!Line.IsEmpty())
			{
				Text += FString::Printf(TEXT(" (line %s)"), *Line);
			}
			if (!Stack.IsEmpty())
			{
				Text += TEXT("\n") + Stack;
			}
		}
		return Text;
	}

	/** console.log and friends, sent to Unreal's log. */
	JSValueRef LogCallback(JSContextRef Context, JSObjectRef, JSObjectRef, size_t ArgumentCount, const JSValueRef Arguments[], JSValueRef*)
	{
		FString Line;
		for (size_t i = 0; i < ArgumentCount; ++i)
		{
			if (i > 0)
			{
				Line += TEXT(" ");
			}
			Line += ValueToString(Context, Arguments[i]);
		}
		UE_LOG(LogTheater, Display, TEXT("Rules: %s"), *Line);
		return JSValueMakeUndefined(Context);
	}
}

/**
 * The rules in a helper of their own. (Brian, 17 November 2026: "how do we make the game run more smooth".)
 *
 * The Mac lets a program compile JavaScript as it runs only if the program is signed with leave to (the entitlement
 * com.apple.security.cs.allow-jit). The Unreal editor the game runs in is not, so inside the game the rules were read
 * a line at a time: a march's answer took 82 ms and a month's end 1.2 s, where the same calls take 11 ms and 0.2 s
 * compiled. The Mac's own JavaScript shell (jsc, part of the system's JavaScriptCore) has that leave. So the rules
 * run in it, as a child of the game, and the game talks to it down a pipe: one line in, one line out, all of it
 * plain ASCII (anything else goes as \uXXXX inside its JSON string). Nothing about the game's signature or the
 * Mac's settings is touched. Where the helper cannot be started the rules run inside the game as they always did;
 * "-TheaterRulesInside" asks for that.
 */
namespace
{
	const TCHAR* const HelperDoorway = TEXT(
		"var __ascii = function (s) { return s.replace(/[\\u007f-\\uffff]/g, function (c) { return '\\\\u' + ('0000' + c.charCodeAt(0).toString(16)).slice(-4); }); };\n"
		"var __say = function () { print('#' + __ascii(Array.prototype.slice.call(arguments).join(' ').split('\\n').join(' | '))); };\n"
		"globalThis.console = { log: __say, info: __say, warn: __say, error: __say, debug: __say };\n"
		"for (;;) {\n"
		"  var __line = readline();\n"
		"  if (__line === undefined || __line === null || __line === '') break;\n"
		"  var __kind = __line.charAt(0), __rest = __line.slice(1), __out;\n"
		"  try {\n"
		"    if (__kind === 'L') { load(JSON.parse(__rest)); __out = 'null'; }\n"
		"    else if (__kind === 'E') { (0, eval)(JSON.parse(__rest)); __out = 'null'; }\n"
		"    else {\n"
		"      var __at = __rest.indexOf('\\t'), __name = __at < 0 ? __rest : __rest.slice(0, __at), __args = __at < 0 ? '' : __rest.slice(__at + 1);\n"
		"      if (typeof TC !== 'object' || typeof TC[__name] !== 'function') throw new Error('The rules have no function called ' + __name);\n"
		"      var __r = TC[__name].apply(TC, __args ? JSON.parse(__args) : []);\n"
		"      __out = __r === undefined ? 'null' : JSON.stringify(__r);\n"
		"    }\n"
		"    print('=' + __ascii(__out));\n"
		"  } catch (__e) {\n"
		"    print('!' + __ascii(String(__e) + (__e && __e.line ? ' (line ' + __e.line + ')' : '') + (__e && __e.stack ? ' | ' + String(__e.stack).split('\\n').join(' | ') : '')));\n"
		"  }\n"
		"}\n");

	/** Text for the helper's one line: a JSON string of it, all in ASCII. */
	FString QuotedAscii(const FString& Text)
	{
		FString Out;
		Out.Reserve(Text.Len() + 16);
		Out += TEXT("\"");
		for (const TCHAR Char : Text)
		{
			if (Char == TEXT('"') || Char == TEXT('\\'))
			{
				Out += TEXT('\\');
				Out += Char;
			}
			else if (Char < 0x20 || Char > 0x7E)
			{
				Out += FString::Printf(TEXT("\\u%04x"), static_cast<uint32>(static_cast<uint16>(Char)));
			}
			else
			{
				Out += Char;
			}
		}
		Out += TEXT("\"");
		return Out;
	}

	/**
	 * JSON that is already JSON, made safe for one ASCII line. Anything outside ASCII can only stand inside a string
	 * of it, where \uXXXX says the same; a line break or a tab can only be the space between its parts.
	 */
	FString JsonOnOneAsciiLine(const FString& Json)
	{
		FString Out;
		Out.Reserve(Json.Len() + 16);
		for (const TCHAR Char : Json)
		{
			if (Char > 0x7E)
			{
				Out += FString::Printf(TEXT("\\u%04x"), static_cast<uint32>(static_cast<uint16>(Char)));
			}
			else if (Char < 0x20)
			{
				Out += TEXT(' ');
			}
			else
			{
				Out += Char;
			}
		}
		return Out;
	}
}

struct FRulesHost::FImpl
{
	JSGlobalContextRef Context = nullptr;
	JSObjectRef Dispatcher = nullptr;

	// The helper, where there is one: its process, the pipe to it and the pipe from it, and what has been read and not yet used.
	pid_t Helper = 0;
	int ToHelper = -1, FromHelper = -1;
	TArray<uint8> Pending;

	bool HasHelper() const { return Helper > 0 && ToHelper >= 0 && FromHelper >= 0; }

	bool StartHelper()
	{
		if (FParse::Param(FCommandLine::Get(), TEXT("TheaterRulesInside")))
		{
			return false;
		}
		FString Shell;
		for (const TCHAR* Each : { TEXT("/System/Library/Frameworks/JavaScriptCore.framework/Versions/Current/Helpers/jsc"), TEXT("/System/Library/Frameworks/JavaScriptCore.framework/Helpers/jsc") })
		{
			if (Shell.IsEmpty() && IFileManager::Get().FileExists(Each))
			{
				Shell = Each;
			}
		}
		if (Shell.IsEmpty())
		{
			return false;
		}
		const FString Doorway = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectIntermediateDir(), TEXT("Rules"), TEXT("doorway.js")));
		if (!FFileHelper::SaveStringToFile(FString(HelperDoorway), *Doorway, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			return false;
		}
		int In[2] = { -1, -1 }, Out[2] = { -1, -1 };
		if (pipe(In) != 0)
		{
			return false;
		}
		if (pipe(Out) != 0)
		{
			close(In[0]);
			close(In[1]);
			return false;
		}
		posix_spawn_file_actions_t Actions;
		posix_spawn_file_actions_init(&Actions);
		posix_spawn_file_actions_adddup2(&Actions, In[0], STDIN_FILENO);
		posix_spawn_file_actions_adddup2(&Actions, Out[1], STDOUT_FILENO);
		posix_spawn_file_actions_addopen(&Actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
		// (the helper is handed its two pipes and nothing else of the game's: no open file, no socket)
		posix_spawnattr_t Attributes;
		posix_spawnattr_init(&Attributes);
		posix_spawnattr_setflags(&Attributes, POSIX_SPAWN_CLOEXEC_DEFAULT);
		const FTCHARToUTF8 ShellPath(*Shell), DoorwayPath(*Doorway);
		char* const Arguments[] = { const_cast<char*>(ShellPath.Get()), const_cast<char*>(DoorwayPath.Get()), nullptr };
		pid_t Child = 0;
		const int Failed = posix_spawn(&Child, ShellPath.Get(), &Actions, &Attributes, Arguments, *_NSGetEnviron());
		posix_spawnattr_destroy(&Attributes);
		posix_spawn_file_actions_destroy(&Actions);
		close(In[0]);
		close(Out[1]);
		if (Failed != 0 || Child <= 0)
		{
			close(In[1]);
			close(Out[0]);
			return false;
		}
		Helper = Child;
		ToHelper = In[1];
		FromHelper = Out[0];
		fcntl(ToHelper, F_SETNOSIGPIPE, 1); // (a helper that has gone is an error to report, never a signal that ends the game)
		fcntl(ToHelper, F_SETFD, FD_CLOEXEC);
		fcntl(FromHelper, F_SETFD, FD_CLOEXEC);
		// One question to be sure it is there and answering.
		FString Answer, Error;
		if (!Ask(TEXT("E") + QuotedAscii(TEXT("1")), Answer, Error))
		{
			StopHelper();
			return false;
		}
		return true;
	}

	void StopHelper()
	{
		if (ToHelper >= 0)
		{
			close(ToHelper); // (its line ends: the helper reads that and goes)
			ToHelper = -1;
		}
		if (FromHelper >= 0)
		{
			close(FromHelper);
			FromHelper = -1;
		}
		if (Helper > 0)
		{
			int Status = 0;
			for (int32 Try = 0; Try < 50 && waitpid(Helper, &Status, WNOHANG) == 0; ++Try)
			{
				usleep(2000);
				if (Try == 25)
				{
					kill(Helper, SIGKILL);
				}
			}
			Helper = 0;
		}
		Pending.Reset();
	}

	/** One line to the helper and its answer back. Log lines that come first are passed on to Unreal's log. */
	bool Ask(const FString& Line, FString& OutAnswer, FString& OutError)
	{
		if (!HasHelper())
		{
			OutError = TEXT("The rules' helper is not running");
			return false;
		}
		{
			const FTCHARToUTF8 Bytes(*Line, Line.Len()); // (all ASCII by now)
			const char NewLine = '\n';
			int32 Sent = 0;
			while (Sent < Bytes.Length())
			{
				const ssize_t Wrote = write(ToHelper, Bytes.Get() + Sent, Bytes.Length() - Sent);
				if (Wrote <= 0)
				{
					if (Wrote < 0 && errno == EINTR)
					{
						continue;
					}
					OutError = TEXT("The rules' helper has stopped (the pipe to it is closed)");
					StopHelper();
					return false;
				}
				Sent += static_cast<int32>(Wrote);
			}
			if (write(ToHelper, &NewLine, 1) != 1)
			{
				OutError = TEXT("The rules' helper has stopped (the pipe to it is closed)");
				StopHelper();
				return false;
			}
		}
		for (;;)
		{
			// A whole line already read?
			int32 End = INDEX_NONE;
			if (Pending.Find(static_cast<uint8>('\n'), End))
			{
				const FString Got = FString::ConstructFromPtrSize(reinterpret_cast<const ANSICHAR*>(Pending.GetData()), End);
				Pending.RemoveAt(0, End + 1, EAllowShrinking::No);
				if (Got.StartsWith(TEXT("#")))
				{
					UE_LOG(LogTheater, Display, TEXT("Rules: %s"), *Got.Mid(1));
					continue;
				}
				if (Got.StartsWith(TEXT("=")))
				{
					OutAnswer = Got.Mid(1);
					return true;
				}
				OutError = Got.StartsWith(TEXT("!")) ? Got.Mid(1) : Got;
				return false;
			}
			uint8 Buffer[65536];
			const ssize_t Read = read(FromHelper, Buffer, sizeof(Buffer));
			if (Read > 0)
			{
				Pending.Append(Buffer, static_cast<int32>(Read));
			}
			else if (Read < 0 && errno == EINTR)
			{
				continue;
			}
			else
			{
				OutError = TEXT("The rules' helper has stopped (it ended in the middle of an answer)");
				StopHelper();
				return false;
			}
		}
	}
};

FRulesHost::FRulesHost()
	: Impl(MakeUnique<FImpl>())
{
	if (Impl->StartHelper())
	{
		UE_LOG(LogTheater, Display, TEXT("Rules: running in the Mac's own JavaScript shell, compiled as they run (helper process %d)"), static_cast<int32>(Impl->Helper));
		return;
	}
	UE_LOG(LogTheater, Display, TEXT("Rules: running inside the game, read a line at a time (no helper)"));
	Impl->Context = JSGlobalContextCreate(nullptr);
	JSObjectRef Global = JSContextGetGlobalObject(Impl->Context);

	// A console, so the rules can say what they are doing.
	{
		JSStringRef LogName = JSStringCreateWithUTF8CString("__log");
		JSObjectSetProperty(Impl->Context, Global, LogName, JSObjectMakeFunctionWithCallback(Impl->Context, LogName, &LogCallback), kJSPropertyAttributeNone, nullptr);
		JSStringRelease(LogName);
	}

	// The one doorway in: call a rule by name with JSON arguments, get JSON back.
	FString Error;
	Evaluate(TEXT(
		"globalThis.console = { log: __log, info: __log, warn: __log, error: __log, debug: __log };\n"
		"globalThis.__call = function (name, argumentsJson) {\n"
		"  if (typeof TC !== 'object' || typeof TC[name] !== 'function') throw new Error('The rules have no function called ' + name);\n"
		"  var out = TC[name].apply(TC, argumentsJson ? JSON.parse(argumentsJson) : []);\n"
		"  return out === undefined ? 'null' : JSON.stringify(out);\n"
		"};\n"), TEXT("doorway"), Error);

	JSStringRef CallName = JSStringCreateWithUTF8CString("__call");
	JSValueRef Dispatcher = JSObjectGetProperty(Impl->Context, Global, CallName, nullptr);
	JSStringRelease(CallName);
	if (Dispatcher && JSValueIsObject(Impl->Context, Dispatcher))
	{
		Impl->Dispatcher = JSValueToObject(Impl->Context, Dispatcher, nullptr);
		JSValueProtect(Impl->Context, Impl->Dispatcher);
	}
}

FRulesHost::~FRulesHost()
{
	Impl->StopHelper();
	if (Impl->Context)
	{
		if (Impl->Dispatcher)
		{
			JSValueUnprotect(Impl->Context, Impl->Dispatcher);
		}
		JSGlobalContextRelease(Impl->Context);
	}
}

bool FRulesHost::IsAvailable() const
{
	return Impl->HasHelper() || (Impl->Context != nullptr && Impl->Dispatcher != nullptr);
}

bool FRulesHost::Evaluate(const FString& Source, const FString& SourceName, FString& OutError)
{
	FScopeLock Hold(&Gate);
	if (Impl->HasHelper())
	{
		FString Answer, Error;
		if (!Impl->Ask(TEXT("E") + QuotedAscii(Source), Answer, Error))
		{
			OutError = FString::Printf(TEXT("%s: %s"), *SourceName, *Error);
			return false;
		}
		return true;
	}
	if (!Impl->Context)
	{
		OutError = TEXT("The rules engine did not start");
		return false;
	}
	JSStringRef Script = ToJSString(Source);
	JSStringRef Name = ToJSString(SourceName);
	JSValueRef Exception = nullptr;
	JSEvaluateScript(Impl->Context, Script, nullptr, Name, 1, &Exception);
	JSStringRelease(Script);
	JSStringRelease(Name);
	if (Exception)
	{
		OutError = FString::Printf(TEXT("%s: %s"), *SourceName, *DescribeException(Impl->Context, Exception));
		return false;
	}
	return true;
}

bool FRulesHost::LoadScript(const FString& Path, FString& OutError)
{
	FScopeLock Hold(&Gate);
	if (Impl->HasHelper())
	{
		// (the helper reads the file itself)
		FString Answer, Error;
		if (!Impl->Ask(TEXT("L") + QuotedAscii(FPaths::ConvertRelativePathToFull(Path)), Answer, Error))
		{
			OutError = FString::Printf(TEXT("%s: %s"), *FPaths::GetCleanFilename(Path), *Error);
			return false;
		}
		return true;
	}
	FString Source;
	if (!FFileHelper::LoadFileToString(Source, *Path))
	{
		OutError = FString::Printf(TEXT("Could not read %s"), *Path);
		return false;
	}
	return Evaluate(Source, FPaths::GetCleanFilename(Path), OutError);
}

bool FRulesHost::Call(const FString& Function, const FString& ArgumentsJson, FString& OutResultJson, FString& OutError)
{
	FScopeLock Hold(&Gate);
	if (!IsAvailable())
	{
		OutError = TEXT("The rules engine did not start");
		return false;
	}
	if (Impl->HasHelper())
	{
		FString Error;
		if (!Impl->Ask(TEXT("C") + Function + TEXT("\t") + JsonOnOneAsciiLine(ArgumentsJson), OutResultJson, Error))
		{
			OutError = FString::Printf(TEXT("%s: %s"), *Function, *Error);
			return false;
		}
		return true;
	}
	JSStringRef Name = ToJSString(Function);
	JSStringRef Arguments = ToJSString(ArgumentsJson);
	const JSValueRef Parameters[2] = { JSValueMakeString(Impl->Context, Name), JSValueMakeString(Impl->Context, Arguments) };
	JSValueRef Exception = nullptr;
	JSValueRef Result = JSObjectCallAsFunction(Impl->Context, Impl->Dispatcher, nullptr, 2, Parameters, &Exception);
	JSStringRelease(Name);
	JSStringRelease(Arguments);
	if (Exception || !Result)
	{
		OutError = FString::Printf(TEXT("%s: %s"), *Function, Exception ? *DescribeException(Impl->Context, Exception) : TEXT("no result"));
		return false;
	}
	OutResultJson = ValueToString(Impl->Context, Result);
	return true;
}

#else // no JavaScript engine wired up for this platform yet

struct FRulesHost::FImpl
{
};

FRulesHost::FRulesHost() : Impl(MakeUnique<FImpl>()) {}
FRulesHost::~FRulesHost() = default;
bool FRulesHost::IsAvailable() const { return false; }
bool FRulesHost::Evaluate(const FString&, const FString&, FString& OutError) { OutError = TEXT("No JavaScript engine on this platform yet"); return false; }
bool FRulesHost::LoadScript(const FString&, FString& OutError) { OutError = TEXT("No JavaScript engine on this platform yet"); return false; }
bool FRulesHost::Call(const FString&, const FString&, FString&, FString& OutError) { OutError = TEXT("No JavaScript engine on this platform yet"); return false; }

#endif
