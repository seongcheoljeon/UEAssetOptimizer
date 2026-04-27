// Copyright (c) 2026 Seongcheol Jeon. Licensed under MIT.
#pragma once

#include "CoreMinimal.h"

class UScriptStruct;

namespace UEAOpt
{
	/**
	 * Internal: open a modal property dialog editing a USTRUCT instance in
	 * place. Caller passes the struct's UScriptStruct* and a raw pointer to
	 * the instance memory. Returns true if the user clicked OK; false on
	 * Cancel or window close (X). Modifies the instance in place if OK.
	 *
	 * Use the templated wrapper below from call sites for type safety.
	 */
	bool ShowModalParamDialog_Internal(const FText& Title, const UScriptStruct* StructDef, uint8* Data);

	/**
	 * Open a modal dialog letting the user edit a USTRUCT via UE's standard
	 * property editor. Auto-renders fields, enum dropdowns, ClampMin/Max,
	 * EditCondition, etc. directly from UPROPERTY metadata on the struct.
	 *
	 * Sized for typical param structs (~420 x 360). Game-thread only.
	 */
	template <typename TStruct>
	inline bool ShowModalParamDialog(const FText& Title, TStruct& Params)
	{
		return ShowModalParamDialog_Internal(
			Title,
			TStruct::StaticStruct(),
			reinterpret_cast<uint8*>(&Params));
	}
}
