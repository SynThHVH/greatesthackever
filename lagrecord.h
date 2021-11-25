#pragma once

// pre-declare.
class LagRecord;

class BackupRecord {
public:
	BoneArray* m_bones;
	int        m_bone_count;
	vec3_t     m_origin, m_abs_origin;
	vec3_t     m_mins;
	vec3_t     m_maxs;
	ang_t      m_abs_ang;

public:
	__forceinline void store(Player* player) {
		// get bone cache ptr.
		CBoneCache* cache = &player->m_BoneCache();

		// store bone data.
		m_bones = cache->m_pCachedBones;
		m_bone_count = cache->m_CachedBoneCount;
		m_origin = player->m_vecOrigin();
		m_mins = player->m_vecMins();
		m_maxs = player->m_vecMaxs();
		m_abs_origin = player->GetAbsOrigin();
		m_abs_ang = player->GetAbsAngles();
	}

	__forceinline void restore(Player* player) {
		// get bone cache ptr.
		CBoneCache* cache = &player->m_BoneCache();

		cache->m_pCachedBones = m_bones;
		cache->m_CachedBoneCount = m_bone_count;

		player->m_vecOrigin() = m_origin;
		player->m_vecMins() = m_mins;
		player->m_vecMaxs() = m_maxs;
		player->SetAbsAngles(m_abs_ang);
		player->SetAbsOrigin(m_origin);
	}
};

class LagRecord {
public:
	// data.
	Player* m_player;
	float   m_immune;
	int     m_tick;
	int     m_lag;
	bool    m_dormant, m_first_after_dormancy, m_animated, m_data_stored, m_valid, m_push_to_aimbot;

	// netvars.
	float  m_sim_time;
	float  m_old_sim_time, m_time_delta;
	int    m_flags;
	vec3_t m_origin;
	vec3_t m_old_origin;
	vec3_t m_velocity;
	vec3_t m_mins;
	vec3_t m_maxs;
	ang_t  m_eye_angles;
	ang_t  m_abs_ang;
	ang_t  set_eye_angles;
	float  m_body;
	float  m_duck;

	// anim stuff.
	C_AnimationLayer m_layers[13];
	float            m_poses[24];
	vec3_t           m_anim_velocity;
	float            m_anim_speed, m_max_current_speed;

	// bone stuff.
	bool       m_setup;
	BoneArray* m_bones;

	// lagfix stuff.
	bool   m_broke_lc, m_exploiting, m_skip_due_to_resolver;
	vec3_t m_extrapolated_origin;
	vec3_t m_extrapolated_velocity;
	float  m_lagcorrected_time;
	int    m_extrapolated_flags, m_simtime_delay, m_started_anim_update_time;

	// resolver stuff.
	size_t m_mode;
	bool   m_fake_walk;
	bool   m_shot;
	int    m_strafe_sequence, m_baim_tick;
	float  m_away;
	float  m_anim_time, m_primary_cycle, m_move_weight, m_goal_feet_yaw, m_current_feet_yaw, m_strafe_change_weight, m_strafe_change_cycle, m_acceleration_weight, m_last_pitch, m_last_pitch_change, m_last_yaw, m_last_yaw_change;

	// other stuff.
	float  m_interp_time;
public:

	// default ctor.
	__forceinline LagRecord() :
		m_setup{ false },
		m_broke_lc{ false },
		m_fake_walk{ false },
		m_shot{ false },
		m_lag{},
		m_bones{} {}

	// ctor.
	__forceinline LagRecord(Player* player) :
		m_setup{ false },
		m_broke_lc{ false },
		m_fake_walk{ false },
		m_shot{ false },
		m_lag{},
		m_bones{} {

		store(player);
	}

	// dtor.
	__forceinline ~LagRecord() {
		// free heap allocated game mem.
		g_csgo.m_mem_alloc->Free(m_bones);
	}

	__forceinline void invalidate() {
		// free heap allocated game mem.
		g_csgo.m_mem_alloc->Free(m_bones);

		// mark as not setup.
		m_setup = false;

		// allocate new memory.
		m_bones = (BoneArray*)g_csgo.m_mem_alloc->Alloc(sizeof(BoneArray) * 128);
	}

	// function: allocates memory for SetupBones and stores relevant data.
	void store(Player* player) {
		// allocate game heap.
		m_bones = (BoneArray*)g_csgo.m_mem_alloc->Alloc(sizeof(BoneArray) * 128);

		// player data.
		m_player = player;
		m_immune = player->m_fImmuneToGunGameDamageTime();
		m_tick = g_csgo.m_cl->m_server_tick;
		m_simtime_delay = m_tick - game::TICKS_TO_TIME(player->m_flSimulationTime());
		m_exploiting = m_simtime_delay >= 12; // come the fuck at me homo

		// netvars.
		m_lagcorrected_time = m_sim_time = player->m_flSimulationTime();
		m_old_sim_time = player->m_flOldSimulationTime();
		m_extrapolated_flags = m_flags = player->m_fFlags();
		m_extrapolated_origin = m_origin = player->m_vecOrigin();
		m_old_origin = player->m_vecOldOrigin();
		m_eye_angles = player->m_angEyeAngles();
		m_abs_ang = player->GetAbsAngles();
		m_body = player->m_flLowerBodyYawTarget();
		m_mins = player->m_vecMins();
		m_maxs = player->m_vecMaxs();
		m_duck = player->m_flDuckAmount();
		m_extrapolated_velocity = m_velocity = player->m_vecVelocity();
		m_max_current_speed = player->m_flMaxspeed();

		// save networked animlayers.
		player->GetAnimLayers(m_layers);

		// normalize eye angles.
		m_eye_angles.normalize();
		math::clamp(m_eye_angles.x, -90.f, 90.f);

		// get lag.
		m_lag = game::TIME_TO_TICKS(m_sim_time - m_old_sim_time);
		m_lag = std::clamp(m_lag, 0, 32); // clamp it so we don't interpolate too far.

		// save the players time delta for later calculations.
		m_time_delta = m_sim_time + m_old_sim_time;

		// compute animtime.
		m_anim_time = m_old_sim_time + g_csgo.m_globals->m_interval;

		// simple record validation check
		m_valid = valid();

		// we have stored all of our data
		m_data_stored = true;
	}

	// function: restores 'predicted' variables to their original.
	__forceinline void predict() {
		m_broke_lc = false;
		m_extrapolated_origin = m_origin;
		m_extrapolated_velocity = m_velocity;
		m_lagcorrected_time = m_sim_time;
		m_extrapolated_flags = m_flags;
	}

	// function: writes current record to bone cache.
	__forceinline void cache() {
		// get bone cache ptr.
		CBoneCache* cache = &m_player->m_BoneCache();

		cache->m_pCachedBones = m_bones;
		cache->m_CachedBoneCount = 128;

		m_player->m_vecOrigin() = m_extrapolated_origin;
		m_player->m_vecMins() = m_mins;
		m_player->m_vecMaxs() = m_maxs;

		m_player->SetAbsAngles(m_abs_ang);
		m_player->SetAbsOrigin(m_extrapolated_origin);
	}

	__forceinline bool dormant() {
		return m_dormant;
	}

	__forceinline bool immune() {
		return m_immune > 0.f;
	}

	// function: checks if LagRecord obj is hittable if we were to fire at it now.
	bool valid() {
		auto nci = g_csgo.m_engine->GetNetChannelInfo();

		// NOTE: predict the servertime.
		float serverTime = g_cl.m_local->m_nTickBase() * g_csgo.m_globals->m_interval;

		// NOTE: get the time of our target.
		float flTargetTime = m_sim_time;

		int latencyticks = std::max(0, game::TIME_TO_TICKS(g_csgo.m_cl->m_net_channel->GetLatency(INetChannel::FLOW_OUTGOING)));
		int server_tickcount = g_csgo.m_cl->m_server_tick + latencyticks + 1;
		float server_time_at_frame_end_from_last_frame = game::TICKS_TO_TIME(server_tickcount - 1);

		int deltaticks = m_lag + 1;
		int updatedelta = g_csgo.m_cl->m_server_tick - m_tick;
		if (latencyticks > deltaticks - updatedelta) {
			//only check if record is deleted if enemy would have sent another tick to the server already

			int flDeadtime = (server_time_at_frame_end_from_last_frame - g_csgo.sv_maxunlag->GetFloat());

			if (flTargetTime < flDeadtime)
				return false; //record won't be valid anymore
		}

		// NOTE: outgoing latency + const viewlag aka TICKS_TO_TIME(TIME_TO_TICKS(GetClientInterpAmount())) and clamp correct to sv_maxunlag
		float correct = std::clamp(nci->GetAvgLatency(INetChannel::FLOW_OUTGOING) + g_cl.m_lerp, 0.f, g_csgo.sv_maxunlag->GetFloat());
		float delta = correct - (serverTime - flTargetTime);
		return fabsf(delta) < 0.2f;

		// previous/supremacy's version of this function
		/*	// use prediction curtime for this.
			float curtime = game::TICKS_TO_TIME( g_cl.m_local->m_nTickBase( ) );

			// correct is the amount of time we have to correct game time,
			float correct = g_cl.m_lerp + g_cl.m_latency;

			// stupid fake latency goes into the incoming latency.
			float in = g_csgo.m_net->GetLatency( INetChannel::FLOW_INCOMING );
			correct += in;

			// check bounds [ 0, sv_maxunlag ]
			math::clamp( correct, 0.f, g_csgo.sv_maxunlag->GetFloat( ) );

			// calculate difference between tick sent by player and our latency based tick.
			// ensure this record isn't too old.
			return std::abs( correct - ( curtime - m_sim_time ) ) < 0.19f;
			*/
	}
};