#include "patcher.h"
#include "launcher.h"

#ifndef _DEBUG
#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")
#endif

int main(int argc, char** argv)
{
	if (!CreateLauncher())
	{
		fprintf(stderr, "Error on Initialization.");
		return -1;
	}

	RunLauncher();

	return 1;
}

