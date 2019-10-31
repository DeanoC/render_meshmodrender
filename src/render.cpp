#include "al2o3_platform/platform.h"
#include "al2o3_memory/memory.h"
#include "render_meshmodrender/render.h"
#include "meshrenderable.hpp"

struct MeshModRender_Manager {
	Handle_Manager32* meshManager;
	Render_RendererHandle renderer;
};

AL2O3_EXTERN_C MeshModRender_Manager* MeshModRender_ManagerCreate(Render_RendererHandle renderer) {
	auto manager = (MeshModRender_Manager*) MEMORY_CALLOC(1, sizeof(MeshModRender_Manager));	

	manager->renderer = renderer;
	manager->meshManager = Handle_Manager32Create(sizeof(MeshMod_MeshRenderable), 1024*16, 32, false);

	return manager;
}

AL2O3_EXTERN_C void MeshModRender_ManagerDestroy( MeshModRender_Manager* manager) {
	if(manager == NULL) {
		return;
	}

	Handle_Manager32Destroy(manager->meshManager);
	MEMORY_FREE(manager);
}

AL2O3_EXTERN_C MeshModRender_MeshHandle MeshModRender_MeshCreate(MeshModRender_Manager* manager, MeshMod_MeshHandle mhandle) {
	MeshModRender_MeshHandle mrhandle;
	mrhandle.handle = Handle_Manager32Alloc(manager->meshManager);

	auto mesh = (MeshMod_MeshRenderable*) Handle_Manager32HandleToPtr(manager->meshManager, mrhandle.handle);
	mesh->MMMesh = mhandle;
	mesh->renderer = manager->renderer;

	MeshModRender_MeshSetStyle(manager, mrhandle, MMR_RS_FACE_COLOURS);

	return mrhandle;
}

AL2O3_EXTERN_C void MeshModRender_MeshDestroy(MeshModRender_Manager* manager, MeshModRender_MeshHandle mrhandle) {
	auto mesh = (MeshMod_MeshRenderable*) Handle_Manager32HandleToPtr(manager->meshManager, mrhandle.handle);

	CADT_VectorDestroy(mesh->cpuVertexBuffer);
	Render_BufferDestroy(mesh->renderer, mesh->gpuVertexBuffer);

	Handle_Manager32Release(manager->meshManager, mrhandle.handle);
}

AL2O3_EXTERN_C void MeshModRender_MeshSetStyle(MeshModRender_Manager* manager, MeshModRender_MeshHandle mrhandle, MeshModRender_RenderStyle style) {
	auto mesh = (MeshMod_MeshRenderable*) Handle_Manager32HandleToPtr(manager->meshManager, mrhandle.handle);
	if(style != mesh->renderStyle) {
		// destroy old buffers
		CADT_VectorDestroy(mesh->cpuVertexBuffer);
		Render_BufferDestroy(mesh->renderer, mesh->gpuVertexBuffer);
		mesh->gpuVertexBufferCount = 0;
		mesh->storedPosHash = 0;
		mesh->storedNormalHash = 0;

		uint32_t sizeOfVertex = 0;
		switch(style) {
			case MMR_RS_FACE_COLOURS:
				sizeOfVertex = sizeof(VertexPosColour);
				break;
			case MMR_RS_NORMAL:
				sizeOfVertex = sizeof(VertexPosNormal);
				break;
		}

		mesh->cpuVertexBuffer = CADT_VectorCreate(sizeOfVertex);
		mesh->renderStyle = style;
	}

}

AL2O3_EXTERN_C void MeshModRender_MeshUpdate(MeshModRender_Manager* manager, MeshModRender_MeshHandle mrhandle) {
	auto mesh = (MeshMod_MeshRenderable*) Handle_Manager32HandleToPtr(manager->meshManager, mrhandle.handle);

	switch(mesh->renderStyle) {
		case MMR_RS_FACE_COLOURS:
			VertexPosColour::UpdateIfNeeded(mesh);
			break;
		case MMR_RS_NORMAL:
			VertexPosNormal::UpdateIfNeeded(mesh);
			break;
	}

}
AL2O3_EXTERN_C void MeshModRender_MeshRender(MeshModRender_Manager* manager, MeshModRender_MeshHandle mrhandle) {

	auto mesh = (MeshMod_MeshRenderable*) Handle_Manager32HandleToPtr(manager->meshManager, mrhandle.handle);

	switch(mesh->renderStyle) {
		case MMR_RS_FACE_COLOURS:
			break;
		case MMR_RS_NORMAL:
			break;
	}
}
