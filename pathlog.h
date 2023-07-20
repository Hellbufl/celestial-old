#pragma once
#include <Windows.h>
#include <fstream>
#include <iostream>
#include <cstdio>
#include <vector>
#include <string>
#include <chrono>

#include "linalg.h"

#define MAX_PATH_NODES 9000
#define PATHS_FOLDER "Paths"
#define PATH_FILE_DIR L"Paths"
#define PATH_FILE_NAME "pathlog"
#define PATH_FILE_TYPE ".p"
#define PATH_FILE_OFFSET 16
#define COMP_FILE_NAME "pathcomparison"
#define COMP_FILE_TYPE ".pcomp"

struct Path
{
	std::vector<Vector3> nodes;
	uint64_t time;
	uint64_t id;
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

	Path recordingPath;

	std::vector<Path> displayedPaths;
	std::vector<Path> comparedPaths;

	std::string currentCompFileName;
	uint64_t currentPathID;

	BoxTrigger recordingTrigger[2];
	Vector3 triggerSize[2];
	int triggerState[2];

	bool playerInTrigger[2];

	std::vector<Vector3> debugLines;
};

namespace pathlog
{
	void Init(PLogState& state);

	void Update(PLogState& state, Vector3* pos, Vector3* rot);

	void ReadPathFile(std::string fileName, uint64_t& pathID, std::vector<Path>& destination);
	void WritePathFile(std::string fileName, std::string fileType, Path& source);
	void ReadCompFile(PLogState& state, std::string fileName);
	void CreateCompFile(PLogState& state);

	void StartRecording(PLogState& state);
	void ResetRecording(PLogState& state);
	void StopRecording(PLogState& state, bool direct);

	void InsertRecording(PLogState& state, Path& newPath);

	void DestroyTriggers(PLogState& state);

	std::string GetPathsDirectory();
}