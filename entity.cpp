#include "includes.h"

void CCSGOPlayerAnimState::reset(void)
{
	m_last_update_frame = 0;
	m_step_height_left = 0;
	m_step_height_right = 0;

	//m_weapon = m_player->get_weapon();
	m_weapon_last = m_weapon;

	m_weapon_last_bone_setup = m_weapon;
	m_eye_position_smooth_lerp = 0;
	m_strafe_change_weight_smooth_fall_off = 0;
	m_first_foot_plant_since_init = true;

	m_last_update_time = 0;
	m_last_update_increment = 0;

	m_eye_yaw = 0;
	m_eye_pitch = 0;
	m_abs_yaw = 0;
	m_abs_yaw_last = 0;
	m_move_yaw = 0;
	m_move_yaw_ideal = 0;
	m_move_yaw_current_to_ideal = 0;

	m_stand_walk_how_long_to_wait_until_transition_can_blend_in = 0.4f;
	m_stand_walk_how_long_to_wait_until_transition_can_blend_out = 0.2f;
	m_stand_run_how_long_to_wait_until_transition_can_blend_in = 0.2f;
	m_stand_run_how_long_to_wait_until_transition_can_blend_out = 0.4f;
	m_crouch_walk_how_long_to_wait_until_transition_can_blend_in = 0.3f;
	m_crouch_walk_how_long_to_wait_until_transition_can_blend_out = 0.3f;

	m_primary_cycle = 0;
	m_move_weight = 0;
	m_move_weight_smoothed = 0;
	m_anim_duck_amount = 0;
	m_duck_additional = 0; // for when we duck a bit after landing from a jump
	m_recrouch_weight = 0;

	m_position_current.clear();
	m_position_last.clear();

	m_velocity.clear();
	m_velocity_normalized.clear();
	m_velocity_normalized_non_zero.clear();
	m_velocity_length_xy = 0;
	m_velocity_length_z = 0;

	m_speed_as_portion_of_run_top_speed = 0;
	m_speed_as_portion_of_walk_top_speed = 0;
	m_speed_as_portion_of_crouch_top_speed = 0;

	m_duration_moving = 0;
	m_duration_still = 0;

	m_on_ground = true;

	m_land_anim_multiplier = 1.0f;
	m_left_ground_height = 0;
	m_landing = false;
	m_jump_to_fall = 0;
	m_duration_in_air = 0;

	m_walk_run_transition = 0;

	m_landed_on_ground_this_frame = false;
	m_left_the_ground_this_frame = false;
	m_in_air_smooth_value = 0;

	m_on_ladder = false;
	m_ladder_weight = 0;
	m_ladder_speed = 0;

	m_walk_to_run_transition_state = 0;

	m_defuse_started = false;
	m_plant_anim_started = false;
	m_twitch_anim_started = false;
	m_adjust_started = false;

	m_next_twitch_time = 0;

	m_time_of_last_known_injury = 0;

	m_last_velocity_test_time = 0;
	m_velocity_last.clear();
	m_target_acceleration.clear();
	m_acceleration.clear();
	m_acceleration_weight = 0;

	m_aim_matrix_transition = 0;
	m_aim_matrix_transition_delay = 0;

	m_flashed = 0;

	m_strafe_change_weight = 0;
	m_strafe_change_target_weight = 0;
	m_strafe_change_cycle = 0;
	m_strafe_sequence = -1;
	m_strafe_changing = false;
	m_duration_strafing = 0;

	m_foot_lerp = 0;

	m_feet_crossed = false;

	m_player_is_accelerating = false;

	m_duration_move_weight_is_too_high = 0;
	m_static_approach_speed = 80;

	m_stutter_step = 0;
	m_previous_move_state = 0;

	m_action_weight_bias_remainder = 0;

	m_aim_yaw_min = CSGO_ANIM_AIMMATRIX_DEFAULT_YAW_MIN;
	m_aim_yaw_max = CSGO_ANIM_AIMMATRIX_DEFAULT_YAW_MAX;
	m_aim_pitch_min = CSGO_ANIM_AIMMATRIX_DEFAULT_PITCH_MIN;
	m_aim_pitch_max = CSGO_ANIM_AIMMATRIX_DEFAULT_PITCH_MAX;

	memset(&m_activity_modifiers_server[0], 0, 20);

	m_first_run_since_init = true;

	m_camera_smooth_height = 0;
	m_smooth_height_valid = false;
	m_last_time_velocity_over_ten = 0;
}

//----------------------------------------------------------------------------------
int animstate_pose_param_cache_t::get_index(void)
{
	if (!m_initialized)
		return -1;

	return m_index;
}

float animstate_pose_param_cache_t::get_value(Player* pPlayer)
{
	//if (!m_initialized)
	//{
	//	init(pPlayer, m_name);
	//}
	//if (m_initialized && pPlayer)
	//{
	//	return pPlayer->GetPoseParameter(m_index);
	//}
	return 0;
}

void animstate_pose_param_cache_t::set_value(Player* pPlayer, float flValue)
{
	//if (!m_initialized)
	//{
		//init(pPlayer, m_name);
	//}
	if (m_initialized && pPlayer && flValue >= 0)
	{
		pPlayer->SetPoseParameters(&flValue);
	}
}

bool Player::SetupHitboxPoints(BoneArray* bones, int index, std::vector< vec3_t >& points) {
	// reset points.
	points.clear();

	const model_t* model = this->GetModel();
	if (!model) return false;

	studiohdr_t* hdr = g_csgo.m_model_info->GetStudioModel(model);
	if (!hdr) return false;

	mstudiohitboxset_t* set = hdr->GetHitboxSet(this->m_nHitboxSet());
	if (!set) return false;

	mstudiobbox_t* bbox = set->GetHitbox(index);
	if (!bbox) return false;

	float POINT_SCALE = 60.f;
	switch (index)
	{
	case HITBOX_HEAD:
		POINT_SCALE = g_menu.main.aimbot.scale.get();
		break;
	case HITBOX_CHEST:
	case HITBOX_UPPER_CHEST:
		POINT_SCALE = g_menu.main.aimbot.chest_scale.get();
		break;
	case HITBOX_PELVIS:
	case HITBOX_BODY:
	case HITBOX_THORAX:
		POINT_SCALE = g_menu.main.aimbot.body_scale.get();
		break;
	case HITBOX_R_THIGH:
	case HITBOX_L_THIGH:
	case HITBOX_R_CALF:
	case HITBOX_L_CALF:
		POINT_SCALE = g_menu.main.aimbot.legs_scale.get();
		break;
	case HITBOX_R_FOOT:
	case HITBOX_L_FOOT:
		POINT_SCALE = g_menu.main.aimbot.feet_scale.get();
		break;
	default:
		break;
	}

	POINT_SCALE *= 0.01f;

	// these indexes represent boxes.
	if (bbox->m_radius <= 0.f) {
		// references: 
		//      https://developer.valvesoftware.com/wiki/Rotation_Tutorial
		//      CBaseAnimating::GetHitboxBonePosition
		//      CBaseAnimating::DrawServerHitboxes

		// convert rotation angle to a matrix.
		matrix3x4_t rot_matrix;
		g_csgo.AngleMatrix(bbox->m_angle, rot_matrix);

		// apply the rotation to the entity input space (local).
		matrix3x4_t matrix;
		math::ConcatTransforms(bones[bbox->m_bone], rot_matrix, matrix);

		// extract origin from matrix.
		const vec3_t origin = matrix.GetOrigin();

		// compute raw center point.
		vec3_t center = (bbox->m_mins + bbox->m_maxs) / 2.f;

		if (!g_menu.main.aimbot.multipoint.get(4) && (index == HITBOX_R_FOOT || index == HITBOX_L_FOOT)) {
			points.emplace_back(center.x, center.y, center.z);
			return true;
		}

		// run our feet multipoint
		else {
			// the feet hiboxes have a side, heel and the toe.
			if (index == HITBOX_R_FOOT || index == HITBOX_L_FOOT) {
				const float d2 = (bbox->m_mins.x - center.x) * POINT_SCALE;
				const float d3 = (bbox->m_maxs.x - center.x) * POINT_SCALE;

				// heel.
				points.emplace_back(center.x + d2, center.y, center.z);

				// toe.
				points.emplace_back(center.x + d3, center.y, center.z);
			}
		}

		// nothing to do here we are done.
		if (points.empty())
			return false;

		// rotate our bbox points by their correct angle
		// and convert our points to world space.
		for (auto& p : points) {
			// VectorRotate.
			// rotate point by angle stored in matrix.
			p = { p.dot(matrix[0]), p.dot(matrix[1]), p.dot(matrix[2]) };

			// transform point to world space.
			p += origin;
		}
	}

	// these hitboxes are capsules.
	else {
		vec3_t min, max;
		math::VectorTransform(bbox->m_mins, bones[bbox->m_bone], min);
		math::VectorTransform(bbox->m_maxs, bones[bbox->m_bone], max);

		// this might be the second issue
		vec3_t center = (bbox->m_maxs + bbox->m_mins) / 2.f;
		vec3_t centerTrans = center;
		math::VectorTransform(centerTrans, bones[bbox->m_bone], centerTrans);

		// commented for testing
		//if (index == HITBOX_R_THIGH || index == HITBOX_L_THIGH || index == HITBOX_R_CALF || index == HITBOX_L_CALF) {
		//	points.emplace_back(centerTrans);
		//	return true;
		//}

		if (!g_menu.main.aimbot.multipoint.get(0) && index == HITBOX_HEAD
		 || !g_menu.main.aimbot.multipoint.get(1) && (index == HITBOX_UPPER_CHEST || index == HITBOX_CHEST)
		 || !g_menu.main.aimbot.multipoint.get(2) && (index == HITBOX_BODY || index == HITBOX_THORAX || index == HITBOX_PELVIS)
		 || !g_menu.main.aimbot.multipoint.get(3) && (index == HITBOX_R_THIGH || index == HITBOX_L_THIGH || index == HITBOX_R_CALF || index == HITBOX_L_CALF)) {
			points.emplace_back(centerTrans);
			return true;
		}

		auto aeye = g_cl.m_local->GetEyePosition();

		auto delta = centerTrans - aeye;
		delta.normalized();

		auto max_min = max - min;
		max_min.normalized();

		auto cr = max_min.cross(delta);

		ang_t d_angle;
		math::VectorAngles(delta, d_angle);

		bool vertical = index == HITBOX_HEAD;

		vec3_t right, up;
		if (vertical) {
			ang_t cr_angle;
			math::VectorAngles(cr, cr_angle);
			cr_angle.to_vectors(&right, &up);
			cr_angle.z = d_angle.x;

			vec3_t _up = up, _right = right, _cr = cr;
			cr = _right;
			right = _cr;
		}
		else math::VectorVectors(delta, up, right);

		RayTracer::Hitbox box(min, max, bbox->m_radius);
		RayTracer::Trace trace;
		points.emplace_back(centerTrans);

		RayTracer::Ray ray = RayTracer::Ray(aeye, centerTrans - ((vertical ? cr : up) * 1000.0f));
		RayTracer::TraceFromCenter(ray, box, trace, RayTracer::Flags_RETURNEND);
		points.push_back(trace.m_traceEnd);

		ray = RayTracer::Ray(aeye, centerTrans + ((vertical ? up : up) * 1000.0f));
		RayTracer::TraceFromCenter(ray, box, trace, RayTracer::Flags_RETURNEND);
		points.push_back(trace.m_traceEnd);

		for (size_t i = 1; i < points.size(); ++i) {
			auto delta_center = points[i] - centerTrans;
			points[i] = centerTrans + delta_center * POINT_SCALE;
		}

		// nothing left to do here.
		if (points.empty()) return false;
	}

	return true;
}