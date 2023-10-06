#include "Common.hlsli"

Texture2D SceneTexture : register(t0);
Texture2D NormalTexture : register(t2);
Texture2D PositionTexture : register(t3);
Texture2D SpecularTexture : register(t4);
SamplerState PointSample : register(s0);

Texture2D DepthMap : register(t1);
SamplerState PointClampSample : register(s1);

float4 main(PostProcessingInput input) : SV_TARGET
{
	
	float4 viewSpacePos = PositionTexture.Sample(PointClampSample, input.sceneUV);
	float4 viewSpacePosOriginal = viewSpacePos;
	float3 viewSpaceNormal = normalize(NormalTexture.Sample(PointClampSample, input.sceneUV).xyz);
	float2 screenSize = float2(gViewportWidth, gViewportHeight);

	//Settings
	float maxDistance = 8;
	float resolution = 0.3f;
	int steps = 5;
	const int iterationCap = 150;
	float thickness = 0.5f;

	float3 origin = float3(0.0f, 0.0f, 0.0f);

	float normalIncidenceReflectance = ((1.0f - gWaterRefractiveIndex) / (1.0f + gWaterRefractiveIndex));
	normalIncidenceReflectance *= normalIncidenceReflectance;
	float3 cameraNormal = normalize(origin - viewSpacePosOriginal);
	float fresnel = normalIncidenceReflectance + (1.0f - normalIncidenceReflectance) * pow((1.0f - dot(viewSpaceNormal, cameraNormal)), 5);

	float3 refractionNormal = normalize(refract(normalize(viewSpacePos), viewSpaceNormal, 1.05));
	float3 reflectionNormal = normalize(reflect(normalize(viewSpacePos.xyz), viewSpaceNormal));
	reflectionNormal = lerp(refractionNormal, reflectionNormal, fresnel);

	//Calculating start and end point in view and screen space
	float4 rayStartPoint = float4(viewSpacePos.xyz + (reflectionNormal * 0), 1);
	float4 rayEndPoint = float4(viewSpacePos.xyz + (reflectionNormal * maxDistance), 1);
	
	float4 startPixel = rayStartPoint;
	startPixel = mul(gProjectionMatrixPP, startPixel);
	startPixel.xyz /= startPixel.w;
	startPixel.xy = startPixel.xy * 0.5 + 0.5;
	startPixel.xy *= screenSize;

	float4 endPixel = rayEndPoint;
	endPixel = mul(gProjectionMatrixPP, endPixel);
	endPixel.xyz /= endPixel.w;
	endPixel.xy = endPixel.xy * 0.5 + 0.5;
	endPixel.xy *= screenSize;

	//Setting up for first pass of Ray Marching
	float2 pixel = startPixel.xy;
	float2 UVCoord = pixel/ screenSize;
	float disX = endPixel.x - startPixel.x;
	float disY = endPixel.y - startPixel.y;
	int useX = abs(disX) >= abs(disY) ? 1 : 0;
	float dist = lerp(abs(disY), abs(disX), useX) * clamp(resolution, 0, 1);
	if (dist > iterationCap) dist = iterationCap;
	float2 increment = float2(disX, disY) / max(dist, 0.001);

	float searchPos0 = 0;
	float searchPos1 = 0;
	int hit0 = 0;
	int hit1 = 0;
	float viewDepth = rayStartPoint.z;
	float depth = thickness;
	int LoopDist = clamp(int(dist), 0, iterationCap);

	//First Pass - iterationCap is temporary as LoopDist won't compile
	for (int i = 0; i < LoopDist; ++i) {
		pixel += increment;
		UVCoord = pixel / screenSize;
		viewSpacePos = PositionTexture.Sample(PointClampSample, UVCoord);
		searchPos1 = lerp((pixel.y - startPixel.y) / disY, (pixel.x - startPixel.x) / disX, useX);
		viewDepth = 1.0f / ((1 / rayStartPoint.z) + (searchPos1 * ((1 / rayEndPoint.z) - (1 / rayStartPoint.z))));
		//viewDepth = (rayStartPoint.z * rayEndPoint.z) / lerp(rayEndPoint.z, rayStartPoint.z, searchPos1);
		//viewDepth = 1.0f / ((1 / rayStartPoint.x) + (searchPos1 * ((1 / rayEndPoint.x) - (1 / rayStartPoint.x))));
		//viewDepth = (rayStartPoint.x * rayEndPoint.x) / lerp(rayEndPoint.x, rayStartPoint.x, searchPos1);
		depth = viewDepth - viewSpacePos.z;

		if (depth > 0 && depth < thickness) {
			hit0 = 1;
			break;
		}
		else {
			searchPos0 = searchPos1;
		}
	}

	searchPos1 = searchPos0 + ((searchPos1 - searchPos0) / 2);
	searchPos1 = clamp(searchPos1, 0, 1);
	steps *= hit0;

	//Second Pass
	for (int i = 0; i < steps; ++i) {
		pixel = lerp(startPixel.xy, endPixel.xy, searchPos1);
		UVCoord = pixel / screenSize;
		viewSpacePos = PositionTexture.Sample(PointClampSample, UVCoord);
		viewDepth = 1.0f / ((1 / rayStartPoint.z) + (searchPos1 * ((1 / rayEndPoint.z) - (1 / rayStartPoint.z))));
		//viewDepth = (rayStartPoint.z * rayEndPoint.z) / lerp(rayEndPoint.z, rayStartPoint.z, searchPos1);
		//viewDepth = 1.0f / ((1 / rayStartPoint.x) + (searchPos1 * ((1 / rayEndPoint.x) - (1 / rayStartPoint.x))));
		//viewDepth = (rayStartPoint.x * rayEndPoint.x) / lerp(rayEndPoint.x, rayStartPoint.x, searchPos1);
		depth = viewDepth - viewSpacePos.z;

		if (depth > 0 && depth < thickness) {
			hit1 = 1;
			searchPos1 = searchPos0 + ((searchPos1 - searchPos0) / 2);
		}
		else {
			float temp = searchPos1;
			searchPos1 = searchPos1 + ((searchPos1 - searchPos0) / 2);
			searchPos0 = temp;
		}
	}

	//Checking point is visible - Doesn't work atm
	float visible = hit1 * viewSpacePos.w
							* (1 - max(dot(-normalize(viewSpacePos), reflectionNormal), 0)) 
							//* (1 - clamp(depth / thickness, 0, 1))
							//* (1 - clamp(length(viewSpacePos - viewSpacePosOriginal) / maxDistance, 0, 1))
							* (UVCoord.x < 0 || UVCoord.x > 1 ? 0 : 1)
							* (UVCoord.y < 0 || UVCoord.y > 1 ? 0 : 1)
		;
	visible = clamp(visible, 0, 1);
	

	float4 colour = SceneTexture.Sample(PointSample, input.sceneUV);
	float4 reflectedColour = SceneTexture.Sample(PointSample, UVCoord);
	//float4 reflectedColour = lerp(colour, SceneTexture.Sample(PointSample, UVCoord), visible);
	//float specularity = lerp(0, SpecularTexture.Sample(PointSample, input.sceneUV).x, visible);
	float specularity = SpecularTexture.Sample(PointSample, input.sceneUV).x;
	//return float4(lerp(input.sceneUV, UVCoord, specularity), 0, 1);
	float3 finalColour = lerp(colour.xyz, reflectedColour.xyz, specularity);
	return float4(finalColour, 1);
	return float4(UVCoord, visible, 1);
}