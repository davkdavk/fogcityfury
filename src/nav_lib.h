// Recast & Detour (third_party/recastnavigation, zlib licence, (c) 2009 Mikko Mononen and contributors), built into the
// game's one translation unit. The sources are unchanged; three of them have file-local helpers of the same name, so
// those are given their own names here while each file is read.
#pragma once
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif
#include "../third_party/recastnavigation/Recast.h"
#include "../third_party/recastnavigation/RecastAlloc.h"
#include "../third_party/recastnavigation/RecastAssert.h"
#include "../third_party/recastnavigation/DetourNavMesh.h"
#include "../third_party/recastnavigation/DetourNavMeshBuilder.h"
#include "../third_party/recastnavigation/DetourNavMeshQuery.h"
#include "../third_party/recastnavigation/DetourCommon.h"
#include "../third_party/recastnavigation/Recast.cpp"
#include "../third_party/recastnavigation/RecastAlloc.cpp"
#include "../third_party/recastnavigation/RecastArea.cpp"
#include "../third_party/recastnavigation/RecastAssert.cpp"
#include "../third_party/recastnavigation/RecastContour.cpp"
#include "../third_party/recastnavigation/RecastFilter.cpp"
#include "../third_party/recastnavigation/RecastLayers.cpp"
#define prev rcM_prev
#define next rcM_next
#define area2 rcM_area2
#define left rcM_left
#define leftOn rcM_leftOn
#define collinear rcM_collinear
#define intersectProp rcM_intersectProp
#define between rcM_between
#define intersect rcM_intersect
#define vequal rcM_vequal
#include "../third_party/recastnavigation/RecastMesh.cpp"
#undef prev
#undef next
#undef area2
#undef left
#undef leftOn
#undef collinear
#undef intersectProp
#undef between
#undef intersect
#undef vequal
#define prev rcD_prev
#define next rcD_next
#include "../third_party/recastnavigation/RecastMeshDetail.cpp"
#undef prev
#undef next
#include "../third_party/recastnavigation/RecastRasterization.cpp"
#include "../third_party/recastnavigation/RecastRegion.cpp"
#include "../third_party/recastnavigation/DetourAlloc.cpp"
#include "../third_party/recastnavigation/DetourAssert.cpp"
#include "../third_party/recastnavigation/DetourCommon.cpp"
#include "../third_party/recastnavigation/DetourNavMesh.cpp"
#include "../third_party/recastnavigation/DetourNavMeshBuilder.cpp"
#include "../third_party/recastnavigation/DetourNavMeshQuery.cpp"
#include "../third_party/recastnavigation/DetourNode.cpp"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
