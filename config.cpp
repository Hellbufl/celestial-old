#include "config.h"

void config::Init(ConfigState& state)
{
	state.showUI = 1;
	state.showPLOG = 0;
	state.directMode = 0;
	state.toggleWindowKeybind = VK_PRIOR;
	state.startKeybind = VK_OEM_COMMA;
	state.stopKeybind = VK_OEM_PERIOD;
	state.resetKeybind = VK_END;
	state.clearKeybind = VK_NEXT;
}

char* config::ReadConfig(ConfigState& state)
{
	std::fstream file;
	file.open(GetIniFilePath(), std::fstream::in);

	char result[128];

	if (!file.is_open())
	{
		sprintf_s(result, "[Celestial] Could not load config file: %s\n", GetIniFilePath().c_str());
		printf(result);
		file.close();
		return result;
	}

	ReadFromIni(file, "showUI", state.showUI);
	ReadFromIni(file, "directMode", state.directMode);
	ReadFromIni(file, "toggleWindowKeybind", state.toggleWindowKeybind);
	ReadFromIni(file, "startKeybind", state.startKeybind);
	ReadFromIni(file, "stopKeybind", state.stopKeybind);
	ReadFromIni(file, "resetKeybind", state.resetKeybind);
	ReadFromIni(file, "clearKeybind", state.clearKeybind);
	ReadFromIni(file, "secret", state.showPLOG);

	file.close();
	sprintf_s(result, "[Celestial] Config Loaded: %s\n", GetIniFilePath().c_str());
	printf(result);

	return result;
}

char* config::WriteConfig(ConfigState& state)
{
	std::fstream file;
	file.open(GetIniFilePath(), std::fstream::out);

	char result[128];

	if (!file.is_open())
	{
		sprintf_s(result, "[Celestial] Could not save config file: %s\n", GetIniFilePath().c_str());
		printf(result);
		file.close();
		return result;
	}

	WriteToIni(file, "showUI", state.showUI);
	WriteToIni(file, "directMode", state.directMode);
	WriteToIni(file, "toggleWindowKeybind", state.toggleWindowKeybind);
	WriteToIni(file, "startKeybind", state.startKeybind);
	WriteToIni(file, "stopKeybind", state.stopKeybind);
	WriteToIni(file, "resetKeybind", state.resetKeybind);
	WriteToIni(file, "clearKeybind", state.clearKeybind);
	WriteToIni(file, "secret", state.showPLOG);

	file.close();
	sprintf_s(result, "[Celestial] Config Saved: %s\n", GetIniFilePath().c_str());
	printf(result);

	return result;
}

void config::ReadFromIni(std::fstream& file, std::string varName, uint32_t& variable)
{
	std::string var;
	std::string val;

	file.clear();
	file.seekg(0);
	while (file >> var >> val)
	{
		if (var != varName) continue;

		variable = std::stoi(val);
		return;
	}
}

void config::WriteToIni(std::fstream& file, std::string varName, uint32_t& value)
{
	file << varName << " " << std::to_string(value) << std::endl;
}

std::string config::GetIniFilePath()
{
	char modPath[MAX_PATH];
	DWORD result = GetModuleFileNameA(nullptr, modPath, MAX_PATH);

	std::string iniString = std::string(modPath);
	size_t i = iniString.find_last_of("/\\");
	std::string iniDir = iniString.substr(0, i + 1);
	std::string iniPathString = iniDir + CONFIG_FILE_NAME;

	std::fstream fs;
	fs.open(iniPathString, std::fstream::in | std::fstream::out);
	fs.close();

	return iniPathString;
}