#include "Rules/RulesHost.h"

#include "TheaterCommand.h"
#include "Misc/FileHelper.h"

#if PLATFORM_MAC

// Apple's own headers have a type called FVector, as Unreal does. Unreal's usual remedy is to rename
// Apple's for the length of the include.
THIRD_PARTY_INCLUDES_START
#define FVector FVectorWorkaround
#include <JavaScriptCore/JavaScript.h>
#undef FVector
THIRD_PARTY_INCLUDES_END

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

struct FRulesHost::FImpl
{
	JSGlobalContextRef Context = nullptr;
	JSObjectRef Dispatcher = nullptr;
};

FRulesHost::FRulesHost()
	: Impl(MakeUnique<FImpl>())
{
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
	return Impl->Context != nullptr && Impl->Dispatcher != nullptr;
}

bool FRulesHost::Evaluate(const FString& Source, const FString& SourceName, FString& OutError)
{
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
	if (!IsAvailable())
	{
		OutError = TEXT("The rules engine did not start");
		return false;
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
