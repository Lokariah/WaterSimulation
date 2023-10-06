#include "CWaterGrid.h"
#include "simple_fft\fft_settings.h"
#include "simple_fft\fft.h"
#include <vector>

CWaveGrid::CWaveGrid(const int size, const float phillips, const CVector2 wind, const float length) :
	mSize(size), mSizePlus1(size + 1), mPhillipsParameter(phillips), mWind(wind), mLength(length) {
	mTilde.resize(mSize * mSize);
	mTildeSlopeX.resize(mSize * mSize);
	mTildeSlopeZ.resize(mSize * mSize);
	mTildeDX.resize(mSize * mSize);
	mTildeDZ.resize(mSize * mSize);

	mWaterGrid = new WaterGridVertex[mSizePlus1 * mSizePlus1];
	mWaterGridMesh = new Mesh(CVector3(-length, 0, -length), CVector3(length, 0, length), size, size, true, true);
	mWaterGridModel = new Model(mWaterGridMesh);
	int i;
	complex_type tilde, tildeConj;

	//float xStep = (-length - length) / size;
	//mWaterGrid[i].originalPos.x = mWaterGrid[i].vertex.x = -length + (xStep * gridX); Investigate further to fix scaling issue.

	for (int gridX = 0; gridX < mSizePlus1; gridX++) {
		for (int gridY = 0; gridY < mSizePlus1; gridY++) {
			i = gridX * mSizePlus1 + gridY;
			tilde = Tilde0(gridY, gridX);
			tildeConj = Tilde0(-gridY, gridX);
			tildeConj = { tildeConj._Val[0], -tildeConj._Val[1] };

			mWaterGrid[i].tilde.x = tilde._Val[0];
			mWaterGrid[i].tilde.y = tilde._Val[1];
			mWaterGrid[i].tildeConj.x = tildeConj._Val[0];
			mWaterGrid[i].tildeConj.y = tildeConj._Val[1];

			mWaterGrid[i].originalPos.x = mWaterGrid[i].vertex.x = (gridY - mSize / 2.0f) * length / mSize;
			mWaterGrid[i].originalPos.y = mWaterGrid[i].vertex.y = 0.0f;
			mWaterGrid[i].originalPos.z = mWaterGrid[i].vertex.z = (gridX - mSize / 2.0f) * length / mSize;

			mWaterGrid[i].normal.x = mWaterGrid[i].normal.z = 0.0f;
			mWaterGrid[i].normal.y = 1.0f;
		}
	}
}

CWaveGrid::~CWaveGrid()
{
	//if (mFft) delete mFft;
	if (mWaterGrid) delete[] mWaterGrid;
	if (mWaterGridMesh) delete mWaterGridMesh;
	if (mWaterGridModel) delete mWaterGridModel;
}

float CWaveGrid::Dispersion(int gridY, int gridX)
{
	float w = 2.0f * PI / 200.0f;
	float kX = PI * (2 * gridY - mSize) / mLength;
	float kZ = PI * (2 * gridX - mSize) / mLength;
	return floor(sqrt(GRAVITY * sqrt(kX * kX + kZ * kZ)) / w) * w;
}

float CWaveGrid::Phillips(int gridY, int gridX)
{
	CVector2 k = CVector2(PI * (2 * gridY - mSize) / mLength, PI * (2 * gridX - mSize) / mLength);
	float kLength = sqrt(k.x * k.x + k.y * k.y);
	if (kLength < 0.000001f) return 0.0f;
	float kLengthSquared = kLength * kLength;
	float kLengthQuadrupled = kLengthSquared * kLengthSquared;
	
	CVector2 kUnit = CVector2(k.x / kLength, k.y / kLength);
	float wLength = sqrt(mWind.x * mWind.x + mWind.y * mWind.y);
	CVector2 wUnit = CVector2(mWind.x / wLength, mWind.y / wLength);
	float kDotW = Dot(kUnit, wUnit);
	float kDotWSquared = kDotW * kDotW;

	float l = wLength * wLength / GRAVITY;
	float lSquared = l * l;

	const float damping = 0.001;
	float lSquaredDamped = lSquared * damping * damping;
	return  mPhillipsParameter * exp(-1.0f / (kLengthSquared * lSquared)) / kLengthQuadrupled * kDotWSquared * exp(-kLengthSquared * lSquaredDamped);
}

complex_type CWaveGrid::Tilde0(int gridY, int gridX)
{
	complex_type gaussianRandom;
	float x, y, z;
	do {
		x = 2.0f * (float)rand() / RAND_MAX - 1.0f;
		y = 2.0f * (float)rand() / RAND_MAX - 1.0f;
		z = x * x + y * y;
	} while (z >= 1.0f);
	z = sqrt((-2.0f * log(z)) / z);
	gaussianRandom = { x * z, y * z };
	float phillipsSqrt = sqrt(Phillips(gridY, gridX) / 2.0f);
	return { gaussianRandom._Val[0] * phillipsSqrt, gaussianRandom._Val[1] * phillipsSqrt };
}

complex_type CWaveGrid::Tilde(float t, int gridY, int gridX)
{
	int i = gridX * mSizePlus1 + gridY;
	complex_type tilde(mWaterGrid[i].tilde.x, mWaterGrid[i].tilde.y);
	complex_type tildeConj(mWaterGrid[i].tildeConj.x, mWaterGrid[i].tildeConj.y);

	float dispersedT = Dispersion(gridY, gridX) * t;
	float cosDispersedT = cos(dispersedT);
	float sinDispersedT = sin(dispersedT);

	complex_type tildeMult = { cosDispersedT, sinDispersedT };
	complex_type tildeMultConj = { cosDispersedT, -sinDispersedT };

	complex_type res = Mult(tilde, tildeMult) + Mult(tildeConj, tildeMultConj);

	return res;
}

WaterGridNode CWaveGrid::HDN(CVector2 x, float t)
{
	WaterGridNode tempNode;
	complex_type c, res, tildeC, k;
	float kX, kY, kLength, kDotX;

	for (int gridX = 0; gridX < mSize; gridX++) {
		kY = 2.0f * PI * (gridX - mSize / 2.0f) / mLength;
		for (int gridY = 0; gridY < mSize; gridY++) {
			kX = 2.0f * PI * (gridY - mSize / 2.0f) / mLength;
			k = { kX, kY };
			kLength = sqrt(k._Val[0] * k._Val[0] + k._Val[1] * k._Val[1]);
			kDotX = Dot(k, x);

			c = { cos(kDotX), sin(kDotX) };
			tildeC = Mult(Tilde(t, gridY, gridX), c);

			tempNode.height = tempNode.height + tildeC;
			tempNode.normal = tempNode.normal + CVector3(-kX * tildeC._Val[1], 0.0f, -kY * tildeC._Val[1]);

			if (kLength < 0.000001) continue;
			tempNode.displacementVector = tempNode.displacementVector + CVector2(kX / kLength * tildeC._Val[1], kY / kLength * tildeC._Val[1]);
		}
	}
	tempNode.normal = Normalise((CVector3(0.0f, 1.0f, 0.0f) - tempNode.normal));
	return tempNode;
}

void CWaveGrid::WavesEvaluationFFT(float t)
{
	std::vector<CVector3> vertexPositions;
	std::vector<CVector3> vertexNormals;

	float kX, kY, len, lambda = -1.0f;
	int i, j;

	for (int gridX = 0; gridX < mSize; gridX++) {
		kY = PI * (2.0f * gridX - mSize) / mLength;
		for (int gridY = 0; gridY < mSize; gridY++) {
			kX = PI * (2.0f * gridY - mSize) / mLength;
			len = sqrt(kX * kX + kY * kY);
			i = gridX * mSize + gridY;

			mTilde[i] = Tilde(t, gridY, gridX);
			mTildeSlopeX[i] = Mult(mTilde[i], complex_type(0.0f, kX));
			mTildeSlopeZ[i] = Mult(mTilde[i], complex_type(0.0f, kY));

			if (len < 0.000001f) {
				mTildeDX[i] = { 0.0f, 0.0f };
				mTildeDZ[i] = { 0.0f, 0.0f };
			}
			else {
				mTildeDX[i] = Mult(mTilde[i], complex_type(0.0f, -kX / len));
				mTildeDZ[i] = Mult(mTilde[i], complex_type(0.0f, -kY / len));
			}
		}
	}
	const char* a;
	bool test;
	//for (int gridX = 0; gridX < mSize; gridX++) {
		test = simple_fft::FFT(mTilde, mTilde, size_t(mSize * mSize), a);
		test = simple_fft::FFT(mTildeSlopeX, mTildeSlopeX, size_t(mSize * mSize), a);
		test = simple_fft::FFT(mTildeSlopeZ, mTildeSlopeZ, size_t(mSize * mSize), a);
		test = simple_fft::FFT(mTildeDX, mTildeDX, size_t(mSize * mSize), a);
		test = simple_fft::FFT(mTildeDZ, mTildeDZ, size_t(mSize * mSize), a);
	//}

	//for (int gridY = 0; gridY < mSize; gridY++) {
	//	test = simple_fft::FFT(mTilde, mTilde, size_t(mSize * mSize), a);
	//	test = simple_fft::FFT(mTildeSlopeX, mTildeSlopeX, size_t(mSize * mSize), a);
	//	test = simple_fft::FFT(mTildeSlopeZ, mTildeSlopeZ, size_t(mSize * mSize), a);
	//	test = simple_fft::FFT(mTildeDX, mTildeDX, size_t(mSize * mSize), a);
	//	test = simple_fft::FFT(mTildeDZ, mTildeDZ, size_t(mSize * mSize), a);
	//}

	int sign;
	float signs[] = { 1.0f, -1.0f };
	CVector3 n;
	
	for (int gridX = 0; gridX < mSize; gridX++) {
		for (int gridY = 0; gridY < mSize; gridY++) {
			i = gridX * mSize + gridY;
			j = gridX * mSizePlus1 + gridY;

			sign = signs[(gridY + gridX) & 1];

			mTilde[i] *= sign;
			mWaterGrid[i].vertex.y = mTilde[i]._Val[0];

			mTildeDX[i] *= sign;
			mTildeDZ[i] *= sign;
			mWaterGrid[i].vertex.x = mWaterGrid[i].originalPos.x + mTildeDX[i]._Val[0] * lambda;
			mWaterGrid[i].vertex.z = mWaterGrid[i].originalPos.z + mTildeDZ[i]._Val[0] * lambda;

			mTildeSlopeX[i] *= sign;
			mTildeSlopeZ[i] *= sign;
			n = Normalise(CVector3( 0.0f - mTildeSlopeX[i]._Val[0], 1.0f, 0.0f - mTildeSlopeZ[i]._Val[0] ));
			mWaterGrid[i].normal = n;

			if (gridX == 0 && gridY == 0) {
				mWaterGrid[j + mSize + mSizePlus1 * mSize].vertex.y = mTilde[i]._Val[0];
				mWaterGrid[j + mSize + mSizePlus1 * mSize].vertex.x = mWaterGrid[j + mSize + mSizePlus1 * mSize].originalPos.x + mTildeDX[i]._Val[0] * lambda;
				mWaterGrid[j + mSize + mSizePlus1 * mSize].vertex.z = mWaterGrid[j + mSize + mSizePlus1 * mSize].originalPos.z + mTildeDZ[i]._Val[0] * lambda;
				mWaterGrid[j + mSize + mSizePlus1 * mSize].normal = n;
			}

			if (gridY == 0) {
				mWaterGrid[j + mSize].vertex.y = mTilde[i]._Val[0];
				mWaterGrid[j + mSize].vertex.x = mWaterGrid[j + mSize].originalPos.x + mTildeDX[i]._Val[0] * lambda;
				mWaterGrid[j + mSize].vertex.z = mWaterGrid[j + mSize].originalPos.z + mTildeDZ[i]._Val[0] * lambda;
				mWaterGrid[j + mSize].normal = n;
			}

			if (gridX == 0) {
				mWaterGrid[j + mSizePlus1 * mSize].vertex.y = mTilde[i]._Val[0];
				mWaterGrid[j + mSizePlus1 * mSize].vertex.x = mWaterGrid[j + mSizePlus1 * mSize].originalPos.x + mTildeDX[i]._Val[0] * lambda;
				mWaterGrid[j + mSizePlus1 * mSize].vertex.z = mWaterGrid[j + mSizePlus1 * mSize].originalPos.z + mTildeDZ[i]._Val[0] * lambda;
				mWaterGrid[j + mSizePlus1 * mSize].normal = n;
			}
		}
	}

	for (int i = 0; i < mSizePlus1; i++) {
		for (int j = 0; j < mSizePlus1; j++) {
			vertexPositions.push_back(mWaterGrid[i * mSizePlus1 + j].vertex);
			vertexNormals.push_back(mWaterGrid[i * mSizePlus1 + j].normal);
		}
	}
	mWaterGridMesh->UpdateNodeVertexBuffer(0, mSize, vertexPositions, vertexNormals);
}

void CWaveGrid::WavesEvaluation(float t) {
	float lambda = -1.0;
	int index;
	CVector2 x;
	CVector2 d;
	WaterGridNode hdn;
	std::vector<CVector3> vertexPositions;
	std::vector<CVector3> vertexNormals;
	for (int m_prime = 0; m_prime < mSize; m_prime++) {
		for (int n_prime = 0; n_prime < mSize; n_prime++) {
			index = m_prime * mSizePlus1 + n_prime;

			x = CVector2(mWaterGrid[index].vertex.x, mWaterGrid[index].vertex.z);

			hdn = HDN(x, t);

			mWaterGrid[index].vertex.y = hdn.height._Val[0];

			mWaterGrid[index].vertex.x = mWaterGrid[index].originalPos.x + lambda * hdn.displacementVector.x;
			mWaterGrid[index].vertex.z = mWaterGrid[index].originalPos.z + lambda * hdn.displacementVector.y;

			mWaterGrid[index].normal.x = hdn.normal.x;
			mWaterGrid[index].normal.y = hdn.normal.y;
			mWaterGrid[index].normal.z = hdn.normal.z;

			if (n_prime == 0 && m_prime == 0) {
				mWaterGrid[index + mSize + mSizePlus1 * mSize].vertex.y = hdn.height._Val[0];
				
				mWaterGrid[index + mSize + mSizePlus1 * mSize].vertex.x = mWaterGrid[index + mSize + mSizePlus1 * mSize].originalPos.x + lambda * hdn.displacementVector.x;
				mWaterGrid[index + mSize + mSizePlus1 * mSize].vertex.z = mWaterGrid[index + mSize + mSizePlus1 * mSize].originalPos.z + lambda * hdn.displacementVector.y;
				
				mWaterGrid[index + mSize + mSizePlus1 * mSize].normal.x = hdn.normal.x;
				mWaterGrid[index + mSize + mSizePlus1 * mSize].normal.y = hdn.normal.y;
				mWaterGrid[index + mSize + mSizePlus1 * mSize].normal.z = hdn.normal.z;
			}
			if (n_prime == 0) {
				mWaterGrid[index + mSize].vertex.y = hdn.height._Val[0];
				
				mWaterGrid[index + mSize].vertex.x = mWaterGrid[index + mSize].originalPos.x + lambda * hdn.displacementVector.x;
				mWaterGrid[index + mSize].vertex.z = mWaterGrid[index + mSize].originalPos.z + lambda * hdn.displacementVector.y;
				
				mWaterGrid[index + mSize].normal.x = hdn.normal.x;
				mWaterGrid[index + mSize].normal.y = hdn.normal.y;
				mWaterGrid[index + mSize].normal.z = hdn.normal.z;
			}
			if (m_prime == 0) {
				mWaterGrid[index + mSizePlus1 * mSize].vertex.y = hdn.height._Val[0];
				
				mWaterGrid[index + mSizePlus1 * mSize].vertex.x = mWaterGrid[index + mSizePlus1 * mSize].originalPos.x + lambda * hdn.displacementVector.x;
				mWaterGrid[index + mSizePlus1 * mSize].vertex.z = mWaterGrid[index + mSizePlus1 * mSize].originalPos.z + lambda * hdn.displacementVector.y;
				
				mWaterGrid[index + mSizePlus1 * mSize].normal.x = hdn.normal.x;
				mWaterGrid[index + mSizePlus1 * mSize].normal.y = hdn.normal.y;
				mWaterGrid[index + mSizePlus1 * mSize].normal.z = hdn.normal.z;
			}
		}
	}

	for (int i = 0; i < mSizePlus1; i++) {
		for (int j = 0; j < mSizePlus1; j++) {
			vertexPositions.push_back(mWaterGrid[i * mSizePlus1 + j].vertex);
			vertexNormals.push_back(mWaterGrid[i * mSizePlus1 + j].normal);
		}
	}
	mWaterGridMesh->UpdateNodeVertexBuffer(0, mSize, vertexPositions, vertexNormals);
}

CVector2 CWaveGrid::Mult(CVector2 x, CVector2 y)
{
	return CVector2(x.x * y.x - x.y * y.y, x.x * y.y + x.y * y.x);
}

complex_type CWaveGrid::Mult(complex_type x, CVector2 y)
{
	return complex_type(x._Val[0] * y.x - x._Val[1] * y.y, x._Val[0] * y.y + x._Val[1] * y.x);
}

complex_type CWaveGrid::Mult(complex_type x, complex_type y)
{
	return complex_type(x._Val[0] * y._Val[0] - x._Val[1] * y._Val[1], x._Val[0] * y._Val[1] + x._Val[1] * y._Val[0]);
}

float CWaveGrid::Dot(complex_type x, CVector2 y)
{
	return x._Val[0] * y.x + x._Val[1] * y.y;
}

float CWaveGrid::Dot(CVector2 x, CVector2 y)
{
	return x.x * y.x + x.y * y.y;
}
