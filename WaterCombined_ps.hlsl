#include "Common.hlsli"

#include "PixelLighting_ps.hlsl"

TextureCube cubeMap : register(t0);
Texture2D WorldHeightMap : register(t1);

SamplerState PointSample : register(s0);

float4 main(LightingPixelShaderInput input) : SV_TARGET
{
	float2 screenUV = input.projectedPosition.xy / float2(gViewportWidth, gViewportHeight);
	float3 cameraToPixel = normalize(input.worldPosition - gCameraPosition);
	float3 pixelToCamera = normalize(gCameraPosition - input.worldPosition);
	cameraToPixel = normalize(cameraToPixel);
	pixelToCamera = normalize(pixelToCamera);

	//-------------------------------------------------------------------------
	//Calculate Refraction Colour
	float3 refractionVector = refract(cameraToPixel, input.worldNormal, gRior);
	float4 refractionColour = cubeMap.Sample(PointSample, refractionVector);

	float backgroundHeight = WorldHeightMap.Sample(PointSample, screenUV).x;
	float objectDepth = input.worldPosition.y - backgroundHeight;
    bool Clip = false;
    if (input.worldPosition.y < backgroundHeight) Clip = true;
    objectDepth = abs(objectDepth);
    clip(Clip);
	float3 waterDepthDarken = objectDepth / gWaterExtinctionLevels;
	refractionColour.xyz = lerp(refractionColour.xyz, normalize(gWaterExtinctionLevels) * gWaterDiffuseLevel, waterDepthDarken);
	//float4 refractionColour = cubeMap.Sample(PointSample, cameraToPixel);

	//-------------------------------------------------------------------------
	//Calculate Reflection Colour
	float3 reflectionVector = reflect(cameraToPixel, input.worldNormal);
	float4 reflectionColour = cubeMap.Sample(PointSample, reflectionVector);

	float3 light1Direction = normalize(gLight1Position - input.worldPosition);
	float3 halfway = normalize(light1Direction + pixelToCamera);
	float3 specularLight1 = gLight1Colour * pow(max(dot(input.worldNormal, halfway), 0), gSpecularPower); 

	float3 light2Direction = normalize(gLight2Position - input.worldPosition);
	halfway = normalize(light2Direction + pixelToCamera);
	float3 specularLight2 = gLight2Colour * pow(max(dot(input.worldNormal, halfway), 0), gSpecularPower); 

	reflectionColour.xyz += specularLight1 + specularLight2;

	//-------------------------------------------------------------------------
	//Use fresnel to combine the two
	float normalIncidenceReflectance = ((1.0f - gWaterRefractiveIndex) / (1.0f + gWaterRefractiveIndex));
	normalIncidenceReflectance *= normalIncidenceReflectance;
	float fresnel = normalIncidenceReflectance + (1.0f - normalIncidenceReflectance) * pow((1.0f - dot(input.worldNormal, pixelToCamera)), 5);

	float4 finalColour = lerp(refractionColour, reflectionColour, saturate(fresnel));
	return finalColour;
}