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

bool BertServer::OnCommand(
	const string& line,
	const string& subject,
	const string& cmd,
	const vector<string>& args)
{
	//If we have a subject, get the channel number
	int chnum = 0;
	if (subject != "")
		chnum = atoi(subject.substr(1).c_str()) - 1;
	if (chnum < 0)
		chnum = 0;
	if (chnum >= 3)
		chnum = 3;

	//See what the command was
	if (cmd == "RXPOLY")
	{
		LogDebug("Setting RX polynomial for channel %d to %s\n", chnum, args[0].c_str());

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
	else if (cmd == "RXINVERT")
	{
		LogDebug("Setting RX invert flag for channel %d to %s\n", chnum, args[0].c_str());

		if (args[0] == "1")
			m_rxInvert[chnum] = true;
		else
			m_rxInvert[chnum] = false;

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
	else if (cmd == "TXPOLY")
	{
		LogDebug("Setting polynomial for channel %d to %s\n", chnum, args[0].c_str());

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
	else if (cmd == "TXINVERT")
	{
		LogDebug("Setting TX invert flag for channel %d to %s\n", chnum, args[0].c_str());

		if (args[0] == "1")
			m_txInvert[chnum] = true;
		else
			m_txInvert[chnum] = false;

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

	else
	{
		LogWarning("Unrecognized command %s\n", cmd.c_str());
		return false;
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

	else
		return false;

	return true;
}