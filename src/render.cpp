#include "al2o3_platform/platform.h"
#include "al2o3_memory/memory.h"
#include "al2o3_vfile/vfile.h"
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
};

struct MeshModRender_Manager {
	Handle_Manager32* meshManager;
	Render_RendererHandle renderer;

	MeshModRender_RenderStyleMaterial styleMaterial[MMR_MAX];

	union {
		Render_GpuView view;
		uint8_t spacer[UNIFORM_BUFFER_MIN_SIZE];
	} uniforms;
	Render_BufferHandle uniformBuffer;
};

static bool CreatePosColour(MeshModRender_Manager *manager, TinyImageFormat colourFormat, TinyImageFormat depthFormat) {
	static char const *const VertexShader = "cbuffer uniformBlock : register(b0, space1)\n"
																					"{\n"
																					"\tfloat4x4 worldToViewMatrix;\n"
																					"\tfloat4x4 viewToNDCMatrix;\n"
																					"\tfloat4x4 worldToNDCMatrix;\n"
																					"};\n"
																					"struct VSInput\n"
																					"{\n"
																					"\tfloat4 Position : POSITION;\n"
																					"\tfloat4 Colour   : COLOR;\n"
																					"};\n"
																					"\n"
																					"struct VSOutput {\n"
																					"\tfloat4 Position : SV_POSITION;\n"
																					"\tfloat4 Colour   : COLOR;\n"
																					"};\n"
																					"\n"
																					"VSOutput VS_main(VSInput input)\n"
																					"{\n"
																					"    VSOutput result;\n"
																					"\n"
																					"\tresult.Position = mul(worldToNDCMatrix, input.Position);\n"
																					"\tresult.Colour = input.Colour;\n"
																					"\treturn result;\n"
																					"}";

	static char const *const FragmentShader = "struct FSInput {\n"
																						"\tfloat4 Position : SV_POSITION;\n"
																						"\tfloat4 Colour   : COLOR;\n"
																						"};\n"
																						"\n"
																						"float4 FS_main(FSInput input) : SV_Target\n"
																						"{\n"
																						"\treturn input.Colour;\n"
																						"}\n";

	static char const *const vertEntryPoint = "VS_main";
	static char const *const fragEntryPoint = "FS_main";

	VFile_Handle vfile = VFile_FromMemory(VertexShader, strlen(VertexShader) + 1, false);
	if (!vfile) {
		return false;
	}
	VFile_Handle ffile = VFile_FromMemory(FragmentShader, strlen(FragmentShader) + 1, false);
	if (!ffile) {
		VFile_Close(vfile);
		return false;
	}
	MeshModRender_RenderStyleMaterial& material = manager->styleMaterial[MMR_RS_FACE_COLOURS];

	material.shader = Render_CreateShaderFromVFile(manager->renderer, vfile, "VS_main", ffile, "FS_main");

	VFile_Close(vfile);
	VFile_Close(ffile);

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
	params[0].name = "uniformBlock";
	params[0].type = Render_DT_BUFFER;
	params[0].buffer = manager->uniformBuffer;
	params[0].offset = 0;
	params[0].size = sizeof(manager->uniforms);
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
			sizeof(manager->uniforms),
			true
	};

	manager->uniformBuffer = Render_BufferCreateUniform(manager->renderer, &ubDesc);
	if (!Render_BufferHandleIsValid(manager->uniformBuffer)) {
		return nullptr;
	}

	if( !CreatePosColour(manager, colourDestFormat, depthDestFormat) )
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
		Render_DescriptorSetDestroy(manager->renderer, material.descriptorSet);
		Render_PipelineDestroy(manager->renderer, material.pipeline);
		Render_RootSignatureDestroy(manager->renderer, material.rootSignature);
		Render_ShaderDestroy(manager->renderer, material.shader);
	}

	Render_BufferDestroy(manager->renderer, manager->uniformBuffer);

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
			default:
				break;
		}
		ASSERT(sizeOfVertex);

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
		default:
			break;
	}

}

AL2O3_EXTERN_C void MeshModRender_ManagerSetView(MeshModRender_Manager* manager, Render_GpuView* view) {
	// upload the uniforms
	memcpy(&manager->uniforms, view, sizeof(Render_GpuView));
	Render_BufferUpdateDesc uniformUpdate = {
			&manager->uniforms,
			0,
			sizeof(manager->uniforms)
	};
	Render_BufferUpload(manager->uniformBuffer, &uniformUpdate);

}

AL2O3_EXTERN_C void MeshModRender_MeshRender(MeshModRender_Manager* manager,
																						 Render_GraphicsEncoderHandle encoder,
																						 MeshModRender_MeshHandle mrhandle) {

	auto mesh = (MeshMod_MeshRenderable*) Handle_Manager32HandleToPtr(manager->meshManager, mrhandle.handle);

	MeshModRender_RenderStyleMaterial const& material = manager->styleMaterial[mesh->renderStyle];

	Render_GraphicsEncoderBindDescriptorSet(encoder, material.descriptorSet, 0);
	Render_GraphicsEncoderBindVertexBuffer(encoder, mesh->gpuVertexBuffer, 0);
	Render_GraphicsEncoderBindPipeline(encoder, material.pipeline);
	Render_GraphicsEncoderDraw(encoder, mesh->gpuVertexBufferCount, 0);

}
