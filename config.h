#pragma once
#include <Windows.h>
#include <cstdint>
#include <fstream>
#include <string>

#define CONFIG_FILE_NAME "PLog.ini"

struct ConfigState
{
	uint32_t showUI;
	uint32_t showPLOG;
	uint32_t directMode;
	uint32_t toggleWindowKeybind;
	uint32_t startKeybind;
	uint32_t stopKeybind;
	uint32_t resetKeybind;
	uint32_t clearKeybind;
};

namespace config
{
	void Init(ConfigState& state);
	char* ReadConfig(ConfigState& state);
	char* WriteConfig(ConfigState& state);

	void ReadFromIni(std::fstream& file, std::string varName, uint32_t& variable);
	void WriteToIni(std::fstream& file, std::string varName, uint32_t& value);
	std::string GetIniFilePath();
}