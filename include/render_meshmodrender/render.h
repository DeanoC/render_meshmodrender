#pragma once

#include "al2o3_platform/platform.h"
#include "al2o3_handle/handle.h"
#include "render_meshmod/mesh.h"
#include "render_basics/api.h"
#include "al2o3_cmath/matrix.h"

enum MeshModRender_RenderStyle {
	MMR_RS_FACE_COLOURS,
	MMR_RS_TRIANGLE_COLOURS,
	MMR_RS_NORMAL,

	MMR_MAX
};

typedef struct MeshModRender_Manager MeshModRender_Manager;
typedef struct { Handle_Handle32 handle; } MeshModRender_MeshHandle;
typedef struct Render_GpuView Render_GpuView;

AL2O3_EXTERN_C MeshModRender_Manager* MeshModRender_ManagerCreate(Render_RendererHandle renderer, TinyImageFormat colourDestFormat, TinyImageFormat depthDestFormat);
AL2O3_EXTERN_C void MeshModRender_ManagerDestroy( MeshModRender_Manager* manager);
AL2O3_EXTERN_C void MeshModRender_ManagerSetView(MeshModRender_Manager* manager, Render_GpuView* view);

AL2O3_EXTERN_C MeshModRender_MeshHandle MeshModRender_MeshCreate(MeshModRender_Manager* manager, MeshMod_MeshHandle mhandle);
AL2O3_EXTERN_C void MeshModRender_MeshDestroy(MeshModRender_Manager* manager, MeshModRender_MeshHandle mrhandle);

AL2O3_EXTERN_C void MeshModRender_MeshSetStyle(MeshModRender_Manager* manager, MeshModRender_MeshHandle mrhandle, MeshModRender_RenderStyle style);
AL2O3_EXTERN_C void MeshModRender_MeshUpdate(MeshModRender_Manager* manager, MeshModRender_MeshHandle mrhandle);
AL2O3_EXTERN_C void MeshModRender_MeshRender(MeshModRender_Manager* manager,
		Render_GraphicsEncoderHandle encoder,
		MeshModRender_MeshHandle mrhandle,
		Math_Mat4F localMatrix);

