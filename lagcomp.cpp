#include "includes.h"

LagCompensation g_lagcomp{};;

bool LagCompensation::StartPrediction(AimPlayer* data) {
	// we have no data to work with.
	// this should never happen if we call this
	if (data->m_records.empty() || data->m_player->dormant()) return false;

	// compute the true amount of updated records
	// since the last time the player entered pvs.
	size_t size{};

	// iterate records.
	for (const auto& it : data->m_records) {
		if (it->dormant()) break;

		// increment total amount of data.
		++size;
	}

	// get first record.
	LagRecord* record = data->m_records[0].get();

	// reset all prediction related variables.
	// this has been a recurring problem in all my hacks lmfao.
	// causes the prediction to stack on eachother.
	record->predict();

	// check if lc broken.
	if (size > 1 && ((record->m_origin - data->m_records[1]->m_origin).length_sqr() > 4096.f || size > 2 && (data->m_records[1]->m_origin - data->m_records[2]->m_origin).length_sqr() > 4096.f)) {
		for (const auto& it : data->m_records) {
			it->m_broke_lc = true;
			it->m_push_to_aimbot = false;
			if (it->valid() && it->m_valid) it->m_push_to_aimbot = true;
		}
	}

	if (record->m_valid && record->m_exploiting && size > 0) {
		for (auto& it : data->m_records) {
			it->m_skip_due_to_resolver = true;
			it->m_push_to_aimbot = true; // makes it force front record /shrug
		}
	}

	// we are not breaking lagcomp at this point.
	// return false so it can aim at all the records it once
	// since server-sided lagcomp is still active and we can abuse that.
	if (!record->m_broke_lc) return false;

	int simulation = game::TIME_TO_TICKS(record->m_sim_time);

	// this is too much lag to fix.
	if (std::abs(g_cl.m_arrival_tick - simulation) >= 100) return true;

	// compute the amount of lag that we will predict for, if we have one set of data, use that.
	// if we have more data available, use the prevoius lag delta to counter weird fakelags that switch between 14 and 2.
	int lag = (size <= 2) ? game::TIME_TO_TICKS(record->m_sim_time - data->m_records[1]->m_sim_time) : game::TIME_TO_TICKS(data->m_records[1]->m_sim_time - data->m_records[2]->m_sim_time);

	// clamp our max amount of choked ticks
	math::clamp(lag, 1, 17); // this can actually go all the way up to 64 fun fact.

	// get the delta in ticks between the last server net update
	// and the net update on which we created this record.
	int updatedelta = g_cl.m_server_tick - record->m_tick;

	// if the lag delta that is remaining is less than the current netlag
	// that means that we can shoot now and when our shot will get processed
	// the origin will still be valid, therefore we do not have to predict.
	if (g_cl.m_latency_ticks <= lag - updatedelta) return true;

	// the next update will come in, wait for it.
	int next = record->m_tick + 1;
	if (next + lag >= g_cl.m_arrival_tick) return true;

	// now actual "proper" ( more like fucking bandaid ) lag comp fix lmfao
	// calculate sim delta through first and second record
	float m_sim_time_delta = record->m_sim_time - data->m_records[1]->m_sim_time;
	// get the delta of the 2 sim times by using time to ticks to convert the simulation time into the amount the entity has choked ticks
	// we need to clamp it too but that's already pretty obvious
	int m_sim_tick_delta = std::clamp(game::TIME_TO_TICKS(m_sim_time_delta), 1, 16);

	bool bInAir = false;
	if (!(record->m_flags & FL_ONGROUND) || !(data->m_records.back()->m_flags & FL_ONGROUND)) bInAir = true;

	// rebuild walk movement and predict the entity for every tick they are breaking lag compensation
	for (; m_sim_tick_delta > 0; m_sim_tick_delta--) {
		Extrapolation(record, bInAir);
		g_gamemovement.FullWalkMove(record->m_player);

		// crashing, soon figure out why;
		// if we implement this i can call the rebuilt gamemovement in that and enginepredict them with it
		//EnginePredictPlayer(record->m_player, 1, true, Jumping, Ducking, true, data);
	}

	// lagcomp broken, invalidate bones.
	record->invalidate();

	// re-setup bones for this record.
	g_bones.setup(data->m_player, nullptr, record);

	// POSSIBLY CALL OUR ANIMATION FIX HERE!

	return true;
}

void LagCompensation::Extrapolation(LagRecord* record, bool m_was_in_air) {
#ifdef SONTHTEST
	g_console.log(XOR("extrapolating entities\n"));
#endif
	vec3_t                start, end, normal;
	CGameTrace            trace;
	CTraceFilterWorldOnly filter;

	// define trace start.
	start = record->m_extrapolated_origin;

	// move trace end one tick into the future using predicted velocity.
	end = start + (record->m_extrapolated_velocity * g_csgo.m_globals->m_interval);

	// trace.
	g_csgo.m_engine_trace->TraceRay(Ray(start, end, record->m_mins, record->m_maxs), mps, &filter, &trace); // was using CONTENTS_SOLID

	// we hit shit
	// we need to fix hit.
	if (trace.m_fraction != 1.f) {
		// fix sliding on planes.
		for (int i = 0; i < 2; i++)
		{
			record->m_extrapolated_velocity -= trace.m_plane.m_normal * record->m_extrapolated_velocity.dot(trace.m_plane.m_normal);

			float adjust = record->m_extrapolated_velocity.dot(trace.m_plane.m_normal);
			if (adjust < 0.f) record->m_extrapolated_velocity -= (trace.m_plane.m_normal * adjust);

			end = trace.m_endpos + (record->m_extrapolated_velocity * (g_csgo.m_globals->m_interval * (1.f - trace.m_fraction)));

			g_csgo.m_engine_trace->TraceRay(Ray(trace.m_endpos, end, record->m_mins, record->m_maxs), mps, &filter, &trace); // was using CONTENTS_SOLID

			if (trace.m_fraction == 1.f) break;
		}
	}

	// set new final origin.
	start = end = record->m_extrapolated_origin = trace.m_endpos;

	// move endpos 2 units down.
	// this way we can check if we are in/on the ground.
	end.z -= 2.f;

	// trace.
	g_csgo.m_engine_trace->TraceRay(Ray(start, end, record->m_mins, record->m_maxs), mps, &filter, &trace); // was using CONTENTS_SOLID

	// on ground check.
	if (trace.m_fraction == 1.f || trace.m_plane.m_normal.z < 0.7f) record->m_extrapolated_flags &= ~FL_ONGROUND;
	else record->m_extrapolated_flags |= FL_ONGROUND;

	if (!(record->m_extrapolated_flags & FL_ONGROUND)) {
		// null cycle and weight while in air during extrapolation
		record->m_layers[4].m_cycle = 0.0f;
		record->m_layers[4].m_weight = 0.0f;

		// correct our extrapolated velocity
		record->m_extrapolated_velocity.z -= g_csgo.m_globals->m_frametime * g_csgo.sv_gravity->GetFloat();
	}
	else if (m_was_in_air) {
		// TODO; test and see if invalidating cycle & weight here would be good or not.
		record->m_extrapolated_velocity.z = g_csgo.sv_jump_impulse->GetFloat();
	}
}

void LagCompensation::PredictAnimations(CCSGOPlayerAnimState* state, LagRecord* record) {
	struct AnimBackup_t { float curtime, frametime; int flags, eflags; vec3_t velocity; };

	// get player ptr.
	Player* player = record->m_player;

	// backup data.
	AnimBackup_t backup;
	backup.curtime = g_csgo.m_globals->m_curtime;
	backup.frametime = g_csgo.m_globals->m_frametime;
	backup.flags = player->m_fFlags();
	backup.eflags = player->m_iEFlags();
	backup.velocity = player->m_vecAbsVelocity();

	// set globals appropriately for animation.
	g_csgo.m_globals->m_curtime = record->m_lagcorrected_time;
	g_csgo.m_globals->m_frametime = g_csgo.m_globals->m_interval;

	// EFL_DIRTY_ABSVELOCITY
	// skip call to C_BaseEntity::CalcAbsoluteVelocity
	player->m_iEFlags() &= ~0x1000;

	// set simulated data to our extrapolated data.
	player->m_fFlags() = record->m_extrapolated_flags;
	player->m_vecAbsVelocity() = record->m_extrapolated_velocity;

	// enable re-animation in the same frame if animated already.
	if (state->m_last_update_frame >= g_csgo.m_globals->m_frame) state->m_last_update_frame = g_csgo.m_globals->m_frame - 1;

	// run the resolver again since we edited the origin to our extrapolated one.
	g_resolver.ResolveAngles(player, record);

	// update animations & poses.
	game::UpdateAnimationState(state, record->m_eye_angles);

	// run the pose correction again as just updated our poses.
	g_resolver.ResolvePoses(player, record);

	// get new rotation poses and layers.
	player->GetPoseParameters(record->m_poses);
	player->GetAnimLayers(record->m_layers);
	record->m_abs_ang = player->GetAbsAngles();

	player->InvalidatePhysicsRecursive(ANGLES_CHANGED);
	player->InvalidatePhysicsRecursive(ANIMATION_CHANGED);

	// restore globals.
	g_csgo.m_globals->m_curtime = backup.curtime;
	g_csgo.m_globals->m_frametime = backup.frametime;

	// restore player data.
	player->m_fFlags() = backup.flags;
	player->m_iEFlags() = backup.eflags;
	player->m_vecAbsVelocity() = backup.velocity;
}

/*
// simulates player movement for x amount of ticks
// will almost perfectly predict players while having extreme amounts of velocity, great for nospread
void LagCompensation::EnginePredictPlayer(Player* Entity, int ticks, bool CalledFromCreateMove, bool WasJumping, bool Ducking, bool ForwardVelocity, AimPlayer* const pCPlayer) {
	if (ForwardVelocity) {
		ang_t VelocityAngles;
		math::VectorAngles(Entity->m_vecVelocity(), VelocityAngles);
		VelocityAngles.clamp();
		Entity->m_angEyeAngles() = VelocityAngles;
	}

	for (int i = 0; i < ticks; i++) {
		ang_t EyeAngles = Entity->m_angEyeAngles();
		EyeAngles.clamp();
		ang_t VelocityAngles;
		math::VectorAngles(Entity->m_vecVelocity(), VelocityAngles);
		float yawdelta = math::ClampYaw(EyeAngles.y - VelocityAngles.y);
		CUserCmd newcmd;
		CMoveData newdata;
		memset(&newdata, 0, sizeof(CMoveData));
		memset(&newcmd, 0, sizeof(CUserCmd));
		newcmd.m_view_angles = Entity->m_angEyeAngles();
		newcmd.m_view_angles.clamp();
		newcmd.m_tick = g_cl.m_cmd->m_tick;

		if (Ducking)
			newcmd.m_buttons = IN_DUCK;

		if ((WasJumping && Entity->m_fFlags() & FL_ONGROUND) || Entity->m_bIsInDuckJump())
			newcmd.m_buttons |= IN_JUMP;

		if (*(DWORD*)((DWORD)Entity + 0x389D))
			newcmd.m_buttons |= IN_SPEED;

		newcmd.m_command_number = g_cl.m_cmd->m_command_number;
		newcmd.m_predicted = false;

		if (Entity->m_vecVelocity().length() != 0.0f) {
			if (yawdelta < 0.0f)
				yawdelta += 360.0f;
			float yawdeltaabs = fabsf(yawdelta);

			if (yawdelta > 359.5f || yawdelta < 0.5f) {
				newcmd.m_buttons |= IN_FORWARD;
				newcmd.m_forward_move = 450.0f;
				newcmd.m_side_move = 0.0f;
			}
			else if (yawdelta > 179.5f && yawdelta < 180.5f) {
				newcmd.m_buttons |= IN_BACK;
				newcmd.m_forward_move = -450.0f;
				newcmd.m_side_move = 0.0f;
			}
			else if (yawdelta > 89.5f && yawdelta < 90.5f) {
				newcmd.m_buttons |= IN_MOVERIGHT;
				newcmd.m_forward_move = 0.0f;
				newcmd.m_side_move = 450.0f;
			}
			else if (yawdelta > 269.5f && yawdelta < 270.5f) {
				newcmd.m_buttons |= IN_MOVELEFT;
				newcmd.m_forward_move = 0.0f;
				newcmd.m_side_move = -450.0f;
			}
			else if (yawdelta > 0.0f && yawdelta < 90.0f) {
				newcmd.m_buttons |= IN_FORWARD;
				newcmd.m_buttons |= IN_MOVERIGHT;
				newcmd.m_forward_move = 450.0f;
				newcmd.m_side_move = 450.0f;
			}
			else if (yawdelta > 90.0f && yawdelta < 180.0f) {
				newcmd.m_buttons |= IN_BACK;
				newcmd.m_buttons |= IN_MOVERIGHT;
				newcmd.m_forward_move = -450.0f;
				newcmd.m_side_move = 450.0f;
			}
			else if (yawdelta > 180.0f && yawdelta < 270.0f) {
				newcmd.m_buttons |= IN_BACK;
				newcmd.m_buttons |= IN_MOVELEFT;
				newcmd.m_forward_move = -450.0f;
				newcmd.m_side_move = -450.0f;
			}
			else {
				//yawdelta > 270.0f && yawdelta > 0.0f
				newcmd.m_buttons |= IN_FORWARD;
				newcmd.m_buttons |= IN_MOVELEFT;
				newcmd.m_forward_move = 450.0f;
				newcmd.m_side_move = -450.0f;
			}
		}

		//pMoveHelperServer = (CMoveHelperServer*)ReadInt(ReadInt(pMoveHelperServerPP));
		g_csgo.m_move_helper->SetHost(Entity);
		DWORD pOriginalPlayer = *(DWORD*)g_csgo.m_pPredictionPlayer;
		*(DWORD*)g_csgo.m_pPredictionPlayer = (DWORD)Entity;
		int flags = Entity->m_fFlags();
		UINT rand = g_cl.m_cmd->m_command_number & 0x7fffffff;
		UINT originalrandomseed = *(UINT*)g_csgo.m_nPredictionRandomSeed;
		*(UINT*)g_csgo.m_nPredictionRandomSeed = rand;
		float frametime = g_csgo.m_globals->m_frametime;
		float curtime = g_csgo.m_globals->m_curtime;
		float tickinterval = g_csgo.m_globals->m_interval;
		g_csgo.m_globals->m_frametime = tickinterval;
		int tickbase = Entity->m_nTickBase();
		g_csgo.m_globals->m_curtime = tickbase * tickinterval;
		CUserCmd* currentcommand = Entity->m_pCurrentCommand();
		Entity->SetCurrentCommand(&newcmd);

		g_csgo.m_game_movement->StartTrackPredictionErrors(Entity);
		g_csgo.m_prediction->SetupMove(Entity, &newcmd, g_csgo.m_move_helper, &newdata);
		g_csgo.m_game_movement->ProcessMovement(Entity, &newdata);
		//g_gamemovement.FullWalkMove(Entity);
		//Interfaces::Prediction->RunCommand(LocalPlayer.Entity, &newcmd, pMoveHelperServer);
		g_csgo.m_prediction->FinishMove(Entity, &newcmd, &newdata);
		g_csgo.m_game_movement->FinishTrackPredictionErrors(Entity);

		Entity->SetCurrentCommand(currentcommand);
		g_csgo.m_globals->m_curtime = curtime;
		g_csgo.m_globals->m_frametime = frametime;
		Entity->m_fFlags() = flags;
		*(DWORD*)g_csgo.m_pPredictionPlayer = pOriginalPlayer;
		g_csgo.m_move_helper->SetHost(!CalledFromCreateMove ? nullptr : g_cl.m_local);
		*(UINT*)g_csgo.m_pPredictionPlayer = originalrandomseed;
	}
}
*/