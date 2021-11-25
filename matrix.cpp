#include "includes.h"

matrix3x4_t matrix3x4_t::ConcatTransforms(matrix3x4_t in) const {
	auto& m = m_flMatVal;
	matrix3x4_t out;
	out[0][0] = m[0][0] * in[0][0] + m[0][1] * in[1][0] + m[0][2] * in[2][0];
	out[0][1] = m[0][0] * in[0][1] + m[0][1] * in[1][1] + m[0][2] * in[2][1];
	out[0][2] = m[0][0] * in[0][2] + m[0][1] * in[1][2] + m[0][2] * in[2][2];
	out[0][3] = m[0][0] * in[0][3] + m[0][1] * in[1][3] + m[0][2] * in[2][3] + m[0][3];
	out[1][0] = m[1][0] * in[0][0] + m[1][1] * in[1][0] + m[1][2] * in[2][0];
	out[1][1] = m[1][0] * in[0][1] + m[1][1] * in[1][1] + m[1][2] * in[2][1];
	out[1][2] = m[1][0] * in[0][2] + m[1][1] * in[1][2] + m[1][2] * in[2][2];
	out[1][3] = m[1][0] * in[0][3] + m[1][1] * in[1][3] + m[1][2] * in[2][3] + m[1][3];
	out[2][0] = m[2][0] * in[0][0] + m[2][1] * in[1][0] + m[2][2] * in[2][0];
	out[2][1] = m[2][0] * in[0][1] + m[2][1] * in[1][1] + m[2][2] * in[2][1];
	out[2][2] = m[2][0] * in[0][2] + m[2][1] * in[1][2] + m[2][2] * in[2][2];
	out[2][3] = m[2][0] * in[0][3] + m[2][1] * in[1][3] + m[2][2] * in[2][3] + m[2][3];
	return out;
}

vec3_t matrix3x4_t::operator*(const vec3_t& vVec) const {
	auto& m = m_flMatVal;
	vec3_t vRet;
	vRet.x = m[0][0] * vVec.x + m[0][1] * vVec.y + m[0][2] * vVec.z + m[0][3];
	vRet.y = m[1][0] * vVec.x + m[1][1] * vVec.y + m[1][2] * vVec.z + m[1][3];
	vRet.z = m[2][0] * vVec.x + m[2][1] * vVec.y + m[2][2] * vVec.z + m[2][3];

	return vRet;
}

matrix3x4_t matrix3x4_t::operator+(const matrix3x4_t& other) const {
	matrix3x4_t ret;
	auto& m = m_flMatVal;
	for (int i = 0; i < 12; i++) {
		((float*)ret.m_flMatVal)[i] = ((float*)m)[i] + ((float*)other.m_flMatVal)[i];
	}
	return ret;
}

matrix3x4_t matrix3x4_t::operator-(const matrix3x4_t& other) const {
	matrix3x4_t ret;
	auto& m = m_flMatVal;
	for (int i = 0; i < 12; i++) {
		((float*)ret.m_flMatVal)[i] = ((float*)m)[i] - ((float*)other.m_flMatVal)[i];
	}
	return ret;
}

matrix3x4_t matrix3x4_t::operator*(const float& other) const {
	matrix3x4_t ret;
	auto& m = m_flMatVal;
	for (int i = 0; i < 12; i++) {
		((float*)ret.m_flMatVal)[i] = ((float*)m)[i] * other;
	}
	return ret;
}