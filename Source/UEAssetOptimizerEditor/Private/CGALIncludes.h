// Copyright (c) 2026 Seongcheol Jeon. Licensed under MIT.
//
// Isolation header for CGAL + Boost. Unreal's core defines macros (check, TEXT,
// PI, dynamic_cast) that collide with Boost/CGAL internals. Always include CGAL
// exclusively through this header.
//
// Pattern references:
//   - Unreal docs: "Including Third Party Libraries"
//   - Community: ValentinKraft/Boost_PCL_UnrealThirdPartyPlugin
#pragma once

#include "CoreMinimal.h"

// Save & remove conflicting UE macros before pulling in Boost/CGAL.
#pragma push_macro("check")
#pragma push_macro("TEXT")
#pragma push_macro("PI")
#pragma push_macro("dynamic_cast")
#undef check
#undef TEXT
#undef PI
#undef dynamic_cast

// Silence MSVC warnings triggered by CGAL/Boost template metaprogramming.
// THIRD_PARTY_INCLUDES_START pushes a warning stack and disables a curated
// set, but several warnings that fire from CGAL's heavy templates aren't on
// that list — surface them here surgically. The matching pop is implicit
// via THIRD_PARTY_INCLUDES_END.
THIRD_PARTY_INCLUDES_START

#if defined(_MSC_VER)
#pragma warning(disable: 4701) // potentially uninitialized local variable (CGAL remesh_impl.h)
#pragma warning(disable: 4702) // unreachable code
#pragma warning(disable: 4127) // conditional expression is constant (heavy template idioms)
#pragma warning(disable: 4459) // declaration hides global declaration
#pragma warning(disable: 4189) // local variable initialized but not referenced
#endif

// Threading: CGAL can use Boost threads; we disable parallelism for simpler
// single-threaded calls. Revisit in Sprint 2+ for perf.
#define CGAL_NO_GMP_FLOATING_POINT 0

#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Surface_mesh.h>
#include <CGAL/alpha_wrap_3.h>
#include <CGAL/Polygon_mesh_processing/IO/polygon_mesh_io.h>
#include <CGAL/Polygon_mesh_processing/bbox.h>
#include <CGAL/Polygon_mesh_processing/remesh.h>

THIRD_PARTY_INCLUDES_END

// Restore UE macros.
#pragma pop_macro("dynamic_cast")
#pragma pop_macro("PI")
#pragma pop_macro("TEXT")
#pragma pop_macro("check")

// Project-wide CGAL type aliases.
namespace UEAOpt
{
	using CGALKernel       = CGAL::Exact_predicates_inexact_constructions_kernel;
	using CGALPoint3       = CGALKernel::Point_3;
	using CGALSurfaceMesh  = CGAL::Surface_mesh<CGALPoint3>;
}
