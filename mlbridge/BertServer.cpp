#include "BertServer.h"
#include "mlbertapi.h"
#include "../lib/log/log.h"

using namespace std;

extern mlbertapi* g_hBert;
extern int g_model;
extern uint64_t g_serial;

BertServer::BertServer(ZSOCKET sock)
	: SCPIServer(sock)
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

		else if (cmd == "PATTERN")
		{
			//16 bit length in low rate mode
			//40 bit length in high rate mode
			//APICALL int  STACKMODE mlBert_SetTxUserPattern(mlbertapi* instance, int channel, unsigned long long UserDefinedPattern);
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
			int ch = 0;
			int idx = 0;

			if (args[0] == "LO")
			{
				//use local oscillator as clock
				ch = 0;
				idx = 0;

				//TODO: we can apparently divide this by 2/4/8/16
				//APICALL int  STACKMODE mlBert_TXClockOut_RateOverEight(mlbertapi* instance);
				//but how do we get other rates??
				LogDebug("Setting refclk out mux to LO\n");
			}

			else
			{
				int div = 0;
				sscanf(args[0].c_str(), "RX%d_DIV%d\n", &ch, &div);
				ch--;
				if (div == 8)
					idx = 1;
				else
					idx = 2;

				LogDebug("Setting refclk out mux to RX channel %d, rate/%d\n", ch+1, div);
			}

			if (1 != mlBert_ClockOut(g_hBert, ch, idx))
				LogError("failed to set clock out mux\n");
		}
		else if (cmd == "RATE")
		{
			//TODO
			// Line rate in Gbps, clock index 0=external, 1=internal
			//APICALL int  STACKMODE mlBert_LineRateConfiguration(mlbertapi* instance, double lineRate, int clockIndex);
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
	if (cmd == "*IDN")
	{
		char tmp[256];
		snprintf(tmp, sizeof(tmp),
			"MultiLANE,ML%d,%016llx,0.0",
			g_model,
			g_serial);
		SendReply(tmp);
	}

	//APICALL int  STACKMODE mlBert_GetEye(mlbertapi* instance, int channel, double *xValues, double *yValues, double *berValues);

	else
		return false;

	return true;
}