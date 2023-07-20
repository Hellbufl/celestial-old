#pragma once
#include <cstdint>
#include "linalg.h"

#define NA_VMATRIX_OFF 0x11553B0ull
#define NA_PLAYER_POS_OFF_0 0x1020948ull
#define NA_PLAYER_POS_OFF_1 0x50ull
#define NA_PLAYER_ROT_OFF_0 0x01020BE0ull
#define NA_PLAYER_ROT_OFF_1 0x90ull

namespace gamedata
{
	Matrix4* GetViewMatrix(uint64_t processStart);
	Vector3* GetPlayerPosition(uint64_t processStart);
	Vector3* GetPlayerRotation(uint64_t processStart);
}