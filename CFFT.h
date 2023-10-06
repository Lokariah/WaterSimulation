//#pragma once
//#include <cmath>
//#include <CVector2.h>
//#include <vector>
//
//class CFFT
//{
//public:
//	CFFT(unsigned int x);
//	~CFFT(); 
//	CVector2 TValue(unsigned int x, unsigned int n);
//	unsigned int Reverse(unsigned int x);
//	void FFT(CVector2* input, CVector2* output, int stride, int offset);
//
//private:
//	CVector2 Mult(CVector2 x, CVector2 y);
//	unsigned int size = 0, n = 0, log2n = 0;
//	const float TWO_PI = PI * 2;
//	unsigned int* bitReversed;
//	std::vector<std::vector<CVector2>> t;
//	CVector2* c[2];
//};
//
