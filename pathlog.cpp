#include "pathlog.h"

void PathLog::Init()
{
	printf("[PathLog] initializing...\n");

	primed = false;
	recording = false;
	triggerState[0] = 0;
	triggerState[1] = 0;

	outFileName = OUT_FILE_NAME;
	outFileType = OUT_FILE_TYPE;

	triggerSize.x = 1.0f;
	triggerSize.y = 1.0f;
	triggerSize.z = 1.0f;

	recordingFilename = outFileName + outFileType;
	currentFilePath = GetPathsDirectory() + recordingFilename;

	if (CreateDirectory(PATH_FILE_DIR, NULL) == ERROR_PATH_NOT_FOUND)
	{
		printf("[PathLog] ERROR: Paths directory not found");
		exit(1);
	}

	recordingPathFile.open(currentFilePath, std::ios::in);

	for (int i = 1; recordingPathFile; i++)
	{
		recordingPathFile.close();

		Path newPath;
		ReadPFile(currentFilePath, newPath);

		if (newPath.nodes.size() > 0)
			displayedPaths.push_back(newPath);

		recordingFilename = outFileName + "_" + std::to_string(i) + outFileType;
		currentFilePath = GetPathsDirectory() + recordingFilename;
		recordingPathFile.open(currentFilePath, std::ios::in);
	}

	printf("[PathLog] current recording file: %s\n", recordingFilename);
}

void PathLog::Update(Vector3* playerPos, Vector3* playerRot)
{
	for (int i = 0; i < 2; i++)
	{
		if (triggerState[i] != 1) continue;
		
		CreateBoxTrigger(playerPos, playerRot, recordingTrigger[i]);
		triggerState[i] = 2;
	}


	// DEBUG \/
	debugLines.clear();
	//bool prevInTrigger[2];
	//prevInTrigger[0] = playerInTrigger[0];
	//prevInTrigger[1] = playerInTrigger[1];
	// DEBUG /\

	CheckTriggers(playerPos);
	
	// DEBUG \/
	//if (prevInTrigger[0] != playerInTrigger[0] || prevInTrigger[1] != playerInTrigger[1])
	//	printf("[PathLog] Triggers (%d, %d) %d\n", playerInTrigger[0], playerInTrigger[1], primed);
	// DEBUG /\

	if (!triggerState[0] || !triggerState[1]) return;

	if (playerInTrigger[0] && !primed)
	{
		primed = true;
		ResetRecording();
	}
	
	if (!playerInTrigger[0] && primed)
	{
		primed = false;
		StartRecording();
	}

	if (playerInTrigger[1] && recording)
	{
		StopRecording();
	}

	if (playerPos && recording)
		LogPosition(*playerPos);
}

void PathLog::LogPosition(Vector3& pos)
{
	recordingPathFile.write((char*)pos.v, sizeof(float) * 3);
}

void PathLog::CreateBoxTrigger(Vector3* pos, Vector3* rot, BoxTrigger& trigger)
{
	if (pos == nullptr) return;
	if (rot == nullptr) return;

	trigger.pos = *pos;

	trigger.basis = Matrix3(	Vector3(cos(rot->y), 0.0f, sin(rot->y)),
								Vector3(0.0f, 1.0f, 0.0f),
								Vector3(-sin(rot->y), 0.0f, cos(rot->y))	);

	Vector3 delta;

	for (int i = 0; i < 8; i++)
	{
		delta.x = (i & 1) * 2 - 1;
		delta.y = ((i >> 2) & 1) * 2 - 1;
		delta.z = ((i >> 1) & 1) * 2 - 1;

		trigger.points[i] = *pos + trigger.basis * delta * triggerSize;
	}

	printf("[PathLog] trigger created successgully\n");
}

void PathLog::CheckTriggers(Vector3* playerPos)
{
	if (!playerPos) return;
	Vector3 relativePos;

	for (int i = 0; i < 2; i++)
	{
		if (!triggerState[i]) continue;

		relativePos = recordingTrigger[i].basis * (*playerPos - recordingTrigger[i].pos);
		playerInTrigger[i] = abs(relativePos.x) < triggerSize.x && abs(relativePos.y) < triggerSize.y && abs(relativePos.z) < triggerSize.z;
	}
}

void PathLog::StartRecording()
{
	if (recording) return;
	recording = true;
	recordingPathFile.open(currentFilePath, std::ios::out | std::ios::binary);
	recordingStart = std::chrono::high_resolution_clock::now();
	printf("[PathLog] recording started...\n");
}

void PathLog::ResetRecording()
{
	if (!recording) return;
	recording = false;
	recordingPathFile.close();
	printf("[PathLog] reset recording\n");
}

void PathLog::StopRecording()
{
	if (!recording) return;

	recording = false;

	long long timeRecorded = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - recordingStart).count();
	printf("[PathLog] recorded time: %lld\n", timeRecorded);
	recordingPathFile.write((char*) &timeRecorded, sizeof(long long));

	recordingPathFile.close();
	
	int suffix;
	int suffixStart = outFileName.length() + 1;
	int suffixEnd = recordingFilename.length() - outFileType.length();

	if (suffixStart < suffixEnd)
		suffix = std::stoi(recordingFilename.substr(suffixStart, (long long) suffixEnd - suffixStart));
	else
		suffix = 0;

	printf("[PathLog] suffix = %d\n", suffix);

	Path newPath;
	ReadPFile(currentFilePath, newPath);

	if (newPath.nodes.size() > 0)
		displayedPaths.push_back(newPath);

	recordingFilename = outFileName + "_" + std::to_string(suffix + 1) + outFileType;

	printf("[PathLog] recording stopped.\n");
}

void PathLog::ReadPFile(std::string filepath, Path& destination)
{
	printf("[PathLog] reading path...\n");

	std::fstream pathFile(filepath, std::ios::in | std::ios::ate | std::ios::binary);

	if (!pathFile)
	{
		printf("[PathLog] ERROR: failed to open file\n");
		return;
	}

	std::streamsize fileSize = pathFile.tellg();
	pathFile.seekg(0, std::ios::beg);

	int nodeCount = fileSize / sizeof(Vector3);

	if (nodeCount == 0)
	{
		printf("[PathLog] ERROR: path of size 0 discarded\n");
		return;
	}

	destination.nodes.reserve(nodeCount);
	char* buffer = new char[fileSize];

	if (!pathFile.read(buffer, fileSize))
	{
		printf("[PathLog] ERROR: path read failed\n");
		return;
	}

	for (int i = 0; i < nodeCount; i++)
	{
		destination.nodes.push_back(*((Vector3*)buffer + i));
	}

	destination.time = *(long long*) (buffer + nodeCount * sizeof(Vector3));

	pathFile.close();

	printf("[PathLog] path of size %d read succussfully!\n", (int)destination.nodes.size());
}

void PathLog::DrawPath(ImDrawList* drawList, ImVec2 screenSize, Matrix4* viewMatrix, ImColor color, float thickness)
{
	Vector2 tempPoint[2];
	ImVec2 points[2];

	for (auto path : displayedPaths)
		for (size_t i = 0; i < path.nodes.size() - 1; i++)
		{
			DXWorldToScreen(viewMatrix, path.nodes[i], screenSize.x, screenSize.y, tempPoint[0]);
			DXWorldToScreen(viewMatrix, path.nodes[i+1], screenSize.x, screenSize.y, tempPoint[1]);
			points[0].x = tempPoint[0].x;
			points[0].y = tempPoint[0].y;
			points[1].x = tempPoint[1].x;
			points[1].y = tempPoint[1].y;

			drawList->AddLine(points[0], points[1], color, thickness);
		}

	Vector2 triggerBoxPoints[8];
	ImVec2 ppPoints[8];
	
	Vector2 debugPoints[2];
	ImVec2 debugLine[2];
	
	// DEBUG \/
	for (int i = 0; i + 1 < debugLines.size(); i += 2)
	{
		DXWorldToScreen(viewMatrix, debugLines[i], screenSize.x, screenSize.y, debugPoints[0]);
		DXWorldToScreen(viewMatrix, debugLines[i+1], screenSize.x, screenSize.y, debugPoints[1]);

		debugLine[0].x = debugPoints[0].x;
		debugLine[0].y = debugPoints[0].y;
		debugLine[1].x = debugPoints[1].x;
		debugLine[1].y = debugPoints[1].y;
		drawList->AddLine(debugLine[0], debugLine[1], color);
	}
	// DEBUG /\

	for (int t = 0; t < 2; t++)
	{
		if (!triggerState[t]) continue;

		for (int i = 0; i < 8; i++)
		{
			DXWorldToScreen(viewMatrix, recordingTrigger[t].points[i], screenSize.x, screenSize.y, triggerBoxPoints[i]);
			ppPoints[i].x = triggerBoxPoints[i].x;
			ppPoints[i].y = triggerBoxPoints[i].y;
		}

		drawList->AddLine(ppPoints[0], ppPoints[1], color);
		drawList->AddLine(ppPoints[0], ppPoints[2], color);
		drawList->AddLine(ppPoints[0], ppPoints[4], color);
		drawList->AddLine(ppPoints[1], ppPoints[3], color);
		drawList->AddLine(ppPoints[1], ppPoints[5], color);
		drawList->AddLine(ppPoints[2], ppPoints[3], color);
		drawList->AddLine(ppPoints[2], ppPoints[6], color);
		drawList->AddLine(ppPoints[3], ppPoints[7], color);
		drawList->AddLine(ppPoints[4], ppPoints[5], color);
		drawList->AddLine(ppPoints[4], ppPoints[6], color);
		drawList->AddLine(ppPoints[5], ppPoints[7], color);
		drawList->AddLine(ppPoints[6], ppPoints[7], color);
	}
}

std::string PathLog::GetPathsDirectory()
{
	char modPath[MAX_PATH];
	DWORD result = GetModuleFileNameA(nullptr, modPath, MAX_PATH);

	std::string exeDirString = std::string(modPath);
	size_t i = exeDirString.find_last_of('\\');

	char slash = '\\';
	if (i == std::string::npos)
	{
		i = exeDirString.find_last_of('/');
		slash = '/';
	}

	std::string gameDir = exeDirString.substr(0, i + 1);
	std::string PathsDirString = gameDir + slash + PATHS_FOLDER + slash;

	return PathsDirString;
}

void PathLog::KeyPress(WPARAM key)
{
	switch (key)
	{
	//case VK_INSERT:
	//	if (!recording)
	//		StartRecording();
	//	else
	//		StopRecording();
	//	break;
	case VK_OEM_COMMA:
		triggerState[0] = 1;
		break;
	
	case VK_OEM_PERIOD:
		triggerState[1] = 1;
		break;

	case VK_END:
		ResetRecording();
		break;

	case VK_NEXT:
		displayedPaths.clear();
		break;

	default:
		break;
	}
}