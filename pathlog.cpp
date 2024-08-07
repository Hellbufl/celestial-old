#include "pathlog.h"

void CreateBoxTrigger(Vector3* pos, Vector3* rot, Vector3 size, BoxTrigger& destTrigger);
void CheckTriggers(PLogState& state, Vector3* playerPos);

void pathlog::Init(PLogState& state)
{
	state.primed = false;
	state.recording = false;
	state.triggerState[0] = 0;
	state.triggerState[1] = 0;

	state.triggerSize[0].x = 1.0f;
	state.triggerSize[0].y = 1.0f;
	state.triggerSize[0].z = 1.0f;
	state.triggerSize[1].x = 1.0f;
	state.triggerSize[1].y = 1.0f;
	state.triggerSize[1].z = 1.0f;

	state.displayedPaths = {};
	state.comparedPaths = {};

	state.currentPathID = std::time(nullptr) * std::time(nullptr);

	if (CreateDirectory(PATH_FILE_DIR, NULL) == ERROR_PATH_NOT_FOUND)
	{
		printf("[PathLog] ERROR: Paths directory not found!");
		exit(1);
	}

	printf("[PathLog] Initialized.\n");
}

void pathlog::Update(PLogState& state, Vector3* playerPos, Vector3* playerRot)
{
	if (playerPos == nullptr) return;

	Vector3 localPlayerPos;

	localPlayerPos.x = playerPos->x;
	localPlayerPos.y = playerPos->y + 1.0f;
	localPlayerPos.z = playerPos->z;

	for (int i = 0; i < 2; i++)
	{
		if (state.triggerState[i] != 1) continue;

		if (state.comparedPaths.empty())
		{
			CreateBoxTrigger(&localPlayerPos, playerRot, state.triggerSize[i], state.recordingTrigger[i]);
			state.currentCompFilePath = "";
		}

		state.triggerState[i] = 2;
	}

	CheckTriggers(state, &localPlayerPos);

	if (state.triggerState[0] && state.triggerState[1])
	{
		if (state.playerInTrigger[0] && !state.primed)
		{
			ResetRecording(state);
			state.primed = true;
		}
		else if (!state.playerInTrigger[0] && state.primed)
		{
			state.primed = false;
			StartRecording(state);
		}

		
		if (state.playerInTrigger[1] && state.recording)
		{
			StopRecording(state);
		}
	}

	if (!state.recording) return;

	if (state.recordingPath.nodes.size() >= MAX_PATH_NODES)
	{
		StopRecording(state);
		printf("[PathLog] ERROR: Exceeded maximum path size!\n");
		return;
	}

	state.recordingPath.nodes.push_back(localPlayerPos);
}

void pathlog::ReadPathFile(std::string filePath, uint64_t& pathID, std::vector<Path>& destination)
{
	//printf("[PathLog] reading path...\n");

	std::fstream pathFile(filePath, std::ios::in | std::ios::ate | std::ios::binary);

	if (!pathFile)
	{
		printf("[PathLog] ERROR: Failed to open file!\n");
		return;
	}

	std::streamsize fileSize = pathFile.tellg();
	pathFile.seekg(0, std::ios::beg);

	char* buffer = new char[fileSize];
	int buffIndex = 0;

	if (!pathFile.read(buffer, fileSize))
	{
		printf("[PathLog] ERROR: Path read failed!\n");
		return;
	}

	if (*((uint32_t*)buffer) != 0x48544150) // 50 41 54 48 = "PATH"
	{
		printf("[PathLog] ERROR: This is not a path file:\n");
		printf("%s\n", filePath.c_str());
		return;
	}

	buffIndex += 4;

	uint32_t nodeCount = *((uint32_t*)(buffer + buffIndex));

	if (nodeCount < 2)
	{
		printf("[PathLog] ERROR: Empty path discarded:\n");
		printf("%s\n", filePath.c_str());
		return;
	}

	buffIndex += 4;

	Path newPath;
	newPath.nodes.reserve(nodeCount);

	newPath.time = *(uint64_t*)(buffer + buffIndex);
	newPath.id = pathID;

	for (int i = 0; i < nodeCount; i++)
	{
		newPath.nodes.push_back(*((Vector3*)(buffer + PATH_FILE_OFFSET) + i));
	}

	destination.push_back(newPath);

	pathFile.close();

	printf("[PathLog] Read path of size %d.\n", (int)newPath.nodes.size());
}

void pathlog::WritePathFile(std::string filePath, Path &source)
{
	std::fstream newPathFile;

	std::string fileName = filePath.substr(0, filePath.find_last_of('.'));
	std::string fileType = filePath.substr(filePath.find_last_of('.'));

	if (fileType == PATH_FILE_TYPE)
	{
		newPathFile.open(filePath, std::ios::in);

		int suffix = 0;
		for (suffix = 1; newPathFile; suffix++)
		{
			newPathFile.close();

			filePath = fileName + "_" + std::to_string(suffix) + fileType;
			newPathFile.open(filePath, std::ios::in);
		}
	}

	newPathFile.open(filePath, std::ios::out | std::ios::binary | std::ios::app);
	
	if (!newPathFile)
	{
		printf("[PathLog] ERROR: Path write failed!\n");
		return;
	}

	uint32_t nodeCount = source.nodes.size();

	newPathFile.write("PATH", sizeof(char) * 4);
	newPathFile.write((char*)&nodeCount, sizeof(uint32_t));
	newPathFile.write((char*)&source.time, sizeof(uint64_t));

	newPathFile.write((char*)source.nodes.data(), sizeof(Vector3) * nodeCount);

	newPathFile.close();
	printf("[PathLog] Saved path of size %lld.\n", nodeCount);
}

void pathlog::ReadCompFile(PLogState& state, std::string filePath)
{
	//printf("[PathLog] Reading comparison...\n");

	state.comparedPaths.clear();

	std::fstream compFile(filePath, std::ios::in | std::ios::ate | std::ios::binary);

	if (!compFile)
	{
		printf("[PathLog] ERROR: Failed to open file!\n");
		return;
	}

	std::streamsize fileSize = compFile.tellg();
	compFile.seekg(0, std::ios::beg);

	char* buffer = new char[fileSize];
	int buffIndex = 0;

	if (!compFile.read(buffer, fileSize))
	{
		printf("[PathLog] ERROR: Path read failed!\n");
		return;
	}
	compFile.close();

	if (*((uint32_t*)buffer) != 0x504D4F43) // 43 4F 4D 50 = "COMP"
	{
		printf("[PathLog] ERROR: This is not a comparison file:\n");
		printf("%s\n", filePath.c_str());
		return;
	}

	buffIndex += 4;
	state.recordingTrigger[0] = ((BoxTrigger*)(buffer + buffIndex))[0];
	state.recordingTrigger[1] = ((BoxTrigger*)(buffer + buffIndex))[1];

	buffIndex += sizeof(BoxTrigger) * 2;
	state.triggerSize[0] = ((Vector3*)(buffer + buffIndex))[0];
	state.triggerSize[1] = ((Vector3*)(buffer + buffIndex))[1];

	state.triggerState[0] = 2;
	state.triggerState[1] = 2;

	buffIndex += sizeof(Vector3) * 2;

	while (buffIndex < fileSize)
	{
		if (*((uint32_t*)(buffer + buffIndex)) != 0x48544150) // 50 41 54 48 = "PATH"
		{
			printf("[PathLog] ERROR: This file is corrupted:\n");
			printf("%s\n", filePath.c_str());
			return;
		}

		buffIndex += 4;

		uint32_t nodeCount = *((uint32_t*)(buffer + buffIndex));

		if (nodeCount < 2)
		{
			printf("[PathLog] ERROR: Empty path discarded:\n");
			printf("%s\n", filePath.c_str());
			return;
		}

		Path newPath;
		newPath.nodes.reserve(nodeCount);

		buffIndex += 4;

		newPath.time = *(uint64_t*)(buffer + buffIndex);
		newPath.id = ++state.currentPathID;

		buffIndex += 8;

		for (int i = 0; i < nodeCount; i++)
		{
			newPath.nodes.push_back(*((Vector3*)(buffer + buffIndex)));
			buffIndex += sizeof(Vector3);
		}

		InsertRecording(state, newPath);
	}

	state.currentCompFilePath = filePath;

	printf("[PathLog] Read comparison with %d paths.\n", state.comparedPaths.size());
}

void pathlog::CreateCompFile(PLogState& state)
{
	std::fstream newCompFile;
	std::string filePath;
	std::string fileName = COMP_FILE_NAME;
	std::string fileType = COMP_FILE_TYPE;

	filePath = GetPathsDirectory() + fileName + fileType;
	newCompFile.open(filePath, std::ios::in);

	int suffix = 0;
	for (suffix = 1; newCompFile; suffix++)
	{
		newCompFile.close();

		filePath = GetPathsDirectory() + fileName + "_" + std::to_string(suffix) + fileType;
		newCompFile.open(filePath, std::ios::in);
	}

	newCompFile.open(filePath, std::ios::out | std::ios::binary | std::ios::app);

	if (!newCompFile)
	{
		printf("[PathLog] ERROR: Path write failed!\n");
		return;
	}

	newCompFile.write("COMP", sizeof(char) * 4);
	newCompFile.write((char*) state.recordingTrigger, sizeof(BoxTrigger) * 2);
	newCompFile.write((char*) state.triggerSize, sizeof(Vector3) * 2);

	newCompFile.close();

	state.currentCompFilePath = filePath;
}

void pathlog::StartRecording(PLogState& state)
{
	if (state.recording) return;
	state.recording = true;
	state.recordingStart = std::chrono::high_resolution_clock::now();
	printf("[PathLog] Recording started.\n");
}

void pathlog::ResetRecording(PLogState& state)
{
	if (!state.recording) return;
	state.recording = false;
	state.recordingPath.nodes.clear();
	state.recordingPath.time = 0;
	printf("[PathLog] Recording reset.\n");
}

void pathlog::StopRecording(PLogState& state)
{
	if (!state.recording) return;

	state.recording = false;

	uint64_t timeRecorded = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - state.recordingStart).count();
	
	state.latestTime = timeRecorded;
	state.recordingPath.time = timeRecorded;
	state.recordingPath.id = ++state.currentPathID;

	if (state.direct)
	{
		WritePathFile(GetPathsDirectory() + PATH_FILE_NAME + PATH_FILE_TYPE, state.recordingPath);
		state.displayedPaths.push_back(state.recordingPath);
	}
	else
	{
		if (state.currentCompFilePath == "")
			CreateCompFile(state);

		WritePathFile(state.currentCompFilePath, state.recordingPath);
		InsertRecording(state, state.recordingPath);
	}

	state.recordingPath.nodes.clear();
	state.recordingPath.time = 0;

	printf("[PathLog] Recording stopped.\n");
}

// inserts a new path into the comparison, keeping the array sorted
void pathlog::InsertRecording(PLogState &state, Path &newPath)
{
	state.comparedPaths.push_back(newPath);

	for (int i = state.comparedPaths.size() - 1; i > 0; i--)
	{
		if (state.comparedPaths[i - 1].time < newPath.time) return;
		
		state.comparedPaths[i] = state.comparedPaths[i - 1];
		state.comparedPaths[i - 1] = newPath;
	}
}


void CreateBoxTrigger(Vector3* pos, Vector3* rot, Vector3 size, BoxTrigger& destTrigger)
{
	if (pos == nullptr) return;
	if (rot == nullptr) return;

	destTrigger.pos = *pos;

	destTrigger.basis = Matrix3(	Vector3(cos(rot->y), 0.0f, sin(rot->y)),
									Vector3(0.0f, 1.0f, 0.0f),
									Vector3(-sin(rot->y), 0.0f, cos(rot->y))	);

	destTrigger.inverseBasis = Matrix3(		Vector3(cos(rot->y), 0.0f, -sin(rot->y)),
											Vector3(0.0f, 1.0f, 0.0f),
											Vector3(sin(rot->y), 0.0f, cos(rot->y))	);

	Vector3 delta;

	for (int i = 0; i < 8; i++)
	{
		delta.x = (i & 1) * 2 - 1;
		delta.y = ((i >> 2) & 1) * 2 - 1;
		delta.z = ((i >> 1) & 1) * 2 - 1;

		destTrigger.points[i] = destTrigger.pos + destTrigger.inverseBasis * delta * size;
	}

	printf("[PathLog] Created trigger.\n");
}

void CheckTriggers(PLogState& state, Vector3* playerPos)
{
	if (!playerPos) return;
	Vector3 relativePos;

	for (int i = 0; i < 2; i++)
	{
		if (!state.triggerState[i]) continue;

		relativePos = state.recordingTrigger[i].basis * (*playerPos - state.recordingTrigger[i].pos);
		state.playerInTrigger[i] = abs(relativePos.x) < state.triggerSize[i].x && abs(relativePos.y) < state.triggerSize[i].y && abs(relativePos.z) < state.triggerSize[i].z;
	}
}

void pathlog::DestroyTriggers(PLogState& state)
{
	state.triggerState[0] = 0;
	state.triggerState[1] = 0;
}

std::string pathlog::GetPathsDirectory()
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
	std::string PathsDirString = gameDir + PATHS_FOLDER + slash;

	return PathsDirString;
}