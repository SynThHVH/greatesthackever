#include "includes.h"

// why i dislike namespaces
RecvVarProxy_t Proxies::m_Pitch_original;
RecvVarProxy_t Proxies::m_Yaw_original;
RecvVarProxy_t Proxies::m_Body_original;
RecvVarProxy_t Proxies::m_SimTime_original;
RecvVarProxy_t Proxies::m_Force_original;
RecvVarProxy_t Proxies::m_AbsYaw_original;

// jitter pitch resolver
void OnReceivedPitch(AimPlayer* m_player, float m_pitch) {
	LagRecord* m_record = m_player->m_records.front().get();
	if (m_player->m_records.size() <= 1) return;
	if (m_record->m_last_pitch == 0.0f)
		m_record->m_last_pitch = m_pitch;
	else {
		float m_degrees_changed = m_pitch - m_record->m_last_pitch;
		m_record->m_last_pitch = m_pitch;

		if (m_degrees_changed > 180.0f)
			m_degrees_changed -= 360.0f;
		else if (m_degrees_changed < -180.0f)
			m_degrees_changed += 360.0f;

		m_record->m_last_pitch_change = m_degrees_changed;
	}
}

void PitchProxy(CRecvProxyData* data, void* ptr, Address out) {
	float old = out ? data->m_Value.m_Float : 0;

	// call original netvar proxy.
	if (Proxies::m_Pitch_original) Proxies::m_Pitch_original(data, ptr, out);
	if (!out) return;

	float m_out_val = data->m_Value.m_Float; // variable for easy access

	// commented this because it might conflict with the function above;
	// do a lot of testing and see for sure.
	//math::NormalizeAngle(m_out_val);
	//math::clamp(m_out_val, -90.f, 90.f);

	Player* pent = (Player*)ptr;
	if (pent && pent->IsPlayer()) /* i make sure that the local player is alive so our console isn't getting spammed while we aren't alive, etc */ {
		int index = pent->index();
		AimPlayer* m_player = &g_aimbot.m_players[index];
		if (m_player) {
			if (m_player->m_records.size() >= 2 && m_player->m_records.empty()) { // we should have at least two records while doing this to compare this too
				if (pent != g_cl.m_local) { // don't do this on the local player, i mean we shouldn't be keeping records of the local player anyways but still.
					for (const auto& records : m_player->m_records) { // do this for every record.
						const float m_sim_time = pent->m_flSimulationTime();
						if (m_sim_time != m_player->m_simtime_recieved_new_eyeangles_x || records->m_eye_angles.x != m_out_val || (m_out_val != 0.0f && records->m_eye_angles.x == 0.0f)) {
							OnReceivedPitch(m_player, m_out_val);
							records->m_eye_angles.x = m_out_val;
							m_player->m_simtime_recieved_new_eyeangles_x = m_sim_time;
//#ifdef SONTHTEST
//							if (m_out_val != old) g_console.log(tfm::format(XOR("%s: Pitch updated %f\n"), game::GetPlayerName(index), m_out_val));
//#endif
						}
					}
				}
			}
		}
	}
}

// spinbot check
void OnReceivedYaw(AimPlayer* m_player, float m_yaw) {
	LagRecord* record = m_player->m_records.front().get();
	if (record->m_last_yaw == 0.0f) {
		record->m_last_yaw = m_yaw;
		record->m_spinbotting = false;
	}
	else {
		float changeddegrees = m_yaw - record->m_last_yaw;
		if (changeddegrees != 0.0f) {
			record->m_last_yaw = m_yaw;
			if (changeddegrees > 180.0f) changeddegrees -= 360.0f;
			else if (changeddegrees < -180.0f) changeddegrees += 360.0f;

			if (record->m_last_yaw_change == 0.0f || changeddegrees == 0.0f) record->m_spinbotting = false;
			else if (((changeddegrees > 0 && record->m_last_yaw_change > 0) || (changeddegrees < 0 && record->m_last_yaw_change < 0)) && fabsf(changeddegrees) >= 15.0f && fabsf(changeddegrees - record->m_last_yaw_change) <= 10.0f) record->m_spinbotting = true;
			else record->m_spinbotting = false;

			record->m_last_yaw_change = changeddegrees;
			record->m_eye_yaw_updated = true;
		}
	}
}

void YawProxy(const CRecvProxyData* pData, void* ent, void* pOut) { // if this works im changing them all to this lmao
	if (Proxies::m_Yaw_original) ((void(*)(const CRecvProxyData*, void*, void*))Proxies::m_Yaw_original)(pData, ent, pOut);
	if (!pOut) return;

	float old = pOut ? *(float*)pOut : 0;

	float m_out_value = pData->m_Value.m_Float;

	Player* pent = (Player*)ent;
	if (pent && pent->IsPlayer()) {
		int index = pent->index();
		AimPlayer* m_player = &g_aimbot.m_players[index];
		if (m_player) {
			if (m_player->m_records.size() >= 2 && m_player->m_records.empty()) { // we should have at least two records while doing this to compare this too
				if (pent != g_cl.m_local) { // don't do this on the local player, i mean we shouldn't be keeping records of the local player anyways but still.
					for (const auto& records : m_player->m_records) { // do this for every record.
						const float SimulationTime = pent->m_flSimulationTime();
						if (SimulationTime != m_player->m_simtime_recieved_new_eyeangles_y || records->m_eye_angles.y != m_out_value) {
							OnReceivedYaw(m_player, m_out_value);
							records->m_eye_angles.y = m_out_value;
							m_player->m_simtime_recieved_new_eyeangles_y = SimulationTime;
//#ifdef SONTHTEST
//							if (m_out_value != old) g_console.log(tfm::format(XOR("%s: Yaw updated %f\n"), game::GetPlayerName(index), m_out_value));
//#endif
						}
					}
				}
			}
		}
	}
}

void BodyProxy(CRecvProxyData* data, Address ptr, Address out) {
	Stack stack;
	static Address RecvTable_Decode{ pattern::find(g_csgo.m_engine_dll, XOR("EB 0D FF 77 10")) };

	// call from entity going into pvs.
	if (stack.next().next().ReturnAddress() != RecvTable_Decode) {
		// convert to player.
		Player* player = ptr.as< Player* >();

		// store data about the update.
		g_resolver.OnBodyUpdate(player, data->m_Value.m_Float);
	}

	// call original proxy.
	if (Proxies::m_Body_original) Proxies::m_Body_original(data, ptr, out);
}

void ForceRagdollProxy(CRecvProxyData* data, Address ptr, Address out) {
	Ragdoll* ragdoll = ptr.as< Ragdoll* >(); // convert to ragdoll.
	Player* player = ragdoll->GetPlayer(); // get ragdoll owner.

	// we only want this happening to noobs we kill.
	if (g_menu.main.misc.ragdoll_force.get() && g_cl.m_local && player && player->enemy(g_cl.m_local)) {
		// get m_vecForce.
		vec3_t vel = { data->m_Value.m_Vector[0], data->m_Value.m_Vector[1], data->m_Value.m_Vector[2] };

		// give some speed to all directions.
		vel *= 1000.f;

		// boost z up a bit.
		if (vel.z <= 1.f)
			vel.z = 2.f;

		vel.z *= 2.f;

		// don't want crazy values for this... probably unlikely though?
		math::clamp(vel.x, std::numeric_limits< float >::lowest(), std::numeric_limits< float >::max());
		math::clamp(vel.y, std::numeric_limits< float >::lowest(), std::numeric_limits< float >::max());
		math::clamp(vel.z, std::numeric_limits< float >::lowest(), std::numeric_limits< float >::max());

		// set new velocity.
		data->m_Value.m_Vector[0] = vel.x;
		data->m_Value.m_Vector[1] = vel.y;
		data->m_Value.m_Vector[2] = vel.z;
	}

	if (Proxies::m_Force_original) Proxies::m_Force_original(data, ptr, out);
}

//Attempts to restore (64 tick servers) Simulation time to the exact value it was on the server before it got compressed
float ExtractLostPrecisionForSimulationTime(float val) {
	char Str1[50];
	char Str2[50];
	// convert float to string
	int n = sprintf(Str1, "%f", val);
	// find the index of the decimal point in the string
	int pointLoc = strchr(Str1, '.') - Str1;
	// remove leading zeroes from the end of the string (Very fast, 5 iterations at max)
	int c = n - 1;
	for (; c > pointLoc + 1 && Str1[c] == '0' && Str1[c - 1] == '0'; c--) {
		Str1[c] = 0;
	}
	// remove the decimal point from the string
	memcpy(Str2, Str1, pointLoc);
	memcpy(Str2 + pointLoc, Str1 + pointLoc + 1, n - (pointLoc + 1) - ((n - 1) - c) + 1);
	// convert the string to an int
	long long NewLL = atoll(Str2);
	// round the last digit to the nearest multiple of 25
	long long num25s = round((double)NewLL / 25.0);
	long long nigNew = num25s * 25;
	// convert the newly rounded int in nigNew to a string
	n = sprintf(Str1, "%lld", nigNew);
	// add the decimal point back into the int string
	Str2[pointLoc] = '.';
	// convert the int string back to a floating point string
	memcpy(Str2, Str1, pointLoc);
	memcpy(Str2 + pointLoc + 1, Str1 + pointLoc, n - pointLoc + 1);
	// store the result in nig ( Due the the limitations of floating point format, I was unable to see a difference for some values =/,
	// step through the code in the debugger and you will see how the string version of the float is perfectly rounded, but when it
	// is converted back to a floating point value, it loses that rounding >:( )
	return atof(Str2);
}

// thanks for this and the function above sharklazer!
void ReceivedSimulationTime(const CRecvProxyData* pData, void* ent, void* pOut) {
	if (Proxies::m_SimTime_original) ((void(*)(const CRecvProxyData*, void*, void*))Proxies::m_SimTime_original)(pData, ent, pOut);
	float time = (uintptr_t)pOut;
	Entity* pent = (Entity*)ent;
	if (pOut && pent && pent->IsPlayer() && pent != g_cl.m_local) {
		int index = pent->index();
		AimPlayer* m_player = &g_aimbot.m_players[index];
		if (m_player) {
			if (m_player->m_records.size() >= 2 && m_player->m_records.empty()) { // we should have at least two records while doing this to compare this too
				if (pent != g_cl.m_local) { // don't do this on the local player, i mean we shouldn't be keeping records of the local player anyways but still.
					for (const auto& records : m_player->m_records) { // do this for every record.
						if (g_csgo.m_globals->m_interval == (1.0f / 64.0f)) time = ExtractLostPrecisionForSimulationTime(time); // extract exact simulation time to what it was before it was compressed.
						records->m_sim_time = time; // set our simtime records to this.
#ifdef SONTHTEST
						g_console.log(XOR("simtime record modulated to proxy\n"));
#endif
					}
				}
			}
		}
	}
}

// TODO: finish hooking everything in mutiny v2 and v4 and start using it for ur resolver and other things!  
void Proxies::InitializeProxies() {
	// set netvar proxies.
	//g_netvars.SetProxy(HASH("DT_CSPlayer"),  HASH("m_angEyeAngles[0]"),      PitchProxy, m_Pitch_original);
	//g_netvars.SetProxy(HASH("DT_CSPlayer"),  HASH("m_angEyeAngles[1]"),      YawProxy, m_Yaw_original);
	g_netvars.SetProxy(HASH("DT_CSPlayer"),  HASH("m_flLowerBodyYawTarget"), BodyProxy, m_Body_original);
	g_netvars.SetProxy(HASH("DT_CSPlayer"),  HASH("m_flSimulationTime"),     ReceivedSimulationTime, m_SimTime_original);
	g_netvars.SetProxy(HASH("DT_CSRagdoll"), HASH("m_vecForce"),             ForceRagdollProxy, m_Force_original);
}