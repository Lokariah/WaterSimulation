//--------------------------------------------------------------------------------------
// Scene geometry and layout preparation
// Scene rendering & update
//--------------------------------------------------------------------------------------

#include "Scene.h"
#include "Mesh.h"
#include "Model.h"
#include "Camera.h"
#include "State.h"
#include "Shader.h"
#include "Input.h"
#include "Common.h"

#include "CVector2.h" 
#include "CVector3.h" 
#include "CMatrix4x4.h"
#include "MathHelpers.h"     // Helper functions for maths
#include "GraphicsHelpers.h" // Helper functions to unclutter the code here
#include "ColourRGBA.h" 
#include "CWaterGrid.h"

#include <array>
#include <sstream>
#include <memory>

enum class PostProcess {
	None,
	Copy,
	SSRPrep,
	SSR
};

//********************


// Constants controlling speed of movement/rotation (measured in units per second because we're using frame time)
const float ROTATION_SPEED = 1.5f;  // Radians per second for rotation
const float MOVEMENT_SPEED = 50.0f; // Units per second for movement (what a unit of length is depends on 3D model - i.e. an artist decision usually)

// Lock FPS to monitor refresh rate, which will typically set it to 60fps. Press 'p' to toggle to full fps
bool lockFPS = true;
static bool cubeMapRenderToggle = false;

// Meshes, models and cameras, same meaning as TL-Engine. Meshes prepared in InitGeometry function, Models & camera in InitScene
Mesh* gStarsMesh;
Mesh* gGroundMesh;
Mesh* gLightMesh;
Mesh* gCargoMesh;
Mesh* gWaveMesh;

Model* gStars;
Model* gGround;
Model* gCargo;
Model* gVisualTestGrid;
Camera* gCamera;
Camera* gCubeMapCameras[6];
CWaveGrid* gWaveGrid;

// Store lights in an array in this exercise
const int NUM_LIGHTS = 2;
struct Light
{
	Model*   model;
	CVector3 colour;
	float    strength;
};
Light gLights[NUM_LIGHTS];


// Additional light information
CVector3 gAmbientColour = { 0.3f, 0.3f, 0.4f }; // Background level of light (slightly bluish to match the far background, which is dark blue)
float    gSpecularPower = 256; // Specular power controls shininess - same for all models in this app

ColourRGBA gBackgroundColor = { 0.3f, 0.3f, 0.4f, 1.0f };

// Variables controlling light1's orbiting of the cube
const float gLightOrbitRadius = 20.0f;
const float gLightOrbitSpeed = 0.7f;

//Variables relating to the water visuals
static float waterRefractiveIndex = 1.33f;
static const int sceneCubeMapSize = 2056;


//--------------------------------------------------------------------------------------
// Constant Buffers
//--------------------------------------------------------------------------------------
// Variables sent over to the GPU each frame
// The structures are now in Common.h
// IMPORTANT: Any new data you add in C++ code (CPU-side) is not automatically available to the GPU
//            Anything the shaders need (per-frame or per-model) needs to be sent via a constant buffer

PerFrameConstants gPerFrameConstants;      // The constants (settings) that need to be sent to the GPU each frame (see common.h for structure)
ID3D11Buffer*     gPerFrameConstantBuffer; // The GPU buffer that will recieve the constants above

PerModelConstants gPerModelConstants;      // As above, but constants (settings) that change per-model (e.g. world matrix)
ID3D11Buffer*     gPerModelConstantBuffer; // --"--

//**************************
PostProcessingConstants gPostProcessingConstants;       // As above, but constants (settings) for each post-process
ID3D11Buffer*           gPostProcessingConstantBuffer; // --"--
//**************************


//--------------------------------------------------------------------------------------
// Textures
//--------------------------------------------------------------------------------------

// DirectX objects controlling textures used in this lab
ID3D11Resource*           gStarsDiffuseSpecularMap = nullptr;
ID3D11ShaderResourceView* gStarsDiffuseSpecularMapSRV = nullptr;
ID3D11Resource*           gGroundDiffuseSpecularMap = nullptr;
ID3D11ShaderResourceView* gGroundDiffuseSpecularMapSRV = nullptr;
ID3D11Resource*           gCrateDiffuseSpecularMap = nullptr;
ID3D11ShaderResourceView* gCrateDiffuseSpecularMapSRV = nullptr;
ID3D11Resource*           gCubeDiffuseSpecularMap = nullptr;
ID3D11ShaderResourceView* gCubeDiffuseSpecularMapSRV = nullptr;


ID3D11Resource*           gLightDiffuseMap = nullptr;
ID3D11ShaderResourceView* gLightDiffuseMapSRV = nullptr;


//****************************

ID3D11Texture2D*			gSceneTexture = nullptr;
ID3D11RenderTargetView*		gSceneRenderTarget = nullptr;
ID3D11ShaderResourceView*	gSceneTextureSRV = nullptr;

ID3D11Texture2D*			gReflectionTexture = nullptr;
ID3D11RenderTargetView*		gReflectionRenderTarget = nullptr;
ID3D11ShaderResourceView*	gReflectionTextureSRV = nullptr;

ID3D11Texture2D*			gRefractionTexture = nullptr;
ID3D11RenderTargetView*		gRefractionRenderTarget = nullptr;
ID3D11ShaderResourceView*	gRefractionTextureSRV = nullptr;

ID3D11Texture2D*			gPositionTexture = nullptr;
ID3D11RenderTargetView*		gPositionRenderTarget = nullptr;
ID3D11ShaderResourceView*	gPositionTextureSRV = nullptr;

ID3D11Texture2D*			gNormalTexture = nullptr;
ID3D11RenderTargetView*		gNormalRenderTarget = nullptr;
ID3D11ShaderResourceView*	gNormalTextureSRV = nullptr;

ID3D11Texture2D*			gSpecularTexture = nullptr;
ID3D11RenderTargetView*		gSpecularRenderTarget = nullptr;
ID3D11ShaderResourceView*	gSpecularTextureSRV = nullptr;

ID3D11Texture2D*			gAlternativeSceneTexture = nullptr;
ID3D11RenderTargetView*		gAlternativeSceneRenderTarget = nullptr;
ID3D11ShaderResourceView*	gAlternativeSceneTextureSRV = nullptr;

ID3D11Texture2D*			gSceneHeightTexture = nullptr;
ID3D11RenderTargetView*		gSceneHeightRenderTarget = nullptr;
ID3D11ShaderResourceView*	gSceneHeightTextureSRV = nullptr;

ID3D11Texture2D*			gSceneCubeMapTexture = nullptr;
ID3D11RenderTargetView*		gSceneCubeMapRenderTarget[6];
ID3D11ShaderResourceView*	gSceneCubeMapTextureSRV = nullptr;
ID3D11Texture2D*			gSceneCubeMapDepthTexture = nullptr;
ID3D11DepthStencilView*		gSceneCubeMapDSV = nullptr;
D3D11_VIEWPORT				gSceneCubeMapViewport;

//--------------------------------------------------------------------------------------
// Initialise scene geometry, constant buffers and states
//--------------------------------------------------------------------------------------

// Prepare the geometry required for the scene
// Returns true on success
bool InitGeometry()
{
	////--------------- Load meshes ---------------////

	// Load mesh geometry data, just like TL-Engine this doesn't create anything in the scene. Create a Model for that.
	try
	{
		gStarsMesh  = new Mesh("Stars.x");
		gGroundMesh = new Mesh("Hills.x");
		gLightMesh  = new Mesh("Light.x");
		gCargoMesh = new Mesh("Cube.x");
		gWaveMesh = new Mesh(CVector3(-500, 0, -500), CVector3(500, 0, 500), 2000, 2000, true, true);
	}
	catch (std::runtime_error e)  // Constructors cannot return error messages so use exceptions to catch mesh errors (fairly standard approach this)
	{
		gLastError = e.what(); // This picks up the error message put in the exception (see Mesh.cpp)
		return false;
	}


	////--------------- Load / prepare textures & GPU states ---------------////

	// Load textures and create DirectX objects for them
	// The LoadTexture function requires you to pass a ID3D11Resource* (e.g. &gCubeDiffuseMap), which manages the GPU memory for the
	// texture and also a ID3D11ShaderResourceView* (e.g. &gCubeDiffuseMapSRV), which allows us to use the texture in shaders
	// The function will fill in these pointers with usable data. The variables used here are globals found near the top of the file.
	if (!LoadTexture("Stars.jpg",                &gStarsDiffuseSpecularMap,  &gStarsDiffuseSpecularMapSRV) ||
		!LoadTexture("GrassDiffuseSpecular.dds", &gGroundDiffuseSpecularMap, &gGroundDiffuseSpecularMapSRV) ||
		!LoadTexture("StoneDiffuseSpecular.dds", &gCubeDiffuseSpecularMap,   &gCubeDiffuseSpecularMapSRV) ||
		!LoadTexture("CargoA.dds",               &gCrateDiffuseSpecularMap,  &gCrateDiffuseSpecularMapSRV) ||
		!LoadTexture("Flare.jpg",                &gLightDiffuseMap,          &gLightDiffuseMapSRV)) {
		gLastError = "Error loading textures";
		return false;
	}


	// Create all filtering modes, blending modes etc. used by the app (see State.cpp/.h)
	if (!CreateStates())
	{
		gLastError = "Error creating states";
		return false;
	}


	////--------------- Prepare shaders and constant buffers to communicate with them ---------------////

	// Load the shaders required for the geometry we will use (see Shader.cpp / .h)
	if (!LoadShaders())
	{
		gLastError = "Error loading shaders";
		return false;
	}

	// Create GPU-side constant buffers to receive the gPerFrameConstants and gPerModelConstants structures above
	// These allow us to pass data from CPU to shaders such as lighting information or matrices
	// See the comments above where these variable are declared and also the UpdateScene function
	gPerFrameConstantBuffer       = CreateConstantBuffer(sizeof(gPerFrameConstants));
	gPerModelConstantBuffer       = CreateConstantBuffer(sizeof(gPerModelConstants));
	gPostProcessingConstantBuffer = CreateConstantBuffer(sizeof(gPostProcessingConstants));
	if (gPerFrameConstantBuffer == nullptr || gPerModelConstantBuffer == nullptr || gPostProcessingConstantBuffer == nullptr)
	{
		gLastError = "Error creating constant buffers";
		return false;
	}

	//--------------------------------------------------------------------------------------------
	// Scene Textures
	//--------------------------------------------------------------------------------------------

	D3D11_TEXTURE2D_DESC sceneTextureDesc = {};
	sceneTextureDesc.Width = gViewportWidth;
	sceneTextureDesc.Height = gViewportHeight;
	sceneTextureDesc.MipLevels = 1;
	sceneTextureDesc.ArraySize = 1;
	sceneTextureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sceneTextureDesc.SampleDesc.Count = 1;
	sceneTextureDesc.SampleDesc.Quality = 0;
	sceneTextureDesc.Usage = D3D11_USAGE_DEFAULT;
	sceneTextureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	sceneTextureDesc.CPUAccessFlags = 0;
	sceneTextureDesc.MiscFlags = 0;
	if (FAILED(gD3DDevice->CreateTexture2D(&sceneTextureDesc, NULL, &gSceneTexture))) {
		gLastError = "Error Creating gSceneTexture";
		return false;
	}

	if (FAILED(gD3DDevice->CreateRenderTargetView(gSceneTexture, NULL, &gSceneRenderTarget))) {
		gLastError = "Error Creating gSceneRenderTarget";
		return false;
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC srDesc = {};
	srDesc.Format = sceneTextureDesc.Format;
	srDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srDesc.Texture2D.MostDetailedMip = 0;
	srDesc.Texture2D.MipLevels = 1;
	if (FAILED(gD3DDevice->CreateShaderResourceView(gSceneTexture, &srDesc, &gSceneTextureSRV))) {
		gLastError = "Error Creating gSceneTextureSRV";
		return false;
	}

	//----------------------------------------------------------------------

	D3D11_TEXTURE2D_DESC sceneAlternativeTextureDesc = {};
	sceneAlternativeTextureDesc.Width = gViewportWidth;
	sceneAlternativeTextureDesc.Height = gViewportHeight;
	sceneAlternativeTextureDesc.MipLevels = 1;
	sceneAlternativeTextureDesc.ArraySize = 1;
	sceneAlternativeTextureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sceneAlternativeTextureDesc.SampleDesc.Count = 1;
	sceneAlternativeTextureDesc.SampleDesc.Quality = 0;
	sceneAlternativeTextureDesc.Usage = D3D11_USAGE_DEFAULT;
	sceneAlternativeTextureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	sceneAlternativeTextureDesc.CPUAccessFlags = 0;
	sceneAlternativeTextureDesc.MiscFlags = 0;
	if (FAILED(gD3DDevice->CreateTexture2D(&sceneAlternativeTextureDesc, NULL, &gAlternativeSceneTexture))) {
		gLastError = "Error Creating gAlternativeSceneTexture";
		return false;
	}

	if (FAILED(gD3DDevice->CreateRenderTargetView(gAlternativeSceneTexture, NULL, &gAlternativeSceneRenderTarget))) {
		gLastError = "Error Creating gAlternativeSceneRenderTarget";
		return false;
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC srAlternativeDesc = {};
	srAlternativeDesc.Format = sceneAlternativeTextureDesc.Format;
	srAlternativeDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srAlternativeDesc.Texture2D.MostDetailedMip = 0;
	srAlternativeDesc.Texture2D.MipLevels = 1;
	if (FAILED(gD3DDevice->CreateShaderResourceView(gAlternativeSceneTexture, &srAlternativeDesc, &gAlternativeSceneTextureSRV))) {
		gLastError = "Error Creating gAlternativeSceneTextureSRV";
		return false;
	}

	//----------------------------------------------------------------------

	D3D11_TEXTURE2D_DESC sceneHeightTextureDesc = {};
	sceneHeightTextureDesc.Width = gViewportWidth;
	sceneHeightTextureDesc.Height = gViewportHeight;
	sceneHeightTextureDesc.MipLevels = 1;
	sceneHeightTextureDesc.ArraySize = 1;
	sceneHeightTextureDesc.Format = DXGI_FORMAT_R8_UNORM;
	sceneHeightTextureDesc.SampleDesc.Count = 1;
	sceneHeightTextureDesc.SampleDesc.Quality = 0;
	sceneHeightTextureDesc.Usage = D3D11_USAGE_DEFAULT;
	sceneHeightTextureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	sceneHeightTextureDesc.CPUAccessFlags = 0;
	sceneHeightTextureDesc.MiscFlags = 0;
	if (FAILED(gD3DDevice->CreateTexture2D(&sceneHeightTextureDesc, NULL, &gSceneHeightTexture))) {
		gLastError = "Error Creating gSceneHeightTexture";
		return false;
	}

	if (FAILED(gD3DDevice->CreateRenderTargetView(gSceneHeightTexture, NULL, &gSceneHeightRenderTarget))) {
		gLastError = "Error Creating gSceneHeightRenderTarget";
		return false;
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC srHeightDesc = {};
	srHeightDesc.Format = sceneHeightTextureDesc.Format;
	srHeightDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srHeightDesc.Texture2D.MostDetailedMip = 0;
	srHeightDesc.Texture2D.MipLevels = 1;
	if (FAILED(gD3DDevice->CreateShaderResourceView(gSceneHeightTexture, &srHeightDesc, &gSceneHeightTextureSRV))) {
		gLastError = "Error Creating gSceneHeightTextureSRV";
		return false;
	}

	//----------------------------------------------------------------------

	D3D11_TEXTURE2D_DESC sceneReflectionTextureDesc = {};
	sceneReflectionTextureDesc.Width = gViewportWidth;
	sceneReflectionTextureDesc.Height = gViewportHeight;
	sceneReflectionTextureDesc.MipLevels = 1;
	sceneReflectionTextureDesc.ArraySize = 1;
	sceneReflectionTextureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sceneReflectionTextureDesc.SampleDesc.Count = 1;
	sceneReflectionTextureDesc.SampleDesc.Quality = 0;
	sceneReflectionTextureDesc.Usage = D3D11_USAGE_DEFAULT;
	sceneReflectionTextureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	sceneReflectionTextureDesc.CPUAccessFlags = 0;
	sceneReflectionTextureDesc.MiscFlags = 0;
	if (FAILED(gD3DDevice->CreateTexture2D(&sceneReflectionTextureDesc, NULL, &gReflectionTexture))) {
		gLastError = "Error Creating gAlternativeSceneTexture";
		return false;
	}

	if (FAILED(gD3DDevice->CreateRenderTargetView(gReflectionTexture, NULL, &gReflectionRenderTarget))) {
		gLastError = "Error Creating gAlternativeSceneRenderTarget";
		return false;
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC srReflectionDesc = {};
	srReflectionDesc.Format = sceneReflectionTextureDesc.Format;
	srReflectionDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srReflectionDesc.Texture2D.MostDetailedMip = 0;
	srReflectionDesc.Texture2D.MipLevels = 1;
	if (FAILED(gD3DDevice->CreateShaderResourceView(gReflectionTexture, &srReflectionDesc, &gReflectionTextureSRV))) {
		gLastError = "Error Creating gAlternativeSceneTextureSRV";
		return false;
	}

	//----------------------------------------------------------------------

	D3D11_TEXTURE2D_DESC sceneRefractionTextureDesc = {};
	sceneRefractionTextureDesc.Width = gViewportWidth;
	sceneRefractionTextureDesc.Height = gViewportHeight;
	sceneRefractionTextureDesc.MipLevels = 1;
	sceneRefractionTextureDesc.ArraySize = 1;
	sceneRefractionTextureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sceneRefractionTextureDesc.SampleDesc.Count = 1;
	sceneRefractionTextureDesc.SampleDesc.Quality = 0;
	sceneRefractionTextureDesc.Usage = D3D11_USAGE_DEFAULT;
	sceneRefractionTextureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	sceneRefractionTextureDesc.CPUAccessFlags = 0;
	sceneRefractionTextureDesc.MiscFlags = 0;
	if (FAILED(gD3DDevice->CreateTexture2D(&sceneRefractionTextureDesc, NULL, &gRefractionTexture))) {
		gLastError = "Error Creating gAlternativeSceneTexture";
		return false;
	}

	if (FAILED(gD3DDevice->CreateRenderTargetView(gRefractionTexture, NULL, &gRefractionRenderTarget))) {
		gLastError = "Error Creating gAlternativeSceneRenderTarget";
		return false;
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC srRefractionDesc = {};
	srRefractionDesc.Format = sceneRefractionTextureDesc.Format;
	srRefractionDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srRefractionDesc.Texture2D.MostDetailedMip = 0;
	srRefractionDesc.Texture2D.MipLevels = 1;
	if (FAILED(gD3DDevice->CreateShaderResourceView(gRefractionTexture, &srRefractionDesc, &gRefractionTextureSRV))) {
		gLastError = "Error Creating gAlternativeSceneTextureSRV";
		return false;
	}

	//----------------------------------------------------------------------

	D3D11_TEXTURE2D_DESC scenePositionTextureDesc = {};
	scenePositionTextureDesc.Width = gViewportWidth;
	scenePositionTextureDesc.Height = gViewportHeight;
	scenePositionTextureDesc.MipLevels = 1;
	scenePositionTextureDesc.ArraySize = 1;
	scenePositionTextureDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	scenePositionTextureDesc.SampleDesc.Count = 1;
	scenePositionTextureDesc.SampleDesc.Quality = 0;
	scenePositionTextureDesc.Usage = D3D11_USAGE_DEFAULT;
	scenePositionTextureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	scenePositionTextureDesc.CPUAccessFlags = 0;
	scenePositionTextureDesc.MiscFlags = 0;
	if (FAILED(gD3DDevice->CreateTexture2D(&scenePositionTextureDesc, NULL, &gPositionTexture))) {
		gLastError = "Error Creating gAlternativeSceneTexture";
		return false;
	}

	if (FAILED(gD3DDevice->CreateRenderTargetView(gPositionTexture, NULL, &gPositionRenderTarget))) {
		gLastError = "Error Creating gAlternativeSceneRenderTarget";
		return false;
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC srPositionDesc = {};
	srPositionDesc.Format = scenePositionTextureDesc.Format;
	srPositionDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srPositionDesc.Texture2D.MostDetailedMip = 0;
	srPositionDesc.Texture2D.MipLevels = 1;
	if (FAILED(gD3DDevice->CreateShaderResourceView(gPositionTexture, &srPositionDesc, &gPositionTextureSRV))) {
		gLastError = "Error Creating gAlternativeSceneTextureSRV";
		return false;
	}

	//----------------------------------------------------------------------

	D3D11_TEXTURE2D_DESC sceneNormalTextureDesc = {};
	sceneNormalTextureDesc.Width = gViewportWidth;
	sceneNormalTextureDesc.Height = gViewportHeight;
	sceneNormalTextureDesc.MipLevels = 1;
	sceneNormalTextureDesc.ArraySize = 1;
	sceneNormalTextureDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	sceneNormalTextureDesc.SampleDesc.Count = 1;
	sceneNormalTextureDesc.SampleDesc.Quality = 0;
	sceneNormalTextureDesc.Usage = D3D11_USAGE_DEFAULT;
	sceneNormalTextureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	sceneNormalTextureDesc.CPUAccessFlags = 0;
	sceneNormalTextureDesc.MiscFlags = 0;
	if (FAILED(gD3DDevice->CreateTexture2D(&sceneNormalTextureDesc, NULL, &gNormalTexture))) {
		gLastError = "Error Creating gAlternativeSceneTexture";
		return false;
	}

	if (FAILED(gD3DDevice->CreateRenderTargetView(gNormalTexture, NULL, &gNormalRenderTarget))) {
		gLastError = "Error Creating gAlternativeSceneRenderTarget";
		return false;
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC srNormalDesc = {};
	srNormalDesc.Format = sceneNormalTextureDesc.Format;
	srNormalDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srNormalDesc.Texture2D.MostDetailedMip = 0;
	srNormalDesc.Texture2D.MipLevels = 1;
	if (FAILED(gD3DDevice->CreateShaderResourceView(gNormalTexture, &srNormalDesc, &gNormalTextureSRV))) {
		gLastError = "Error Creating gAlternativeSceneTextureSRV";
		return false;
	}

	//----------------------------------------------------------------------

	D3D11_TEXTURE2D_DESC sceneSpecularTextureDesc = {};
	sceneSpecularTextureDesc.Width = gViewportWidth;
	sceneSpecularTextureDesc.Height = gViewportHeight;
	sceneSpecularTextureDesc.MipLevels = 1;
	sceneSpecularTextureDesc.ArraySize = 1;
	sceneSpecularTextureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sceneSpecularTextureDesc.SampleDesc.Count = 1;
	sceneSpecularTextureDesc.SampleDesc.Quality = 0;
	sceneSpecularTextureDesc.Usage = D3D11_USAGE_DEFAULT;
	sceneSpecularTextureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	sceneSpecularTextureDesc.CPUAccessFlags = 0;
	sceneSpecularTextureDesc.MiscFlags = 0;
	if (FAILED(gD3DDevice->CreateTexture2D(&sceneSpecularTextureDesc, NULL, &gSpecularTexture))) {
		gLastError = "Error Creating gSpecularTexture";
		return false;
	}

	if (FAILED(gD3DDevice->CreateRenderTargetView(gSpecularTexture, NULL, &gSpecularRenderTarget))) {
		gLastError = "Error Creating gSpecularRenderTarget";
		return false;
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC srSpecularDesc = {};
	srSpecularDesc.Format = sceneSpecularTextureDesc.Format;
	srSpecularDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srSpecularDesc.Texture2D.MostDetailedMip = 0;
	srSpecularDesc.Texture2D.MipLevels = 1;
	if (FAILED(gD3DDevice->CreateShaderResourceView(gSpecularTexture, &srSpecularDesc, &gSpecularTextureSRV))) {
		gLastError = "Error Creating gSpecularTextureSRV";
		return false;
	}

	//----------------------------------------------------------------------

	D3D11_TEXTURE2D_DESC sceneCubeMapTextureDesc = {};
	sceneCubeMapTextureDesc.Width = sceneCubeMapSize;
	sceneCubeMapTextureDesc.Height = sceneCubeMapSize;
	sceneCubeMapTextureDesc.MipLevels = 0;
	sceneCubeMapTextureDesc.ArraySize = 6;
	sceneCubeMapTextureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sceneCubeMapTextureDesc.SampleDesc.Count = 1;
	sceneCubeMapTextureDesc.SampleDesc.Quality = 0;
	sceneCubeMapTextureDesc.Usage = D3D11_USAGE_DEFAULT;
	sceneCubeMapTextureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	sceneCubeMapTextureDesc.CPUAccessFlags = 0;
	sceneCubeMapTextureDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS | D3D11_RESOURCE_MISC_TEXTURECUBE;
	if (FAILED(gD3DDevice->CreateTexture2D(&sceneCubeMapTextureDesc, NULL, &gSceneCubeMapTexture))) {
		gLastError = "Error Creating gSceneCubeMapTexture";
		return false;
	}

	D3D11_RENDER_TARGET_VIEW_DESC sceneCubeMapRTVDesc;
	sceneCubeMapRTVDesc.Format = sceneCubeMapTextureDesc.Format;
	sceneCubeMapRTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
	sceneCubeMapRTVDesc.Texture2DArray.MipSlice = 0;
	sceneCubeMapRTVDesc.Texture2DArray.ArraySize = 1;

	for (int i = 0; i < 6; ++i) {
		sceneCubeMapRTVDesc.Texture2DArray.FirstArraySlice = i;
		if (FAILED(gD3DDevice->CreateRenderTargetView(gSceneCubeMapTexture, &sceneCubeMapRTVDesc, &gSceneCubeMapRenderTarget[i]))) {
			gLastError = "Error Creating gSceneCubeMapRenderTarget" + i;
			return false;
		}
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC srCubeMapDesc = {};
	srCubeMapDesc.Format = sceneCubeMapTextureDesc.Format;
	srCubeMapDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
	srCubeMapDesc.Texture2D.MostDetailedMip = 0;
	srCubeMapDesc.Texture2D.MipLevels = 1;
	if (FAILED(gD3DDevice->CreateShaderResourceView(gSceneCubeMapTexture, &srCubeMapDesc, &gSceneCubeMapTextureSRV))) {
		gLastError = "Error Creating gSceneCubeMapTextureSRV";
		return false;
	}

	D3D11_TEXTURE2D_DESC sceneCubeMapDepthTextureDesc = {};
	sceneCubeMapDepthTextureDesc.Width = sceneCubeMapSize;
	sceneCubeMapDepthTextureDesc.Height = sceneCubeMapSize;
	sceneCubeMapDepthTextureDesc.MipLevels = 1;
	sceneCubeMapDepthTextureDesc.ArraySize = 1;
	sceneCubeMapDepthTextureDesc.Format = DXGI_FORMAT_D32_FLOAT;
	sceneCubeMapDepthTextureDesc.SampleDesc.Count = 1;
	sceneCubeMapDepthTextureDesc.SampleDesc.Quality = 0;
	sceneCubeMapDepthTextureDesc.Usage = D3D11_USAGE_DEFAULT;
	sceneCubeMapDepthTextureDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	sceneCubeMapDepthTextureDesc.CPUAccessFlags = 0;
	sceneCubeMapDepthTextureDesc.MiscFlags = 0;
	if (FAILED(gD3DDevice->CreateTexture2D(&sceneCubeMapDepthTextureDesc, NULL, &gSceneCubeMapDepthTexture))) {
		gLastError = "Error Creating gSceneCubeMapDepthTexture";
		return false;
	}

	D3D11_DEPTH_STENCIL_VIEW_DESC sceneCubeMapDepthSVDesc = {};
	sceneCubeMapDepthSVDesc.Format = sceneCubeMapDepthTextureDesc.Format;
	sceneCubeMapDepthSVDesc.Flags = 0;
	sceneCubeMapDepthSVDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	sceneCubeMapDepthSVDesc.Texture2D.MipSlice = 0;

	if (FAILED(gD3DDevice->CreateDepthStencilView(gSceneCubeMapDepthTexture, &sceneCubeMapDepthSVDesc, &gSceneCubeMapDSV))) {
		gLastError = "Error Creating gSceneCubeMapDSV";
		return false;
	}

	gSceneCubeMapViewport.TopLeftX = 0.0f;
	gSceneCubeMapViewport.TopLeftY = 0.0f;
	gSceneCubeMapViewport.Width = (float)sceneCubeMapSize;
	gSceneCubeMapViewport.Height = (float)sceneCubeMapSize;
	gSceneCubeMapViewport.MinDepth = 0.0f;
	gSceneCubeMapViewport.MaxDepth = 1.0f;


	return true;
}


// Prepare the scene
// Returns true on success
bool InitScene()
{
	////--------------- Set up scene ---------------////

	gStars = new Model(gStarsMesh);
	gGround = new Model(gGroundMesh);
	gWaveGrid = new CWaveGrid(32, 0.0005f, { 16.0f, 16.0f }, 32);
	gVisualTestGrid = new Model(gWaveMesh);
	gCargo = new Model(gCargoMesh);
	// Initial positions
	gStars->SetScale(8000.0f);
	gWaveGrid->mWaterGridModel->SetPosition({ 0, 17.5f, 0 });
	gCargo->SetScale(10.0f);
	gCargo->SetPosition({ 0.0f, -100.0f, -120.0f });
	gGround->SetPosition({0.0f, 0.0f, -10.0f});
	// Light set-up - using an array this time
	for (int i = 0; i < NUM_LIGHTS; ++i)
	{
		gLights[i].model = new Model(gLightMesh);
	}

	gLights[0].colour = { 0.8f, 0.8f, 1.0f };
	gLights[0].strength = 10;
	gLights[0].model->SetPosition({ 30, 10, 0 });
	gLights[0].model->SetScale(pow(gLights[0].strength, 1.0f)); // Convert light strength into a nice value for the scale of the light - equation is ad-hoc.

	gLights[1].colour = { 1.0f, 0.8f, 0.2f };
	gLights[1].strength = 40;
	gLights[1].model->SetPosition({ -70, 30, 100 });
	gLights[1].model->SetScale(pow(gLights[1].strength, 1.0f));


	////--------------- Set up camera ---------------////

	gCamera = new Camera();
	gCamera->SetPosition({ 25, 18, -45 });
	gCamera->SetRotation({ ToRadians(10.0f), ToRadians(7.0f), 0.0f });

	for (int i = 0; i < 6; i++) {
		gCubeMapCameras[i] = new Camera();
	}
	return true;
}


// Release the geometry and scene resources created above
void ReleaseResources()
{
	ReleaseStates();
	if (gLightDiffuseMapSRV)           gLightDiffuseMapSRV->Release();
	if (gLightDiffuseMap)              gLightDiffuseMap->Release();
	if (gCrateDiffuseSpecularMapSRV)   gCrateDiffuseSpecularMapSRV->Release();
	if (gCrateDiffuseSpecularMap)      gCrateDiffuseSpecularMap->Release();
	if (gCubeDiffuseSpecularMapSRV)    gCubeDiffuseSpecularMapSRV->Release();
	if (gCubeDiffuseSpecularMap)       gCubeDiffuseSpecularMap->Release();
	if (gGroundDiffuseSpecularMapSRV)  gGroundDiffuseSpecularMapSRV->Release();
	if (gGroundDiffuseSpecularMap)     gGroundDiffuseSpecularMap->Release();
	if (gStarsDiffuseSpecularMapSRV)   gStarsDiffuseSpecularMapSRV->Release();
	if (gStarsDiffuseSpecularMap)      gStarsDiffuseSpecularMap->Release();

	if (gPostProcessingConstantBuffer)  gPostProcessingConstantBuffer->Release();
	if (gPerModelConstantBuffer)        gPerModelConstantBuffer->Release();
	if (gPerFrameConstantBuffer)        gPerFrameConstantBuffer->Release();

	if (gSceneTextureSRV)				gSceneTextureSRV->Release();
	if (gSceneRenderTarget)				gSceneRenderTarget->Release();
	if (gSceneTexture)					gSceneTexture->Release();

	if (gReflectionTextureSRV)			gReflectionTextureSRV->Release();
	if (gReflectionRenderTarget)		gReflectionRenderTarget->Release();
	if (gReflectionTexture)				gReflectionTexture->Release();

	if (gRefractionTextureSRV)			gRefractionTextureSRV->Release();
	if (gRefractionRenderTarget)		gRefractionRenderTarget->Release();
	if (gRefractionTexture)				gRefractionTexture->Release();

	if (gPositionTextureSRV)			gPositionTextureSRV->Release();
	if (gPositionRenderTarget)			gPositionRenderTarget->Release();
	if (gPositionTexture)				gPositionTexture->Release();

	if (gNormalTextureSRV)				gNormalTextureSRV->Release();
	if (gNormalRenderTarget)			gNormalRenderTarget->Release();
	if (gNormalTexture)					gNormalTexture->Release();

	if (gSpecularTextureSRV)			gSpecularTextureSRV->Release();
	if (gSpecularRenderTarget)			gSpecularRenderTarget->Release();
	if (gSpecularTexture)				gSpecularTexture->Release();

	if (gAlternativeSceneTextureSRV)	gAlternativeSceneTextureSRV->Release();
	if (gAlternativeSceneRenderTarget)	gAlternativeSceneRenderTarget->Release();
	if (gAlternativeSceneTexture)		gAlternativeSceneTexture->Release();

	if (gSceneHeightTextureSRV)			gSceneHeightTextureSRV->Release();
	if (gSceneHeightRenderTarget)		gSceneHeightRenderTarget->Release();
	if (gSceneHeightTexture)			gSceneHeightTexture->Release();

	if (gSceneCubeMapTexture) gSceneCubeMapTexture->Release();
	for(int i = 0; i < 6; i++) if (gSceneCubeMapRenderTarget[i]) gSceneCubeMapRenderTarget[i]->Release();
	if (gSceneCubeMapTextureSRV) gSceneCubeMapTextureSRV->Release();
	if (gSceneCubeMapDepthTexture)gSceneCubeMapDepthTexture->Release();
	if (gSceneCubeMapDSV)gSceneCubeMapDSV->Release();

	ReleaseShaders();

	// See note in InitGeometry about why we're not using unique_ptr and having to manually delete
	for (int i = 0; i < NUM_LIGHTS; ++i)
	{
		delete gLights[i].model;  gLights[i].model = nullptr;
	}
	delete gCamera;  gCamera = nullptr;
	delete gGround;  gGround = nullptr;
	delete gStars;   gStars = nullptr;
	delete gCargo; gCargo = nullptr;

	delete gWaveGrid; gWaveGrid = nullptr;
	delete gVisualTestGrid; gVisualTestGrid = nullptr;
	delete gLightMesh;   gLightMesh = nullptr;
	delete gGroundMesh;  gGroundMesh = nullptr;
	delete gStarsMesh;   gStarsMesh = nullptr;
	delete gCargoMesh; gCargoMesh = nullptr;
	delete gWaveMesh; gWaveMesh = nullptr;
}

void BuildCubeCameras(CVector3 pos) {
	CVector3 worldUp = { 0.0f, 1.0f, 0.0f };
	
	CVector3 targets[6] = {
		{pos.x + 1.0f, pos.y, pos.z },
		{pos.x - 1.0f, pos.y, pos.z },
		{pos.x, pos.y, pos.z + 1.0f },
		{pos.x, pos.y, pos.z - 1.0f },
		{pos.x, pos.y + 1.0f, pos.z },
		{pos.x, pos.y - 1.0f, pos.z }
	};

	CVector3 ups[6] = {
		{0.0f, ToRadians(90.0f), 0.0f},
		{0.0f, ToRadians(180.0f), 0.0f},
		{ToRadians(-90.0f), 0.0f, 0.0f},
		{ToRadians(90.0f), 0.0f, 0.0f},
		{0.0f, ToRadians(-90.0f), 0.0f},
		{0.0f, ToRadians(0.0f), 0.0f}

	};

	for (int i = 0; i < 6; i++) {
		gCubeMapCameras[i]->SetPosition(pos);
		gCubeMapCameras[i]->SetRotation(ups[i]);
	}
}

//--------------------------------------------------------------------------------------
// Scene Rendering
//--------------------------------------------------------------------------------------

// Render everything in the scene from the given camera																 //////Hook Up The World Height Render using the Bool bRenderReflectantObjects
void RenderSceneFromCamera(Camera* camera, bool bRenderReflectantObjects)
{
	// Set camera matrices in the constant buffer and send over to GPU
	gPerFrameConstants.cameraMatrix = camera->WorldMatrix();
	gPerFrameConstants.viewMatrix = camera->ViewMatrix();
	gPerFrameConstants.projectionMatrix = camera->ProjectionMatrix();
	gPostProcessingConstants.gProjectionMatrix = camera->ProjectionMatrix();
	gPerFrameConstants.viewProjectionMatrix = camera->ViewProjectionMatrix();
	gPostProcessingConstants.gInverseViewProjectionMatrix = camera->InverseViewProjectionMatrix();
	UpdateConstantBuffer(gPerFrameConstantBuffer, gPerFrameConstants);

	// Indicate that the constant buffer we just updated is for use in the vertex shader (VS), geometry shader (GS) and pixel shader (PS)
	gD3DContext->VSSetConstantBuffers(0, 1, &gPerFrameConstantBuffer); // First parameter must match constant buffer number in the shader 
	gD3DContext->GSSetConstantBuffers(0, 1, &gPerFrameConstantBuffer);
	gD3DContext->PSSetConstantBuffers(0, 1, &gPerFrameConstantBuffer);

	////--------------- PreRenderWaterTextures ---------------////
	
	if (bRenderReflectantObjects) {
		gD3DContext->OMSetRenderTargets(1, &gSceneHeightRenderTarget, gDepthStencil);

		// Select which shaders to use next
		gD3DContext->VSSetShader(gWorldHeightVertexShader, nullptr, 0);
		gD3DContext->PSSetShader(gWorldHeightPixelShader, nullptr, 0);
		gD3DContext->GSSetShader(nullptr, nullptr, 0);  // Switch off geometry shader when not using it (pass nullptr for first parameter)

		// States - no blending, normal depth buffer and back-face culling (standard set-up for opaque models)
		gD3DContext->OMSetBlendState(gNoBlendingState, nullptr, 0xffffff);
		gD3DContext->OMSetDepthStencilState(gDepthReadOnlyState, 0);

		gD3DContext->RSSetState(gCullBackState);

		// Render lit models, only change textures for each onee
		gD3DContext->PSSetSamplers(0, 1, &gAnisotropic4xSampler);

		//Render Non Transparent Models
		gGround->Render();
		gStars->Render();

		gD3DContext->OMSetDepthStencilState(gUseDepthBufferState, 0);
		gD3DContext->OMSetRenderTargets(1, &gBackBufferRenderTarget, gDepthStencil);
	}

	gD3DContext->PSSetShader(gPixelLightingPixelShader, nullptr, 0);


	////--------------- Render ordinary models ---------------///

	// Select which shaders to use next
	gD3DContext->VSSetShader(gPixelLightingVertexShader, nullptr, 0);
	gD3DContext->PSSetShader(gPixelLightingPixelShader, nullptr, 0);
	gD3DContext->GSSetShader(nullptr, nullptr, 0);  // Switch off geometry shader when not using it (pass nullptr for first parameter)

	if (!bRenderReflectantObjects) {

		// States - no blending, normal depth buffer and back-face culling (standard set-up for opaque models)
		gD3DContext->OMSetBlendState(gNoBlendingState, nullptr, 0xffffff);
		gD3DContext->OMSetDepthStencilState(gUseDepthBufferState, 0);
		gD3DContext->RSSetState(gCullBackState);

		// Render lit models, only change textures for each onee
		gD3DContext->PSSetSamplers(0, 1, &gAnisotropic4xSampler);
	}

	gD3DContext->PSSetShaderResources(0, 1, &gGroundDiffuseSpecularMapSRV); // First parameter must match texture slot number in the shader
	gGround->Render();

	if (bRenderReflectantObjects) {
		gD3DContext->PSSetShader(gWaterCombinedPixelShader, nullptr, 0);
		//gD3DContext->PSSetShaderResources(0, 1, &gGroundDiffuseSpecularMapSRV);
		gD3DContext->PSSetShaderResources(0, 1, &gSceneCubeMapTextureSRV);
		gD3DContext->PSSetShaderResources(1, 1, &gSceneHeightTextureSRV);
		//gD3DContext->RSSetState(gWireframeState);
		gCargo->Render();
		gWaveGrid->mWaterGridModel->Render();
		gVisualTestGrid->Render();
	}
	
	////--------------- Render sky ---------------////

	// Select which shaders to use next
	gD3DContext->VSSetShader(gBasicTransformVertexShader, nullptr, 0);
	gD3DContext->PSSetShader(gTintedTexturePixelShader, nullptr, 0);

	// Using a pixel shader that tints the texture - don't need a tint on the sky so set it to white
	gPerModelConstants.objectColour = { 1, 1, 1 };

	// Stars point inwards
	gD3DContext->RSSetState(gCullNoneState);

	// Render sky
	gD3DContext->PSSetShaderResources(0, 1, &gStarsDiffuseSpecularMapSRV);
	gStars->Render();



	////--------------- Render lights ---------------////

	// Select which shaders to use next (actually same as before, so we could skip this)
	gD3DContext->VSSetShader(gBasicTransformVertexShader, nullptr, 0);
	gD3DContext->PSSetShader(gTintedTexturePixelShader, nullptr, 0);

	// Select the texture and sampler to use in the pixel shader
	gD3DContext->PSSetShaderResources(0, 1, &gLightDiffuseMapSRV); // First parameter must match texture slot number in the shaer

	// States - additive blending, read-only depth buffer and no culling (standard set-up for blending)
	gD3DContext->OMSetBlendState(gAdditiveBlendingState, nullptr, 0xffffff);
	gD3DContext->OMSetDepthStencilState(gDepthReadOnlyState, 0);
	gD3DContext->RSSetState(gCullNoneState);

	// Render all the lights in the array
	for (int i = 0; i < NUM_LIGHTS; ++i)
	{
		gPerModelConstants.objectColour = gLights[i].colour; // Set any per-model constants apart from the world matrix just before calling render (light colour here)
		gLights[i].model->Render();
	}
}

void SelectPostProcessShaderAndTextures(PostProcess postProcess) {
	if (postProcess == PostProcess::Copy) {
		gD3DContext->PSSetShader(gCopyPixelShader, nullptr, 0);
		gD3DContext->PSSetShaderResources(0, 1, &gAlternativeSceneTextureSRV);
	}
	else if (postProcess == PostProcess::SSRPrep) {
		gD3DContext->OMSetRenderTargets(1, &gAlternativeSceneRenderTarget, nullptr);
		gD3DContext->PSSetShaderResources(1, 1, &gDepthShaderView);
		gD3DContext->PSSetShaderResources(2, 1, &gNormalTextureSRV);
		gD3DContext->PSSetShaderResources(3, 1, &gPositionTextureSRV);
		gD3DContext->PSSetShaderResources(4, 1, &gSpecularTextureSRV);
		gD3DContext->PSSetSamplers(1, 1, &gPointSampler);
		gD3DContext->PSSetShader(gScreenSpaceReflectionPrepPixelShader, nullptr, 0);
	}
	else if (postProcess == PostProcess::SSR) {

	}
}

void PostProcessing(PostProcess postProcess) {
	gD3DContext->OMSetRenderTargets(1, &gBackBufferRenderTarget, gDepthStencil);
	gD3DContext->PSSetShaderResources(0, 1, &gSceneTextureSRV);
	gD3DContext->PSSetSamplers(0, 1, &gPointSampler);
	gD3DContext->VSSetShader(g2DQuadVertexShader, nullptr, 0);
	gD3DContext->GSSetShader(nullptr, nullptr, 0);
	gD3DContext->OMSetBlendState(gNoBlendingState, nullptr, 0xffffff);
	gD3DContext->OMSetDepthStencilState(gDepthReadOnlyState, 0);
	gD3DContext->RSSetState(gCullNoneState);
	gD3DContext->IASetInputLayout(NULL);
	gD3DContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	SelectPostProcessShaderAndTextures(postProcess);

	gPostProcessingConstants.area2DTopLeft = { 0,0 };
	gPostProcessingConstants.area2DSize = { 1, 1 };
	gPostProcessingConstants.area2DDepth = 0;
	gPostProcessingConstants.ProjectionA = gCamera->FarClip() / (gCamera->FarClip() - gCamera->NearClip());
	gPostProcessingConstants.ProjectionB = (-gCamera->FarClip() * gCamera->NearClip()) / (gCamera->FarClip() - gCamera->NearClip());
	UpdateConstantBuffer(gPostProcessingConstantBuffer, gPostProcessingConstants);
	gD3DContext->VSSetConstantBuffers(1, 1, &gPostProcessingConstantBuffer);
	gD3DContext->PSSetConstantBuffers(1, 1, &gPostProcessingConstantBuffer);

	gD3DContext->Draw(4, 0);

	if (postProcess == PostProcess::SSRPrep) {
		ID3D11ShaderResourceView* nullSRV = nullptr;
		gD3DContext->PSSetShaderResources(1, 1, &nullSRV);
		gD3DContext->PSSetShaderResources(2, 1, &nullSRV);
		gD3DContext->PSSetShaderResources(3, 1, &nullSRV);
		gD3DContext->PSSetShaderResources(4, 1, &nullSRV);

	}
}

// Rendering the scene
void RenderScene()
{
	//// Common settings ////

	// Set up the light information in the constant buffer
	// Don't send to the GPU yet, the function RenderSceneFromCamera will do that
	gPerFrameConstants.light1Colour   = gLights[0].colour * gLights[0].strength;
	gPerFrameConstants.light1Position = gLights[0].model->Position();
	gPerFrameConstants.light2Colour   = gLights[1].colour * gLights[1].strength;
	gPerFrameConstants.light2Position = gLights[1].model->Position();

	gPerFrameConstants.ambientColour  = gAmbientColour;
	gPerFrameConstants.specularPower  = gSpecularPower;
	gPerFrameConstants.cameraPosition = gCamera->Position();
	gPostProcessingConstants.cameraPosition = gCamera->Position();
	gPerFrameConstants.viewportWidth  = static_cast<float>(gViewportWidth);
	gPerFrameConstants.viewportHeight = static_cast<float>(gViewportHeight);
	gPerFrameConstants.rior = 1.05f;
	gPerFrameConstants.WaterExtinctionLevels = { 150.0f, 75.0f, 30.0f };
	gPerFrameConstants.WaterDiffuseLevel = 0.5f;
	gPerFrameConstants.waterRefractiveIndex = waterRefractiveIndex;
	//Cube Map Render
	if (cubeMapRenderToggle) {
		gD3DContext->RSSetViewports(1, &gSceneCubeMapViewport);
		static bool firstRun = true;
		if (firstRun) BuildCubeCameras(gWaveGrid->mWaterGridModel->Position() + CVector3(0.0f, 35.0f, 0.0f));
		for (int i = 0; i < 6; i++) {
			gD3DContext->ClearRenderTargetView(gSceneCubeMapRenderTarget[i], &gBackgroundColor.r);
			gD3DContext->ClearDepthStencilView(gSceneCubeMapDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
			gD3DContext->OMSetRenderTargets(1, &gSceneCubeMapRenderTarget[i], gSceneCubeMapDSV);
			RenderSceneFromCamera(gCubeMapCameras[i], false);
		}
		gD3DContext->GenerateMips(gSceneCubeMapTextureSRV);
		cubeMapRenderToggle = false;
	}


	////--------------- Main scene rendering ---------------////

	// Set the target for rendering and select the main depth buffer.
	// If using post-processing then render to the scene texture, otherwise to the usual back buffer
	// Also clear the render target to a fixed colour and the depth buffer to the far distance

	gD3DContext->OMSetRenderTargets(1, &gBackBufferRenderTarget, gDepthStencil);
	gD3DContext->ClearRenderTargetView(gBackBufferRenderTarget, &gBackgroundColor.r);
	//ID3D11RenderTargetView* targets[4] = { gSceneRenderTarget, gPositionRenderTarget, gNormalRenderTarget, gSpecularRenderTarget };
	//gD3DContext->OMSetRenderTargets(4, targets, gDepthStencil);
	//gD3DContext->OMSetRenderTargets(1, &gSceneRenderTarget, gDepthStencil);
	//gD3DContext->ClearRenderTargetView(gSceneRenderTarget, &gBackgroundColor.r);
	gD3DContext->ClearRenderTargetView(gBackBufferRenderTarget, &gBackgroundColor.r);
	gD3DContext->ClearRenderTargetView(gSceneHeightRenderTarget, &gBackgroundColor.r);
	//gD3DContext->ClearRenderTargetView(gAlternativeSceneRenderTarget, &gBackgroundColor.r);
	//gD3DContext->ClearRenderTargetView(gNormalRenderTarget, &gBackgroundColor.r);
	//gD3DContext->ClearRenderTargetView(gPositionRenderTarget, &gBackgroundColor.r);
	gD3DContext->ClearDepthStencilView(gDepthStencil, D3D11_CLEAR_DEPTH, 1.0f, 0);

	// Setup the viewport to the size of the main window
	D3D11_VIEWPORT vp;
	vp.Width = static_cast<FLOAT>(gViewportWidth);
	vp.Height = static_cast<FLOAT>(gViewportHeight);
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	gD3DContext->RSSetViewports(1, &vp);

	// Render the scene from the main camera
	RenderSceneFromCamera(gCamera, true);

	//PostProcessing(PostProcess::SSRPrep);
	//PostProcessing(PostProcess::Copy);

	gD3DContext->Draw(4, 0);

	ID3D11ShaderResourceView* nullSRV = nullptr;
	gD3DContext->PSSetShaderResources(0, 1, &nullSRV);
	gD3DContext->PSSetShaderResources(1, 1, &nullSRV);

	// When drawing to the off-screen back buffer is complete, we "present" the image to the front buffer (the screen)
	// Set first parameter to 1 to lock to vsync
	gSwapChain->Present(lockFPS ? 1 : 0, 0);
}


//--------------------------------------------------------------------------------------
// Scene Update
//--------------------------------------------------------------------------------------


// Update models and camera. frameTime is the time passed since the last frame
void UpdateScene(float frameTime)
{
	float refractiveIndexIncrease = 1.0f;
	if (KeyHeld(Key_N)) waterRefractiveIndex -= refractiveIndexIncrease * frameTime;
	else if (KeyHeld(Key_M)) waterRefractiveIndex += refractiveIndexIncrease * frameTime;
	if (waterRefractiveIndex < 1.33f) waterRefractiveIndex = 1.33f;
	else if (waterRefractiveIndex > 7.0f) waterRefractiveIndex = 7.0f;


	// Orbit one light - a bit of a cheat with the static variable [ask the tutor if you want to know what this is]
	static float lightRotate = 0.0f;
	static bool go = true;
	static float timeScale = 0.0f;
	gLights[0].model->SetPosition({ 20 + cos(lightRotate) * gLightOrbitRadius, 10, 20 + sin(lightRotate) * gLightOrbitRadius });
	if (go)  lightRotate -= gLightOrbitSpeed * frameTime;
	if (KeyHit(Key_L))  go = !go;

	// Control of camera
	gCamera->Control(frameTime, Key_Up, Key_Down, Key_Left, Key_Right, Key_W, Key_S, Key_A, Key_D);

	// Toggle FPS limiting
	if (KeyHit(Key_P))  lockFPS = !lockFPS;
	if (KeyHit(Key_E))  cubeMapRenderToggle = true;

	static bool waterSimOn = false;

	if (KeyHit(Key_G)) waterSimOn = !waterSimOn;
	if (waterSimOn) {
		timeScale += frameTime;
		gWaveGrid->WavesEvaluation(timeScale);
	}
	// Show frame time / FPS in the window title //
	const float fpsUpdateTime = 0.5f; // How long between updates (in seconds)
	static float totalFrameTime = 0;
	static int frameCount = 0;
	totalFrameTime += frameTime;
	++frameCount;
	if (totalFrameTime > fpsUpdateTime)
	{
		// Displays FPS rounded to nearest int, and frame time (more useful for developers) in milliseconds to 2 decimal places
		float avgFrameTime = totalFrameTime / frameCount;
		std::ostringstream frameTimeMs;
		frameTimeMs.precision(2);
		frameTimeMs << std::fixed << avgFrameTime * 1000;
		std::string windowTitle = "Third Year Project - Water Simulation - Frame Time: " + frameTimeMs.str() +
			"ms, FPS: " + std::to_string(static_cast<int>(1 / avgFrameTime + 0.5f));
		SetWindowTextA(gHWnd, windowTitle.c_str());
		totalFrameTime = 0;
		frameCount = 0;
	}
}
