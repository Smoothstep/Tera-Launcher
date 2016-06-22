#include "patcher.h"
#include "launcher.h"
#include "log.h"
#include "parser.h"

#ifndef _DEBUG
#ifndef _CONSOLE
#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")
#endif
#endif

int main(int argc, char** argv)
{
	if (!CreateLauncher())
	{
		TRACEN("Error on Initialization.");
		return -1;
	}

	RunLauncher();

	TRACEN("Launcher thread ended - Shutting down.");

	return 1;
}

