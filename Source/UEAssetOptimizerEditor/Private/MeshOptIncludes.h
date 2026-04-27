// Copyright (c) 2026 Seongcheol Jeon. Licensed under MIT.
//
// Thin include wrapper for meshoptimizer. Unlike CGALIncludes.h this does
// *not* need macro isolation: meshoptimizer is a small pure-C-style library
// that does not collide with UE core macros (`check`, `TEXT`, `PI`, etc).
// The wrapper exists purely for consistency so consumers have one canonical
// include path per third-party dependency.
#pragma once

#include "meshoptimizer.h"
