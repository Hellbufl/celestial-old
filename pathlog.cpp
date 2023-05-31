#include "pathlog.h"

void LogPosition(std::fstream& file, Vector3& pos);
void CreateBoxTrigger(Vector3* pos, Vector3 size, Vector3* rot, BoxTrigger& destTrigger);
void CheckTriggers(PLogState& state, Vector3* playerPos);

void pathlog::Init(PLogState& state)
{
	printf("[PathLog] initializing...\n");

	state.primed = false;
	state.recording = false;
	state.triggerState[0] = 0;
	state.triggerState[1] = 0;

	state.outFileName = OUT_FILE_NAME;
	state.outFileType = OUT_FILE_TYPE;

	state.triggerSize.x = 1.0f;
	state.triggerSize.y = 1.0f;
	state.triggerSize.z = 1.0f;

	state.recordingFilename = state.outFileName + state.outFileType;
	state.currentFilePath = GetPathsDirectory() + state.recordingFilename;

	if (CreateDirectory(PATH_FILE_DIR, NULL) == ERROR_PATH_NOT_FOUND)
	{
		printf("[PathLog] ERROR: Paths directory not found");
		exit(1);
	}

	state.recordingPathFile.open(state.currentFilePath, std::ios::in);

	for (int i = 1; state.recordingPathFile; i++)
	{
		state.recordingPathFile.close();

		Path newPath;
		ReadPFile(state.currentFilePath, newPath);

		if (newPath.nodes.size() > 0)
			state.displayedPaths.push_back(newPath);

		state.recordingFilename = state.outFileName + "_" + std::to_string(i) + state.outFileType;
		state.currentFilePath = GetPathsDirectory() + state.recordingFilename;
		state.recordingPathFile.open(state.currentFilePath, std::ios::in);
	}

	printf("[PathLog] current recording file: %s\n", state.recordingFilename);
}

void pathlog::Update(PLogState& state, Vector3* playerPos, Vector3* playerRot)
{
	for (int i = 0; i < 2; i++)
	{
		if (state.triggerState[i] != 1) continue;
		
		CreateBoxTrigger(playerPos, state.triggerSize, playerRot, state.recordingTrigger[i]);
		state.triggerState[i] = 2;
	}

	// DEBUG \/
	state.debugLines.clear();
	bool prevInTrigger[2];
	prevInTrigger[0] = state.playerInTrigger[0];
	prevInTrigger[1] = state.playerInTrigger[1];
	// DEBUG /\

	CheckTriggers(state, playerPos);

	// DEBUG \/
	if (prevInTrigger[0] != state.playerInTrigger[0] || prevInTrigger[1] != state.playerInTrigger[1])
		printf("[PathLog] Triggers: (%d, %d) %d\n", state.playerInTrigger[0], state.playerInTrigger[1], state.primed);
	// DEBUG /\

	if (!state.triggerState[0] || !state.triggerState[1]) return;

	if (state.playerInTrigger[0] && !state.primed)
	{
		state.primed = true;
		ResetRecording(state);
	}
	
	if (!state.playerInTrigger[0] && state.primed)
	{
		state.primed = false;
		StartRecording(state);
	}

	if (state.playerInTrigger[1] && state.recording)
	{
		StopRecording(state);
	}

	if (playerPos && state.recording)
		LogPosition(state.recordingPathFile, *playerPos);
}

void pathlog::ReadPFile(std::string filepath, Path& destination)
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

	destination.time = *(long long*)(buffer + nodeCount * sizeof(Vector3));

	pathFile.close();

	printf("[PathLog] path of size %d read succussfully!\n", (int)destination.nodes.size());
}

void pathlog::StartRecording(PLogState& state)
{
	if (state.recording) return;
	state.recording = true;
	state.recordingPathFile.open(state.currentFilePath, std::ios::out | std::ios::binary);
	state.recordingStart = std::chrono::high_resolution_clock::now();
	printf("[PathLog] recording started...\n");
}

void pathlog::ResetRecording(PLogState& state)
{
	if (!state.recording) return;
	state.recording = false;
	state.recordingPathFile.close();
	printf("[PathLog] reset recording\n");
}

void pathlog::StopRecording(PLogState& state)
{
	if (!state.recording) return;

	state.recording = false;

	long long timeRecorded = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - state.recordingStart).count();
	printf("[PathLog] recorded time: %lld\n", timeRecorded);
	state.recordingPathFile.write((char*) &timeRecorded, sizeof(long long));

	state.recordingPathFile.close();
	
	int suffix;
	int suffixStart = state.outFileName.length() + 1;
	int suffixEnd = state.recordingFilename.length() - state.outFileType.length();

	if (suffixStart < suffixEnd)
		suffix = std::stoi(state.recordingFilename.substr(suffixStart, (long long) suffixEnd - suffixStart));
	else
		suffix = 0;

	printf("[PathLog] suffix = %d\n", suffix);

	Path newPath;
	ReadPFile(state.currentFilePath, newPath);

	if (newPath.nodes.size() > 0)
		state.displayedPaths.push_back(newPath);

	state.recordingFilename = state.outFileName + "_" + std::to_string(suffix + 1) + state.outFileType;

	printf("[PathLog] recording stopped.\n");
}

void LogPosition(std::fstream& file, Vector3& pos)
{
	file.write((char*)pos.v, sizeof(float) * 3);
}

void CreateBoxTrigger(Vector3* pos, Vector3 size, Vector3* rot, BoxTrigger& destTrigger)
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

		destTrigger.points[i] = *pos + destTrigger.inverseBasis * delta * size;
	}

	printf("[PathLog] trigger created successgully\n");
}

void CheckTriggers(PLogState& state, Vector3* playerPos)
{
	if (!playerPos) return;
	Vector3 relativePos;

	for (int i = 0; i < 2; i++)
	{
		if (!state.triggerState[i]) continue;

		relativePos = state.recordingTrigger[i].basis * (*playerPos - state.recordingTrigger[i].pos);
		state.playerInTrigger[i] = abs(relativePos.x) < state.triggerSize.x && abs(relativePos.y) < state.triggerSize.y && abs(relativePos.z) < state.triggerSize.z;
	}
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
	std::string PathsDirString = gameDir + slash + PATHS_FOLDER + slash;

	return PathsDirString;
}