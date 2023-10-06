#include "Common.hlsli"

Texture2D SceneTexture : register(t0);
SamplerState PointSample : register(s0);

float4 main(PostProcessingInput input) : SV_Target{
	float3 colour = SceneTexture.Sample(PointSample, input.sceneUV).rgb;
	return float4(colour, 1.0f);
}