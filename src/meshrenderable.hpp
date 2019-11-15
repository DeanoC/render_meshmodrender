#pragma once

#include "render_basics/api.h"
#include "al2o3_cadt/vector.h"
#include "render_meshmod/mesh.h"
#include "render_meshmodrender/render.h"
#include "al2o3_cmath/vector.h"
#include "al2o3_cmath/matrix.h"
#include "render_basics/view.h"

struct MeshMod_MeshRenderable {
	MeshMod_MeshHandle MMMesh;
	MeshModRender_RenderStyle renderStyle;

	Render_RendererHandle renderer;

	CADT_VectorHandle cpuVertexBuffer;
	Render_BufferHandle gpuVertexBuffer;
	uint32_t gpuVertexBufferCount;

	uint64_t storedPosHash;
	uint64_t storedNormalHash;

	Render_DescriptorSetHandle descriptorSet;
	union {
		struct {
			Math_Mat4F localToWorld;
			Math_Mat4F localToWorldTranspose;
		};

		uint8_t spacer[UNIFORM_BUFFER_MIN_SIZE];
	} localUniforms;
	Render_BufferHandle localUniformBuffer;
};

struct VertexPosNormal {
	Math_Vec3F position;
	Math_Vec3F normal;

	static void UpdateIfNeeded(MeshMod_MeshRenderable* mr);
};

struct VertexPosColour {
	Math_Vec3F position;
	uint32_t colour;

	static void UpdateIfNeededFaceColours(MeshMod_MeshRenderable* mr);
	static void UpdateIfNeededTriColours(MeshMod_MeshRenderable* mr);
};

struct VertexPosNormalColour {
	Math_Vec3F position;
	Math_Vec3F normal;
	uint32_t colour;

	static void UpdateIfNeeded(MeshMod_MeshRenderable* mr);
};
