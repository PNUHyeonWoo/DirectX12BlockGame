#pragma once

#include <directxmath.h>

bool is3DCubeAndCubeCollision(XMFLOAT3 box1low, XMFLOAT3 box1high, XMFLOAT3 box2low, XMFLOAT3 box2high);
bool is2DSquareAndSqueareCollision(XMFLOAT2 sq1low, XMFLOAT2 sq1high, XMFLOAT2 sq2low, XMFLOAT2 sq2high);
bool is1DLineAndLineCollision(float line1low, float line1high, float line2low, float line2high);

bool is3DCubeAndCubeCollision(XMFLOAT3 box1low, XMFLOAT3 box1high, XMFLOAT3 box2low, XMFLOAT3 box2high)
{
	return is1DLineAndLineCollision(box1low.x, box1high.x, box2low.x, box2high.x)
		&& is1DLineAndLineCollision(box1low.y, box1high.y, box2low.y, box2high.y)
		&& is1DLineAndLineCollision(box1low.z, box1high.z, box2low.z, box2high.z);
}

bool is2DSquareAndSqueareCollision(XMFLOAT2 sq1low, XMFLOAT2 sq1high, XMFLOAT2 sq2low, XMFLOAT2 sq2high)
{
	return is1DLineAndLineCollision(sq1low.x, sq1high.x, sq2low.x, sq2high.x) && is1DLineAndLineCollision(sq1low.y, sq1high.y, sq2low.y, sq2high.y);
}

bool is1DLineAndLineCollision(float line1low, float line1high, float line2low, float line2high)
{
	return !(line2high < line1low || line2low > line1high);
}