#pragma once

class RebuildGameMovement
{
private:

	int bumpcount[64], numbumps[64];
	vec3_t dir[64];
	float d[64];
	int numplanes[64];
	vec3_t primal_velocity[64], original_velocity[64];
	vec3_t new_velocity[64];
	int i[64], j[64];
	CGameTrace pm[64];
	vec3_t end[64];
	vec3_t vec3_origin[64];
	float time_left[64], allFraction[64];
	int blocked[64];

	float addspeed[64], accelspeed[64], currentspeed[64];
	float wishspd[64];
	vec3_t wishvel[64];
	float spd[64];
	float fmove[64], smove[64];
	vec3_t wishdir[64];
	float wishspeed[64];

	vec3_t dest[64];
	vec3_t forward[64], right[64], up[64];
	float speed[64], newspeed[64], control[64];
	float friction[64];
	float drop[64];
	float ent_gravity[64];

public:
	void SetAbsOrigin(Player* C_BasePlayer, const vec3_t& vec);
	void FullWalkMove(Player* C_BasePlayer);
	void CheckVelocity(Player* C_BasePlayer);
	void FinishGravity(Player* C_BasePlayer);
	void StepMove(Player* C_BasePlayer, vec3_t& vecDestination, CGameTrace& trace);
	int ClipVelocity(vec3_t& in, vec3_t& normal, vec3_t& out, float overbounce);
	int TryPlayerMove(Player* C_BasePlayer, vec3_t* pFirstDest, CGameTrace* pFirstTrace);
	void Accelerate(Player* C_BasePlayer, vec3_t& wishdir, float wishspeed, float accel);
	void Friction(Player* C_BasePlayer);
	void AirAccelerate(Player* C_BasePlayer, vec3_t& wishdir, float wishspeed, float accel);
	void AirMove(Player* C_BasePlayer);
	void WalkMove(Player* C_BasePlayer);
	//void CheckFalling(Player* C_BasePlayer);
	void StartGravity(Player* C_BasePlayer);
	void TracePlayerBBox(const vec3_t& start, const vec3_t& end, unsigned int fMask, int collisionGroup, CGameTrace& pm, Player* C_BasePlayer);
};
extern RebuildGameMovement g_gamemovement;