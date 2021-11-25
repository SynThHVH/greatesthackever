#include "includes.h"

/* note: one step closer to being gigantic supremacy paster LAFF */

RebuildGameMovement g_gamemovement{};;

void RebuildGameMovement::SetAbsOrigin(Player* player, const vec3_t& vec)
{
	player->SetAbsOrigin(vec);
}

int RebuildGameMovement::ClipVelocity(vec3_t& in, vec3_t& normal, vec3_t& out, float overbounce)
{
	float	backoff;
	float	change;
	float angle;
	int		i, blocked;

	angle = normal[2];

	blocked = 0x00;         // Assume unblocked.
	if (angle > 0)			// If the plane that is blocking us has a positive z component, then assume it's a floor.
		blocked |= 0x01;	// 
	if (!angle)				// If the plane has no Z, it is vertical (wall/step)
		blocked |= 0x02;	// 

							// Determine how far along plane to slide based on incoming direction.
	backoff = in.dot(normal) * overbounce;

	for (i = 0; i < 3; i++)
	{
		change = normal[i] * backoff;
		out[i] = in[i] - change;
	}

	// iterate once to make sure we aren't still moving through the plane
	float adjust = out.dot(normal);
	if (adjust < 0.0f)
	{
		out -= (normal * adjust);
		//		Msg( "Adjustment = %lf\n", adjust );
	}

	// Return blocking flags.
	return blocked;
}

int RebuildGameMovement::TryPlayerMove(Player* player, vec3_t* pFirstDest, CGameTrace* pFirstTrace)
{
	vec3_t  planes[5];
	numbumps[player->index()] = 4;           // Bump up to four times

	blocked[player->index()] = 0;           // Assume not blocked
	numplanes[player->index()] = 0;           //  and not sliding along any planes

	original_velocity[player->index()] = player->m_vecVelocity(); // Store original velocity
	primal_velocity[player->index()] = player->m_vecVelocity();

	allFraction[player->index()] = 0;
	time_left[player->index()] = g_csgo.m_globals->m_frametime;   // Total time for this movement operation.

	new_velocity[player->index()].IsZero();

	for (bumpcount[player->index()] = 0; bumpcount[player->index()] < numbumps[player->index()]; bumpcount[player->index()]++)
	{
		if (player->m_vecVelocity().length() == 0.0)
			break;

		// Assume we can move all the way from the current origin to the
		//  end point.
		VectorMA(player->GetAbsOrigin(), time_left[player->index()], player->m_vecVelocity(), end[player->index()]);

		// See if we can make it from origin to end point.
		if (true)
		{
			// If their velocity Z is 0, then we can avoid an extra trace here during WalkMove.
			if (pFirstDest && end[player->index()] == *pFirstDest)
				pm[player->index()] = *pFirstTrace;
			else
			{
				TracePlayerBBox(player->GetAbsOrigin(), end[player->index()], MASK_PLAYERSOLID, 8, pm[player->index()], player);
			}
		}
		else
		{
			TracePlayerBBox(player->GetAbsOrigin(), end[player->index()], MASK_PLAYERSOLID, 8, pm[player->index()], player);
		}

		allFraction[player->index()] += pm[player->index()].m_fraction;

		// If we started in a solid object, or we were in solid space
		//  the whole way, zero out our velocity and return that we
		//  are blocked by floor and wall.
		if (pm[player->index()].m_allsolid)
		{
			// Player is trapped in another solid
			player->m_vecVelocity() = vec3_origin[player->index()];
			return 4;
		}

		// If we moved some portion of the total distance, then
		//  copy the end position into the pmove.origin and 
		//  zero the plane counter.
		if (pm[player->index()].m_fraction > 0)
		{
			if (numbumps[player->index()] > 0 && pm[player->index()].m_fraction == 1)
			{
				// There's a precision issue with terrain tracing that can cause a swept box to successfully trace
				// when the end position is stuck in the triangle.  Re-run the test with an uswept box to catch that
				// case until the bug is fixed.
				// If we detect getting stuck, don't allow the movement
				CGameTrace stuck;
				TracePlayerBBox(pm[player->index()].m_endpos, pm[player->index()].m_endpos, MASK_PLAYERSOLID, 8, stuck, player);
				if (stuck.m_startsolid || stuck.m_fraction != 1.0f)
				{
					//Msg( "Player will become stuck!!!\n" );
					player->m_vecVelocity() = vec3_origin[player->index()];
					break;
				}
			}

			// actually covered some distance
			SetAbsOrigin(player, pm[player->index()].m_endpos);
			original_velocity[player->index()] = player->m_vecVelocity();
			numplanes[player->index()] = 0;
		}

		// If we covered the entire distance, we are done
		//  and can return.
		if (pm[player->index()].m_fraction == 1)
		{
			break;		// moved the entire distance
		}

		// If the plane we hit has a high z component in the normal, then
		//  it's probably a floor
		if (pm[player->index()].m_plane.m_normal[2] > 0.7)
		{
			blocked[player->index()] |= 1;		// floor
		}
		// If the plane has a zero z component in the normal, then it's a 
		//  step or wall
		if (!pm[player->index()].m_plane.m_normal[2])
		{
			blocked[player->index()] |= 2;		// step / wall
		}

		// Reduce amount of m_flFrameTime left by total time left * fraction
		//  that we covered.
		time_left[player->index()] -= time_left[player->index()] * pm[player->index()].m_fraction;

		// Did we run out of planes to clip against?
		if (numplanes[player->index()] >= 5)
		{
			// this shouldn't really happen
			//  Stop our movement if so.
			player->m_vecVelocity() = vec3_origin[player->index()];
			//Con_DPrintf("Too many planes 4\n");

			break;
		}

		// Set up next clipping plane
		planes[numplanes[player->index()]] = pm[player->index()].m_plane.m_normal;
		numplanes[player->index()]++;

		// modify original_velocity so it parallels all of the clip planes
		//

		// reflect player velocity 
		// Only give this a try for first impact plane because you can get yourself stuck in an acute corner by jumping in place
		//  and pressing forward and nobody was really using this bounce/reflection feature anyway...
		if (numplanes[player->index()] == 1 &&
			player->m_fFlags() & FL_ONGROUND)
		{
			for (i[player->index()] = 0; i[player->index()] < numplanes[player->index()]; i[player->index()]++)
			{
				if (planes[i[player->index()]][2] > 0.7)
				{
					// floor or slope
					ClipVelocity(original_velocity[player->index()], planes[i[player->index()]], new_velocity[player->index()], 1);
					original_velocity[player->index()] = new_velocity[player->index()];
				}
				else
				{
					ClipVelocity(original_velocity[player->index()], planes[i[player->index()]], new_velocity[player->index()], 1.0 + g_csgo.sv_bounce->GetFloat() * (1 - player->m_surfaceFriction()));
				}
			}

			player->m_vecVelocity() = new_velocity[player->index()];
			original_velocity[player->index()] = new_velocity[player->index()];
		}
		else
		{
			for (i[player->index()] = 0; i[player->index()] < numplanes[player->index()]; i[player->index()]++)
			{


				for (j[player->index()] = 0; j[player->index()] < numplanes[player->index()]; j[player->index()]++)
					if (j[player->index()] != i[player->index()])
					{
						// Are we now moving against this plane?
						if (player->m_vecVelocity().dot(planes[j[player->index()]]) < 0)
							break;	// not ok
					}
				if (j[player->index()] == numplanes[player->index()])  // Didn't have to clip, so we're ok
					break;
			}

			// Did we go all the way through plane set
			if (i[player->index()] != numplanes[player->index()])
			{	// go along this plane
				// pmove.velocity is set in clipping call, no need to set again.
				;
			}
			else
			{	// go along the crease
				if (numplanes[player->index()] != 2)
				{
					player->m_vecVelocity() = vec3_origin[player->index()];
					break;
				}

				dir[player->index()] = planes[0].cross(planes[1]);
				dir[player->index()].normalize_in_place();
				d[player->index()] = dir[player->index()].dot(player->m_vecVelocity());
				VectorMultiply(dir[player->index()], d[player->index()], player->m_vecVelocity());
			}

			//
			// if original velocity is against the original velocity, stop dead
			// to avoid tiny occilations in sloping corners
			//
			d[player->index()] = player->m_vecVelocity().dot(primal_velocity[player->index()]);
			if (d[player->index()] <= 0)
			{
				//Con_DPrintf("Back\n");
				player->m_vecVelocity() = vec3_origin[player->index()];
				break;
			}
		}
	}

	if (allFraction == 0)
	{
		player->m_vecVelocity() = vec3_origin[player->index()];
	}

	// Check if they slammed into a wall
	float fSlamVol = 0.0f;

	float fLateralStoppingAmount = primal_velocity[player->index()].length_2d() - player->m_vecVelocity().length_2d();
	if (fLateralStoppingAmount > 580.f * 2.0f)
	{
		fSlamVol = 1.0f;
	}
	else if (fLateralStoppingAmount > 580.f)
	{
		fSlamVol = 0.85f;
	}

	return blocked[player->index()];
}

void RebuildGameMovement::Accelerate(Player* player, vec3_t& wishdir, float wishspeed, float accel)
{
	if (!player || player == g_cl.m_local)
		return;

	// See if we are changing direction a bit
	currentspeed[player->index()] = player->m_vecVelocity().dot(wishdir);

	// Reduce wishspeed by the amount of veer.
	addspeed[player->index()] = wishspeed - currentspeed[player->index()];

	// If not going to add any speed, done.
	if (addspeed[player->index()] <= 0)
		return;

	// Determine amount of accleration.
	accelspeed[player->index()] = accel * g_csgo.m_globals->m_frametime * wishspeed * player->m_surfaceFriction();

	// Cap at addspeed
	if (accelspeed[player->index()] > addspeed[player->index()])
		accelspeed[player->index()] = addspeed[player->index()];

	// Adjust velocity.
	for (i[player->index()] = 0; i[player->index()] < 3; i[player->index()]++)
	{
		player->m_vecVelocity()[i[player->index()]] += accelspeed[player->index()] * wishdir[i[player->index()]];
	}
}

void RebuildGameMovement::AirAccelerate(Player* player, vec3_t& wishdir, float wishspeed, float accel)
{
	if (!player || player == g_cl.m_local)
		return;

	wishspd[player->index()] = wishspeed;

	// Cap speed
	if (wishspd[player->index()] > 30.f)
		wishspd[player->index()] = 30.f;

	// Determine veer amount
	currentspeed[player->index()] = player->m_vecVelocity().dot(wishdir);

	// See how much to add
	addspeed[player->index()] = wishspd[player->index()] - currentspeed[player->index()];

	// If not adding any, done.
	if (addspeed <= 0)
		return;

	// Determine acceleration speed after acceleration
	accelspeed[player->index()] = accel * wishspeed * g_csgo.m_globals->m_frametime * player->m_surfaceFriction();

	// Cap it
	if (accelspeed[player->index()] > addspeed[player->index()])
		accelspeed[player->index()] = addspeed[player->index()];

	// Adjust pmove vel.
	for (i[player->index()] = 0; i[player->index()] < 3; i[player->index()]++)
	{
		player->m_vecVelocity()[i[player->index()]] += accelspeed[player->index()] * wishdir[i[player->index()]];
		g_csgo.m_move_helper->SetHost(player);
		g_csgo.m_move_helper->m_outWishVel[i[player->index()]] += accelspeed[player->index()] * wishdir[i[player->index()]];

	}
}

void RebuildGameMovement::AirMove(Player* player)
{
	if (!player || player == g_cl.m_local)
		return;

	math::AngleVectors(player->m_angEyeAngles(), &forward[player->index()], &right[player->index()], &up[player->index()]);  // Determine movement angles

	// Copy movement amounts
	g_csgo.m_move_helper->SetHost(player);
	fmove[player->index()] = g_csgo.m_move_helper->m_flForwardMove;
	smove[player->index()] = g_csgo.m_move_helper->m_flSideMove;

	// Zero out z components of movement vectors
	forward[player->index()][2] = 0;
	right[player->index()][2] = 0;
	math::NormalizeVector(forward[player->index()]);  // Normalize remainder of vectors
	math::NormalizeVector(right[player->index()]);    // 

	for (i[player->index()] = 0; i[player->index()] < 2; i[player->index()]++)       // Determine x and y parts of velocity
		wishvel[player->index()][i[player->index()]] = forward[player->index()][i[player->index()]] * fmove[player->index()] + right[player->index()][i[player->index()]] * smove[player->index()];

	wishvel[player->index()][2] = 0;             // Zero out z part of velocity

	wishdir[player->index()] = wishvel[player->index()]; // Determine maginitude of speed of move
	wishspeed[player->index()] = wishdir[player->index()].normalize();

	//
	// clamp to server defined max speed
	//
	if (wishspeed != 0 && (wishspeed[player->index()] > player->m_flMaxspeed()))
	{
		VectorMultiply(wishvel[player->index()], player->m_flMaxspeed() / wishspeed[player->index()], wishvel[player->index()]);
		wishspeed[player->index()] = player->m_flMaxspeed();
	}

	AirAccelerate(player, wishdir[player->index()], wishspeed[player->index()], g_csgo.sv_airaccelerate->GetFloat());

	// Add in any base velocity to the current velocity.
	VectorAdd(player->m_vecVelocity(), player->m_vecBaseVelocity(), player->m_vecVelocity());
	CGameTrace trace;
	TryPlayerMove(player, &dest[player->index()], &trace);

	// Now pull the base velocity back out.   Base velocity is set if you are on a moving object, like a conveyor (or maybe another monster?)
	VectorSubtract(player->m_vecVelocity(), player->m_vecBaseVelocity(), player->m_vecVelocity());
}

void RebuildGameMovement::StepMove(Player* player, vec3_t& vecDestination, CGameTrace& trace)
{
	if (!player || player == g_cl.m_local)
		return;

	vec3_t vecEndPos;
	vecEndPos = vecDestination;

	// Try sliding forward both on ground and up 16 pixels
	//  take the move that goes farthest
	vec3_t vecPos, vecVel;
	vecPos = player->GetAbsOrigin();
	vecVel = player->m_vecVelocity();

	// Slide move down.
	TryPlayerMove(player, &vecEndPos, &trace);

	// Down results.
	vec3_t vecDownPos, vecDownVel;
	vecDownPos = player->GetAbsOrigin();
	vecDownVel = player->m_vecVelocity();

	// Reset original values.
	SetAbsOrigin(player, vecPos);
	player->m_vecVelocity() = vecVel;

	// Move up a stair height.
	vecEndPos = player->GetAbsOrigin();

	vecEndPos.z += player->m_flStepSize() + 0.03125;


	TracePlayerBBox(player->GetAbsOrigin(), vecEndPos, MASK_PLAYERSOLID, 8, trace, player);
	if (!trace.m_startsolid && !trace.m_allsolid)
	{
		SetAbsOrigin(player, trace.m_endpos);
	}

	TryPlayerMove(player, &dest[player->index()], &trace);

	// Move down a stair (attempt to).
	vecEndPos = player->GetAbsOrigin();

	vecEndPos.z -= player->m_flStepSize() + 0.03125;


	TracePlayerBBox(player->GetAbsOrigin(), vecEndPos, MASK_PLAYERSOLID, 8, trace, player);

	// If we are not on the ground any more then use the original movement attempt.
	if (trace.m_plane.m_normal[2] < 0.7)
	{
		SetAbsOrigin(player, vecDownPos);
		player->m_vecVelocity() = vecDownVel;

		float flStepDist = player->GetAbsOrigin().z - vecPos.z;
		if (flStepDist > 0.0f)
		{
			g_csgo.m_move_helper->SetHost(player);
			g_csgo.m_move_helper->m_outStepHeight += flStepDist;
			g_csgo.m_move_helper->SetHost(nullptr);
		}
		return;
	}

	// If the trace ended up in empty space, copy the end over to the origin.
	if (!trace.m_startsolid && !trace.m_allsolid)
	{
		player->SetAbsOrigin(trace.m_endpos);
	}

	// Copy this origin to up.
	vec3_t vecUpPos;
	vecUpPos = player->GetAbsOrigin();

	// decide which one went farther
	float flDownDist = (vecDownPos.x - vecPos.x) * (vecDownPos.x - vecPos.x) + (vecDownPos.y - vecPos.y) * (vecDownPos.y - vecPos.y);
	float flUpDist = (vecUpPos.x - vecPos.x) * (vecUpPos.x - vecPos.x) + (vecUpPos.y - vecPos.y) * (vecUpPos.y - vecPos.y);
	if (flDownDist > flUpDist)
	{
		SetAbsOrigin(player, vecDownPos);
		player->m_vecVelocity() = vecDownVel;
	}
	else
	{
		// copy z value from slide move
		player->m_vecVelocity() = vecDownVel;
	}

	float flStepDist = player->GetAbsOrigin().z - vecPos.z;
	if (flStepDist > 0)
	{
		g_csgo.m_move_helper->SetHost(player);
		g_csgo.m_move_helper->m_outStepHeight += flStepDist;
		g_csgo.m_move_helper->SetHost(nullptr);
	}
}

void RebuildGameMovement::TracePlayerBBox(const vec3_t& start, const vec3_t& end, unsigned int fMask, int collisionGroup, CGameTrace& pm, Player* player)
{
	if (!player || player == g_cl.m_local)
		return;

	CTraceFilterSimple_game filter;
	filter.SetPassEntity(player);

	g_csgo.m_engine_trace->TraceRay(Ray(start, end, player->m_vecMins(), player->m_vecMaxs()), fMask, (ITraceFilter*)&filter, &pm);
}

void RebuildGameMovement::WalkMove(Player* player)
{
	if (!player || player == g_cl.m_local)
		return;

	math::AngleVectors(player->m_angEyeAngles(), &forward[player->index()], &right[player->index()], &up[player->index()]);  // Determine movement angles
	// Copy movement amounts
	g_csgo.m_move_helper->SetHost(player);
	fmove[player->index()] = g_csgo.m_move_helper->m_flForwardMove;
	smove[player->index()] = g_csgo.m_move_helper->m_flSideMove;
	g_csgo.m_move_helper->SetHost(nullptr);

	if (forward[player->index()][2] != 0)
	{
		forward[player->index()][2] = 0;
		math::NormalizeVector(forward[player->index()]);
	}

	if (right[player->index()][2] != 0)
	{
		right[player->index()][2] = 0;
		math::NormalizeVector(right[player->index()]);
	}

	for (i[player->index()] = 0; i[player->index()] < 2; i[player->index()]++)       // Determine x and y parts of velocity
		wishvel[player->index()][i[player->index()]] = forward[player->index()][i[player->index()]] * fmove[player->index()] + right[player->index()][i[player->index()]] * smove[player->index()];

	wishvel[player->index()][2] = 0;             // Zero out z part of velocity

	wishdir[player->index()] = wishvel[player->index()]; // Determine maginitude of speed of move
	wishspeed[player->index()] = wishdir[player->index()].normalize();

	//
	// Clamp to server defined max speed
	//
	g_csgo.m_move_helper->SetHost(player);
	if ((wishspeed[player->index()] != 0.0f) && (wishspeed[player->index()] > g_csgo.m_move_helper->m_flMaxSpeed))
	{
		VectorMultiply(wishvel[player->index()], player->m_flMaxspeed() / wishspeed[player->index()], wishvel[player->index()]);
		wishspeed[player->index()] = player->m_flMaxspeed();
	}
	g_csgo.m_move_helper->SetHost(nullptr);
	// Set pmove velocity
	player->m_vecVelocity()[2] = 0;
	Accelerate(player, wishdir[player->index()], wishspeed[player->index()], g_csgo.sv_airaccelerate->GetFloat());
	player->m_vecVelocity()[2] = 0;

	// Add in any base velocity to the current velocity.
	VectorAdd(player->m_vecVelocity(), player->m_vecBaseVelocity(), player->m_vecVelocity());

	spd[player->index()] = player->m_vecVelocity().length();

	if (spd[player->index()] < 1.0f)
	{
		player->m_vecVelocity().IsZero();
		// Now pull the base velocity back out.   Base velocity is set if you are on a moving object, like a conveyor (or maybe another monster?)
		VectorSubtract(player->m_vecVelocity(), player->m_vecBaseVelocity(), player->m_vecVelocity());
		return;
	}

	// first try just moving to the destination	
	dest[player->index()][0] = player->GetAbsOrigin()[0] + player->m_vecVelocity()[0] * g_csgo.m_globals->m_frametime;
	dest[player->index()][1] = player->GetAbsOrigin()[1] + player->m_vecVelocity()[1] * g_csgo.m_globals->m_frametime;
	dest[player->index()][2] = player->GetAbsOrigin()[2];

	// first try moving directly to the next spot
	TracePlayerBBox(player->GetAbsOrigin(), dest[player->index()], MASK_PLAYERSOLID, 8, pm[player->index()], player);

	// If we made it all the way, then copy trace end as new player position.
	g_csgo.m_move_helper->SetHost(player);
	g_csgo.m_move_helper->m_outWishVel += wishdir[player->index()] * wishspeed[player->index()];
	g_csgo.m_move_helper->SetHost(nullptr);

	if (pm[player->index()].m_fraction == 1)
	{
		player->SetAbsOrigin(pm[player->index()].m_endpos);
		// Now pull the base velocity back out.   Base velocity is set if you are on a moving object, like a conveyor (or maybe another monster?)
		VectorSubtract(player->m_vecVelocity(), player->m_vecBaseVelocity(), player->m_vecVelocity());

		return;
	}

	// Don't walk up stairs if not on ground.
	if (!(player->m_fFlags() & FL_ONGROUND))
	{
		// Now pull the base velocity back out.   Base velocity is set if you are on a moving object, like a conveyor (or maybe another monster?)
		VectorSubtract(player->m_vecVelocity(), player->m_vecBaseVelocity(), player->m_vecVelocity());
		return;
	}

	StepMove(player, dest[player->index()], pm[player->index()]);

	// Now pull the base velocity back out.   Base velocity is set if you are on a moving object, like a conveyor (or maybe another monster?)
	VectorSubtract(player->m_vecVelocity(), player->m_vecBaseVelocity(), player->m_vecVelocity());
}

void RebuildGameMovement::FinishGravity(Player* player)
{
	if (!player || player == g_cl.m_local)
		return;

	float ent_gravity;

	ent_gravity = g_csgo.sv_gravity->GetFloat();

	// Get the correct velocity for the end of the dt 
	player->m_vecVelocity()[2] -= (ent_gravity * g_csgo.sv_gravity->GetFloat() * g_csgo.m_globals->m_frametime * 0.5);

	CheckVelocity(player);
}

void RebuildGameMovement::FullWalkMove(Player* player)
{
	if (!player || player == g_cl.m_local)
		return;

	StartGravity(player);

	// Fricion is handled before we add in any base velocity. That way, if we are on a conveyor, 
	//  we don't slow when standing still, relative to the conveyor.
	if (player->m_fFlags() & FL_ONGROUND)
	{
		player->m_vecVelocity()[2] = 0.0;
		Friction(player);
	}

	// Make sure velocity is valid.
	CheckVelocity(player);

	if (player->m_fFlags() & FL_ONGROUND)
	{
		WalkMove(player);
	}
	else
	{
		AirMove(player);  // Take into account movement when in air.
	}

	// Make sure velocity is valid.
	CheckVelocity(player);

	// Add any remaining gravitational component.
	FinishGravity(player);

	// If we are on ground, no downward velocity.
	if (player->m_fFlags() & FL_ONGROUND)
	{
		player->m_vecVelocity()[2] = 0;
	}

	// nigger bool
	// if we start crashing again this is the reason...
	// i changed m_flFallVelocity from a netvar into an offset though so we should be fine
	//CheckFalling(player);
}

void RebuildGameMovement::Friction(Player* player)
{
	if (!player || player == g_cl.m_local)
		return;

	// Calculate speed
	speed[player->index()] = player->m_vecVelocity().length();

	// If too slow, return
	if (speed[player->index()] < 0.1f)
	{
		return;
	}

	drop[player->index()] = 0;

	// apply ground friction
	if (player->m_fFlags() & FL_ONGROUND)  // On an Player that is the ground
	{
		friction[player->index()] = g_csgo.sv_friction->GetFloat() * player->m_surfaceFriction();

		//  Bleed off some speed, but if we have less than the bleed
		//  threshold, bleed the threshold amount.


		control[player->index()] = (speed[player->index()] < g_csgo.sv_stopspeed->GetFloat()) ? g_csgo.sv_stopspeed->GetFloat() : speed[player->index()];

		// Add the amount to the drop amount.
		drop[player->index()] += control[player->index()] * friction[player->index()] * g_csgo.m_globals->m_frametime;
	}

	// scale the velocity
	newspeed[player->index()] = speed[player->index()] - drop[player->index()];
	if (newspeed[player->index()] < 0)
		newspeed[player->index()] = 0;

	if (newspeed[player->index()] != speed[player->index()])
	{
		// Determine proportion of old speed we are using.
		newspeed[player->index()] /= speed[player->index()];
		// Adjust velocity according to proportion.
		VectorMultiply(player->m_vecVelocity(), newspeed[player->index()], player->m_vecVelocity());
	}

	player->m_vecVelocity() -= (1.f - newspeed[player->index()]) * player->m_vecVelocity();
}

//void RebuildGameMovement::CheckFalling(Player* player)
//{
//	if (!player || player == g_cl.m_local)
//		return;
//
//	// this function really deals with landing, not falling, so early out otherwise
//	if (player->m_flFallVelocity() <= 0)
//		return;
//
//	if (!player->m_iHealth() && player->m_flFallVelocity() >= 303.0f)
//	{
//		bool bAlive = true;
//		float fvol = 0.5;
//
//		//
//		// They hit the ground.
//		//
//		// m_vecOrigin().y ?????
//		if (player->m_vecVelocity().z < 0.0f)
//		{
//			// Player landed on a descending object. Subtract the velocity of the ground Player.
//			player->m_flFallVelocity() += player->m_vecVelocity().z;
//			player->m_flFallVelocity() = math::max(0.1f, player->m_flFallVelocity());
//		}
//
//		if (player->m_flFallVelocity() > 526.5f)
//		{
//			fvol = 1.0;
//		}
//		else if (player->m_flFallVelocity() > 526.5f / 2)
//		{
//			fvol = 0.85;
//		}
//		else if (player->m_flFallVelocity() < 173)
//		{
//			fvol = 0;
//		}
//
//	}
//
//	// let any subclasses know that the player has landed and how hard
//
//	//
//	// Clear the fall velocity so the impact doesn't happen again.
//	//
//	player->m_flFallVelocity() = 0;
//}

const int nanmask = 255 << 23;
#define	IS_NAN(x) (((*(int *)&x)&nanmask)==nanmask)

void RebuildGameMovement::CheckVelocity(Player* player)
{
	vec3_t org = player->GetAbsOrigin();

	for (i[player->index()] = 0; i[player->index()] < 3; i[player->index()]++)
	{
		// See if it's bogus.
		if (IS_NAN(player->m_vecVelocity()[i[player->index()]]))
		{
			player->m_vecVelocity()[i[player->index()]] = 0;
		}

		if (IS_NAN(org[i[player->index()]]))
		{
			org[i[player->index()]] = 0;
			player->SetAbsOrigin(org);
		}

		// Bound it.
		if (player->m_vecVelocity()[i[player->index()]] > g_csgo.sv_maxvelocity->GetFloat())
		{
			player->m_vecVelocity()[i[player->index()]] = g_csgo.sv_maxvelocity->GetFloat();
		}
		else if (player->m_vecVelocity()[i[player->index()]] < -g_csgo.sv_maxvelocity->GetFloat())
		{
			player->m_vecVelocity()[i[player->index()]] = -g_csgo.sv_maxvelocity->GetFloat();
		}
	}
}

void RebuildGameMovement::StartGravity(Player* player)
{
	if (!player || !player->m_iHealth())
		return;

	vec3_t pVel = player->m_vecVelocity();

	pVel[2] -= (g_csgo.sv_gravity->GetFloat() * 0.5f * g_csgo.m_globals->m_interval);
	pVel[2] += (player->m_vecBaseVelocity()[2] * g_csgo.m_globals->m_interval);

	player->m_vecVelocity() = pVel;

	vec3_t tmp = player->m_vecBaseVelocity();
	tmp[2] = 0.f;
	player->m_vecVelocity() = tmp;
}