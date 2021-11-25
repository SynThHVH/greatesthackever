#include "includes.h"
#include "pred.h"

InputPrediction g_inputpred{};;

void InputPrediction::UpdatePrediction( ) {
	bool        valid{ g_csgo.m_cl->m_delta_tick > 0 };
	int         outgoing_command, current_command, set_tickbase;
	CUserCmd    *cmd;

	m_first_command_predicted = *(bool*)(uintptr_t(g_csgo.m_prediction) + 0x18);
	m_in_prediction = g_csgo.m_prediction->m_in_prediction;

	m_predicting = false;

	// render start was not called.
	if( g_cl.m_stage == FRAME_NET_UPDATE_END ) {
		outgoing_command = g_csgo.m_cl->m_last_outgoing_command + g_csgo.m_cl->m_choked_commands;

		// this must be done before update ( update will mark the unpredicted commands as predicted ).
		for( int i{}; ; ++i ) {
			current_command = g_csgo.m_cl->m_last_command_ack + i;

			// caught up / invalid.
			if( current_command > outgoing_command || i >= MULTIPLAYER_BACKUP )
				break;

			// get command.
			cmd = g_csgo.m_input->GetUserCmd( current_command );
			if( !cmd )
				break;

			set_tickbase = g_cl.m_local->m_nTickBase();

			// cmd hasn't been predicted.
			// m_nTickBase is incremented inside RunCommand ( which is called frame by frame, we are running tick by tick here ) and prediction hasn't run yet,
			// so we must fix tickbase by incrementing it ourselves on non-predicted commands.
			if (!cmd->m_predicted) {
				++set_tickbase;
				g_cl.m_local->m_nTickBase() = set_tickbase;
			}
			else g_cl.m_local->m_nTickBase() = set_tickbase;
		}

		int start = g_csgo.m_cl->m_last_command_ack;
		int stop  = g_csgo.m_cl->m_last_outgoing_command + g_csgo.m_cl->m_choked_commands;

		const auto ticks = game::GetHostFrameticks() + g_csgo.m_cl->m_choked_commands;
		const auto lag_limit = (int)g_menu.main.antiaim.lag_limit.get() + 2;

		// detect rubber-banding.
//		if (ticks > lag_limit) {
//			auto delta = ticks - lag_limit;
//#ifdef SONTHTEST
//			console::log(XOR("rubber-banding detected!\n"));
//#endif
//			for (int i = 0; i < delta; ++i) {
//#ifdef SONTHTEST
//				console::log(XOR("fixing rubber-banding\n"));
//#endif
//				g_csgo.m_prediction->Update(g_csgo.m_cl->m_delta_tick, true, g_csgo.m_cl->m_last_command_ack, g_csgo.m_cl->m_last_outgoing_command + i);
//			}
//		}
		
		// we aren't going to rubber-band if we continue; call normal prediction.
//		else {
			// call CPrediction::Update.
			g_csgo.m_prediction->Update(g_csgo.m_cl->m_delta_tick, valid, start, stop);
//		}	
	}

	static bool unlocked_fakelag = false;
	if( !unlocked_fakelag ) {
		auto cl_move_clamp = pattern::find( g_csgo.m_engine_dll, XOR("B8 ? ? ? ? 3B F0 0F 4F F0 89 5D FC") ) + 1;
		unsigned long protect = 0;

		VirtualProtect( ( void * )cl_move_clamp, 4, PAGE_EXECUTE_READWRITE, &protect );
		*( std::uint32_t * )cl_move_clamp = 62;
		VirtualProtect( ( void * )cl_move_clamp, 4, protect, &protect );
		unlocked_fakelag = true;
	}
}

void InputPrediction::Predict( ) {
	static CMoveData data{};

	m_predicting = true;

	*(BYTE*)(uintptr_t(g_csgo.m_prediction) + 0x18) = 0;
	g_csgo.m_prediction->m_in_prediction = true;

	// CPrediction::StartCommand
	g_cl.m_local->m_pCurrentCommand( ) = g_cl.m_cmd;
	g_cl.m_local->m_PlayerCommand( )   = *g_cl.m_cmd;

	*g_csgo.m_nPredictionRandomSeed = g_cl.m_cmd->m_random_seed;
	g_csgo.m_pPredictionPlayer      = g_cl.m_local;

	// backup globals.
	m_curtime   = g_csgo.m_globals->m_curtime;
	m_frametime = g_csgo.m_globals->m_frametime;

	// CPrediction::RunCommand

	// set globals appropriately.
	g_csgo.m_globals->m_curtime   = game::TICKS_TO_TIME( g_cl.m_local->m_nTickBase( ) );
	g_csgo.m_globals->m_frametime = g_csgo.m_prediction->m_engine_paused ? 0.f : g_csgo.m_globals->m_interval;

	// set target player ( host ).
	g_csgo.m_move_helper->SetHost( g_cl.m_local );
	g_csgo.m_game_movement->StartTrackPredictionErrors( g_cl.m_local );

	// setup input.
	g_csgo.m_prediction->SetupMove( g_cl.m_local, g_cl.m_cmd, g_csgo.m_move_helper, &data );

	// run movement.
	g_csgo.m_game_movement->ProcessMovement( g_cl.m_local, &data );
	g_csgo.m_prediction->FinishMove( g_cl.m_local, g_cl.m_cmd, &data );
	g_csgo.m_game_movement->FinishTrackPredictionErrors( g_cl.m_local );

	// reset target player ( host ).
	g_csgo.m_move_helper->SetHost( nullptr );

	m_predicting = false;
}

void InputPrediction::PrePrediction( ) {
	*(bool*)(uintptr_t(g_csgo.m_prediction) + 0x18) = m_first_command_predicted;
	g_csgo.m_prediction->m_in_prediction = m_in_prediction;

	*g_csgo.m_nPredictionRandomSeed = -1;
	g_csgo.m_pPredictionPlayer      = nullptr;

	// restore globals.
	g_csgo.m_globals->m_curtime   = m_curtime;
	g_csgo.m_globals->m_frametime = m_frametime;

	m_predicting = false;
}

float InputPrediction::ReturnPredictedCurtime() {
	if (m_predicting) return game::TICKS_TO_TIME(g_cl.m_local->m_nTickBase());
	else return g_csgo.m_globals->m_curtime;
}