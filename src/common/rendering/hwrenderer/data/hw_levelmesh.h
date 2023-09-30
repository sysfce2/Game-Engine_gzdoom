
#pragma once

#include "tarray.h"
#include "vectors.h"
#include "hw_collision.h"
#include "bounds.h"
#include "common/utility/matrix.h"
#include <memory>
#include <cstring>
#include "textureid.h"

#include <dp_rect_pack.h>


typedef dp::rect_pack::RectPacker<int> RectPacker;

class LevelMeshLight
{
public:
	FVector3 Origin;
	FVector3 RelativeOrigin;
	float Radius;
	float Intensity;
	float InnerAngleCos;
	float OuterAngleCos;
	FVector3 SpotDir;
	FVector3 Color;
	int SectorGroup;
};

struct LevelMeshSurface
{
	int numVerts;
	unsigned int startVertIndex;
	unsigned int startUvIndex;
	unsigned int startElementIndex;
	unsigned int numElements;
	FVector4 plane;
	bool bSky;

	// Surface location in lightmap texture
	struct
	{
		int X = 0;
		int Y = 0;
		int Width = 0;
		int Height = 0;
		int ArrayIndex = 0;
	} AtlasTile;

	// True if the surface needs to be rendered into the lightmap texture before it can be used
	bool needsUpdate = true;

	//
	// Required for internal lightmapper:
	//

	FTextureID texture = FNullTextureID();
	float alpha = 1.0;
	
	int portalIndex = 0;
	int sectorGroup = 0;

	BBox bounds;
	uint16_t sampleDimension = 0;

	// Calculate world coordinates to UV coordinates
	FVector3 translateWorldToLocal = { 0.f, 0.f, 0.f };
	FVector3 projLocalToU = { 0.f, 0.f, 0.f };
	FVector3 projLocalToV = { 0.f, 0.f, 0.f };

	// Smoothing group surface is to be rendered with
	int smoothingGroupIndex = -1;

	// Surfaces that are visible within the lightmap tile
	TArray<LevelMeshSurface*> tileSurfaces;

	//
	// Utility/Info
	//
	inline uint32_t Area() const { return AtlasTile.Width * AtlasTile.Height; }

	int LightListPos = -1;
	int LightListCount = 0;
	int LightListResetCounter = -1;
};

inline float IsInFrontOfPlane(const FVector4& plane, const FVector3& point)
{
	return (plane.X * point.X + plane.Y * point.Y + plane.Z * point.Z) >= plane.W;
}

struct LevelMeshSmoothingGroup
{
	FVector4 plane = FVector4(0, 0, 1, 0);
	int sectorGroup = 0;
	std::vector<LevelMeshSurface*> surfaces;
};

struct LevelMeshPortal
{
	LevelMeshPortal() { transformation.loadIdentity(); }

	VSMatrix transformation;

	int sourceSectorGroup = 0;
	int targetSectorGroup = 0;

	inline FVector3 TransformPosition(const FVector3& pos) const
	{
		auto v = transformation * FVector4(pos, 1.0);
		return FVector3(v.X, v.Y, v.Z);
	}

	inline FVector3 TransformRotation(const FVector3& dir) const
	{
		auto v = transformation * FVector4(dir, 0.0);
		return FVector3(v.X, v.Y, v.Z);
	}

	// Checks only transformation
	inline bool IsInverseTransformationPortal(const LevelMeshPortal& portal) const
	{
		auto diff = portal.TransformPosition(TransformPosition(FVector3(0, 0, 0)));
		return abs(diff.X) < 0.001 && abs(diff.Y) < 0.001 && abs(diff.Z) < 0.001;
	}

	// Checks only transformation
	inline bool IsEqualTransformationPortal(const LevelMeshPortal& portal) const
	{
		auto diff = portal.TransformPosition(FVector3(0, 0, 0)) - TransformPosition(FVector3(0, 0, 0));
		return (abs(diff.X) < 0.001 && abs(diff.Y) < 0.001 && abs(diff.Z) < 0.001);
	}

	// Checks transformation, source and destiantion sector groups
	inline bool IsEqualPortal(const LevelMeshPortal& portal) const
	{
		return sourceSectorGroup == portal.sourceSectorGroup && targetSectorGroup == portal.targetSectorGroup && IsEqualTransformationPortal(portal);
	}

	// Checks transformation, source and destiantion sector groups
	inline bool IsInversePortal(const LevelMeshPortal& portal) const
	{
		return sourceSectorGroup == portal.targetSectorGroup && targetSectorGroup == portal.sourceSectorGroup && IsInverseTransformationPortal(portal);
	}
};

// for use with std::set to recursively go through portals and skip returning portals
struct RecursivePortalComparator
{
	bool operator()(const LevelMeshPortal& a, const LevelMeshPortal& b) const
	{
		return !a.IsInversePortal(b) && std::memcmp(&a.transformation, &b.transformation, sizeof(VSMatrix)) < 0;
	}
};

// for use with std::map to reject portals which have the same effect for light rays
struct IdenticalPortalComparator
{
	bool operator()(const LevelMeshPortal& a, const LevelMeshPortal& b) const
	{
		return !a.IsEqualPortal(b) && std::memcmp(&a.transformation, &b.transformation, sizeof(VSMatrix)) < 0;
	}
};

struct LevelMeshSurfaceStats
{
	struct Stats
	{
		uint32_t total = 0, dirty = 0, sky = 0;
	};

	Stats surfaces, pixels;
};

class LevelSubmesh
{
public:
	LevelSubmesh()
	{
		// Default portal
		LevelMeshPortal portal;
		Portals.Push(portal);

		// Default empty mesh (we can't make it completely empty since vulkan doesn't like that)
		float minval = -100001.0f;
		float maxval = -100000.0f;
		MeshVertices.Push({ minval, minval, minval });
		MeshVertices.Push({ maxval, minval, minval });
		MeshVertices.Push({ maxval, maxval, minval });
		MeshVertices.Push({ minval, minval, minval });
		MeshVertices.Push({ minval, maxval, minval });
		MeshVertices.Push({ maxval, maxval, minval });
		MeshVertices.Push({ minval, minval, maxval });
		MeshVertices.Push({ maxval, minval, maxval });
		MeshVertices.Push({ maxval, maxval, maxval });
		MeshVertices.Push({ minval, minval, maxval });
		MeshVertices.Push({ minval, maxval, maxval });
		MeshVertices.Push({ maxval, maxval, maxval });

		MeshVertexUVs.Resize(MeshVertices.Size());

		for (int i = 0; i < 3 * 4; i++)
			MeshElements.Push(i);

		UpdateCollision();
	}

	virtual ~LevelSubmesh() = default;

	virtual LevelMeshSurface* GetSurface(int index) { return nullptr; }
	virtual unsigned int GetSurfaceIndex(const LevelMeshSurface* surface) const { return 0xffffffff; }
	virtual int GetSurfaceCount() { return 0; }

	TArray<FVector3> MeshVertices;
	TArray<FVector2> MeshVertexUVs;
	TArray<int> MeshUVIndex;
	TArray<uint32_t> MeshElements;
	TArray<int> MeshSurfaceIndexes;

	TArray<LevelMeshPortal> Portals;

	std::unique_ptr<TriangleMeshShape> Collision;

	// Lightmap atlas
	int LMTextureCount = 0;
	int LMTextureSize = 0;
	TArray<uint16_t> LMTextureData;

	uint16_t LightmapSampleDistance = 16;

	uint32_t AtlasPixelCount() const { return uint32_t(LMTextureCount * LMTextureSize * LMTextureSize); }

	void UpdateCollision()
	{
		Collision = std::make_unique<TriangleMeshShape>(MeshVertices.Data(), MeshVertices.Size(), MeshElements.Data(), MeshElements.Size());
	}

	void GatherSurfacePixelStats(LevelMeshSurfaceStats& stats)
	{
		int count = GetSurfaceCount();
		for (int i = 0; i < count; ++i)
		{
			const auto* surface = GetSurface(i);
			auto area = surface->Area();

			stats.pixels.total += area;

			if (surface->needsUpdate)
			{
				stats.surfaces.dirty++;
				stats.pixels.dirty += area;
			}
			if (surface->bSky)
			{
				stats.surfaces.sky++;
				stats.pixels.sky += area;
			}
		}
		stats.surfaces.total += count;
	}

	void BuildSmoothingGroups()
	{
		TArray<LevelMeshSmoothingGroup> SmoothingGroups;

		for (int i = 0, count = GetSurfaceCount(); i < count; i++)
		{
			auto surface = GetSurface(i);

			// Is this surface in the same plane as an existing smoothing group?
			int smoothingGroupIndex = -1;

			for (size_t j = 0; j < SmoothingGroups.Size(); j++)
			{
				if (surface->sectorGroup == SmoothingGroups[j].sectorGroup)
				{
					float direction = SmoothingGroups[j].plane.XYZ() | surface->plane.XYZ();
					if (direction >= 0.9999f && direction <= 1.001f)
					{
						auto point = (surface->plane.XYZ() * surface->plane.W);
						auto planeDistance = (SmoothingGroups[j].plane.XYZ() | point) - SmoothingGroups[j].plane.W;

						float dist = std::abs(planeDistance);
						if (dist <= 0.01f)
						{
							smoothingGroupIndex = (int)j;
							break;
						}
					}
				}
			}

			// Surface is in a new plane. Create a smoothing group for it
			if (smoothingGroupIndex == -1)
			{
				smoothingGroupIndex = SmoothingGroups.Size();

				LevelMeshSmoothingGroup group;
				group.plane = surface->plane;
				group.sectorGroup = surface->sectorGroup;
				SmoothingGroups.Push(group);
			}

			SmoothingGroups[smoothingGroupIndex].surfaces.push_back(surface);
			surface->smoothingGroupIndex = smoothingGroupIndex;
		}

		for (int i = 0, count = GetSurfaceCount(); i < count; i++)
		{
			auto targetSurface = GetSurface(i);
			targetSurface->tileSurfaces.Clear();
			for (LevelMeshSurface* surface : SmoothingGroups[targetSurface->smoothingGroupIndex].surfaces)
			{
				FVector2 minUV = ToUV(surface->bounds.min, targetSurface);
				FVector2 maxUV = ToUV(surface->bounds.max, targetSurface);
				if (surface != targetSurface && (maxUV.X < 0.0f || maxUV.Y < 0.0f || minUV.X > 1.0f || minUV.Y > 1.0f))
					continue; // Bounding box not visible

				targetSurface->tileSurfaces.Push(surface);
			}
		}
	}

private:
	FVector2 ToUV(const FVector3& vert, const LevelMeshSurface* targetSurface)
	{
		FVector3 localPos = vert - targetSurface->translateWorldToLocal;
		float u = (1.0f + (localPos | targetSurface->projLocalToU)) / (targetSurface->AtlasTile.Width + 2);
		float v = (1.0f + (localPos | targetSurface->projLocalToV)) / (targetSurface->AtlasTile.Height + 2);
		return FVector2(u, v);
	}
};

class LevelMesh
{
public:
	virtual ~LevelMesh() = default;

	std::unique_ptr<LevelSubmesh> StaticMesh = std::make_unique<LevelSubmesh>();
	std::unique_ptr<LevelSubmesh> DynamicMesh = std::make_unique<LevelSubmesh>();

	virtual int AddSurfaceLights(const LevelMeshSurface* surface, LevelMeshLight* list, int listMaxSize) { return 0; }

	LevelMeshSurfaceStats GatherSurfacePixelStats()
	{
		LevelMeshSurfaceStats stats;
		StaticMesh->GatherSurfacePixelStats(stats);
		DynamicMesh->GatherSurfacePixelStats(stats);
		return stats;
	}

	// Map defaults
	FVector3 SunDirection = FVector3(0.0f, 0.0f, -1.0f);
	FVector3 SunColor = FVector3(0.0f, 0.0f, 0.0f);

	LevelMeshSurface* Trace(const FVector3& start, FVector3 direction, float maxDist)
	{
		maxDist = std::max(maxDist - 10.0f, 0.0f);

		FVector3 origin = start;

		LevelMeshSurface* hitSurface = nullptr;

		while (true)
		{
			FVector3 end = origin + direction * maxDist;

			TraceHit hit0 = TriangleMeshShape::find_first_hit(StaticMesh->Collision.get(), origin, end);
			TraceHit hit1 = TriangleMeshShape::find_first_hit(DynamicMesh->Collision.get(), origin, end);

			LevelSubmesh* hitmesh = hit0.fraction < hit1.fraction ? StaticMesh.get() : DynamicMesh.get();
			TraceHit hit = hit0.fraction < hit1.fraction ? hit0 : hit1;

			if (hit.triangle < 0)
			{
				return nullptr;
			}

			hitSurface = hitmesh->GetSurface(hitmesh->MeshSurfaceIndexes[hit.triangle]);
			auto portal = hitSurface->portalIndex;

			if (!portal)
			{
				break;
			}

			auto& transformation = hitmesh->Portals[portal];

			auto travelDist = hit.fraction * maxDist + 2.0f;
			if (travelDist >= maxDist)
			{
				break;
			}

			origin = transformation.TransformPosition(origin + direction * travelDist);
			direction = transformation.TransformRotation(direction);
			maxDist -= travelDist;
		}

		return hitSurface; // I hit something
	}
};
