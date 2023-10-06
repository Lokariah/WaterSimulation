#include "Common.hlsli"

PostProcessingInput main(uint vertexID: SV_VertexID) {
	PostProcessingInput output;

	const float2 Quad[4] = { float2(0.0, 0.0),
							 float2(0.0, 1.0),
							 float2(1.0, 0.0),
							 float2(1.0, 1.0) };

	float2 quadCoord = Quad[vertexID];
	float2 areaCoord = gArea2DTopLeft + quadCoord * gArea2DSize;
	float2 screenCoord = areaCoord * 2 - 1;
	screenCoord.y = -screenCoord.y;

	output.areaUV = quadCoord;
	output.sceneUV = areaCoord;
	output.projectedPosition = float4(screenCoord, gArea2DDepth, 1);

	return output;
}