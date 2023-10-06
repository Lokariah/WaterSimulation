#include "CVector2.h"
#include "simple_fft\fft_settings.h"
#include "CVector3.h"
#include "Mesh.h"
#include "Model.h"

struct WaterGridVertex {
	CVector3 vertex = { 0.0f, 0.0f, 0.0f };
	CVector3 normal = { 0.0f, 0.0f, 0.0f };
	CVector3 tilde = { 0.0f, 0.0f, 0.0f };
	CVector3 tildeConj = { 0.0f, 0.0f, 0.0f };
	CVector3 originalPos = { 0.0f, 0.0f, 0.0f };
};

struct WaterGridNode {
	complex_type height = { 0.0f, 0.0f };
	CVector2 displacementVector = { 0.0f, 0.0f };
	CVector3 normal = { 0.0f, 0.0f, 0.0f };
};

#pragma once
class CWaveGrid 
{
public:
	CWaveGrid(const int size, const float phillips, const CVector2 wind, const float length);
	~CWaveGrid();
	float Dispersion(int gridY, int gridX);
	float Phillips(int gridY, int gridX);
	complex_type Tilde0(int gridY, int gridX);
	complex_type Tilde(float t, int gridY, int gridX);
	WaterGridNode HDN(CVector2 x, float t);
	void WavesEvaluationFFT(float t);
	void WavesEvaluation(float t);
	Mesh* mWaterGridMesh;
	Model* mWaterGridModel;

private:
	CVector2 Mult(CVector2 x, CVector2 y);
	complex_type Mult(complex_type x, CVector2 y);
	complex_type Mult(complex_type x, complex_type y);
	float Dot(complex_type x, CVector2 y);
	float Dot(CVector2 x, CVector2 y);

	const float GRAVITY = 9.81f;
	int mSize, mSizePlus1;
	float mPhillipsParameter;
	CVector2 mWind;
	float mLength;
	std::vector<complex_type> mTilde, mTildeSlopeX, mTildeSlopeZ, mTildeDX, mTildeDZ;
	WaterGridVertex* mWaterGrid;
};

