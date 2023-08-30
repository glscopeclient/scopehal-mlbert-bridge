#include "BertServer.h"
#include "mlbertapi.h"
#include "../lib/log/log.h"

using namespace std;

extern mlbertapi* g_hBert;
extern int g_model;
extern uint64_t g_serial;

BertServer::BertServer(ZSOCKET sock)
	: SCPIServer(sock)
	, m_deferring(false)
{
	//Default everything to PRBS7
	for (int i = 0; i < 4; i++)
	{
		m_txPoly[i] = 0;
		m_rxPoly[i] = 0;
		m_txInvert[i] = false;
		m_rxInvert[i] = false;
	}
}

BertServer::~BertServer()
{
}

void BertServer::RefreshChannelPolynomialAndInvert(int chnum)
{
	if (!mlBert_SetPRBSPattern(
		g_hBert,
		chnum,
		m_txPoly[chnum],
		m_rxPoly[chnum],
		m_txInvert[chnum],
		m_rxInvert[chnum]))
	{
		LogError("mlBert_SetPrbsPattern failed\n");
	}
}

/*
void BertServer::SelectUserPatternClockOut()
{
	//no i have no idea why this works
	//this was way too hard to figure out
	uint16_t value = 0xffff;
	mlBert_AccessBoardRegister(g_hBert, 1, 0, 0x0a, &value);
	mlBert_AccessBoardRegister(g_hBert, 1, 0, 0x0b, &value);
	mlBert_AccessBoardRegister(g_hBert, 1, 1, 0x0103, &value);
	mlBert_AccessBoardRegister(g_hBert, 1, 0, 0x0103, &value);

	value = 0xd150;
	mlBert_AccessBoardRegister(g_hBert, 1, 1, 0x0103, &value);
}
*/

bool BertServer::OnCommand(
	const string& line,
	const string& subject,
	const string& cmd,
	const vector<string>& args)
{
	//If we have a subject, get the channel number
	//Should be TXn or RXn
	bool chIsTx = false;
	int chnum = 0;
	if (subject[0] == 'T')
		chIsTx = true;
	if (subject != "")
		chnum = atoi(subject.substr(2).c_str()) - 1;
	if (chnum < 0)
		chnum = 0;
	if (chnum >= 3)
		chnum = 3;

	//TX channel
	if (chIsTx)
	{
		if (cmd == "POLY")
		{
			LogDebug("Setting polynomial for %s to %s\n", subject.c_str(), args[0].c_str());

			if (args[0] == "PRBS7")
				m_txPoly[chnum] = 0;
			else if (args[0] == "PRBS9")
				m_txPoly[chnum] = 1;
			else if (args[0] == "PRBS15")
				m_txPoly[chnum] = 2;
			else if (args[0] == "PRBS23")
				m_txPoly[chnum] = 3;
			else if (args[0] == "PRBS31")
				m_txPoly[chnum] = 4;
			else if (args[0] == "USER")
				m_txPoly[chnum] = 5;
			else
				LogWarning("Unrecognized polynomial %s requested", args[0].c_str());

			RefreshChannelPolynomialAndInvert(chnum);
		}

		else if (cmd == "ENABLE")
		{
			bool en = false;
			if(args[0] == "1")
				en = true;

			if(!mlBert_TXEnable(g_hBert, chnum, en))
				LogError("Failed to set TX enable\n");
		}

		else if (cmd == "PRECURSOR")
		{
			int value = atoi(args[0].c_str());

			if(!mlBert_PreEmphasis(g_hBert, chnum, value))
				LogError("Failed to set precursor tap\n");
		}

		else if (cmd == "POSTCURSOR")
		{
			int value = atoi(args[0].c_str());

			if (!mlBert_PostEmphasis(g_hBert, chnum, value))
				LogError("Failed to set postcursor tap\n");
		}

		else if (cmd == "INVERT")
		{
			LogDebug("Setting invert flag for %s to %s\n", subject.c_str(), args[0].c_str());

			if (args[0] == "1")
				m_txInvert[chnum] = true;
			else
				m_txInvert[chnum] = false;

			RefreshChannelPolynomialAndInvert(chnum);
		}

		//legal values for 4039 are 0, 100, 200, 300, 400
		else if (cmd == "SWING")
		{
			LogDebug("Setting swing for %s to %s\n", subject.c_str(), args[0].c_str());

			if(!mlBert_OutputLevel(g_hBert, chnum, atof(args[0].c_str())))
				LogError("Failed to set swing\n");
		}

		else
		{
			LogWarning("Unrecognized command %s\n", cmd.c_str());
			return false;
		}
	}

	//RX channel
	else if (subject != "")
	{
		if (cmd == "POLY")
		{
			LogDebug("Setting polynomial for channel %s to %s\n", subject.c_str(), args[0].c_str());

			if (args[0] == "PRBS7")
				m_rxPoly[chnum] = 0;
			else if (args[0] == "PRBS9")
				m_rxPoly[chnum] = 1;
			else if (args[0] == "PRBS15")
				m_rxPoly[chnum] = 2;
			else if (args[0] == "PRBS23")
				m_rxPoly[chnum] = 3;
			else if (args[0] == "PRBS31")
				m_rxPoly[chnum] = 4;
			else if (args[0] == "AUTO")
				m_rxPoly[chnum] = 5;
			else
				LogWarning("Unrecognized polynomial %s requested", args[0].c_str());

			RefreshChannelPolynomialAndInvert(chnum);
		}

		else if (cmd == "INVERT")
		{
			LogDebug("Setting invert flag for %s to %s\n", subject.c_str(), args[0].c_str());

			if (args[0] == "1")
				m_rxInvert[chnum] = true;
			else
				m_rxInvert[chnum] = false;

			RefreshChannelPolynomialAndInvert(chnum);
		}

		else
		{
			LogWarning("Unrecognized command %s\n", cmd.c_str());
			return false;
		}
	}

	//No subject
	else
	{
		if (cmd == "CLKOUT")
		{
			if (args[0] == "LO_DIV32")
			{
				LogDebug("Setting refclk out mux to divided LO\n");

				//despite the API name this is *not* a 1/8 rate clock for the 4039
				//but is this actually selecting the serdes???
				if(1 != mlBert_TXClockOut_RateOverEight(g_hBert))
					LogError("failed to set clock out mux\n");
			}

			else if (args[0] == "SERDES")
			{
				//This seems to select serdes mode!
				LogDebug("Setting refclk out mux to SERDES\n");
				if (1 != mlBert_ClockOut(g_hBert, 0, 0))
					LogError("failed to set clock out mux\n");
			}

			//RX
			else
			{
				int ch = 0;
				int idx = 0;
				int div = 0;
				sscanf(args[0].c_str(), "RX%d_DIV%d\n", &ch, &div);
				ch--;
				if (div == 8)
					idx = 1;
				else
					idx = 2;

				LogDebug("Setting refclk out mux to RX channel %d, rate/%d\n", ch+1, div);

				if (1 != mlBert_ClockOut(g_hBert, ch, idx))
					LogError("failed to set clock out mux\n");
			}
		}
		else if (cmd == "USERPATTERN")
		{
			uint64_t userpattern = 0;
			sscanf(args[0].c_str(), "%llx", &userpattern);
			LogDebug("Setting user pattern to 0x%llx\n", userpattern);

			//note that for ML4039 at least, this is global not a per channel setting
			//TODO: 0x211, 212 seems to be a *32 bit* pattern not 16?
			//Docs say: 16 bit length in low rate mode
			//40 bit length in high rate mode
			if(!mlBert_SetTxUserPattern(g_hBert, 0, userpattern))
				LogError("failed to set user pattern\n");
		}
		else if(cmd == "DEFER")
		{
			LogDebug("Deferring channel config changes\n");
			m_deferring = true;
		}
		else if (cmd == "APPLY")
		{
			for(int i=0; i<4; i++)
				RefreshChannelPolynomialAndInvert(chnum);
			m_deferring = false;
		}
		else if (cmd == "RATE")
		{
			int64_t bps = stoll(args[0].c_str());
			double gbps = bps * 1e-9;

			//TODO: support ext ref clk
			
			// Line rate in Gbps, clock index 0=external, 1=internal
			LogDebug("Setting line rate to %f Gbps\n", gbps);
			if(!mlBert_LineRateConfiguration(g_hBert, gbps, 1))
				LogError("Failed to set line rate\n");

			//if in DEFER mode, we're doing initial startup
			//so don't restore config because we're about to reconfigure everything anyway
			if(!m_deferring)
			{
				if (!mlBert_RestoreAllConfig(g_hBert))
					LogError("Failed to restore config\n");
			}
		}
		else
		{
			LogWarning("Unrecognized command %s\n", cmd.c_str());
			return false;
		}
	}

	return true;
}

bool BertServer::OnQuery(
	const string& line,
	const string& subject,
	const string& cmd)
{
	//If we have a subject, get the channel number
	//Should be TXn or RXn
	bool chIsTx = false;
	int chnum = 0;
	if (subject[0] == 'T')
		chIsTx = true;
	if (subject != "")
		chnum = atoi(subject.substr(2).c_str()) - 1;
	if (chnum < 0)
		chnum = 0;
	if (chnum >= 3)
		chnum = 3;

	if (cmd == "*IDN")
	{
		char tmp[256];
		snprintf(tmp, sizeof(tmp),
			"MultiLANE,ML%d,%016llx,0.0",
			g_model,
			g_serial);
		SendReply(tmp);
	}

	//Channel queries
	else if(subject != "")
	{
		if(!chIsTx)
		{
			if(cmd == "LOCK")
			{
				//very confusingly named (and misspelled) API
				if(mlBert_PRBSVerfication(g_hBert, chnum))
					SendReply("1");
				else
					SendReply("0");
			}

			else if(cmd == "HBATHTUB")
			{
				//Read a quick bathtub
				//Docs say we only need 128 entries but we get memory corruption
				//unless we have 129 entries of space allocated...
				double xvalues[129] = {0};
				double berValues[129] = {0};
				if(!mlBert_GetBathTub(g_hBert, chnum, xvalues, berValues))
					LogError("Failed to get bathtub\n");
				if(!mlBert_CalculateBathtubDualDirac(g_hBert, chnum, xvalues, berValues))
					LogError("Failed to do dual dirac\n");

				char tmp[128];
				string reply;
				for(int i=0; i<128; i++)
				{
					snprintf(tmp, sizeof(tmp), "%f,%e,", (float)xvalues[i], (float)berValues[i]);
					reply += tmp;
				}
				SendReply(reply);
			}
			else
				return false;
		}

		//TX channel
		else
		{
			if (cmd == "LOCK")
			{
				//for some reason this always seems to be false?
				bool status = false;
				if(!mlBert_ReadTXLock(g_hBert, chnum, &status))
					LogError("failed to read tx lock\n");

				if(status)
					SendReply("1");
				else
					SendReply("0");
			}

			else
				return false;
		}
	}

	/*
	//Read temperatures
	double temp0 = mlBert_TempRead(g_hBert, 0);
	double temp1 = mlBert_TempRead(g_hBert, 1);
	LogVerbose("Temperatures: TX=%.1f, RX=%.1f\n", temp0, temp1);
	*/

	//APICALL int  STACKMODE mlBert_GetEye(mlbertapi* instance, int channel, double *xValues, double *yValues, double *berValues);

	else
		return false;

	return true;
}