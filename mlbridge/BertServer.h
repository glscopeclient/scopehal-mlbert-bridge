#pragma once

#include "../lib/scpi-server-tools/SCPIServer.h"
#include "../lib/xptools/Socket.h"

class BertServer : public SCPIServer
{
public:
	BertServer(ZSOCKET sock);
	virtual ~BertServer();

protected:
	virtual bool OnCommand(
		const std::string& line,
		const std::string& subject,
		const std::string& cmd,
		const std::vector<std::string>& args) override;

	virtual bool OnQuery(
		const std::string& line,
		const std::string& subject,
		const std::string& cmd) override;

	void RefreshChannelPolynomialAndInvert(int chnum);

	int m_txPoly[4];
	int m_rxPoly[4];
	bool m_txInvert[4];
	bool m_rxInvert[4];
	int m_dx[4];
	int m_dy[4];
	int64_t m_integrationLength;

	bool m_deferring;
};