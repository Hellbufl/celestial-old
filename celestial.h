// proc handling, custom rendering and ImGUI setup thx to Woeful_Wolf //

#pragma once
#include <windows.h>
#include <cstdint>
#include <ocular.h>
#include <DirectXMath.h>
#include "ReadData.h"
#include "pathlog.h"
#include "gamedata.h"
#include "config.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"

#define MAX_VERTS 1024
#define GUI_BUTTON_SIZE 150
#define KEYBIND_COUNT 5

struct LineVertex
{
	DirectX::XMFLOAT3 pos;
	DirectX::XMFLOAT3 nextPos;
	float thickness;
	DirectX::XMFLOAT4 colour;
};

struct BoxVertex
{
	DirectX::XMFLOAT3 pos;
	DirectX::XMFLOAT4 colour;
};

struct VS_CONSTANT_BUFFER
{
	DirectX::XMFLOAT4X4 mWorldViewProj;
};

namespace celestial
{
	void MainLoop();

	void Init();
	void InitRendering(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);

	HRESULT hkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);
	HRESULT hkResizeBuffers(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags);

	LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	void SetupRenderTarget(IDXGISwapChain* pSwapChain);
	void CleanupRenderTarget();

	void hkOMSetRenderTargets(ID3D11DeviceContext* pDeviceContext, UINT NumViews, ID3D11RenderTargetView* const* ppRenderTargetViews, ID3D11DepthStencilView* pDepthStencilView);

	void CreateLineVertexBuffer(std::vector<Vector3> pathNodes, DirectX::XMFLOAT4 color, float lineThickness, std::vector<LineVertex>& destBuffer);
	void CreateBoxVertexBuffer(std::vector<Vector3> points, DirectX::XMFLOAT4 color, std::vector<BoxVertex>& destBuffer);

	void RenderGUI();
	void KeyPress(WPARAM key);
}