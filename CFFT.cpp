//#include "CFFT.h"
//#include "CVector2.h"
//
//CFFT::CFFT(unsigned int x) : size(x), bitReversed(0), t(0)
//{
//	c[0] = c[1] = 0;
//	log2n = log(x) / log(2);
//	bitReversed = new unsigned int[x];
//	for (int i = 0; i < x; i++) bitReversed[i] = Reverse(i);
//
//	int pow2 = 1;
//	
//	for (int i = 0; i < log2n; i++) {
//		std::vector<CVector2> tempVec;
//		for (int j = 0; j < pow2; j++) tempVec.push_back(TValue(j, pow2 * 2));
//		t.push_back(tempVec);
//		pow2 *= 2;
//	}
//
//	c[0] = new CVector2[x];
//	c[1] = new CVector2[x];
//	n = 0;
//}
//
//CFFT::~CFFT()
//{
//	if (c[0]) delete[] c[0];
//	if (c[1]) delete[] c[0];
//	while (t.size() > 0) {
//		int index = t.size() - 1;
//		while (!t[index].empty()) {
//			t[index].pop_back();
//		}
//		t.pop_back();
//	}
//	if (bitReversed) delete[] bitReversed;
//}
//
//CVector2 CFFT::TValue(unsigned int x, unsigned int n)
//{
//	float xDividedN = x / n;
//	return CVector2(cos(TWO_PI * xDividedN), sin(TWO_PI * xDividedN));
//}
//
//unsigned int CFFT::Reverse(unsigned int x)
//{
//	//Takes the number and flips the first bit
//	unsigned int res = 0;
//	for (int i = 0; i < log2n; i++) {
//		res = (res << 1) + (x & 1);
//		x >>= 1;
//	}
//	return res;
//}
//
//void CFFT::FFT(CVector2* input, CVector2* output, int stride, int offset)
//{
//	for (int i = 0; i < size; i++) c[n][i] = input[bitReversed[i] * stride + offset];
//	int loops = size >> 1;
//	int Size = 1 << 1;
//	int SizeOverTwo = 1;
//	int w = 0;
//	for (int i = 1; i <= log2n; i++) {
//		n ^= 1;
//		for (int j = 0; j < loops; j++) {
//			for (int k = 0; k < SizeOverTwo; k++) {
//				int comIndex = Size * j + k;
//				c[n][comIndex] = c[n^1][comIndex] + Mult(c[n ^ 1][comIndex + SizeOverTwo], t[w][k]);
//			}
//			for (int k = SizeOverTwo; k < Size; k++) {
//				int comIndex = Size * j + k;
//				c[n][comIndex] = c[n ^ 1][comIndex - SizeOverTwo] - Mult(c[n ^ 1][comIndex], t[w][k - SizeOverTwo]);
//			}
//		}
//		loops >>= 1;
//		Size <<= 1;
//		SizeOverTwo << 1;
//		w++;
//	}
//	for (int i = 0; i < size; i++) output[i * stride + offset] = c[n][i];
//}
//
//CVector2 CFFT::Mult(CVector2 x, CVector2 y)
//{
//	return CVector2(x.x * y.x - x.y * y.y, x.x * y.y + x.y * y.x);
//}
