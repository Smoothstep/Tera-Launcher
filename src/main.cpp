#include "patcher.h"
#include "launcher.h"
#include "log.h"

#ifndef _DEBUG
#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")
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

