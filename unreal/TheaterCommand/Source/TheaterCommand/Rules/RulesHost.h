#pragma once

#include "CoreMinimal.h"

/**
 * Runs the game's rules.
 *
 * The rules of the campaign were worked out and tested in the browser
 * prototype, in TypeScript. They are not rewritten for Unreal: the very same
 * code is bundled into one JavaScript file and run here, inside the game, by a
 * JavaScript engine. So there is one set of rules, one set of tests, and the
 * 3D game cannot drift away from them.
 *
 * Everything crosses this boundary as JSON text: the game asks a question or
 * gives an order by name, with its arguments as JSON, and gets JSON back. That
 * keeps the two sides independent, and it means the engine underneath can be
 * swapped (the Mac's own JavaScriptCore today; a portable one for Windows
 * later) without anything else changing.
 */
class FRulesHost
{
public:
	FRulesHost();
	~FRulesHost();

	FRulesHost(const FRulesHost&) = delete;
	FRulesHost& operator=(const FRulesHost&) = delete;

	/** True when this build has a JavaScript engine to run the rules on. */
	bool IsAvailable() const;

	/** Runs a script file. The rules bundle defines a global object called TC whose functions are the game's rules. */
	bool LoadScript(const FString& Path, FString& OutError);

	/** Runs a piece of script text. Used to hand data to the rules before they start. */
	bool Evaluate(const FString& Source, const FString& SourceName, FString& OutError);

	/**
	 * Calls TC.<Function>(...Arguments) and returns what it returned, as JSON text.
	 * ArgumentsJson is a JSON array, e.g. TEXT("[12, \"Warsaw\"]"), or empty for no arguments.
	 */
	bool Call(const FString& Function, const FString& ArgumentsJson, FString& OutResultJson, FString& OutError);

private:
	struct FImpl;
	TUniquePtr<FImpl> Impl;
};
