#pragma once
#include <Windows.h>
#include <fstream>
#include <iostream>
#include <cstdio>
#include <vector>
#include <string>
#include <chrono>

#include "linalg.h"

#define PATHS_FOLDER "Paths"
#define PATH_FILE_DIR L"Paths"
#define OUT_FILE_NAME "pathlog"
#define OUT_FILE_TYPE ".p"

struct Path
{
	std::vector<Vector3> nodes;
	long long time;
};

struct BoxTrigger
{
	Vector3 pos;
	Matrix3 basis;
	Matrix3 inverseBasis;
	Vector3 points[8];
};

struct PLogState
{
	bool primed;
	bool recording;

	std::chrono::time_point<std::chrono::high_resolution_clock> recordingStart;

	std::string recordingFilename;
	std::string currentFilePath;
	std::string outFileName;
	std::string outFileType;

	std::fstream recordingPathFile;

	std::vector<Path> displayedPaths;
	std::vector<Path> comparedPaths;

	BoxTrigger recordingTrigger[2];
	Vector3 triggerSize;
	int triggerState[2];

	bool playerInTrigger[2];

	std::vector<Vector3> debugLines;
};

namespace pathlog
{
	void Init(PLogState& state);

	void Update(PLogState& state, Vector3* pos, Vector3* rot);

	void ReadPFile(std::string filename, Path& destination);

	void StartRecording(PLogState& state);
	void ResetRecording(PLogState& state);
	void StopRecording(PLogState& state);

	std::string GetPathsDirectory();
}