#pragma once

#define mps (CONTENTS_SOLID | CONTENTS_MOVEABLE | CONTENTS_PLAYERCLIP | CONTENTS_WINDOW | CONTENTS_MONSTER | CONTENTS_GRATE)

class AimPlayer;
class LagCompensation {
public:
	bool StartPrediction(AimPlayer*);
	void Extrapolation(LagRecord*, bool);
	void PredictAnimations(CCSGOPlayerAnimState*, LagRecord*);
};

extern LagCompensation g_lagcomp;