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
	Vector3 points[8];
};

class PathLog
{
public:

	void Init();

	void Update(Vector3* pos, Vector3* rot);

	void ReadPFile(std::string filename, Path& destination);

	void DrawPath(ImDrawList* drawList, ImVec2 screenSize, Matrix4* viewMatrix, ImColor color, float thickness);

	void KeyPress(WPARAM key);

	std::string GetPathsDirectory();

private:

	void StartRecording();
	void ResetRecording();
	void StopRecording();

	void LogPosition(Vector3& pos);

	void CreateBoxTrigger(Vector3* pos, Vector3* rot, BoxTrigger& trigger);
	void CheckTriggers(Vector3* playerPos);

	bool primed;
	bool recording;

	std::chrono::time_point<std::chrono::high_resolution_clock> recordingStart;

	std::string recordingFilename;
	std::string currentFilePath;
	std::string outFileName;
	std::string outFileType;

	std::fstream recordingPathFile;

	std::vector<Path> displayedPaths;

	BoxTrigger recordingTrigger[2];
	Vector3 triggerSize;
	int triggerState[2];

	bool playerInTrigger[2];

	std::vector<Vector3> debugLines;
};