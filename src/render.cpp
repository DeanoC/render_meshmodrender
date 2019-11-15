#include "al2o3_platform/platform.h"
#include "al2o3_memory/memory.h"
#include "al2o3_vfile/vfile.hpp"
#include "render_meshmodrender/render.h"
#include "render_basics/shader.h"
#include "render_basics/rootsignature.h"
#include "render_basics/pipeline.h"
#include "render_basics/descriptorset.h"
#include "render_basics/view.h"
#include "render_basics/buffer.h"
#include "render_basics/graphicsencoder.h"

#include "meshrenderable.hpp"

struct MeshModRender_RenderStyleMaterial {
	Render_ShaderHandle shader;
	Render_RootSignatureHandle rootSignature;
	Render_PipelineHandle pipeline;
	Render_DescriptorSetHandle descriptorSet;

	bool copyDontFree;
};

struct MeshModRender_Manager {
	Handle_Manager32* meshManager;
	Render_RendererHandle renderer;

	MeshModRender_RenderStyleMaterial styleMaterial[MMR_MAX];

	union {
		Render_GpuView view;
		uint8_t spacer[UNIFORM_BUFFER_MIN_SIZE];
	} viewUniforms;
	Render_BufferHandle viewUniformBuffer;
};

static bool CreatePosColour(MeshModRender_Manager *manager, TinyImageFormat colourFormat, TinyImageFormat depthFormat) {

	VFile::ScopedFile vfile = VFile::FromFile("resources/poscolour_vertex.hlsl", Os_FM_Read);
	if (!vfile) {
		return false;
	}
	VFile::ScopedFile ffile = VFile::FromFile("resources/copycolour_fragment.hlsl", Os_FM_Read);
	if (!ffile) {
		return false;
	}

	MeshModRender_RenderStyleMaterial& material = manager->styleMaterial[MMR_RS_FACE_COLOURS];

	material.shader = Render_CreateShaderFromVFile(manager->renderer, vfile, "VS_main", ffile, "FS_main");

	if (!Render_ShaderHandleIsValid(material.shader)) {
		return false;
	}

	Render_RootSignatureDesc rootSignatureDesc{};
	rootSignatureDesc.shaderCount = 1;
	rootSignatureDesc.shaders = &material.shader;
	rootSignatureDesc.staticSamplerCount = 0;
	material.rootSignature = Render_RootSignatureCreate(manager->renderer, &rootSignatureDesc);
	if (!Render_RootSignatureHandleIsValid(material.rootSignature)) {
		return false;
	}

	TinyImageFormat colourFormats[] = { colourFormat };

	Render_GraphicsPipelineDesc gfxPipeDesc{};
	gfxPipeDesc.shader = material.shader;
	gfxPipeDesc.rootSignature = material.rootSignature;
	gfxPipeDesc.vertexLayout = Render_GetStockVertexLayout(manager->renderer, Render_SVL_3D_COLOUR);
	gfxPipeDesc.blendState = Render_GetStockBlendState(manager->renderer, Render_SBS_OPAQUE);
	if(depthFormat == TinyImageFormat_UNDEFINED) {
		gfxPipeDesc.depthState = Render_GetStockDepthState(manager->renderer, Render_SDS_IGNORE);
	} else {
		gfxPipeDesc.depthState = Render_GetStockDepthState(manager->renderer, Render_SDS_READWRITE_LESS);
	}
	gfxPipeDesc.rasteriserState = Render_GetStockRasterisationState(manager->renderer, Render_SRS_BACKCULL);
	gfxPipeDesc.colourRenderTargetCount = 1;
	gfxPipeDesc.colourFormats = colourFormats;
	gfxPipeDesc.depthStencilFormat = depthFormat;
	gfxPipeDesc.sampleCount = 1;
	gfxPipeDesc.sampleQuality = 0;
	gfxPipeDesc.primitiveTopo = Render_PT_TRI_LIST;
	material.pipeline = Render_GraphicsPipelineCreate(manager->renderer, &gfxPipeDesc);
	if (!Render_PipelineHandleIsValid(material.pipeline)) {
		return false;
	}

	Render_DescriptorSetDesc const setDesc = {
			material.rootSignature,
			Render_DUF_PER_FRAME,
			1
	};

	material.descriptorSet = Render_DescriptorSetCreate(manager->renderer, &setDesc);
	if (!Render_DescriptorSetHandleIsValid(material.descriptorSet)) {
		return false;
	}
	Render_DescriptorDesc params[1];
	params[0].name = "View";
	params[0].type = Render_DT_BUFFER;
	params[0].buffer = manager->viewUniformBuffer;
	params[0].offset = 0;
	params[0].size = sizeof(manager->viewUniforms);
	Render_DescriptorPresetFrequencyUpdated(material.descriptorSet, 0, 1, params);

	MeshModRender_RenderStyleMaterial& materialCopy = manager->styleMaterial[MMR_RS_TRIANGLE_COLOURS];
	materialCopy = material;
	materialCopy.copyDontFree = true;

	return true;
}

static bool CreatePosNormal(MeshModRender_Manager *manager, TinyImageFormat colourFormat, TinyImageFormat depthFormat) {
	VFile::ScopedFile vfile = VFile::FromFile("resources/posnormal_vertex.hlsl", Os_FM_Read);
	if (!vfile) {
		return false;
	}
	VFile::ScopedFile ffile = VFile::FromFile("resources/copycolour_fragment.hlsl", Os_FM_Read);
	if (!ffile) {
		VFile_Close(vfile);
		return false;
	}

	MeshModRender_RenderStyleMaterial& material = manager->styleMaterial[MMR_RS_NORMAL];

	material.shader = Render_CreateShaderFromVFile(manager->renderer, vfile, "VS_main", ffile, "FS_main");

	if (!Render_ShaderHandleIsValid(material.shader)) {
		return false;
	}

	Render_RootSignatureDesc rootSignatureDesc{};
	rootSignatureDesc.shaderCount = 1;
	rootSignatureDesc.shaders = &material.shader;
	rootSignatureDesc.staticSamplerCount = 0;
	material.rootSignature = Render_RootSignatureCreate(manager->renderer, &rootSignatureDesc);
	if (!Render_RootSignatureHandleIsValid(material.rootSignature)) {
		return false;
	}

	TinyImageFormat colourFormats[] = { colourFormat };

	Render_GraphicsPipelineDesc gfxPipeDesc{};
	gfxPipeDesc.shader = material.shader;
	gfxPipeDesc.rootSignature = material.rootSignature;
	gfxPipeDesc.vertexLayout = Render_GetStockVertexLayout(manager->renderer, Render_SVL_3D_NORMAL);
	gfxPipeDesc.blendState = Render_GetStockBlendState(manager->renderer, Render_SBS_OPAQUE);
	if(depthFormat == TinyImageFormat_UNDEFINED) {
		gfxPipeDesc.depthState = Render_GetStockDepthState(manager->renderer, Render_SDS_IGNORE);
	} else {
		gfxPipeDesc.depthState = Render_GetStockDepthState(manager->renderer, Render_SDS_READWRITE_LESS);
	}
	gfxPipeDesc.rasteriserState = Render_GetStockRasterisationState(manager->renderer, Render_SRS_BACKCULL);
	gfxPipeDesc.colourRenderTargetCount = 1;
	gfxPipeDesc.colourFormats = colourFormats;
	gfxPipeDesc.depthStencilFormat = depthFormat;
	gfxPipeDesc.sampleCount = 1;
	gfxPipeDesc.sampleQuality = 0;
	gfxPipeDesc.primitiveTopo = Render_PT_TRI_LIST;
	material.pipeline = Render_GraphicsPipelineCreate(manager->renderer, &gfxPipeDesc);
	if (!Render_PipelineHandleIsValid(material.pipeline)) {
		return false;
	}

	Render_DescriptorSetDesc const setDesc = {
			material.rootSignature,
			Render_DUF_PER_FRAME,
			1
	};

	material.descriptorSet = Render_DescriptorSetCreate(manager->renderer, &setDesc);
	if (!Render_DescriptorSetHandleIsValid(material.descriptorSet)) {
		return false;
	}
	Render_DescriptorDesc params[1];
	params[0].name = "View";
	params[0].type = Render_DT_BUFFER;
	params[0].buffer = manager->viewUniformBuffer;
	params[0].offset = 0;
	params[0].size = sizeof(manager->viewUniforms);
	Render_DescriptorPresetFrequencyUpdated(material.descriptorSet, 0, 1, params);

	return true;
}


AL2O3_EXTERN_C MeshModRender_Manager* MeshModRender_ManagerCreate(Render_RendererHandle renderer, TinyImageFormat colourDestFormat, TinyImageFormat depthDestFormat) {
	auto manager = (MeshModRender_Manager*) MEMORY_CALLOC(1, sizeof(MeshModRender_Manager));
	if(!manager) {
		return nullptr;
	}

	manager->renderer = renderer;
	manager->meshManager = Handle_Manager32Create(sizeof(MeshMod_MeshRenderable), 1024*16, 32, false);

	static Render_BufferUniformDesc const ubDesc{
			sizeof(manager->viewUniforms),
			true
	};

	manager->viewUniformBuffer = Render_BufferCreateUniform(manager->renderer, &ubDesc);
	if (!Render_BufferHandleIsValid(manager->viewUniformBuffer)) {
		return nullptr;
	}

	if( !CreatePosColour(manager, colourDestFormat, depthDestFormat) )
	{
		MeshModRender_ManagerDestroy(manager);
		return nullptr;
	}

	if( !CreatePosNormal(manager, colourDestFormat, depthDestFormat) )
	{
		MeshModRender_ManagerDestroy(manager);
		return nullptr;
	}

	return manager;
}

AL2O3_EXTERN_C void MeshModRender_ManagerDestroy( MeshModRender_Manager* manager) {
	if(manager == NULL) {
		return;
	}

	for (uint32_t i = 0u; i < MMR_MAX; ++i) {
		MeshModRender_RenderStyleMaterial& material = manager->styleMaterial[i];
		if(material.copyDontFree) {
			continue;
		}
		Render_DescriptorSetDestroy(manager->renderer, material.descriptorSet);
		Render_PipelineDestroy(manager->renderer, material.pipeline);
		Render_RootSignatureDestroy(manager->renderer, material.rootSignature);
		Render_ShaderDestroy(manager->renderer, material.shader);
	}

	Render_BufferDestroy(manager->renderer, manager->viewUniformBuffer);

	Handle_Manager32Destroy(manager->meshManager);
	MEMORY_FREE(manager);
}

AL2O3_EXTERN_C MeshModRender_MeshHandle MeshModRender_MeshCreate(MeshModRender_Manager* manager, MeshMod_MeshHandle mhandle) {
	MeshModRender_MeshHandle mrhandle;
	mrhandle.handle = Handle_Manager32Alloc(manager->meshManager);

	auto mesh = (MeshMod_MeshRenderable*) Handle_Manager32HandleToPtr(manager->meshManager, mrhandle.handle);
	mesh->MMMesh = mhandle;
	mesh->renderer = manager->renderer;
	mesh->renderStyle = MMR_MAX; // force a change

	MeshModRender_MeshSetStyle(manager, mrhandle, MMR_RS_FACE_COLOURS);


	return mrhandle;
}

AL2O3_EXTERN_C void MeshModRender_MeshDestroy(MeshModRender_Manager* manager, MeshModRender_MeshHandle mrhandle) {
	auto mesh = (MeshMod_MeshRenderable*) Handle_Manager32HandleToPtr(manager->meshManager, mrhandle.handle);

	Render_DescriptorSetDestroy(mesh->renderer, mesh->descriptorSet);
	Render_BufferDestroy(mesh->renderer, mesh->localUniformBuffer);
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
		Render_DescriptorSetDestroy(mesh->renderer, mesh->descriptorSet);
		Render_BufferDestroy(mesh->renderer, mesh->localUniformBuffer);

		mesh->gpuVertexBufferCount = 0;
		mesh->storedPosHash = 0;
		mesh->storedNormalHash = 0;

		uint32_t sizeOfVertex = 0;
		switch(style) {
			case MMR_RS_FACE_COLOURS:
			case MMR_RS_TRIANGLE_COLOURS:
				sizeOfVertex = sizeof(VertexPosColour);
				break;

			case MMR_RS_NORMAL:
				sizeOfVertex = sizeof(VertexPosNormal);
				break;
			case MMR_MAX:
				break;
		}
		ASSERT(sizeOfVertex);

		mesh->cpuVertexBuffer = CADT_VectorCreate(sizeOfVertex);
		mesh->renderStyle = style;

		MeshModRender_RenderStyleMaterial const& material = manager->styleMaterial[mesh->renderStyle];

		static Render_BufferUniformDesc const ubDesc{
				sizeof(mesh->localUniforms),
				true
		};
		mesh->localUniformBuffer = Render_BufferCreateUniform(manager->renderer, &ubDesc);

		Render_DescriptorSetDesc const setDesc = {
				material.rootSignature,
				Render_DUF_PER_DRAW,
				1
		};
		mesh->descriptorSet = Render_DescriptorSetCreate(manager->renderer, &setDesc);
		Render_DescriptorDesc params[1];
		params[0].name = "LocalToWorld";
		params[0].type = Render_DT_BUFFER;
		params[0].buffer = mesh->localUniformBuffer;
		params[0].offset = 0;
		params[0].size = sizeof(mesh->localUniforms);
		Render_DescriptorPresetFrequencyUpdated(mesh->descriptorSet, 0, 1, params);

	}

}

AL2O3_EXTERN_C void MeshModRender_MeshUpdate(MeshModRender_Manager* manager, MeshModRender_MeshHandle mrhandle) {
	auto mesh = (MeshMod_MeshRenderable*) Handle_Manager32HandleToPtr(manager->meshManager, mrhandle.handle);

	switch(mesh->renderStyle) {
		case MMR_RS_FACE_COLOURS:
			VertexPosColour::UpdateIfNeededFaceColours(mesh);
			break;
		case MMR_RS_TRIANGLE_COLOURS:
			VertexPosColour::UpdateIfNeededTriColours(mesh);
			break;
		case MMR_RS_NORMAL:
			VertexPosNormal::UpdateIfNeeded(mesh);
			break;
		case MMR_MAX:
			break;
	}

}

AL2O3_EXTERN_C void MeshModRender_ManagerSetView(MeshModRender_Manager* manager, Render_GpuView* view) {
	// upload the uniforms
	memcpy(&manager->viewUniforms, view, sizeof(Render_GpuView));
	Render_BufferUpdateDesc uniformUpdate = {
			&manager->viewUniforms,
			0,
			sizeof(manager->viewUniforms)
	};
	Render_BufferUpload(manager->viewUniformBuffer, &uniformUpdate);
}

AL2O3_EXTERN_C void MeshModRender_MeshRender(MeshModRender_Manager* manager,
																						 Render_GraphicsEncoderHandle encoder,
																						 MeshModRender_MeshHandle mrhandle,
																						 Math_Mat4F localMatrix,
																						 Math_Mat4F inverseLocalMatrix) {

	auto mesh = (MeshMod_MeshRenderable*) Handle_Manager32HandleToPtr(manager->meshManager, mrhandle.handle);

	// upload the uniforms
	memcpy(&mesh->localUniforms.localToWorld, Math_TransposeMat4F(localMatrix).v, sizeof(Math_Mat4F));
	memcpy(&mesh->localUniforms.localToWorldTranspose, inverseLocalMatrix.v, sizeof(Math_Mat4F));
	Render_BufferUpdateDesc uniformUpdate = {
			&mesh->localUniforms,
			0,
			sizeof(mesh->localUniforms)
	};
	Render_BufferUpload(mesh->localUniformBuffer, &uniformUpdate);

	MeshModRender_RenderStyleMaterial const& material = manager->styleMaterial[mesh->renderStyle];

	Render_GraphicsEncoderBindDescriptorSet(encoder, material.descriptorSet, 0);
	Render_GraphicsEncoderBindDescriptorSet(encoder, mesh->descriptorSet, 0);
	Render_GraphicsEncoderBindVertexBuffer(encoder, mesh->gpuVertexBuffer, 0);
	Render_GraphicsEncoderBindPipeline(encoder, material.pipeline);
	Render_GraphicsEncoderDraw(encoder, mesh->gpuVertexBufferCount, 0);

}
