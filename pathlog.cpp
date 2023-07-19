#include "pathlog.h"

//void LogPosition(std::fstream& file, Vector3& pos);
void CreateBoxTrigger(Vector3* pos, Vector3* rot, Vector3 size, BoxTrigger& destTrigger);
void CheckTriggers(PLogState& state, Vector3* playerPos);

void pathlog::Init(PLogState& state)
{
	printf("[PathLog] initializing...\n");

	state.primed = false;
	state.recording = false;
	state.triggerState[0] = 0;
	state.triggerState[1] = 0;

	state.triggerSize.x = 1.0f;
	state.triggerSize.y = 1.0f;
	state.triggerSize.z = 1.0f;

	state.displayedPaths = {};
	state.comparedPaths = {};

	if (CreateDirectory(PATH_FILE_DIR, NULL) == ERROR_PATH_NOT_FOUND)
	{
		printf("[PathLog] ERROR: Paths directory not found");
		exit(1);
	}
	
	// debug
	//ReadPathFile(pathlog::GetPathsDirectory() + "pathlog.p", state.displayedPaths);
}

void pathlog::Update(PLogState& state, Vector3* playerPos, Vector3* playerRot)
{
	for (int i = 0; i < 2; i++)
	{
		if (state.triggerState[i] != 1) continue;

		if (state.comparedPaths.empty())
			CreateBoxTrigger(playerPos, playerRot, state.triggerSize, state.recordingTrigger[i]);

		state.triggerState[i] = 2;
	}

	// DEBUG
	//state.debugLines.clear();
	//bool prevInTrigger[2];
	//prevInTrigger[0] = state.playerInTrigger[0];
	//prevInTrigger[1] = state.playerInTrigger[1];
	// DEBUG

	CheckTriggers(state, playerPos);

	// DEBUG
	//if (prevInTrigger[0] != state.playerInTrigger[0] || prevInTrigger[1] != state.playerInTrigger[1])
	//	printf("[PathLog] Triggers: (%d, %d) %d\n", state.playerInTrigger[0], state.playerInTrigger[1], state.primed);
	// DEBUG

	if (state.triggerState[0] && state.triggerState[1])
	{

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
			StopRecording(state, false);
		}
	}

	if (!playerPos) return;
	if (!state.recording) return;

	state.recordingPath.nodes.push_back(*playerPos);
}

void pathlog::ReadPathFile(std::string filepath, std::vector<Path>& destination)
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

	//int nodeCount = (fileSize - PATH_FILE_OFFSET) / sizeof(Vector3);

	
	char* buffer = new char[fileSize];

	if (!pathFile.read(buffer, fileSize))
	{
		printf("[PathLog] ERROR: path read failed\n");
		return;
	}

	if (*((uint32_t*)buffer) != 0x48544150) // 50 41 54 48 = "PATH"
	{
		printf("[PathLog] ERROR: This is not a path file:\n");
		printf("%s\n", filepath.c_str());
		return;
	}

	uint32_t nodeCount = *((uint32_t*) buffer + 1);

	if (nodeCount < 2)
	{
		printf("[PathLog] ERROR: empty path discarded:\n");
		printf("%s\n", filepath.c_str());
		return;
	}

	Path newPath;
	newPath.nodes.reserve(nodeCount);

	for (int i = 0; i < nodeCount; i++)
	{
		newPath.nodes.push_back(*((Vector3*)(buffer + PATH_FILE_OFFSET) + i));
	}

	newPath.time = *(uint64_t*) (buffer + 4);

	destination.push_back(newPath);

	pathFile.close();

	printf("[PathLog] path of size %d read succussfully!\n", (int)newPath.nodes.size());
}

void ReadCompFile(PLogState& state, std::string filename)
{

}

void pathlog::StartRecording(PLogState& state)
{
	if (state.recording) return;
	state.recording = true;
	state.recordingStart = std::chrono::high_resolution_clock::now();
	printf("[PathLog] recording started...\n");
}

void pathlog::ResetRecording(PLogState& state)
{
	if (!state.recording) return;
	state.recording = false;
	printf("[PathLog] reset recording\n");
}

void pathlog::StopRecording(PLogState& state, bool direct)
{
	if (!state.recording) return;

	state.recording = false;

	uint64_t timeRecorded = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - state.recordingStart).count();
	printf("[PathLog] recorded time: %lld\n", timeRecorded);

	std::fstream newPathFile;
	std::string recordingFilename;
	std::string currentFilePath;
	std::string outFileName;
	std::string outFileType;

	if (direct)
	{
		outFileName = PATH_FILE_NAME;
		outFileType = PATH_FILE_TYPE;
	}
	else
	{
		outFileName = COMP_FILE_NAME;
		outFileType = COMP_FILE_TYPE;
	}
	
	recordingFilename = outFileName + outFileType;
	currentFilePath = GetPathsDirectory() + recordingFilename;

	newPathFile.open(currentFilePath, std::ios::in);

	int suffix = 0;
	for (suffix = 1; newPathFile; suffix++)
	{
		newPathFile.close();

		recordingFilename = outFileName + "_" + std::to_string(suffix) + outFileType;
		currentFilePath = GetPathsDirectory() + recordingFilename;
		newPathFile.open(currentFilePath, std::ios::in);
	}

	if (!direct)
	{
		suffix--;
		recordingFilename = outFileName + "_" + std::to_string(suffix) + outFileType;
		currentFilePath = GetPathsDirectory() + recordingFilename;
	}

	newPathFile.open(currentFilePath, std::ios::out | std::ios::binary);

	if (!newPathFile)
	{
		printf("[PathLog] ERROR: path write failed\n");
		return;
	}

	uint32_t nodeCount = state.recordingPath.nodes.size();

	newPathFile.write("PATH", sizeof(char) * 4);
	newPathFile.write((char*) &nodeCount, sizeof(uint32_t));
	newPathFile.write((char*) &timeRecorded, sizeof(uint64_t));

	//for (int i = 0; i < state.recordingPath.nodes.size(); i++)
	newPathFile.write((char*) state.recordingPath.nodes.data(), sizeof(Vector3) * nodeCount);

	newPathFile.close();

	state.recordingPath.time = timeRecorded;

	if (direct)
		state.displayedPaths.push_back(state.recordingPath);
	else
		InsertRecording(state, state.recordingPath);

	state.recordingPath.nodes.clear();
	state.recordingPath.time = 0;

	printf("[PathLog] saved path of size: %lld\n", nodeCount);
	printf("[PathLog] recording stopped.\n");
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
	//destTrigger.pos.y += size.y;

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
	std::string PathsDirString = gameDir + slash + PATHS_FOLDER + slash;

	return PathsDirString;
}