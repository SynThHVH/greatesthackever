#pragma once

class InputPrediction {
public:
	bool  m_first_command_predicted, m_in_prediction, m_predicting;
	float m_curtime, m_frametime;

public:
	void UpdatePrediction( );
	void Predict( );
	void PrePrediction( );
	float ReturnPredictedCurtime();
};

extern InputPrediction g_inputpred;