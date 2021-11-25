#include "includes.h"

vec3_t ang_t::to_vectors(vec3_t* side, vec3_t* up)
{
	auto rad_pitch = math::deg_to_rad(this->x);
	auto rad_yaw = math::deg_to_rad(this->y);
	float rad_roll;

	auto sin_pitch = sinf(rad_pitch);
	auto sin_yaw = sinf(rad_yaw);
	float sin_roll;

	auto cos_pitch = cosf(rad_pitch);
	auto cos_yaw = cosf(rad_yaw);
	float cos_roll;

	if (side || up) {
		rad_roll = math::deg_to_rad(this->z);
		sin_roll = sinf(rad_roll);
		cos_roll = cosf(rad_roll);
	}

	if (side) {
		side->x = -1.0f * sin_roll * sin_pitch * cos_yaw + -1.0f * cos_roll * -sin_yaw;
		side->y = -1.0f * sin_roll * sin_pitch * sin_yaw + -1.0f * cos_roll * cos_yaw;
		side->z = -1.0f * sin_roll * cos_pitch;
	}

	if (up) {
		up->x = cos_roll * sin_pitch * cos_yaw + -sin_roll * -sin_yaw;
		up->y = cos_roll * sin_pitch * sin_yaw + -sin_roll * cos_yaw;
		up->z = cos_roll * cos_pitch;
	}

	return { cos_pitch * cos_yaw, cos_pitch * sin_yaw, -sin_pitch };
}