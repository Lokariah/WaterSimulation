#include "Common.hlsli"

float4 main(SimpleWorldHeightPixelShaderInput input) : SV_TARGET
{
	return input.worldPosition.y;
}