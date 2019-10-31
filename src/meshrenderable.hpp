#pragma once

#include "render_basics/api.h"
#include "al2o3_cadt/vector.h"
#include "render_meshmod/mesh.h"
#include "render_meshmodrender/render.h"
#include "al2o3_cmath/vector.h"

struct MeshMod_MeshRenderable {
	MeshMod_MeshHandle MMMesh;
	MeshModRender_RenderStyle renderStyle;

	Render_RendererHandle renderer;
	CADT_VectorHandle cpuVertexBuffer;
	Render_BufferHandle gpuVertexBuffer;
	uint64_t storedPosHash;
	uint64_t storedNormalHash;

	uint32_t gpuVertexBufferCount;
};

struct VertexPosNormal {
	Math_Vec3F position;
	Math_Vec3F normal;

	static void UpdateIfNeeded(MeshMod_MeshRenderable* mr);
};

struct VertexPosColour {
	Math_Vec3F position;
	uint32_t colour;

	static void UpdateIfNeeded(MeshMod_MeshRenderable* mr);
};