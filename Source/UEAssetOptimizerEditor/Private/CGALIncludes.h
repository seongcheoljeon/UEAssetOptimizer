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
THIRD_PARTY_INCLUDES_START

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
