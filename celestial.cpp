#include "celestial.h"

void Celestial::MainLoop()
{
	bool running = true;
	uint64_t processStartPtr = reinterpret_cast<uint64_t>(GetModuleHandle(nullptr));

	PathLog ppLog;
	ppLog.Init();

	Vector3* playerPos;
	Vector3* playerRot;
	
	while (running)
	{
		playerPos = GameData::GetPlayerPosition(processStartPtr);
		playerRot = GameData::GetPlayerRotation(processStartPtr);

		ppLog.Update(playerPos, playerRot);
	}
}