#include <stdio.h>

#include <FNA3D.h>
#include <FNA3D_Image.h>
#include <FNA3D_SysRenderer.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define SDL_MAIN_HANDLED
#include <SDL.h>

#define MOJOSHADER_NO_VERSION_INCLUDE
#define MOJOSHADER_EFFECT_SUPPORT
#include "../FNA3D/MojoShader/mojoshader.h"
#include "../FNA3D/MojoShader/mojoshader_effects.h"

#include <d3d11.h>


typedef struct Vertex
{
	float x, y;
	float u, v;
	uint32_t color;
} Vertex;

int main(int argc, char* argv[])
{
	/* Set up devices */

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) < 0)
	{
		fprintf(stderr, "Failed to initialize SDL\n\t%s\n", SDL_GetError());
		return -1;
	}

	const int windowWidth = 1280;
	const int windowHeight = 720;

	SDL_SetHint("FNA3D_FORCE_DRIVER", "D3D11");

	uint32_t windowFlags = FNA3D_PrepareWindowAttributes();

	SDL_Window* window = SDL_CreateWindow(
		"Refresh Test",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		windowWidth,
		windowHeight,
		windowFlags
	);

	int width, height;
	FNA3D_GetDrawableSize(window, &width, &height);

	FNA3D_PresentationParameters presentationParameters;
	SDL_memset(&presentationParameters, 0, sizeof(presentationParameters));
	presentationParameters.backBufferWidth = width;
	presentationParameters.backBufferHeight = height;
	presentationParameters.deviceWindowHandle = window;

	FNA3D_Device* device = FNA3D_CreateDevice(&presentationParameters, 1);

	FNA3D_SysRendererEXT d3dRenderingContext;
	d3dRenderingContext.version = 0;
	FNA3D_GetSysRendererEXT(device, &d3dRenderingContext);

	/* set up texture */

	int imageWidth, imageHeight, imageChannels;
	uint8_t* imagePixels = stbi_load("woodgrain.png", &imageWidth, &imageHeight, &imageChannels, 4);

	D3D11_TEXTURE2D_DESC desc;
	desc.Width = imageWidth;
	desc.Height = imageHeight;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	desc.MiscFlags = 0;

	D3D11_BOX dstBox;
	dstBox.left = 0;
	dstBox.top = 0;
	dstBox.front = 0;
	dstBox.right = imageWidth;
	dstBox.bottom = imageHeight;
	dstBox.back = 1;

	ID3D11Device *d3dDevice = (ID3D11Device*) d3dRenderingContext.renderer.d3d11.device;
	ID3D11DeviceContext* d3dContext = (ID3D11DeviceContext*) d3dRenderingContext.renderer.d3d11.context;
	ID3D11Texture2D* pTexture = NULL;
	ID3D11ShaderResourceView* pShaderResourceView = NULL;

	HRESULT result = d3dDevice->lpVtbl->CreateTexture2D(d3dDevice, &desc, NULL, &pTexture);
	d3dContext->lpVtbl->UpdateSubresource(
		d3dContext,
		(ID3D11Resource*) pTexture,
		0,
		&dstBox,
		imagePixels,
		imageWidth * 4,
		0
	);

	result = d3dDevice->lpVtbl->CreateShaderResourceView(d3dDevice, (ID3D11Resource*)pTexture, NULL, &pShaderResourceView);

	stbi_image_free(imagePixels);

	FNA3D_SysTextureEXT sysTextureInfo;
	sysTextureInfo.version = 0;
	sysTextureInfo.rendererType = FNA3D_RENDERER_TYPE_D3D11_EXT;
	sysTextureInfo.texture.d3d11.handle = pTexture;
	sysTextureInfo.texture.d3d11.shaderView = pShaderResourceView;

	FNA3D_Texture *sysTexture = FNA3D_CreateSysTextureEXT(device, &sysTextureInfo);

	/* load mojoshader effect */

	FNA3D_Effect* effect = NULL;
	MOJOSHADER_effect* effectData = NULL;

	FILE* effectFile = fopen("SpriteEffect.fxb", "rb");
	fseek(effectFile, 0, SEEK_END);
	uint32_t effectCodeLength = ftell(effectFile);
	fseek(effectFile, 0, SEEK_SET);
	uint8_t* effectCode = malloc(effectCodeLength);
	fread(effectCode, 1, effectCodeLength, effectFile);
	fclose(effectFile);
	FNA3D_CreateEffect(device, effectCode, effectCodeLength, &effect, &effectData);
	free(effectCode);

	/* set up FNA3D states */

	FNA3D_Viewport viewport;
	SDL_zero(viewport);
	viewport.w = width;
	viewport.h = height;
	FNA3D_SetViewport(device, &viewport);

	FNA3D_BlendState blendState;
	blendState.alphaBlendFunction = FNA3D_BLENDFUNCTION_ADD;
	blendState.alphaDestinationBlend = FNA3D_BLEND_INVERSESOURCEALPHA;
	blendState.alphaSourceBlend = FNA3D_BLEND_ONE;
	FNA3D_Color blendFactor = { 0xff, 0xff, 0xff, 0xff };
	blendState.blendFactor = blendFactor;
	blendState.colorBlendFunction = FNA3D_BLENDFUNCTION_ADD;
	blendState.colorDestinationBlend = FNA3D_BLEND_INVERSESOURCEALPHA;
	blendState.colorSourceBlend = FNA3D_BLEND_ONE;
	blendState.colorWriteEnable = FNA3D_COLORWRITECHANNELS_ALL;
	blendState.colorWriteEnable1 = FNA3D_COLORWRITECHANNELS_ALL;
	blendState.colorWriteEnable2 = FNA3D_COLORWRITECHANNELS_ALL;
	blendState.colorWriteEnable3 = FNA3D_COLORWRITECHANNELS_ALL;
	blendState.multiSampleMask = -1;
	FNA3D_SetBlendState(device, &blendState);

	FNA3D_DepthStencilState depthStencilState;
	SDL_zero(depthStencilState);
	depthStencilState.depthBufferEnable = 0;
	depthStencilState.stencilEnable = 0;
	FNA3D_SetDepthStencilState(device, &depthStencilState);

	FNA3D_RasterizerState rasterizerState;
	rasterizerState.cullMode = FNA3D_CULLMODE_NONE;
	rasterizerState.fillMode = FNA3D_FILLMODE_SOLID;
	rasterizerState.depthBias = 0;
	rasterizerState.multiSampleAntiAlias = 1;
	rasterizerState.scissorTestEnable = 0;
	rasterizerState.slopeScaleDepthBias = 0;
	FNA3D_ApplyRasterizerState(device, &rasterizerState);

	/* Vertex Buffer */

	FNA3D_VertexElement vertexElements[3];
	vertexElements[0].offset = 0;
	vertexElements[0].usageIndex = 0;
	vertexElements[0].vertexElementFormat = FNA3D_VERTEXELEMENTFORMAT_VECTOR2;
	vertexElements[0].vertexElementUsage = FNA3D_VERTEXELEMENTUSAGE_POSITION;

	vertexElements[1].offset = sizeof(float) * 2;
	vertexElements[1].usageIndex = 0;
	vertexElements[1].vertexElementFormat = FNA3D_VERTEXELEMENTFORMAT_VECTOR2;
	vertexElements[1].vertexElementUsage = FNA3D_VERTEXELEMENTUSAGE_TEXTURECOORDINATE;

	vertexElements[2].offset = sizeof(float) * 4;
	vertexElements[2].usageIndex = 0;
	vertexElements[2].vertexElementFormat = FNA3D_VERTEXELEMENTFORMAT_COLOR;
	vertexElements[2].vertexElementUsage = FNA3D_VERTEXELEMENTUSAGE_COLOR;

	FNA3D_VertexDeclaration vertexDeclaration;
	vertexDeclaration.elementCount = 3;
	vertexDeclaration.vertexStride = sizeof(Vertex);
	vertexDeclaration.elements = vertexElements;

	Vertex vertices[6] =
	{
		{ 50, 50, 0, 0, 0xffff0000 },
		{ 150, 50, 1, 0, 0xff0000ff },
		{ 150, 150, 1, 1, 0xff00ffff },
		{ 150, 150, 1, 1, 0xff00ffff },
		{ 50, 150, 0, 1, 0xff00ff00 },
		{ 50, 50, 0, 0, 0xffff0000 },
	};

	// vertex buffer
	FNA3D_Buffer* vertexBuffer = FNA3D_GenVertexBuffer(device, 0, FNA3D_BUFFERUSAGE_WRITEONLY, 6 * sizeof(Vertex));
	FNA3D_SetVertexBufferData(device, vertexBuffer, 0, vertices, sizeof(Vertex) * 6, 1, 1, FNA3D_SETDATAOPTIONS_NONE);

	FNA3D_VertexBufferBinding vertexBufferBinding;
	vertexBufferBinding.instanceFrequency = 0;
	vertexBufferBinding.vertexBuffer = vertexBuffer;
	vertexBufferBinding.vertexDeclaration = vertexDeclaration;
	vertexBufferBinding.vertexOffset = 0;

	/* SDL main loop */

	uint8_t quit = 0;

	double t = 0.0;
	double dt = 0.01;

	uint64_t currentTime = SDL_GetPerformanceCounter();
	double accumulator = 0.0;

	while (!quit)
	{
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			switch (event.type)
			{
			case SDL_QUIT:
				quit = 0;
				break;
			}
		}

		uint64_t newTime = SDL_GetPerformanceCounter();
		double frameTime = (newTime - currentTime) / (double)SDL_GetPerformanceFrequency();

		if (frameTime > 0.25)
			frameTime = 0.25;
		currentTime = newTime;

		accumulator += frameTime;

		uint8_t updateThisLoop = (accumulator >= dt);

		while (accumulator >= dt && !quit)
		{
			// Update here!

			t += dt;
			accumulator -= dt;
		}

		if (updateThisLoop && !quit)
		{
			// Draw here!

			FNA3D_Vec4 color = { 1, 1, 1, 1 };
			FNA3D_Clear(device, FNA3D_CLEAROPTIONS_TARGET, &color, 0, 0);

			MOJOSHADER_effectStateChanges stateChanges;
			SDL_zero(stateChanges);
			FNA3D_ApplyEffect(device, effect, 0, &stateChanges);

			FNA3D_SamplerState samplerState;
			memset(&samplerState, 0, sizeof(samplerState));
			samplerState.addressU = FNA3D_TEXTUREADDRESSMODE_CLAMP;
			samplerState.addressV = FNA3D_TEXTUREADDRESSMODE_CLAMP;
			samplerState.addressW = FNA3D_TEXTUREADDRESSMODE_WRAP;
			samplerState.filter = FNA3D_TEXTUREFILTER_LINEAR;
			samplerState.maxAnisotropy = 4;
			samplerState.maxMipLevel = 0;
			samplerState.mipMapLevelOfDetailBias = 0;
			FNA3D_VerifySampler(device, 0, sysTexture, &samplerState);

			for (int i = 0; i < effectData->param_count; i++)
			{
				if (strcmp("MatrixTransform", effectData->params[i].value.name) == 0)
				{
					// OrthographicOffCenter Matrix - value copied from XNA project
					// todo: Do I need to worry about row-major/column-major?
					float projectionMatrix[16] =
					{
						0.0015625f,
						0,
						0,
						-1,
						0,
						-0.00277777785f,
						0,
						1,
						0,
						0,
						1,
						0,
						0,
						0,
						0,
						1
					};
					memcpy(effectData->params[i].value.values, projectionMatrix, sizeof(float) * 16);
					break;
				}
			}

			// draw primitives
			FNA3D_ApplyVertexBufferBindings(device, &vertexBufferBinding, 1, 0, 0);
			FNA3D_DrawPrimitives(device, FNA3D_PRIMITIVETYPE_TRIANGLELIST, 0, 2);

			FNA3D_SwapBuffers(device, NULL, NULL, window);
		}
	}

	/* TODO: clean up */

	FNA3D_DestroyDevice(device);

	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}
