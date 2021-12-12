#include "includes.h"

Aimbot g_aimbot{ };;

// now this is a fucking animation fix god damn lmfao
void AimPlayer::UpdateAnimations(LagRecord* record) {
	CCSGOPlayerAnimState* state = m_player->m_PlayerAnimState();
	if (!state || !m_player) return;

	auto weapon = m_player->GetActiveWeapon();
	if (!weapon) return;

	auto weaponinfo = weapon->GetWpnData();
	if (!weaponinfo) return;

	// player respawned.
	if (m_player->m_flSpawnTime() != m_spawn) {
		// reset animation state.
		game::ResetAnimationState(state);

		// note new spawn time.
		m_spawn = m_player->m_flSpawnTime();
	}

	// is player a bot?
	bool bot = game::IsFakePlayer(m_player->index());

	// we don't need to apply anything else if we are targetting a bot
	if (bot) return;

	// backup stuff that we do not want to fuck with.
	AnimationBackup_t backup;

	// first off lets backup our globals.
	auto curtime = g_csgo.m_globals->m_curtime;
	auto realtime = g_csgo.m_globals->m_realtime;
	auto frametime = g_csgo.m_globals->m_frametime;
	auto absframetime = g_csgo.m_globals->m_abs_frametime;
	auto framecount = g_csgo.m_globals->m_frame;
	auto tickcount = g_csgo.m_globals->m_tick_count;
	auto interpolation = g_csgo.m_globals->m_interp_amt;

	backup.m_origin = m_player->m_vecOrigin();
	backup.m_abs_origin = m_player->GetAbsOrigin();
	backup.m_velocity = m_player->m_vecVelocity();
	backup.m_abs_velocity = m_player->m_vecAbsVelocity();
	backup.m_flags = m_player->m_fFlags();
	backup.m_eflags = m_player->m_iEFlags();
	backup.m_duck = m_player->m_flDuckAmount();
	backup.m_body = m_player->m_flLowerBodyYawTarget();
	backup.m_move_weight = state->m_move_weight;
	backup.m_primary_cycle = state->m_primary_cycle;
	m_player->GetAnimLayers(backup.m_layers);

	// store our layer data for later usage when fixing animations.
	record->m_move_weight = record->m_layers[6].m_weight;
	record->m_primary_cycle = record->m_layers[6].m_cycle; // ANIMATION_LAYER_MOVEMENT_MOVE
	record->m_strafe_sequence = record->m_layers[7].m_sequence; // ANIMATION_LAYER_MOVEMENT_STRAFECHANGE
	record->m_strafe_change_weight = record->m_layers[7].m_weight;
	record->m_strafe_change_cycle = record->m_layers[7].m_cycle;
	record->m_acceleration_weight = record->m_layers[12].m_weight; // ANIMATION_LAYER_LEAN
	// we should probably back up this data too to restore it after modulation; TODO: add that motherfucker

	// prepare our targeted entities animations for modulation
	if (m_records.size() >= 2) { // jesus fucking christ how many goddamn times are you going to do this check you stupid ass motherfucker holy shit
		// get pointer to previous record.
		LagRecord* previous = m_records[1].get();

		if (previous && !previous->m_dormant && previous->m_data_stored && !record->m_first_after_dormancy) {
			state->m_primary_cycle = previous->m_primary_cycle;
			state->m_move_weight = previous->m_move_weight;
			state->m_strafe_sequence = previous->m_strafe_sequence;
			state->m_strafe_change_weight = previous->m_strafe_change_weight;
			state->m_strafe_change_cycle = previous->m_strafe_change_cycle;
			state->m_acceleration_weight = previous->m_acceleration_weight;

			memcpy(m_player->AnimLayersPointer(), previous->m_layers, 0x38 * m_player->GetNumAnimOverlays());
		}
		else {
			if (record->m_flags & FL_ONGROUND) {
				state->m_on_ground = true;
				state->m_landing = false;
			}

			state->m_primary_cycle = record->m_layers[6].m_cycle;
			state->m_move_weight = record->m_layers[6].m_weight;
			state->m_strafe_sequence = record->m_strafe_sequence;
			state->m_strafe_change_weight = record->m_strafe_change_weight;
			state->m_strafe_change_cycle = record->m_strafe_change_cycle;
			state->m_acceleration_weight = record->m_acceleration_weight;
			state->m_duration_in_air = 0.f;
			m_player->m_flPoseParameter()[6] = 0.f;

			memcpy(m_player->AnimLayersPointer(), record->m_layers, 0x38 * m_player->GetNumAnimOverlays());
			state->m_last_update_time = (record->m_sim_time - g_csgo.m_globals->m_interval);
		}
	}

	int ticks_to_simulate = 1;

	if (m_records.size() >= 2) {
		// get pointer to previous record.
		LagRecord* previous = m_records[1].get();

		if (previous && !previous->dormant() && previous->m_data_stored) {
			int simulation_ticks = game::TIME_TO_TICKS(record->m_sim_time - previous->m_sim_time);

			if ((simulation_ticks - 1) > 31 || previous->m_sim_time == 0.f)
				simulation_ticks = 1;

			auto layer_cycle = record->m_layers[11].m_cycle;
			auto previous_playback = previous->m_layers[11].m_playback_rate;

			if (previous_playback > 0.f && record->m_layers[11].m_playback_rate > 0.f
				&& previous->m_layers[11].m_sequence == record->m_layers[11].m_sequence) {
				auto previous_cycle = previous->m_layers[11].m_cycle;
				simulation_ticks = 0;

				if (previous_cycle > layer_cycle)
					layer_cycle = layer_cycle + 1.0f;

				while (layer_cycle > previous_cycle) {
					const auto ticks_backup = simulation_ticks;
					const auto playback_mult_ipt = g_csgo.m_globals->m_interval * previous_playback;

					previous_cycle = previous_cycle + (g_csgo.m_globals->m_interval * previous_playback);

					if (previous_cycle >= 1.0f)
						previous_playback = record->m_layers[11].m_playback_rate;

					++simulation_ticks;

					if (previous_cycle > layer_cycle && (previous_cycle - layer_cycle) > (playback_mult_ipt * 0.5f))
						simulation_ticks = ticks_backup;
				}
			}

			ticks_to_simulate = simulation_ticks;

			//if (record->m_exploiting) record->m_sim_time = previous->m_sim_time + game::TICKS_TO_TIME(simulation_ticks); // fix exploiting check then this will probably be decent lmfao
		}

		ticks_to_simulate = std::clamp(ticks_to_simulate, 1, 64);

		record->m_lag = ticks_to_simulate;
		record->m_time_delta = float(ticks_to_simulate) * g_csgo.m_globals->m_interval;
		record->m_started_anim_update_time = record->m_sim_time - record->m_time_delta;
	}

	// reset resolver/fakewalk/fakeflick state.
	record->m_mode = Resolver::Modes::RESOLVE_NONE;
	record->m_fake_walk = false;

	// set these fucks, they'll both get overriden later.
	record->m_anim_velocity = record->m_velocity;
	record->m_anim_speed = record->m_anim_velocity.length_2d();

	// define this for later usage/
	record->m_max_current_speed = std::fminf(m_player->m_flMaxspeed(), m_player->m_bIsScoped() ? weaponinfo->m_max_player_speed_alt : weaponinfo->m_max_player_speed);

	// thanks llama.
	if (record->m_flags & FL_ONGROUND) {
		// they are on ground.
		state->m_on_ground = true;
		// no they didnt land.
		state->m_landing = false;
	}

	// modulate animation speeed to the correct values.
	if (record->m_flags & FL_ONGROUND
		&& record->m_layers[11].m_weight > 0.0f
		&& record->m_layers[11].m_weight < 1.0f) {
		auto val = (1.0f - record->m_layers[11].m_weight) * 0.35f;
		if (val > 0.0f && val < 1.0f) record->m_anim_speed = val + 0.55f;
		else record->m_anim_speed = -1.f;
	}

	if (_fdtest(&record->m_velocity.x) > 0
		|| _fdtest(&record->m_velocity.y) > 0
		|| _fdtest(&record->m_velocity.z) > 0)
		record->m_velocity.clear();

	// fix velocity.
	// thanks again L3D451R7
	if (record->m_lag > 0 && record->m_lag < 16 && m_records.size() >= 2) {
		// get pointer to previous record.
		LagRecord* previous = m_records[1].get();
		record->m_teleporting = record->m_origin.dist_to(previous->m_origin) > 4096.0f;

		// valid previous record.
		if (!record->dormant() && previous && !previous->dormant() && previous->valid() && previous->m_data_stored) {
			//
			//	calculate new velocity based on (new_origin - old_origin) / (new_time - old_time) formula.
			//
			if (record->m_lag > 1 && record->m_lag <= 20)
				record->m_velocity = (record->m_origin - previous->m_origin) / record->m_time_delta;

			if (abs(record->m_velocity.x) < 0.001f) record->m_velocity.x = 0.0f;
			if (abs(record->m_velocity.y) < 0.001f) record->m_velocity.y = 0.0f;
			if (abs(record->m_velocity.z) < 0.001f) record->m_velocity.z = 0.0f;

			if (_fdtest(&record->m_velocity.x) > 0
				|| _fdtest(&record->m_velocity.y) > 0
				|| _fdtest(&record->m_velocity.z) > 0)
				record->m_velocity.clear();

			auto curr_direction = math::rad_to_deg(std::atan2f(record->m_velocity.y, record->m_velocity.x));
			auto prev_direction = previous == nullptr ? FLT_MAX : math::rad_to_deg(std::atan2f(previous->m_velocity.y, previous->m_velocity.x));
			auto delta = math::normalize_float(curr_direction - prev_direction);

			//
			// these requirements pass only when layer[6].weight is accurate to normalized velocity.
			//
			if (record->m_flags & FL_ONGROUND
				&& record->m_velocity.length_2d() >= 0.1f
				&& std::abs(delta) < 1.0f
				&& std::abs(record->m_duck - previous->m_duck) <= 0.0f
				&& record->m_layers[6].m_playback_rate > previous->m_layers[6].m_playback_rate
				&& record->m_layers[6].m_weight > previous->m_layers[6].m_weight) {
				auto weight_speed = record->m_layers[6].m_weight;

				if (weight_speed <= 0.7f && weight_speed > 0.0f) {
					if (record->m_layers[6].m_playback_rate == 0.0f)
						record->m_velocity.clear();
					else {
						const auto m_post_velocity_lenght = record->m_velocity.length_2d();

						if (m_post_velocity_lenght != 0.0f) {
							float mult = 1;
							if (record->m_flags & 6) mult = 0.34f;
							else if (record->m_fake_walk) mult = 0.52f;

							record->m_velocity.x = (record->m_velocity.x / m_post_velocity_lenght) * (weight_speed * (record->m_max_current_speed * mult));
							record->m_velocity.y = (record->m_velocity.y / m_post_velocity_lenght) * (weight_speed * (record->m_max_current_speed * mult));
						}
					}
				}
			}

			//
			// fix velocity with fakelag.
			//
			if (record->m_flags & FL_ONGROUND && record->m_velocity.length_2d() > 0.1f && record->m_lag > 1) {
				//
				// get velocity lenght from 11th layer calc.
				//
				if (record->m_anim_speed > 0) {
					const auto m_pre_velocity_lenght = record->m_velocity.length_2d();
					auto* weapon = record->m_player->GetActiveWeapon();

					if (weapon) {
						auto wdata = weapon->GetWpnData();
						if (wdata) {
							auto adjusted_velocity = (record->m_anim_speed * record->m_max_current_speed) / m_pre_velocity_lenght;
							record->m_velocity.x *= adjusted_velocity;
							record->m_velocity.y *= adjusted_velocity;
						}
					}
				}
			}

			if (m_records.size() > 2 && record->m_lag > 1 && !record->dormant()
				&& previous->m_velocity.length() > 0 && !(record->m_flags & FL_ONGROUND && previous->m_flags & FL_ONGROUND)) {
				auto pre_pre_record = m_records[(m_records.size() - 2) & 63].get();

				if (!pre_pre_record->dormant() && pre_pre_record->valid()) {
					const auto prev_direction = math::rad_to_deg(std::atan2f(previous->m_velocity.y, previous->m_velocity.x));

					auto real_velocity = record->m_velocity.length_2d();

					float delta = curr_direction - prev_direction;

					if (delta <= 180.0f) {
						if (delta < -180.0f) delta = delta + 360.0f;
					}
					else delta = delta - 360.0f;

					float v63 = delta * 0.5f + curr_direction;
					auto direction = (v63 + 90.f) * 0.017453292f;

					record->m_velocity.x = sinf(direction) * real_velocity;
					record->m_velocity.y = cosf(direction) * real_velocity;
				}
			}

			if (!(record->m_flags & FL_ONGROUND)) record->m_velocity.z -= g_csgo.sv_gravity->GetFloat() * record->m_time_delta * 0.5f;
		}
		else if (record->valid()) {
			auto weight_speed = record->m_layers[6].m_weight;

			if (record->m_layers[6].m_playback_rate < 0.00001f) record->m_velocity.clear();
			else {
				const auto m_post_velocity_lenght = record->m_velocity.length_2d();

				if (m_post_velocity_lenght != 0.0f && weight_speed > 0.01f && weight_speed < 0.95f) {
					float mult = 1;
					if (record->m_flags & 6) mult = 0.34f;
					else if (record->m_fake_walk) mult = 0.52f;

					record->m_velocity.x = (record->m_velocity.x / m_post_velocity_lenght) * (weight_speed * (record->m_max_current_speed * mult));
					record->m_velocity.y = (record->m_velocity.y / m_post_velocity_lenght) * (weight_speed * (record->m_max_current_speed * mult));
				}
			}

			if (record->m_flags & FL_ONGROUND) record->m_velocity.z = 0;
		}

		if (_fdtest(&record->m_velocity.x) > 0
			|| _fdtest(&record->m_velocity.y) > 0
			|| _fdtest(&record->m_velocity.z) > 0)
			record->m_velocity.clear();

		//
		//	if server had 0 velocity at animation time -> reset velocity
		//
		if (record->m_flags & FL_ONGROUND && record->m_lag > 1 && record->m_velocity.length() > 0.1f && record->m_layers[6].m_playback_rate < 0.00001f) record->m_velocity.clear();

		//
		//  apply proper velocity and force flags so game will not try to recalculate it.
		//
		record->m_player->m_vecVelocity() = record->m_velocity;

		// TODO; add this and implement our fakewalk resolver!
		//if (previous->m_velocity.length_2d() == 0.0f && record->m_velocity.length_2d() != 0.0f) {
		//	math::VectorAngles(record->m_velocity, record->m_dir_when_first_started_moving);
		//}
	}

	// fix CGameMovement::FinishGravity
	if (!(m_player->m_fFlags() & FL_ONGROUND)) record->m_velocity.z -= game::TICKS_TO_TIME(g_csgo.sv_gravity->GetFloat());
	else record->m_velocity.z = 0.0f;

	// fix various issues with the game.
	// these issues can only occur when a player is choking data.
	if (record->m_lag > 1) {
		auto speed = record->m_velocity.length();

		// or theres this way; this can go where the old on was at
		bool bFakewalking = false,
			stage1 = false,            // stages needed cause we are iterating all layers, eitherwise won't work :)
			stage2 = false,
			stage3 = false;

		for (int i = 0; i < 13; i++) {
			if (record->m_layers[i].m_sequence == 26 && record->m_layers[i].m_weight < 0.4f)
				stage1 = true;
			if (record->m_layers[i].m_sequence == 7 && record->m_layers[i].m_weight > 0.001f)
				stage2 = true;
			if (record->m_layers[i].m_sequence == 2 && record->m_layers[i].m_weight == 0)
				stage3 = true;
		}

		if (stage1 && stage2)
			if (stage3 || (record->m_player->m_fFlags() & FL_DUCKING)) // since weight from stage3 can be 0 aswell when crouching, we need this kind of check, cause you can fakewalk while crouching, thats why it's nested under stage1 and stage2
				bFakewalking = true;
			else
				bFakewalking = false;
		else
			bFakewalking = false;

		record->m_fake_walk = bFakewalking;

		// null this out while fakewalking.
		if (record->m_fake_walk) record->m_anim_velocity = record->m_velocity = { 0.f, 0.f, 0.f };

		// we need atleast 2 updates/records
		// to fix these issues.
		if (m_records.size() >= 2) {
			// get pointer to previous record.
			LagRecord* previous = m_records[1].get();

			// valid previous record.
			if (previous && !previous->dormant() && previous->m_data_stored) {
				/*if ((record->m_layers[12].m_flWeight < 0.01f
					|| abs(previous->m_layers[12].m_flWeight - record->m_layers[12].m_flWeight) < 0.01f)
					&& previous->m_layers[6].m_nSequence == record->m_layers[6].m_nSequence)
				{
					//2 = -1; 3 = 1; 1 = fake;
					if (std::abs(record->m_layers[6].m_flWeight - previous->m_layers[6].m_flWeight) < 0.01f)
					{
						float delta1 = std::abs(resolver_info->resolver_layers[0][6].m_flPlaybackRate - record->anim_layers[6].m_flPlaybackRate);
						float delta2 = std::abs(resolver_info->resolver_layers[1][6].m_flPlaybackRate - record->anim_layers[6].m_flPlaybackRate);
						float delta3 = std::abs(resolver_info->resolver_layers[2][6].m_flPlaybackRate - record->anim_layers[6].m_flPlaybackRate);

						if (int(delta1 * 1000.f) < int(delta2 * 1000.f) || int(delta3 * 1000.f) <= int(delta2 * 1000.f) || int(delta2 * 1000.0f)) {
							if (int(delta1 * 1000.f) >= int(delta3 * 1000.f) && int(delta2 * 1000.f) > int(delta3 * 1000.f) && !int(delta3 * 1000.0f))
							{
								record->animations_index = 1;
								resolver_info->anims_pre_resolving = 1;
								resolver_info->anim_time = csgo.m_globals()->realtime;
								resolver_info->did_anims_update = true;
								record->animations_updated = true;
							}
						}
						else
						{
							record->animations_index = 2;
							resolver_info->anims_pre_resolving = 2;
							resolver_info->anim_time = csgo.m_globals()->realtime;
							resolver_info->did_anims_update = true;
							record->animations_updated = true;
						}
					}
				}*/

				const auto velocity_per_tick = (record->m_velocity - previous->m_velocity) / record->m_lag;

				for (auto i = 1; i <= record->m_lag; i++) {
					auto simulated_time = record->m_started_anim_update_time + game::TICKS_TO_TIME(i);
					auto velocity = ((velocity_per_tick * (float)i) + previous->m_velocity);

					// LOL.
					if ((record->m_origin - previous->m_origin).IsZero()) record->m_anim_velocity = record->m_velocity = { 0.f, 0.f, 0.f };

					// jumpfall.
					bool bOnGround = record->m_flags & FL_ONGROUND;
					bool bJumped = false;
					bool bLandedOnServer = false;
					float flLandTime = 0.f;

					// magic llama code.
					if (record->m_layers[4].m_cycle < 0.5f && (!(record->m_flags & FL_ONGROUND) || !(previous->m_flags & FL_ONGROUND))) {
						flLandTime = record->m_sim_time - float(record->m_layers[4].m_playback_rate / record->m_layers[4].m_cycle);
						bLandedOnServer = flLandTime >= previous->m_sim_time;
					}

					// jump_fall fix
					if (bLandedOnServer && !bJumped) {
						if (flLandTime <= record->m_anim_time) {
							bJumped = true;
							bOnGround = true;
						}
						else bOnGround = previous->m_flags & FL_ONGROUND;
					}

					// do the fix. hahaha
					if (bOnGround) m_player->m_fFlags() |= FL_ONGROUND;
					else m_player->m_fFlags() &= ~FL_ONGROUND;

					// delta in duckamt and delta in time..
					float duck = record->m_duck - previous->m_duck;
					float time = record->m_sim_time - previous->m_sim_time;

					// get the duckamt change per tick.
					float change = (duck / time) * g_csgo.m_globals->m_interval;

					// fix crouching players.
					m_player->m_flDuckAmount() = previous->m_duck + change;

					if (!record->m_fake_walk) {
						// fix the velocity till the moment of animation.
						vec3_t velo = record->m_velocity - previous->m_velocity;

						// accel per tick.
						vec3_t accel = (velo / time) * g_csgo.m_globals->m_interval;

						// set the anim velocity to the previous velocity.
						// and predict one tick ahead.
						record->m_anim_velocity = previous->m_velocity + accel;
					}

					if (i == record->m_lag) {
						simulated_time = record->m_sim_time;
						m_player->m_flDuckAmount() = fminf(fmaxf(record->m_duck, 0.0f), 1.0f);
						m_player->m_flLowerBodyYawTarget() = record->m_body;
						m_player->m_fFlags() = record->m_flags;
					}

					m_player->m_vecAbsVelocity() = m_player->m_vecVelocity() = velocity;

					auto realtime_backup = g_csgo.m_globals->m_realtime;
					auto curtime = g_csgo.m_globals->m_curtime;
					auto frametime = g_csgo.m_globals->m_frametime;
					auto absoluteframetime = g_csgo.m_globals->m_abs_frametime;
					auto framecount = g_csgo.m_globals->m_frame;
					auto tickcount = g_csgo.m_globals->m_tick_count;
					auto interpolation_amount = g_csgo.m_globals->m_interp_amt;

					int ticks = game::TIME_TO_TICKS(simulated_time);

					g_csgo.m_globals->m_realtime = simulated_time;
					g_csgo.m_globals->m_curtime = simulated_time;
					g_csgo.m_globals->m_frametime = g_csgo.m_globals->m_interval;
					g_csgo.m_globals->m_abs_frametime = g_csgo.m_globals->m_interval;
					g_csgo.m_globals->m_frame = ticks;
					g_csgo.m_globals->m_tick_count = ticks;
					g_csgo.m_globals->m_interp_amt = 0.f;

					// these are both reset at the end of the function
					//g_csgo.m_globals->m_realtime = realtime_backup;
					//g_csgo.m_globals->m_curtime = curtime;
					//g_csgo.m_globals->m_frametime = frametime;
					//g_csgo.m_globals->m_abs_frametime = absoluteframetime;
					//g_csgo.m_globals->m_frame = framecount;
					//g_csgo.m_globals->m_tick_count = tickcount;
					//g_csgo.m_globals->m_interp_amt = interpolation_amount;
				}
			}
		}
	}
	else {
		m_player->m_flLowerBodyYawTarget() = record->m_body;
		auto vel = record->m_velocity;

		m_player->m_flDuckAmount() = fminf(fmaxf(record->m_duck, 0.0f), 1.0f);
		m_player->m_fFlags() = record->m_flags;

		auto realtime_backup = g_csgo.m_globals->m_realtime;
		auto curtime = g_csgo.m_globals->m_curtime;
		auto frametime = g_csgo.m_globals->m_frametime;
		auto absoluteframetime = g_csgo.m_globals->m_abs_frametime;
		auto framecount = g_csgo.m_globals->m_frame;
		auto tickcount = g_csgo.m_globals->m_tick_count;
		auto interpolation_amount = g_csgo.m_globals->m_interp_amt;

		int ticks = game::TIME_TO_TICKS(record->m_sim_time);

		g_csgo.m_globals->m_realtime = record->m_sim_time;
		g_csgo.m_globals->m_curtime = record->m_sim_time;
		g_csgo.m_globals->m_frametime = g_csgo.m_globals->m_interval;
		g_csgo.m_globals->m_abs_frametime = g_csgo.m_globals->m_interval;
		g_csgo.m_globals->m_frame = ticks;
		g_csgo.m_globals->m_tick_count = ticks;
		g_csgo.m_globals->m_interp_amt = 0.f;

		m_player->m_vecAbsVelocity() = m_player->m_vecVelocity() = vel;

		//g_csgo.m_globals->m_realtime = realtime_backup;
		//g_csgo.m_globals->m_curtime = curtime;
		//g_csgo.m_globals->m_frametime = frametime;
		//g_csgo.m_globals->m_absoluteframetime = absoluteframetime;
		//g_csgo.m_globals->m_framecount = framecount;
		//g_csgo.m_globals->m_tickcount = tickcount;
		//g_csgo.m_globals->m_interp_amt = interpolation_amount;
	}

	for (int i = 0; i < 13; i++) m_player->m_AnimOverlay()[i].m_owner = m_player;

	// if using fake angles, correct angles.
	g_resolver.ResolveAngles(m_player, record);

	// force to use correct abs origin and velocity ( no CalcAbsolutePosition and CalcAbsoluteVelocity calls )
	m_player->m_iEFlags() &= ~(EFL_DIRTY_ABSTRANSFORM | EFL_DIRTY_ABSVELOCITY);

	// set stuff before animating.
	m_player->m_vecOrigin() = record->m_origin;
	m_player->m_vecVelocity() = m_player->m_vecAbsVelocity() = record->m_anim_velocity;
	m_player->m_flLowerBodyYawTarget() = record->m_body;

	// write potentially resolved angles.
	m_player->m_angEyeAngles() = record->m_eye_angles;

	// get invalidated bone cache.
	static auto& invalidatebonecache = pattern::find(g_csgo.m_client_dll, XOR("C6 05 ? ? ? ? ? 89 47 70")).add(0x2);

	// make sure we keep track of the original invalidation state
	const auto oldbonecache = invalidatebonecache;

	// update animtions now.
	m_player->m_bClientSideAnimation() = true;
	m_player->UpdateClientSideAnimation();
	m_player->m_bClientSideAnimation() = false;

	// we don't want to enable cache invalidation by accident
	invalidatebonecache = oldbonecache;

	// player animations & angles have updated; state that to the game so it rerenders them.
	m_player->InvalidatePhysicsRecursive(InvalidatePhysicsBits_t::ANGLES_CHANGED);
	m_player->InvalidatePhysicsRecursive(InvalidatePhysicsBits_t::ANIMATION_CHANGED);

	// correct poses.
	g_resolver.ResolvePoses(m_player, record);

	// store updated/animated poses and rotation in lagrecord.
	m_player->GetPoseParameters(record->m_poses);
	record->m_abs_ang = m_player->GetAbsAngles();

	// restore backup data.
	m_player->m_vecOrigin() = backup.m_origin;
	m_player->m_vecVelocity() = backup.m_velocity;
	m_player->m_vecAbsVelocity() = backup.m_abs_velocity;
	m_player->m_fFlags() = backup.m_flags;
	m_player->m_iEFlags() = backup.m_eflags;
	m_player->m_flDuckAmount() = backup.m_duck;
	m_player->m_flLowerBodyYawTarget() = backup.m_body;
	m_player->SetAbsOrigin(backup.m_abs_origin);
	m_player->SetAnimLayers(backup.m_layers);

	// restore globals.
	g_csgo.m_globals->m_curtime = curtime;
	g_csgo.m_globals->m_realtime = realtime;
	g_csgo.m_globals->m_frametime = frametime;
	g_csgo.m_globals->m_abs_frametime = absframetime;
	g_csgo.m_globals->m_frame = framecount;
	g_csgo.m_globals->m_tick_count = tickcount;
	g_csgo.m_globals->m_interp_amt = interpolation;

	record->m_animated = true;

	// IMPORTANT: do not restore poses here, since we want to preserve them for rendering.
	// also dont restore the render angles which indicate the model rotation.
}

void AimPlayer::OnNetUpdate(Player* player) {
	bool reset = (!g_menu.main.aimbot.enable.get() || player->m_lifeState() == LIFE_DEAD || !player->enemy(g_cl.m_local));
	bool disable = (!reset && !g_cl.m_processing);

	// if this happens, delete all the lagrecords.
	if (reset) {
		player->m_bClientSideAnimation() = true;
		m_records.clear();
		return;
	}

	// just disable anim if this is the case.
	if (disable) {
		player->m_bClientSideAnimation() = true;
		return;
	}

	// update player ptr if required.
	// reset player if changed.
	if (m_player != player)
		m_records.clear();

	// update player ptr.
	m_player = player;

	// indicate that this player has been out of pvs.
	// insert dummy record to separate records
	// to fix stuff like animation and prediction.
	if (player->dormant()) {
		bool insert = true;

		// we have any records already?
		if (!m_records.empty()) {

			LagRecord* front = m_records.front().get();

			// we already have a dormancy separator.
			if (front->dormant())
				insert = false;
		}

		if (insert) {
			// add new record.
			m_records.emplace_front(std::make_shared< LagRecord >(player));

			// get reference to newly added record.
			LagRecord* current = m_records.front().get();

			// mark as dormant.
			current->m_dormant = true;
		}
	}

	bool update = (m_records.empty() || player->m_flSimulationTime() > m_records.front().get()->m_sim_time), silent_update = false;

	if (!update && !player->dormant() && player->m_vecOrigin() != player->m_vecOldOrigin()) {
		update = true;

		// fix data.
		player->m_flSimulationTime() = game::TICKS_TO_TIME(g_csgo.m_cl->m_server_tick);
	}

	if (m_records.size() > 0) {
		LagRecord* current = m_records.front().get();

		if (current && current->m_data_stored && !current->m_dormant) { // when i added this this wasn't if isn't dormant so maybe change this; but it doesn't really make any sense to have that lmao
			const auto layer_count = m_player->GetNumAnimOverlays();
			if (layer_count > 0) {
				bool animlayers_changed = false;

				for (auto i = 0; i < layer_count; i++) {
					auto layer = m_player->m_AnimOverlay()[i];
					auto& p_layer = current->m_layers[i];

					if (layer.m_cycle != p_layer.m_cycle
						|| layer.m_playback_rate != p_layer.m_playback_rate
						|| layer.m_weight != p_layer.m_weight
						|| layer.m_sequence != p_layer.m_sequence) {
						animlayers_changed = true;
						break;
					}
				}

				if (animlayers_changed && !update) {
					silent_update = true;
				}

				// old shit
				//if (!animlayers_changed) {
				//	m_player->m_flSimulationTime() = m_player->m_flOldSimulationTime();
				//	update = false;
				//}
				//else {
				//	if (!update) {
				//		silent_update = true;
				//		update = true;
				//	}
				//}
			}
		}
	}

	// this is the first data update we are receving
	// OR we received data with a newer simulation context.
	if (update) {
		// add new record.
		m_records.emplace_front(std::make_shared< LagRecord >(player));

		// get reference to newly added record.
		LagRecord* current = m_records.front().get();

		// mark as non dormant.
		current->m_dormant = false;
		current->m_first_after_dormancy = m_records.size() <= 1 || current->m_sim_time < 1;
		current->m_animated = false;

		// update animations on current record.
		// call resolver.
		UpdateAnimations(current);

		// create bone matrix for this record.
		g_bones.setup(m_player, nullptr, current);
	}

	if (!m_records.front()->m_valid && m_records.front()->m_teleporting && m_records.size() > 0) { for (auto& record : m_records) record->m_skip_due_to_resolver = true; }

	// no need to store insane amt of data.
	while (m_records.size() > 128) m_records.pop_back(); // was 256; i thought that was a little much

	//	 wow this guy is a gigantic homosexual
	//	while (m_records.size() > 0 && m_records.front()->m_exploiting) { // TODO; fix why our exploiting check always returns true! ( either that or the nigga i tested it against was exploiting lmao )
	//#ifdef SONTHTEST
	//		g_console.log(tfm::format(XOR("player: %s is exploiting; deleting all of his records"), game::GetPlayerName(m_records.front()->m_player->index())).c_str());
	//#endif
	//		m_records.clear(); // TODO; test and see if our animation fix will correct this so we won't have to do this!
	//	}
}

void AimPlayer::OnRoundStart(Player* player) {
	m_player = player;
	m_walk_record = LagRecord{ };
	m_shots = 0;
	m_missed_shots = 0;

	// reset stand and body index.
	m_stand_index = 0;
	m_stand_index2 = 0;
	m_body_index = 0;

	m_records.clear();
	m_hitboxes.clear();

	// IMPORTANT: DO NOT CLEAR LAST HIT SHIT.
}

void AimPlayer::SetupHitboxes(LagRecord* record, bool history) {
	// reset hitboxes.
	m_hitboxes.clear();


	bool prefer_head = record->m_velocity.length_2d() > 71.f;

	// prefer

	if (g_menu.main.aimbot.head1.get(0))
		m_hitboxes.push_back({ HITBOX_HEAD, HitscanMode::PREFER });

	if (g_menu.main.aimbot.head1.get(1) && prefer_head)
		m_hitboxes.push_back({ HITBOX_HEAD, HitscanMode::PREFER });

	if (g_menu.main.aimbot.head1.get(2) && !(record->m_mode != Resolver::Modes::RESOLVE_NONE && record->m_mode != Resolver::Modes::RESOLVE_WALK && record->m_mode != Resolver::Modes::RESOLVE_BODY))
		m_hitboxes.push_back({ HITBOX_HEAD, HitscanMode::PREFER });

	if (g_menu.main.aimbot.head1.get(3) && !(record->m_extrapolated_flags & FL_ONGROUND))
		m_hitboxes.push_back({ HITBOX_HEAD, HitscanMode::PREFER });

	// prefer, always.
	if (g_menu.main.aimbot.baim1.get(0))
		m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::PREFER });

	// prefer, lethal.
	if (g_menu.main.aimbot.baim1.get(1))
		m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::LETHAL });

	// prefer, lethal x2.
	if (g_menu.main.aimbot.baim1.get(2))
		m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::LETHAL2 });

	// prefer, fake.
	if (g_menu.main.aimbot.baim1.get(3) && record->m_mode != Resolver::Modes::RESOLVE_NONE && record->m_mode != Resolver::Modes::RESOLVE_WALK && record->m_mode != Resolver::Modes::RESOLVE_BODY)
		m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::PREFER });

	// prefer, in air.
	if (g_menu.main.aimbot.baim1.get(4) && !(record->m_extrapolated_flags & FL_ONGROUND))
		m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::PREFER });

	// prefer, in air.
	if (g_menu.main.aimbot.baim1.get(5) && (m_last_move >= g_menu.main.aimbot.misses.get() || m_unknown_move >= g_menu.main.aimbot.misses.get()))
		m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::PREFER });

	bool only{ false };

	// only, always.
	if (g_menu.main.aimbot.baim2.get(0)) {
		only = true;
		m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::PREFER });
	}

	// only, health.
	if (g_menu.main.aimbot.baim2.get(1) && m_player->m_iHealth() <= (int)g_menu.main.aimbot.baim_hp.get()) {
		only = true;
		m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::PREFER });
	}

	// only, fake.
	if (g_menu.main.aimbot.baim2.get(2) && record->m_mode != Resolver::Modes::RESOLVE_NONE && record->m_mode != Resolver::Modes::RESOLVE_WALK && record->m_mode != Resolver::Modes::RESOLVE_BODY) {
		only = true;
		m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::PREFER });
	}

	// only, in air.
	if (g_menu.main.aimbot.baim2.get(3) && !(record->m_extrapolated_flags & FL_ONGROUND)) {
		only = true;
		m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::PREFER });
	}

	// only, in air.
	if (g_menu.main.aimbot.baim2.get(4) && (m_last_move >= g_menu.main.aimbot.misses.get() || m_unknown_move >= g_menu.main.aimbot.misses.get())) {
		only = true;
		m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::PREFER });
	}

	// only, on key.
	if (g_input.GetKeyState(g_menu.main.aimbot.baim_key.get())) {
		only = true;
		m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::PREFER });
	}

	// only baim conditions have been met.
	// do not insert more hitboxes.
	if (only)
		return;

	std::vector< size_t > hitbox{ history ? g_menu.main.aimbot.hitbox_history.GetActiveIndices() : g_menu.main.aimbot.hitbox.GetActiveIndices() };
	if (hitbox.empty())
		return;

	for (const auto& h : hitbox) {
		if (g_cl.m_weapon_id == ZEUS) {
			// hitboxes for the zeus.
			m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::NORMAL });
			return;
		}

		// head.
		if (h == 0)
			m_hitboxes.push_back({ HITBOX_HEAD, HitscanMode::NORMAL });

		// neck
		if (h == 1) {
			m_hitboxes.push_back({ HITBOX_NECK, HitscanMode::NORMAL });
			m_hitboxes.push_back({ HITBOX_LOWER_NECK, HitscanMode::NORMAL });
		}

		// chest.
		if (h == 2) {
			m_hitboxes.push_back({ HITBOX_THORAX, HitscanMode::NORMAL });
			m_hitboxes.push_back({ HITBOX_CHEST, HitscanMode::NORMAL });
			m_hitboxes.push_back({ HITBOX_UPPER_CHEST, HitscanMode::NORMAL });
		}

		// stomach.
		if (h == 3) {
			m_hitboxes.push_back({ HITBOX_PELVIS, HitscanMode::NORMAL });
			m_hitboxes.push_back({ HITBOX_BODY, HitscanMode::NORMAL });
		}

		// arms.
		if (h == 4) {
			m_hitboxes.push_back({ HITBOX_L_UPPER_ARM, HitscanMode::NORMAL });
			m_hitboxes.push_back({ HITBOX_R_UPPER_ARM, HitscanMode::NORMAL });
			m_hitboxes.push_back({ HITBOX_R_FOREARM, HitscanMode::NORMAL });
			m_hitboxes.push_back({ HITBOX_L_FOREARM, HitscanMode::NORMAL });
		}

		// legs.
		if (h == 5) {
			m_hitboxes.push_back({ HITBOX_L_THIGH, HitscanMode::NORMAL });
			m_hitboxes.push_back({ HITBOX_R_THIGH, HitscanMode::NORMAL });
			m_hitboxes.push_back({ HITBOX_L_CALF, HitscanMode::NORMAL });
			m_hitboxes.push_back({ HITBOX_R_CALF, HitscanMode::NORMAL });
		}

		// feet
		if (h == 6) {
			m_hitboxes.push_back({ HITBOX_L_FOOT, HitscanMode::NORMAL });
			m_hitboxes.push_back({ HITBOX_R_FOOT, HitscanMode::NORMAL });
		}
	}
}

void Aimbot::init() {
	// clear old targets.
	m_targets.clear();

	m_target = nullptr;
	m_aim = vec3_t{ };
	m_angle = ang_t{ };
	m_damage = 0.f;
	m_record = nullptr;
	m_previous_record = nullptr;
	m_stop = false;

	m_best_dist = std::numeric_limits< float >::max();
	m_best_fov = 180.f + 1.f;
	m_best_damage = 0.f;
	m_best_hp = 100 + 1;
	m_best_lag = std::numeric_limits< float >::max();
	m_best_height = std::numeric_limits< float >::max();
}

void Aimbot::StripAttack() {
	if (g_cl.m_weapon_id == REVOLVER) g_cl.m_cmd->m_buttons &= ~IN_ATTACK2;
	else g_cl.m_cmd->m_buttons &= ~IN_ATTACK;
}

void Aimbot::think() {
	// do all startup routines.
	init();

	// sanity.
	if (!g_cl.m_weapon) return;

	// no grenades or bomb.
	if (g_cl.m_weapon_type == WEAPONTYPE_GRENADE || g_cl.m_weapon_type == WEAPONTYPE_C4) return;

	if (!g_cl.m_weapon_fire) StripAttack();

	// we have no aimbot enabled.
	if (!g_menu.main.aimbot.enable.get()) return;

	// animation silent aim, prevent the ticks with the shot in it to become the tick that gets processed.
	// we can do this by always choking the tick before we are able to shoot.
	bool revolver = g_cl.m_weapon_id == REVOLVER && g_cl.m_revolver_cock != 0;

	// one tick before being able to shoot.
	if (revolver && g_cl.m_revolver_cock > 0 && g_cl.m_revolver_cock == g_cl.m_revolver_query) {
		*g_cl.m_packet = false;
		return;
	}

	// we have a normal weapon or a non cocking revolver
	// choke if its the processing tick.
	if (g_cl.m_weapon_fire && !g_cl.m_lag && !revolver) {
		*g_cl.m_packet = false;
		StripAttack();
		return;
	}

	// no point in aimbotting if we cannot fire this tick.
	if (!g_cl.m_weapon_fire) return;

	// setup bones for all valid targets.
	for (int i{ 1 }; i <= g_csgo.m_globals->m_max_clients; ++i) {
		Player* player = g_csgo.m_entlist->GetClientEntity< Player* >(i);
		if (!IsValidTarget(player)) continue;

		AimPlayer* data = &m_players[i - 1];
		if (!data) continue;

		// store player as potential target this tick.
		m_targets.emplace_back(data);
	}

	// run knifebot.
	if (g_cl.m_weapon_type == WEAPONTYPE_KNIFE && g_cl.m_weapon_id != ZEUS) {
		if (g_menu.main.aimbot.knifebot.get()) knife();
		return;
	}

	// scan available targets... if we even have any.
	find();

	// finally set data when shooting.
	apply();
}

void Aimbot::find() {
	struct BestTarget_t { Player* player; vec3_t pos; float damage; int hitbox; LagRecord* record; LagRecord* prev_record; };
	vec3_t       tmp_pos;
	float        tmp_damage;
	int          tmp_hitbox;
	BestTarget_t best;
	best.player = nullptr;
	best.damage = -1.f;
	best.pos = vec3_t{ };
	best.record = nullptr;
	best.prev_record = nullptr;
	best.hitbox = -1;

	if (m_targets.empty()) return;
	if (g_cl.m_weapon_id == ZEUS && !g_menu.main.aimbot.zeusbot.get()) return;

	// iterate all targets.
	for (const auto& t : m_targets) {
		if (t->m_records.empty())
			continue;

		LagRecord* ideal = g_resolver.FindIdealRecord(t);
		if (ideal && !ideal->m_broke_lc) {
			t->SetupHitboxes(ideal, false);
			if (t->m_hitboxes.empty()) continue;

			// try to select best record as target.
			if (t->GetBestAimPosition(tmp_pos, tmp_damage, tmp_hitbox, ideal) && SelectTarget(ideal, tmp_pos, tmp_damage)) {
				// if we made it so far, set shit.
				best.player = t->m_player;
				best.hitbox = tmp_hitbox;
				best.pos = tmp_pos;
				best.damage = tmp_damage;
				best.record = ideal;
				best.prev_record = t->m_records.at(1).get();
#ifdef SONTHTEST // one of the very reasons i love this ide but this shit is fucked up let me do it in one line cunt
				g_console.log(XOR("targeting ideal record\n"));
#endif
			}
		}

		//TODO; test and see if we should add this check or not.
		//if (g_inputpred->m_frametime > g_csgo.m_globals->m_interval && ideal && ideal->m_data_filled && !ideal->m_broke_lc || !g_cl.m_weapon_fire) continue;

		LagRecord* last = g_resolver.FindLastRecord(t);
		if (!last || last == ideal) continue;

		t->SetupHitboxes(last, true);
		if (t->m_hitboxes.empty()) continue;

		// rip something went wrong..
		if (t->GetBestAimPosition(tmp_pos, tmp_damage, tmp_hitbox, last) && SelectTarget(last, tmp_pos, tmp_damage)) {
			// if we made it so far, set shit.
			best.player = t->m_player;
			best.hitbox = tmp_hitbox;
			best.pos = tmp_pos;
			best.damage = tmp_damage;
			best.record = last;
			best.prev_record = last;
#ifdef SONTHTEST
			g_console.log(XOR("targeting final record\n"));
#endif
		}

		LagRecord* front = t->m_records.front().get();
		if ((!front || !front->m_data_stored || front->m_dormant || !front->m_push_to_aimbot) || (front->m_broke_lc && !g_lagcomp.StartPrediction(t))) continue;

		t->SetupHitboxes(front, false);
		if (t->m_hitboxes.empty()) continue;

		// rip something went wrong..
		if (t->GetBestAimPosition(tmp_pos, tmp_damage, tmp_hitbox, front) && SelectTarget(front, tmp_pos, tmp_damage)) {
			// if we made it so far, set shit.
			best.player = t->m_player;
			best.hitbox = tmp_hitbox;
			best.pos = tmp_pos;
			best.damage = tmp_damage;
			best.record = front;
			best.prev_record = t->m_records.at(1).get();
#ifdef SONTHTEST
			g_console.log(XOR("targeting front record\n"));
#endif
		}
	}

	// verify our target and set needed data.
	if (best.player && best.record) {
		// calculate aim angle.
		math::VectorAngles(best.pos - g_cl.m_shoot_pos, m_angle);

		// set member vars.
		m_target = best.player;
		m_aim = best.pos;
		m_damage = best.damage;
		m_record = best.record;
		m_previous_record = best.prev_record;
		m_hitbox = best.hitbox;

		// write data, needed for traces / etc.
		m_record->cache();

		bool on = g_menu.main.config.mode.get() == 0;
		bool hit = (!g_cl.m_ground && g_cl.m_weapon_id == SSG08 && g_cl.m_weapon && g_cl.m_weapon->GetInaccuracy() < 0.009f) || (on && CheckHitchance(m_target, m_angle, m_record, best.hitbox));

		// do some premium autostop bullshit lmfao
		if (g_menu.main.aimbot.autostop.get() && (g_cl.m_weapon_type != WEAPONTYPE_KNIFE || g_cl.m_weapon_id != ZEUS)) {
			// set autostop shit.
			m_stop = !(g_cl.m_buttons & IN_JUMP) && on && !hit;

			// sanity check to make us not autostop when players aren't targetable
			if (!m_target || m_target->dormant() || m_record->m_dormant || !m_damage || !(m_damage >= best.damage || (m_damage <= best.damage && m_damage >= m_target->m_iHealth()))) return;

			// l3d math
			const auto choked_ticks = (g_cl.m_cmd->m_command_number % 2) != 1 ? (14 - g_csgo.m_cl->m_choked_commands) : g_csgo.m_cl->m_choked_commands;

			if (g_menu.main.aimbot.autostopconditions.get() == 0) g_movement.QuickStop();
			else if (g_menu.main.aimbot.autostopconditions.get() == 1) g_movement.LimitSpeed(std::fminf(g_cl.m_local->m_flMaxspeed(), g_cl.m_local->m_bIsScoped() ? g_cl.m_weapon_info->m_max_player_speed_alt : g_cl.m_weapon_info->m_max_player_speed) - (((g_cl.m_weapon && g_cl.m_weapon_id) && g_cl.m_weapon_id == AWP ? 1.2f : 1.4f) * choked_ticks));
		}

		// if we can scope.
		bool can_scope = !g_cl.m_local->m_bIsScoped() && (g_cl.m_weapon_id == AUG || g_cl.m_weapon_id == SG553 || g_cl.m_weapon_type == WEAPONTYPE_SNIPER_RIFLE);
		if (can_scope && g_menu.main.aimbot.zoom.get() == 1 && g_cl.m_local->m_fFlags() & (1 << 0) && g_cl.m_flags & (1 << 0)) {
			g_cl.m_cmd->m_buttons |= IN_ATTACK2;
			return;
		}

		if (hit || !on) {
			// right click attack.
			if (g_menu.main.config.mode.get() == 1 && g_cl.m_weapon_id == REVOLVER) {
#ifdef SONTHTEST
				g_console.log(XOR("pushing attack flag\n"));
#endif
				g_cl.m_cmd->m_buttons |= IN_ATTACK2;
			}

			// left click attack.
			else {
#ifdef SONTHTEST
				g_console.log(XOR("pushing attack flag\n"));
#endif
				g_cl.m_cmd->m_buttons |= IN_ATTACK;
			}
		}
	}
}

void Aimbot::GenerateSeedTable() {
	if (seeds_filled >= total_seeds) return; // what the fuck happened here; return

	seeds_filled = 0; // null our seed count since we haven't generated our seed table yet
	for (auto i = 0; i < total_seeds; i++) {
		if (seeds_filled >= 128) break; // we've already generated enough seeds, break outta the loop

		math::random_seed(seeds[i]); // do valve "math"
		float a = math::random_float(0.0f, 6.2831855f);
		float c = math::random_float(0.0f, 1.0f);
		float b = math::random_float(0.0f, 6.2831855f);

		precomputed_seeds[seeds_filled++] = std::tuple<float, float, float, float, float>(c, sin(a), cos(b), sin(b), cos(a));
	}
}

// old hitchance function, has a couple problems.
/*bool Aimbot::CheckHitchance(Player* player, const ang_t& angle, LagRecord* record, int hitbox) {
	if (!g_cl.m_weapon) return false;

	const auto info = g_cl.m_weapon_info;
	if (!info) return false;

	// generate our seed table to enhance performance & accuracy.
	GenerateSeedTable();

	vec3_t forward, right, up;
	vec3_t src = g_cl.m_shoot_pos;
	math::AngleVectors(angle, &forward, &right, &up);
	CGameTrace tr;

	size_t total_hits{ }, needed_hits{ (size_t)std::ceil((g_menu.main.aimbot.hitchance.get() * total_hits) / 100)}, needed_accuracy{ (size_t)std::ceil((g_menu.main.aimbot.accuracy_boost.get() * total_hits) / 100) };

	g_cl.m_weapon->UpdateAccuracyPenalty();
	float weap_spread = g_cl.m_weapon->GetSpread();
	float weap_inaccuracy = g_cl.m_weapon->GetInaccuracy();
	float weapon_range = g_cl.m_weapon->GetWpnData()->m_range;
	static float c, spread_value, inaccuracy_value;
	static std::tuple<float, float, float, float, float>* seed;
	int32_t accuracy_boost_seeds = 0;

	// setup calculation parameters.
	const auto round_acc = [](const float accuracy) { return roundf(accuracy * 1000.f) / 1000.f; };
	const auto sniper = g_cl.m_weapon->m_iItemDefinitionIndex() == AWP || g_cl.m_weapon->m_iItemDefinitionIndex() == G3SG1 || g_cl.m_weapon->m_iItemDefinitionIndex() == SCAR20 || g_cl.m_weapon->m_iItemDefinitionIndex() == SSG08;
	const auto crouched = g_cl.m_local->m_fFlags() & IN_DUCK;

	// there's no need for hitchance if we're already as accurate as we can be.
	if (crouched) { if (round_acc(weap_inaccuracy) == round_acc(sniper ? info->m_inaccuracy_crouch_alt : info->m_inaccuracy_crouch)) return true; }
	else { if (round_acc(weap_inaccuracy) == round_acc(sniper ? info->m_inaccuracy_stand_alt : info->m_inaccuracy_stand)) return true; }

	// pre-define our variable here so it won't constantly be fucking true
	g_cl.m_is_vulnereable = false;

	for (int i = 0; i < total_seeds; i++) {
		// get seed.
		seed = &precomputed_seeds[i];
		c = std::get<0>(*seed);

		inaccuracy_value = c * weap_inaccuracy;
		spread_value = c * weap_spread;

		vec3_t spreadView((std::get<2>(*seed) * spread_value) + (std::get<4>(*seed) * inaccuracy_value), (std::get<3>(*seed) * spread_value) + (std::get<1>(*seed) * inaccuracy_value), 0), direction;
		direction.x = forward.x + (spreadView.x * right.x) + (spreadView.y * up.x);
		direction.y = forward.y + (spreadView.x * right.y) + (spreadView.y * up.y);
		direction.z = forward.z + (spreadView.x * right.z) + (spreadView.y * up.z);
		direction.normalized();

		ang_t viewAnglesSpread;
		math::VectorAngles3(direction, up, viewAnglesSpread);
		math::Normalize(viewAnglesSpread);

		vec3_t viewForward;
		math::AngleVectors(viewAnglesSpread, viewForward);
		viewForward.normalize_in_place();

		viewForward = src + (viewForward * weapon_range);

		penetration::PenetrationInput_t in;
		in.m_damage = m_damage;
		in.m_damage_pen = m_best_damage;
		in.m_can_pen = true;
		in.m_target = player;
		in.m_from = g_cl.m_local;
		in.m_pos = viewForward;
		penetration::PenetrationOutput_t out;
		penetration::run(&in, &out);

		if (&out && out.m_damage > 0.0f) { accuracy_boost_seeds++; g_cl.m_is_vulnereable = true; }
		if (CanHit(src, viewForward, record, hitbox)) { total_hits++; g_cl.m_is_vulnereable = true; }

		// we're done!
		if (total_hits >= needed_hits && accuracy_boost_seeds >= needed_accuracy) { m_hitchance_amount = total_hits - needed_hits; m_accuracy_amount = accuracy_boost_seeds - needed_accuracy; return true; }

		// we cannot hit this entity anymore
		if ((total_seeds - i + total_hits) < needed_hits || (total_seeds - i + accuracy_boost_seeds) < needed_accuracy) return false;
	}
	return false;
}*/

bool Aimbot::CheckHitchance(Player* player, const ang_t& angle, LagRecord* record, int hitbox) {
	if (!g_cl.m_weapon) return false;

	const auto info = g_cl.m_weapon_info;
	if (!info) return false;

	float HITCHANCE_MAX = 100.f;
	constexpr int   SEED_MAX = 255;

	vec3_t     start{ g_cl.m_shoot_pos }, end, fwd, right, up, dir, wep_spread;
	float      inaccuracy, spread;
	CGameTrace tr;
	size_t     total_hits{ }, accuracy_boost_seeds{ }, needed_hits{ (size_t)std::ceil((g_menu.main.aimbot.hitchance.get() * SEED_MAX) / HITCHANCE_MAX) }, needed_accuracy{ (size_t)std::ceil((g_menu.main.aimbot.accuracy_boost.get() * SEED_MAX) / HITCHANCE_MAX) };

	// setup calculation parameters.
	const auto round_acc = [](const float accuracy) { return roundf(accuracy * 1000.f) / 1000.f; };
	const auto sniper = g_cl.m_weapon->m_iItemDefinitionIndex() == AWP || g_cl.m_weapon->m_iItemDefinitionIndex() == G3SG1 || g_cl.m_weapon->m_iItemDefinitionIndex() == SCAR20 || g_cl.m_weapon->m_iItemDefinitionIndex() == SSG08;
	const auto crouched = g_cl.m_local->m_fFlags() & IN_DUCK;

	// no need for hitchance, if we can't increase it anyway.
	if (crouched) { if (round_acc(g_cl.m_weapon->GetInaccuracy()) == round_acc(sniper ? info->m_inaccuracy_crouch_alt : info->m_inaccuracy_crouch)) return true; }
	else { if (round_acc(g_cl.m_weapon->GetInaccuracy()) == round_acc(sniper ? info->m_inaccuracy_stand_alt : info->m_inaccuracy_stand)) return true; }

	// get needed directional vectors.
	math::AngleVectors(angle, &fwd, &right, &up);

	// store off inaccuracy / spread ( these functions are quite intensive and we only need them once ).
	inaccuracy = g_cl.m_weapon->GetInaccuracy();
	spread = g_cl.m_weapon->GetSpread();
	g_cl.m_weapon->UpdateAccuracyPenalty();

	// iterate all possible seeds.
	for (int i{ }; i <= SEED_MAX; ++i) {
		// get spread.
		wep_spread = g_cl.m_weapon->CalculateSpread(i, inaccuracy, spread);

		// get spread direction.
		dir = (fwd + (right * wep_spread.x) + (up * wep_spread.y)).normalized();

		// get end of trace.
		end = start + (dir * g_cl.m_weapon_info->m_range);

		//penetration::PenetrationInput_t in;
		//in.m_damage = m_damage;
		//in.m_damage_pen = m_best_damage;
		//in.m_can_pen = true;
		//in.m_target = player;
		//in.m_from = g_cl.m_local;
		//in.m_pos = end;
		//penetration::PenetrationOutput_t out;

		//if (penetration::run(&in, &out)) { if (out.m_damage > 0.0f) { accuracy_boost_seeds++; g_cl.m_is_vulnerable = true; } else g_cl.m_is_vulnerable = false; }
		if (CanHit(start, end, record, hitbox)) { total_hits++; g_cl.m_is_vulnerable = true; } else g_cl.m_is_vulnerable = false;

		// we're done!
		if (total_hits >= needed_hits /* && accuracy_boost_seeds >= needed_accuracy */) {
			m_hitchance_amount = total_hits - needed_hits; m_accuracy_amount = needed_accuracy;
#ifdef SONTHTEST
			g_console.log(tfm::format(XOR("hc: %i (%i, %i) | ab: %i:%i\n"), m_hitchance_amount, total_hits, needed_hits, accuracy_boost_seeds, needed_accuracy).c_str());
#endif
			return true;
		}

		// we cannot hit this entity anymore
		if ((SEED_MAX - i + total_hits) < needed_hits /* || (SEED_MAX - i + accuracy_boost_seeds) < needed_accuracy*/) {

			if ((SEED_MAX - i + total_hits) < needed_hits) {
#ifdef SONTHTEST
				g_console.log(tfm::format(XOR("seed generation failed!\n%i < %i"), (SEED_MAX - i + total_hits), needed_hits).c_str());
#endif
			}

//			if ((SEED_MAX - i + accuracy_boost_seeds) < needed_accuracy + 1) {
//#ifdef SONTHTEST
//				g_console.log(tfm::format(XOR("accuracy seed generation failed!\n%i < %i"), (SEED_MAX - i + accuracy_boost_seeds), needed_accuracy).c_str());
//#endif
//			}

			return false;
		}
	}

	return false;
}

bool Aimbot::CanHit(vec3_t start, vec3_t end, LagRecord* record, int box, bool in_shot, BoneArray* bones) {
	if (!record || !record->m_player) return false;

	// backup player
	const auto backup_origin = record->m_player->m_vecOrigin();
	const auto backup_abs_origin = record->m_player->GetAbsOrigin();
	const auto backup_abs_angles = record->m_player->GetAbsAngles();
	const auto backup_obb_mins = record->m_player->m_vecMins();
	const auto backup_obb_maxs = record->m_player->m_vecMaxs();
	const auto backup_cache = record->m_player->m_iBoneCache();

	// always try to use our aimbot matrix first.
	auto matrix = record->m_bones;

	// this is basically for using a custom matrix.
	if (in_shot) matrix = bones;
	if (!matrix) return false;

	const model_t* model = record->m_player->GetModel();
	if (!model) return false;

	studiohdr_t* hdr = g_csgo.m_model_info->GetStudioModel(model);
	if (!hdr) return false;

	mstudiohitboxset_t* set = hdr->GetHitboxSet(record->m_player->m_nHitboxSet());
	if (!set) return false;

	mstudiobbox_t* bbox = set->GetHitbox(box);
	if (!bbox) return false;

	vec3_t min, max;
	const auto IsCapsule = bbox->m_radius != -1.f;

	if (IsCapsule) {
		math::VectorTransform(bbox->m_mins, matrix[bbox->m_bone], min);
		math::VectorTransform(bbox->m_maxs, matrix[bbox->m_bone], max);
		const auto dist = math::SegmentToSegment(start, end, min, max);

		if (dist < bbox->m_radius) return true;
	}
	else {
		CGameTrace tr;

		// setup trace data
		record->m_player->m_vecOrigin() = record->m_origin;
		record->m_player->SetAbsOrigin(record->m_origin);
		record->m_player->SetAbsAngles(record->m_abs_ang);
		record->m_player->m_vecMins() = record->m_mins;
		record->m_player->m_vecMaxs() = record->m_maxs;
		record->m_player->m_iBoneCache() = reinterpret_cast<matrix3x4_t**>(matrix);

		// setup ray and trace.
		g_csgo.m_engine_trace->ClipRayToEntity(Ray(start, end), MASK_SHOT, record->m_player, &tr);

		record->m_player->m_vecOrigin() = backup_origin;
		record->m_player->SetAbsOrigin(backup_abs_origin);
		record->m_player->SetAbsAngles(backup_abs_angles);
		record->m_player->m_vecMins() = backup_obb_mins;
		record->m_player->m_vecMaxs() = backup_obb_maxs;
		record->m_player->m_iBoneCache() = backup_cache;

		// check if we hit a valid player / hitgroup on the player and increment total hits.
		if (tr.m_entity == record->m_player && game::IsValidHitgroup(tr.m_hitgroup)) return true;
	}

	return false;
}

bool IsVisible(Entity* pEntity, vec3_t vEnd) {
	CGameTrace tr;
	CTraceFilterWorldOnly filter;

	g_csgo.m_engine_trace->TraceRay(Ray(g_cl.m_local->GetEyePosition(), vEnd), 0x4600400B, &filter, &tr);

	return (tr.m_entity == pEntity || tr.m_fraction == 1.0f);
}

// mutiny v2
float GetFov(const ang_t& viewAngle, const ang_t& aimAngle) {
	vec3_t ang, aim;

	math::AngleVectors(viewAngle, &aim);
	math::AngleVectors(aimAngle, &ang);

	return math::rad_to_deg(acos(aim.dot(ang) / aim.length_sqr()));
}

bool AimPlayer::GetBestAimPosition(vec3_t& aim, float& damage, int& hitbox, LagRecord* record) {
	bool                  done, pen;
	float                 dmg, pendmg;
	HitscanData_t         scan;
	std::vector< vec3_t > points;

	// get player hp.
	int hp = std::min(100, m_player->m_iHealth());
	int half_hp = hp / 2;

	// fix our min damage and autowall with zeus
	if (g_cl.m_weapon_id == ZEUS) {
		dmg = pendmg = hp;
		pen = false;
	}

	// override check
	//else if (g_input.GetKeyState(g_menu.main.aimbot.min_key.get())) {
	//	dmg = pendmg = g_menu.main.aimbot.min_key_amt.get();
	//	pen = g_menu.main.aimbot.penetrate.get();
	//}

	// back to the normal bullshit
	else
	{
		dmg = g_menu.main.aimbot.minimal_damage.get();
		if (g_menu.main.aimbot.minimal_damage_hp.get()) {
			if (hp < dmg) dmg = hp + 5;
			else if (half_hp < dmg) dmg = std::ceil((half_hp)+5);
			else dmg = std::ceil((dmg / 100.f) * hp);
		}

		pendmg = g_menu.main.aimbot.penetrate_minimal_damage.get();
		if (g_menu.main.aimbot.penetrate_minimal_damage_hp.get()) {
			if (hp < pendmg) pendmg = hp + 5;
			else if (half_hp < pendmg) pendmg = std::ceil((half_hp)+5);
			else pendmg = std::ceil((pendmg / 100.f) * hp);
		}

		pen = g_menu.main.aimbot.penetrate.get();
	}

	// write our bone cache for later modulation and usage.
	record->cache();

	// iterate hitboxes.
	for (const auto& it : m_hitboxes) {
		done = false;

		// setup points on hitbox.
		if (!m_player->SetupHitboxPoints(record->m_bones, it.m_index, points)) continue;
		//if (!SetupHitboxPoints(record, record->m_bones, it.m_index, points)) continue; -- this one shoots over entities heads lmfao

		// iterate points on hitbox.
		for (const auto& point : points) {
			penetration::PenetrationInput_t in;

			in.m_damage = dmg;
			in.m_damage_pen = pendmg;
			in.m_can_pen = pen;
			in.m_target = m_player;
			in.m_from = g_cl.m_local;
			in.m_pos = point;

			// ignore mindmg.
			//if (it.m_mode == HitscanMode::LETHAL || it.m_mode == HitscanMode::LETHAL2)
			//	in.m_damage = in.m_damage_pen = 1.f;

			penetration::PenetrationOutput_t out;

			// we can hit p!
			if (penetration::run(&in, &out)) {

				// nope we did not hit head..
				if (it.m_index == HITBOX_HEAD && out.m_hitgroup != HITGROUP_HEAD)
					continue;

				// prefered hitbox, just stop now.
				if (it.m_mode == HitscanMode::PREFER)
					done = true;

				// this hitbox requires lethality to get selected, if that is the case.
				// we are done, stop now.
				else if (it.m_mode == HitscanMode::LETHAL && out.m_damage >= m_player->m_iHealth()) {
#ifdef SONTHTEST
					g_console.log(XOR("best aim position targeted [lethal]; continuing loop\n"));
#endif
					done = true;
				}

				// 2 shots will be sufficient to kill.
				else if (it.m_mode == HitscanMode::LETHAL2 && (out.m_damage * 2.f) >= m_player->m_iHealth()) {
#ifdef SONTHTEST
					g_console.log(XOR("best aim position targeted [lethal2x]; continuing loop\n"));
#endif
					done = true;
				}

				// this hitbox has normal selection, it needs to have more damage.
				else if (it.m_mode == HitscanMode::NORMAL) {
					// we did more damage.
					if (out.m_damage > scan.m_damage) {
						// save new best data.
						scan.m_damage = out.m_damage;
						scan.m_pos = point;
						scan.m_hitbox = it.m_index;
#ifdef SONTHTEST
						g_console.log(XOR("best aim position targeted [normal]; continuing loop\n"));
#endif
						// if the first point is lethal
						// screw the other ones.
						if (point == points.front() && out.m_damage >= m_player->m_iHealth()) {
#ifdef SONTHTEST
							g_console.log(XOR("best aim position targeted [first point]; breaking out of loop\n"));
#endif
							break;
						}
					}
				}

				// we found a preferred / lethal hitbox.
				if (done) {
					// save new best data.
					scan.m_damage = out.m_damage;
					scan.m_pos = point;
					scan.m_hitbox = it.m_index;
#ifdef SONTHTEST
					g_console.log(XOR("best aim position targeted [prefered]; breaking out of loop\n"));
#endif
					break;
				}
			}
		}

		// ghetto break out of outer loop.
		if (done) {
#ifdef SONTHTEST
			g_console.log(XOR("best aim position recieved; breaking out of loop\n"));
#endif
			break;
		}
	}

	// we found something that we can damage.
	// set out vars.
	if (scan.m_damage > 0.f) {
		aim = scan.m_pos;
		damage = scan.m_damage;
		hitbox = scan.m_hitbox;
#ifdef SONTHTEST
		g_console.log(XOR("best target set; finish aimbotting!\n"));
#endif
		return true;
	}

	return false;
}

bool Aimbot::SelectTarget(LagRecord* record, const vec3_t& aim, float damage) {
	float dist, fov, height;
	int   hp;

	switch (g_menu.main.aimbot.selection.get()) {

		// distance.
	case 0:
		dist = (record->m_extrapolated_origin - g_cl.m_shoot_pos).length();
		if (dist < m_best_dist) {
			m_best_dist = dist;
			return true;
		}

		break;

		// crosshair.
	case 1:
		fov = math::GetFOV(g_cl.m_view_angles, g_cl.m_shoot_pos, aim);
		if (fov < m_best_fov) {
			m_best_fov = fov;
			return true;
		}

		break;

		// damage.
	case 2:
		if (damage > m_best_damage) {
			m_best_damage = damage;
			return true;
		}

		break;

		// lowest hp.
	case 3:
		// fix for retarded servers?
		hp = std::min(100, record->m_player->m_iHealth());
		if (hp < m_best_hp) {
			m_best_hp = hp;
			return true;
		}

		break;

		// least lag.
	case 4:
		if (record->m_lag < m_best_lag) {
			m_best_lag = record->m_lag;
			return true;
		}

		break;

		// height.
	case 5:
		height = record->m_extrapolated_origin.z - g_cl.m_local->m_vecOrigin().z;

		if (height < m_best_height) {
			m_best_height = height;
			return true;
		}

		break;

	default:
		return false;
	}

	return false;
}

void Aimbot::apply() {
	bool attack, attack2;

	// attack states.
	attack = (g_cl.m_cmd->m_buttons & IN_ATTACK);
	attack2 = (g_cl.m_weapon_id == REVOLVER && g_cl.m_cmd->m_buttons & IN_ATTACK2);

	// ensure we're attacking.
	if (attack || attack2) {
		if (g_menu.main.antiaim.choke_on_shot.get()) { if (g_cl.m_lag < 14) *g_cl.m_packet = false; }
		else *g_cl.m_packet = true;

		if (m_target) {
			// make sure to aim at un-interpolated data.
			// do this so BacktrackEntity selects the exact record.
			if (m_record && !m_record->m_broke_lc) {
				//if (m_record->m_valid) {
					//TODO; add another check to see if our simtime is valid & add our simtime proxy so we can have the most pristine data
					g_cl.m_cmd->m_tick = game::TIME_TO_TICKS(m_record->m_sim_time + g_cl.m_lerp);
#ifdef SONTHTEST
					g_console.log(XOR("aiming at noninterpolated data\n"));
#endif
//				}
//
//				// we should aim at interpolated data while our selected tick isn't valid
//				else {
//					g_cl.m_cmd->m_tick = game::TIME_TO_TICKS(m_target->m_flSimulationTime() + g_cl.m_lerp);
//#ifdef SONTHTEST
//					g_console.log(XOR("aiming at interpolated data\n"));
//#endif
//				}
			}

			// i made an else statement here because this is something i should implement soon.
			// i have an idea where i can make some sort of correction here for while our entity is breaking lag compensation,
			// exploiting, etc; and by doing that making it so we can accurately hit these fuckers while they are doing so.
			// i have a couple ideas about how to fix some but not all
			//else if (m_record && m_record->m_broke_lc && !m_record->m_exploiting && !m_record->m_skip_due_to_resolver) {
				
			//}

//			else if (m_record && (m_record->m_exploiting || m_record->m_skip_due_to_resolver)) {
//				int ticks_to_simulate = 1;
//				if (m_previous_record && !m_previous_record->dormant() && m_previous_record->m_data_stored) {
//					int simulation_ticks = game::TIME_TO_TICKS(m_record->m_sim_time - m_previous_record->m_sim_time);
//					if ((simulation_ticks - 1) > 31 || m_previous_record->m_sim_time == 0.f) simulation_ticks = 1;
//					auto layer_cycle = m_record->m_layers[11].m_cycle;
//					auto previous_playback = m_previous_record->m_layers[11].m_playback_rate;
//					if (previous_playback > 0.f && m_record->m_layers[11].m_playback_rate > 0.f
//						&& m_previous_record->m_layers[11].m_sequence == m_record->m_layers[11].m_sequence) {
//						auto previous_cycle = m_previous_record->m_layers[11].m_cycle;
//						simulation_ticks = 0;
//
//						if (previous_cycle > layer_cycle) layer_cycle = layer_cycle + 1.0f;
//						while (layer_cycle > previous_cycle) {
//							const auto ticks_backup = simulation_ticks;
//							const auto playback_mult_ipt = g_csgo.m_globals->m_interval * previous_playback;
//							previous_cycle = previous_cycle + (g_csgo.m_globals->m_interval * previous_playback);
//							if (previous_cycle >= 1.0f) previous_playback = m_record->m_layers[11].m_playback_rate;
//							++simulation_ticks;
//							if (previous_cycle > layer_cycle && (previous_cycle - layer_cycle) > (playback_mult_ipt * 0.5f)) simulation_ticks = ticks_backup;
//						}
//					}
//
//					ticks_to_simulate = simulation_ticks;
//
//					m_record->m_sim_time = m_previous_record->m_sim_time + game::TICKS_TO_TIME(simulation_ticks);
//#ifdef SONTHTEST
//					g_console.log(XOR("modulating simulation time to fix exploits\n"));
//#endif
//				}
//
//				 we don't need a valid record check here because this shit ain't gonna be valid lmfao
//				 maybe a valid sim time check after doing these calculations
//				g_cl.m_cmd->m_tick = game::TIME_TO_TICKS(m_record->m_sim_time + g_cl.m_lerp);
//
//				 push this shit to our aimbot
//				 i was really high when i wrote this explanation i hope someone can make some sense out of it as i still can.
//				 ( making this true will make our aimbot target our front tick only since our targeted entity is doing things that will make us not target backtrack; 
//				   these other conditions should also not push to aimbot until we correct it by setting it to false when those flags are set to true )
//				m_record->m_push_to_aimbot = true;
//#ifdef SONTHTEST
//				g_console.log(XOR("modulation pushed to aimbot\n"));
//#endif
//			}

			// set angles to target.
			g_cl.m_cmd->m_view_angles = m_angle;

			// if not silent aim, apply the viewangles.
			if (!g_menu.main.aimbot.silent.get() || g_cl.m_weapon_id == ZEUS) g_csgo.m_engine->SetViewAngles(m_angle);

			g_visuals.DrawHitboxMatrix(m_record, colors::transparent_green, 7.f);
		}

		// nospread.
		if (g_menu.main.aimbot.nospread.get() && g_menu.main.config.mode.get() == 1) NoSpread();

		// norecoil.
		if (g_menu.main.aimbot.norecoil.get()) g_cl.m_cmd->m_view_angles -= g_cl.m_local->m_aimPunchAngle() * g_csgo.weapon_recoil_scale->GetFloat();

		// store fired shot.
		g_shots.OnShotFire(m_target ? m_target : nullptr, m_target ? m_damage : -1.f, g_cl.m_weapon_info->m_bullets, m_target ? m_record : nullptr, m_hitbox);

		// set that we fired.
		g_cl.m_shot = true;
	}
}

void Aimbot::NoSpread() {
	bool    attack2;
	vec3_t  spread, forward, right, up, dir;

	// revolver state.
	attack2 = (g_cl.m_weapon_id == REVOLVER && (g_cl.m_cmd->m_buttons & IN_ATTACK2));

	// get spread.
	spread = g_cl.m_weapon->CalculateSpread(g_cl.m_cmd->m_random_seed, attack2);

	// compensate.
	g_cl.m_cmd->m_view_angles -= { -math::rad_to_deg(std::atan(spread.length_2d())), 0.f, math::rad_to_deg(std::atan2(spread.x, spread.y)) };
}