#include "mlbertapi.h"
#include "../lib/log/log.h"
#include "BertServer.h"

//These APIs are mentioned in the sample code but not in the header??
extern "C" __declspec(dllimport) double APIGetAPIRev();

using namespace std;

Socket g_scpiSocket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

mlbertapi* g_hBert;
int g_model;
uint64_t g_serial;

int main(int argc, char* argv[])
{
	//Global settings
	Severity console_verbosity = Severity::NOTICE;

	//Parse command-line arguments
	uint16_t scpi_port = 6025;//5025;
	for (int i = 1; i < argc; i++)
	{
		string s(argv[i]);

		//Let the logger eat its args first
		if (ParseLoggerArguments(i, argc, argv, console_verbosity))
			continue;

		if (s == "--help")
		{
			//help();
			return 0;
		}

		else if (s == "--scpi-port")
		{
			if (i + 1 < argc)
				scpi_port = atoi(argv[++i]);
		}

		else
		{
			fprintf(stderr, "Unrecognized command-line argument \"%s\", use --help\n", s.c_str());
			return 1;
		}
	}

	//Set up logging
	g_log_sinks.emplace(g_log_sinks.begin(), new ColoredSTDLogSink(console_verbosity));

	LogVerbose("Creating BERT API instance\n");
	g_hBert = mlBert_createInstance();
	if (!g_hBert)
	{
		LogError("Failed to create BERT API instance\n");
		return 1;
	}

	//this crashes, not sure why
	//auto rev = APIGetAPIRev();
	//LogVerbose("MLBert API rev %f\n", rev);

	//only IP is supported, not FQDN.
	//If we want to use DNS names we have to resolve that ourself
	LogVerbose("Connecting to BERT\n");
	char ipstring[128] = "10.2.4.14";
	if (!mlBert_Connect(g_hBert, ipstring))
	{
		LogError("Failed to connect to BERT\n");
		return 1;
	}

	//Figure out what the BERT is
	g_model = mlBert_ReadBoardID(g_hBert);
	int revision = 1;
	double fw = 0;
	if (!mlBert_ReadSerialNumber(g_hBert, &g_serial, &revision))
	{
		LogError("Failed to read serial\n");
		return 1;
	}
	if (!mlBert_ReadFirmwareRevision(g_hBert))
	{
		LogError("Failed to read FW\n");
		return 1;
	}
	LogVerbose("Successfully connected to ML%d S/N %016llx, running firmware %.1f\n",
		g_model, g_serial, (float) fw);

	/*
	//Read temperatures
	double temp0 = mlBert_TempRead(g_hBert, 0);
	double temp1 = mlBert_TempRead(g_hBert, 1);
	LogVerbose("Temperatures: TX=%.1f, RX=%.1f\n", temp0, temp1);

	//DEBUG: Set channel 0 to PRBS15
	if (!mlBert_SetPRBSPattern(g_hBert, 0, 2, 2, 0, 0))
		LogError("mlBert_SetPrbsPattern failed\n");
	*/

	//Launch the socket server
	g_scpiSocket.Bind(scpi_port);
	g_scpiSocket.Listen();

	LogVerbose("Listening for incoming connections on TCP port %d\n", (int)scpi_port);
	while (true)
	{
		Socket scpiClient = g_scpiSocket.Accept();
		if (!scpiClient.IsValid())
			break;

		//Create a server object for this connection
		BertServer server(scpiClient.Detach());
		server.MainLoop();
	}

	//Done, clean up
	mlBert_Disconnect(g_hBert);
	return 0;
}
