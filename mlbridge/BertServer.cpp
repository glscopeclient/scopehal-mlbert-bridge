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
		m_dx[i] = 0;
		m_dy[i] = 0;
		m_precursor[i] = 0;
		m_postcursor[i] = 0;
		m_txEnable[i] = true;
		m_rxEnable[i] = true;
		m_txSwing[i] = 200;
		m_ctleStep[i] = 4;
	}

	m_integrationLength = (int64_t)1e7;
	m_dataRateGbps = 10.3125;
	m_useExternalRefclk = false;
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

			if(!m_deferring)
				RefreshChannelPolynomialAndInvert(chnum);
		}

		else if (cmd == "ENABLE")
		{
			bool en = false;
			if(args[0] == "1")
				en = true;
			m_txEnable[chnum] = en;

			if(!mlBert_TXEnable(g_hBert, chnum, en))
				LogError("Failed to set TX enable\n");
		}

		else if (cmd == "PRECURSOR")
		{
			m_precursor[chnum] = atoi(args[0].c_str());

			if(!mlBert_PreEmphasis(g_hBert, chnum, m_precursor[chnum]))
				LogError("Failed to set precursor tap\n");
		}

		else if (cmd == "POSTCURSOR")
		{
			m_postcursor[chnum] = atoi(args[0].c_str());

			if (!mlBert_PostEmphasis(g_hBert, chnum, m_postcursor[chnum]))
				LogError("Failed to set postcursor tap\n");
		}

		else if (cmd == "INVERT")
		{
			LogDebug("Setting invert flag for %s to %s\n", subject.c_str(), args[0].c_str());

			if (args[0] == "1")
				m_txInvert[chnum] = true;
			else
				m_txInvert[chnum] = false;

			if (!m_deferring)
				RefreshChannelPolynomialAndInvert(chnum);
		}

		//legal values for 4039 are 0, 100, 200, 300, 400
		else if (cmd == "SWING")
		{
			LogDebug("Setting swing for %s to %s\n", subject.c_str(), args[0].c_str());
			m_txSwing[chnum] = (float)atof(args[0].c_str());

			if(!mlBert_OutputLevel(g_hBert, chnum, m_txSwing[chnum]))
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

			if (!m_deferring)
				RefreshChannelPolynomialAndInvert(chnum);
		}

		else if (cmd == "SAMPLE")
		{
			//sampling point x (ps), y(V) in arg0 and arg1
			LogDebug("Setting sampling point for channel %s to (%s ps, %s mV)\n", subject.c_str(), args[0].c_str(), args[1].c_str());

			m_dx[chnum] = (int)atof(args[0].c_str());
			m_dy[chnum] = (int)atof(args[1].c_str());
		}
			
		else if (cmd == "CTLESTEP")
		{
			//CTLE gain step
			//note that the API is called DFE, and the popup dialog in the GUI calls it a DFE,
			//but all other signs point to it actually being a CTLE!
			//(datasheet and table header call it a CTLE, and it only has a single gain setting rather than per-tap cursor values)
			LogDebug("Setting CTLE step for channel %s to %s\n", subject.c_str(), args[0].c_str());
			auto step = atoi(args[0].c_str());

			m_ctleStep[chnum] = step;

			if (!mlBert_DFESetValue(g_hBert, chnum, step))
				LogError("Failed to set tap value\n");
		}

		else if (cmd == "INVERT")
		{
			LogDebug("Setting invert flag for %s to %s\n", subject.c_str(), args[0].c_str());

			if (args[0] == "1")
				m_rxInvert[chnum] = true;
			else
				m_rxInvert[chnum] = false;

			if (!m_deferring)
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
		else if (cmd == "REFCLK")
		{
			if(args[0] == "EXT")
				m_useExternalRefclk = true;
			else
				m_useExternalRefclk = false;

			RefreshTimebase();
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
			LogDebug("Applying deferred channel config\n");

			for (int i = 0; i < 4; i++)
				RefreshChannelPolynomialAndInvert(i);

			m_deferring = false;
		}
		else if (cmd == "INTEGRATION")
		{
			m_integrationLength = stoll(args[0].c_str());
		}
		else if (cmd == "RATE")
		{
			int64_t bps = stoll(args[0].c_str());
			m_dataRateGbps = bps * 1e-9;

			RefreshTimebase();
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
			"multiLane,ML%d,%016llx,0.0",
			g_model,
			g_serial);
		SendReply(tmp);
	}

	else if(cmd == "BER")
	{
		//Set sampling point
		for(int i=0; i<4; i++)
		{
			if(!mlBert_ChangeBERPhaseAndOffset_pS_mV(g_hBert, i, m_dx[i], m_dy[i]))
				LogError("Failed to set BER phase\n");
		}

		//Reset integration length for each measurement
		if(!mlBert_SetBERCounter(g_hBert, static_cast<uint32_t>(m_integrationLength / 25000L)))
			LogError("Failed to set BER counter\n");

		//Read all four channel BERs and report values
		double values[4] = {0};
		if(!mlBert_DoInstantBER(g_hBert, values))
			LogError("Failed to read BER\n");

		char tmp[256];
		snprintf(tmp, sizeof(tmp), "%e,%e,%e,%e", values[0], values[1], values[2], values[3]);
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

			else if(cmd == "EYE")
			{
				LogDebug("Eye pattern for channel %d\n", chnum);

				size_t paddedSize = 32768 + 1024;
				double* xvalues  = new double[paddedSize];
				double* yvalues = new double[paddedSize];
				double* bervalues = new double[paddedSize];

				if(!mlBert_GetEye(g_hBert, chnum, xvalues, yvalues, bervalues))
					LogError("Failed to get eye\n");

				//X and Y values are redundant so don't waste bandwidth sending them
				//All we really need are the point spacings
				//x[0] and x[1] give the x delta, in ps
				//y[128] and y[127] give the y delta, in mv (?)
				//we have 128 phases (x) and 256 adc codes (y)
				//assume centered at nominal decision point and since we're AC coupled, we don't care about offset
				//so report y as centered at zero
				double dx = xvalues[0] - xvalues[1];
				double dy = yvalues[128] - yvalues[127];

				//We don't really care about performance since the measurement takes quite a while.
				//Just spit it all out as ASCII for now.
				//It's gonna be a few hundred kB but whatever
				char tmp[128];
				string reply = to_string(dx) + "," + to_string(dy) + ",";
				for(int i=0; i<32768; i++)
				{
					snprintf(tmp, sizeof(tmp), "%e,", bervalues[i]);
					reply += tmp;
				}
				SendReply(reply);

				delete[] xvalues;
				delete[] yvalues;
				delete[] bervalues;
			}

			else if(cmd == "HBATHTUB")
			{
				LogDebug("Horizontal bathtub for channel %d\n", chnum);

				//Read a quick bathtub
				//Docs say we only need 128 entries but we get memory corruption unless we allocate a lot more space.
				//But we still get random failures with infinities, out of order points, etc
				/*
				double xvalues[2048] = {0};
				double berValues[2048] = {0};
				if(!mlBert_GetBathTub(g_hBert, chnum, xvalues, berValues))
					LogError("Failed to get bathtub\n");
				*/

				//Acquire an eye pattern then grab a slice across it to make the eye
				size_t paddedSize = 32768 + 1024;
				double* xvalues = new double[paddedSize];
				double* yvalues = new double[paddedSize];
				double* bervalues = new double[paddedSize];

				if (!mlBert_GetEye(g_hBert, chnum, xvalues, yvalues, bervalues))
					LogError("Failed to get eye\n");

				//Find the widest spot in the eye (seems to be slightly offset above the nominal midpoint ADC code)
				int nrow = 0;
				int opening = 0;
				for(int y=0; y<256; y++)
				{
					//find right edge with ber at 1e-4 or worse
					//(remember, indexing is right to left)
					double threshold = 1e-4;
					int left = 0;
					int right = 0;
					for(int x=63; x>=0; x--)
					{
						if(bervalues[y*128 + x] > threshold)
						{
							right = x+1;
							break;
						}
					}

					//repeat for left edge
					for (int x = 64; x < 128; x++)
					{
						if (bervalues[y * 128 + x] > threshold)
						{
							left = x-1;
							break;
						}
					}

					int width = left - right;
					if(width > opening)
					{
						opening = width;
						nrow = y;
					}

					//LogDebug("opening at y=%d is %d (left=%d right=%d)\n", y, width, left, right);
				}
				LogDebug("widest opening is %d (at %d)\n", opening, nrow);

				//eye is read right to left, so flip the row
				//Dual-Dirac jitter extrapolation in the SDK
				//sometimes corrupts stack so allocate extra space for it to scribble over
				double selxvalues[256] = {0};
				double selbervalues[256] = {0};
				for(int i=0; i<128; i++)
				{
					selxvalues[i] = xvalues[128*nrow + 127 - i];
					selbervalues[i] = bervalues[128 * nrow + 127 - i];
				}

				//we really need to replace this with something that works!
				//For now, skip the DD outright and just give raw samples
				//if(!mlBert_CalculateBathtubDualDirac(g_hBert, chnum, selxvalues, selbervalues))
				//	LogError("Failed to do dual dirac\n");

				char tmp[128];
				string reply;
				for(int i=0; i<128; i++)
				{
					snprintf(tmp, sizeof(tmp), "%f,%e,", (float)selxvalues[i], (float)selbervalues[i]);
					reply += tmp;
				}
				SendReply(reply);

				delete[] xvalues;
				delete[] yvalues;
				delete[] bervalues;
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

	else
		return false;

	return true;
}

void BertServer::RefreshTimebase()
{
	LogDebug("Setting line rate to %f Gbps with %s reference\n", m_dataRateGbps, m_useExternalRefclk ? "external" : "internal");

	// Line rate in Gbps, clock index 0=external, 1=internal
	if (!mlBert_LineRateConfiguration(g_hBert, m_dataRateGbps, m_useExternalRefclk ? 0 : 1))
		LogError("Failed to set line rate\n");

	//if in DEFER mode, we're doing initial startup
	//so don't restore config because we're about to reconfigure everything anyway
	if (!m_deferring)
	{
		LogDebug("Restoring config and refreshing channels\n");

		for (int i = 0; i < 4; i++)
		{
			RefreshChannelPolynomialAndInvert(i);

			if (!mlBert_PreEmphasis(g_hBert, i, m_precursor[i]))
				LogError("Failed to set precursor tap\n");
			if (!mlBert_PostEmphasis(g_hBert, i, m_postcursor[i]))
				LogError("Failed to set postcursor tap\n");
			if (!mlBert_TXEnable(g_hBert, i, m_txEnable[i]))
				LogError("Failed to set TX enable\n");
			if (!mlBert_RXEnable(g_hBert, i, m_rxEnable[i]))
				LogError("Failed to set RX enable\n");
			if (!mlBert_OutputLevel(g_hBert, i, m_txSwing[i]))
				LogError("Failed to set swing\n");
			if (!mlBert_DFESetValue(g_hBert, i, m_ctleStep[i]))
				LogError("Failed to set tap value\n");
		}
	}
}