#include "gamedata.h"

Matrix4* GameData::GetViewMatrix(uint64_t processStart)
{
	uint64_t* vMatrixPtr = reinterpret_cast<uint64_t*>(processStart + NA_VMATRIX_OFF);
	if (!vMatrixPtr) return nullptr;
	return reinterpret_cast<Matrix4*>(vMatrixPtr);
}

Vector3* GameData::GetPlayerPosition(uint64_t processStart)
{
	uint64_t* positionPtr = reinterpret_cast<uint64_t*>(processStart + NA_PLAYER_POS_OFF_0);
	if (!positionPtr) return nullptr;
	return reinterpret_cast<Vector3*>(*positionPtr + NA_PLAYER_POS_OFF_1);
}

Vector3* GameData::GetPlayerRotation(uint64_t processStart)
{
	uint64_t* rotationPtr = reinterpret_cast<uint64_t*>(processStart + NA_PLAYER_ROT_OFF_0);
	if (!rotationPtr) return nullptr;
	return reinterpret_cast<Vector3*>(*rotationPtr + NA_PLAYER_ROT_OFF_1);
}