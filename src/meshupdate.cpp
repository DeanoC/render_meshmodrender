#include "render_meshmod/registry.h"
#include "render_meshmod/mesh.h"
#include "render_meshmod/vertex/position.h"
#include "render_meshmod/vertex/normal.h"
#include "render_meshmod/polygon/tribrep.h"
#include "render_meshmod/polygon/quadbrep.h"
#include "render_meshmod/polygon/convexbrep.h"
#include "render_meshmod/edge/halfedge.h"
#include "render_meshmod/basicalgos.h"
#include "render_basics/api.h"
#include "render_basics/buffer.h"
#include "al2o3_cadt/vector.h"
#include "al2o3_cmath/vector.hpp"
#include "meshrenderable.hpp"

static uint32_t PickVisibleColour(uint32_t primitiveId) {
#define MU_PACKCOLOUR(r, g, b, a) (((uint32_t)r) << 0) | ((g) << 8) | ((b) << 16) | ((a) << 24)
	static const uint32_t ColourTableSize = 8;
	static const uint32_t ColourTable[ColourTableSize] = {
			MU_PACKCOLOUR(0xe6, 0x26, 0x1f, 0xFF),
			MU_PACKCOLOUR(0xeb, 0x75, 0x32, 0xFF),
			MU_PACKCOLOUR(0xf7, 0xd0, 0x38, 0xFF),
			MU_PACKCOLOUR(0xa2, 0xe0, 0x48, 0xFF),
			MU_PACKCOLOUR(0x49, 0xda, 0x9a, 0xFF),
			MU_PACKCOLOUR(0x34, 0xbb, 0xe6, 0xFF),
			MU_PACKCOLOUR(0x43, 0x55, 0xdb, 0xFF),
			MU_PACKCOLOUR(0xd2, 0x3b, 0xe7, 0xFF)
	};
#undef MU_PACKCOLOUR
	primitiveId = primitiveId % ColourTableSize;
	return ColourTable[primitiveId];
}

void VertexPosNormal::UpdateIfNeeded(MeshMod_MeshRenderable* mr) {
	ASSERT(MeshMod_MeshHandleIsValid(mr->MMMesh));

	using namespace Math;

	uint64_t const actualPosHash = MeshMod_MeshVertexTagGetOrComputeHash(mr->MMMesh, MeshMod_VertexPositionTag);
	uint64_t const actualNormalHash = MeshMod_MeshVertexTagGetOrComputeHash(mr->MMMesh, MeshMod_VertexNormalTag);

	if(mr->storedPosHash != actualPosHash || mr->storedNormalHash != actualNormalHash) {
		// has changed position or normal so regenerate

		// if we need to triangulate clone to not change the original mesh
		bool const isTriangleBRepOnly = (!MeshMod_MeshPolygonTagExists(mr->MMMesh, MeshMod_PolygonQuadBRepTag)) &&
				(!MeshMod_MeshPolygonTagExists(mr->MMMesh, MeshMod_PolygonConvexBRepTag));
		MeshMod_MeshHandle clone;
		if (isTriangleBRepOnly) {
			clone = mr->MMMesh;
		} else {
			clone = MeshMod_MeshClone(mr->MMMesh);
			MeshMod_MeshTrianglate(clone);
		}

		// TODO compacted fast path for meshmod...

		// clear old data
		CADT_VectorResize(mr->cpuVertexBuffer, 0);

		MeshMod_PolygonHandle phandle = MeshMod_MeshPolygonTagIterate(clone, MeshMod_PolygonTriBRepTag, NULL);
		while (MeshMod_MeshPolygonIsValid(clone, phandle)) {
			auto tri = MeshMod_MeshPolygonTriBRepTagHandleToPtr(clone, phandle, 0);

			for (int i = 0; i < 3; ++i) {
				VertexPosNormal vert;
				MeshMod_VertexHandle vh = MeshMod_MeshEdgeHalfEdgeTagHandleToPtr(clone, tri->edge[i], 0)->vertex;
				memcpy(&vert.position, MeshMod_MeshVertexPositionTagHandleToPtr(clone, vh, 0), sizeof(Vec3F));
				memcpy(&vert.normal, MeshMod_MeshVertexNormalTagHandleToPtr(clone, vh, 0), sizeof(Vec3F));
				CADT_VectorPushElement(mr->cpuVertexBuffer, &vert);
			}

			phandle = MeshMod_MeshPolygonTagIterate(clone, MeshMod_PolygonTriBRepTag, &phandle);
		}

		uint32_t const vertexCount = (uint32_t) CADT_VectorSize(mr->cpuVertexBuffer);
		if(vertexCount > mr->gpuVertexBufferCount) {
			Render_BufferDestroy(mr->renderer, mr->gpuVertexBuffer);

			Render_BufferVertexDesc const ibDesc {
					vertexCount,
					sizeof(VertexPosNormal),
					false
			};
			mr->gpuVertexBuffer = Render_BufferCreateVertex(mr->renderer, &ibDesc);
			mr->gpuVertexBufferCount = vertexCount;
		}

		if(vertexCount) {
			// upload the instance data
			Render_BufferUpdateDesc instanceUpdate = {
					CADT_VectorData(mr->cpuVertexBuffer),
					0,
					sizeof(VertexPosNormal) * vertexCount
			};
			Render_BufferUpload(mr->gpuVertexBuffer, &instanceUpdate);
		}

		if (!isTriangleBRepOnly) {
			MeshMod_MeshDestroy(clone);
		}
	}
}

void VertexPosColour::UpdateIfNeeded(MeshMod_MeshRenderable* mr) {
	ASSERT(MeshMod_MeshHandleIsValid(mr->MMMesh));

	using namespace Math;

	uint64_t const actualPosHash = MeshMod_MeshVertexTagGetOrComputeHash(mr->MMMesh, MeshMod_VertexPositionTag);
	uint64_t const actualNormalHash = MeshMod_MeshVertexTagGetOrComputeHash(mr->MMMesh, MeshMod_VertexNormalTag);

	if(mr->storedPosHash != actualPosHash || mr->storedNormalHash != actualNormalHash) {
		// has changed position or normal so regenerate
		mr->storedPosHash = actualPosHash;
		mr->storedNormalHash = actualNormalHash;

		// if we need to triangulate clone to not change the original mesh
		bool const isTriangleBRepOnly = (!MeshMod_MeshPolygonTagExists(mr->MMMesh, MeshMod_PolygonQuadBRepTag)) &&
				(!MeshMod_MeshPolygonTagExists(mr->MMMesh, MeshMod_PolygonConvexBRepTag));
		MeshMod_MeshHandle clone = {0};
		if (isTriangleBRepOnly) {
			clone = mr->MMMesh;
		} else {
			clone = MeshMod_MeshClone(mr->MMMesh);
			MeshMod_MeshTrianglate(clone);
		}

		// TODO compacted fast path for meshmod...

		// clear old data
		CADT_VectorResize(mr->cpuVertexBuffer, 0);

		uint32_t primitiveId = 0;
		MeshMod_PolygonHandle phandle = MeshMod_MeshPolygonTagIterate(clone, MeshMod_PolygonTriBRepTag, NULL);
		while (MeshMod_MeshPolygonIsValid(clone, phandle)) {
			auto tri = MeshMod_MeshPolygonTriBRepTagHandleToPtr(clone, phandle, 0);

			for (int i = 0; i < 3; ++i) {
				VertexPosColour vert;
				MeshMod_VertexHandle vh = MeshMod_MeshEdgeHalfEdgeTagHandleToPtr(clone, tri->edge[i], 0)->vertex;
				memcpy(&vert.position, MeshMod_MeshVertexPositionTagHandleToPtr(clone, vh, 0), sizeof(Vec3F));
				vert.colour = PickVisibleColour(primitiveId);
				CADT_VectorPushElement(mr->cpuVertexBuffer, &vert);
			}

			phandle = MeshMod_MeshPolygonTagIterate(clone, MeshMod_PolygonTriBRepTag, &phandle);
			primitiveId++;
		}

		uint32_t const vertexCount = (uint32_t) CADT_VectorSize(mr->cpuVertexBuffer);
		if(vertexCount > mr->gpuVertexBufferCount) {
			Render_BufferDestroy(mr->renderer, mr->gpuVertexBuffer);

			Render_BufferVertexDesc const vbDesc {
					vertexCount,
					sizeof(VertexPosColour),
					false
			};
			mr->gpuVertexBuffer = Render_BufferCreateVertex(mr->renderer, &vbDesc);
			mr->gpuVertexBufferCount = vertexCount;
		}

		if(vertexCount) {
			// upload the vertex data
			Render_BufferUpdateDesc instanceUpdate = {
					CADT_VectorData(mr->cpuVertexBuffer),
					0,
					sizeof(VertexPosColour) * vertexCount
			};
			Render_BufferUpload(mr->gpuVertexBuffer, &instanceUpdate);
		}

		if (!isTriangleBRepOnly) {
			MeshMod_MeshDestroy(clone);
		}
	}
}
