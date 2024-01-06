#include "celestial.h"

void DrawLine(ID3D11DeviceContext* pContext, std::vector<Vector3> nodes, DirectX::XMFLOAT4 color, float thiccness);
void DrawBox(ID3D11DeviceContext* pContext, std::vector<Vector3> boxPoints, DirectX::XMFLOAT4 color);
void DrawBoxOutline(ID3D11DeviceContext* pContext, std::vector<Vector3> boxPoints, DirectX::XMFLOAT4 color, float thickness);

void GUIPathsTab();
void GUIConfigTab();
void GUIKeybind(ImGuiIO& io, const char* name, uint32_t& keybind, int rebindingIndex);

// DEBUG //
void DrawDebug(ImColor color, float thickness);

uint64_t processStartPtr;
PLogState ppLog;
ConfigState currentConfig;

// Drawing stuff //
bool initImGui = false;
bool initCustomRender = false;

unsigned int bbWidth;
unsigned int bbHeight;
int depthRenderPassIndex;

std::vector<uint8_t> lineVertexShaderCode, vertexShaderCode, pixelShaderCode;

ID3D11VertexShader* lineVertexShader;
ID3D11VertexShader* vertexShader;
ID3D11PixelShader* pixelShader;

ID3D11BlendState* blendState;
ID3D11DepthStencilState* depthStencilState;

ID3D11InputLayout* lineInputLayout;
ID3D11InputLayout* boxInputLayout;

ID3D11Buffer* lineVertexBuffer;
ID3D11Buffer* boxVertexBuffer;

ID3D11Resource* rtRes;
ID3D11Resource* dsRes;

ID3D11Buffer* constVMatrixBuffer;

ID3D11RenderTargetView* g_mainRenderTargetView = NULL;
HWND window;
WNDPROC oWndProc;

bool rebindingKey[KEYBIND_COUNT];
std::vector<uint32_t> currentKeybinds;

uint64_t selectedPath;

std::map<uint64_t, bool> pathMute;
std::map<uint64_t, bool> pathSolo;
std::map<uint64_t, bool> compMute;
std::map<uint64_t, bool> compSolo;
int pathSoloActive;
int compSoloActive;

void celestial::MainLoop()
{
	bool running = true;
	int tickLength = 1000 / TICKRATE;
	std::chrono::time_point<std::chrono::high_resolution_clock> lastTick = std::chrono::high_resolution_clock::now();

	Vector3* playerPos;
	Vector3* playerRot;

	while (running)
	{
		uint64_t timePassed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - lastTick).count();
		if (timePassed < tickLength) continue;

		lastTick = std::chrono::high_resolution_clock::now();

		playerPos = gamedata::GetPlayerPosition(processStartPtr);
		playerRot = gamedata::GetPlayerRotation(processStartPtr);

		pathlog::Update(ppLog, playerPos, playerRot);
	}
}

void celestial::Init()
{
	processStartPtr = reinterpret_cast<uint64_t>(GetModuleHandle(nullptr));

	config::Init(currentConfig);
	config::ReadConfig(currentConfig);

	pathlog::Init(ppLog);

	printf("[Celestial] Initialized.\n");
}

void celestial::InitRendering(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
	lineVertexShaderCode = DX::ReadData(L"line_vertex_shader.cso");
	vertexShaderCode = DX::ReadData(L"vertex_shader.cso");
	pixelShaderCode = DX::ReadData(L"pixel_shader.cso");

	pDevice->CreateVertexShader(lineVertexShaderCode.data(), lineVertexShaderCode.size(), NULL, &lineVertexShader);
	HRESULT vShaderRes = pDevice->CreateVertexShader(vertexShaderCode.data(), vertexShaderCode.size(), NULL, &vertexShader);

	if (vShaderRes != S_OK)
	{
		printf("[Celestial] ERROR: Failed to create vertex shader: %X\n", vShaderRes);
	}

	pDevice->CreatePixelShader(pixelShaderCode.data(), pixelShaderCode.size(), NULL, &pixelShader);

	// Input layouts
	D3D11_INPUT_ELEMENT_DESC iedLine[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"NEXTPOSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"THICC", 0, DXGI_FORMAT_R32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0}
	};

	HRESULT creatIARes = pDevice->CreateInputLayout(iedLine, ARRAYSIZE(iedLine), lineVertexShaderCode.data(), lineVertexShaderCode.size(), &lineInputLayout);

	if (creatIARes != S_OK)
	{
		printf("[Celestial] ERROR: Failed to create line input layout: %X\n", creatIARes);
	}

	D3D11_INPUT_ELEMENT_DESC iedBox[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0}
	};

	creatIARes = pDevice->CreateInputLayout(iedBox, ARRAYSIZE(iedBox), vertexShaderCode.data(), vertexShaderCode.size(), &boxInputLayout);

	if (creatIARes != S_OK)
	{
		printf("[Celestial] ERROR: Failed to create box input layout: %X\n", creatIARes);
	}

	// Line Vertex buffer
	D3D11_BUFFER_DESC lineVertexBufferDesc;
	ZeroMemory(&lineVertexBufferDesc, sizeof(lineVertexBufferDesc));
	lineVertexBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	lineVertexBufferDesc.ByteWidth = sizeof(LineVertex) * MAX_PATH_NODES * 6;
	lineVertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	lineVertexBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	pDevice->CreateBuffer(&lineVertexBufferDesc, NULL, &lineVertexBuffer);
	if (!lineVertexBuffer) {
		printf("[Celestial] ERROR: Failed to create vertex buffer!\n");
		return;
	}

	// Box Vertex buffer
	D3D11_BUFFER_DESC boxVertexBufferDesc;
	ZeroMemory(&boxVertexBufferDesc, sizeof(boxVertexBufferDesc));
	boxVertexBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	boxVertexBufferDesc.ByteWidth = sizeof(BoxVertex) * MAX_BOX_VERTS;
	boxVertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	boxVertexBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	pDevice->CreateBuffer(&boxVertexBufferDesc, NULL, &boxVertexBuffer);
	if (!boxVertexBuffer) {
		printf("[Celestial] ERROR: Failed to create vertex buffer!\n");
		return;
	}

	// View Matrix Buffer //
	D3D11_BUFFER_DESC cbDesc;
	cbDesc.ByteWidth = sizeof(VS_CONSTANT_BUFFER);
	cbDesc.Usage = D3D11_USAGE_DYNAMIC;
	cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	cbDesc.MiscFlags = 0;
	cbDesc.StructureByteStride = 0;

	pDevice->CreateBuffer(&cbDesc, NULL, &constVMatrixBuffer);

	// Blending
	D3D11_BLEND_DESC blendDesc;
	ZeroMemory(&blendDesc, sizeof(blendDesc));

	D3D11_RENDER_TARGET_BLEND_DESC rtbd;
	ZeroMemory(&rtbd, sizeof(rtbd));

	rtbd.BlendEnable = true;
	rtbd.SrcBlend = D3D11_BLEND_SRC_COLOR;
	rtbd.DestBlend = D3D11_BLEND_BLEND_FACTOR;
	rtbd.BlendOp = D3D11_BLEND_OP_ADD;
	rtbd.SrcBlendAlpha = D3D11_BLEND_ONE;
	rtbd.DestBlendAlpha = D3D11_BLEND_ZERO;
	rtbd.BlendOpAlpha = D3D11_BLEND_OP_ADD;
	rtbd.RenderTargetWriteMask = D3D10_COLOR_WRITE_ENABLE_ALL;

	blendDesc.AlphaToCoverageEnable = false;
	blendDesc.RenderTarget[0] = rtbd;

	pDevice->CreateBlendState(&blendDesc, &blendState);

	// Depth Stencil
	D3D11_DEPTH_STENCIL_DESC dsDesc;
	dsDesc.DepthEnable = true;
	dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	dsDesc.DepthFunc = D3D11_COMPARISON_LESS;

	dsDesc.StencilEnable = true;
	dsDesc.StencilReadMask = 0xFF;
	dsDesc.StencilWriteMask = 0xFF;

	dsDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	dsDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
	dsDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	dsDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

	dsDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	dsDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
	dsDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	dsDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

	// Create depth stencil state
	HRESULT cdssRes = pDevice->CreateDepthStencilState(&dsDesc, &depthStencilState);
	if (cdssRes != S_OK)
	{
		printf("[Celestial] ERROR: Failed to create depth stencil state: %X\n", cdssRes);
	}

	initCustomRender = true;
	printf("[Celestial] Initialized rendering.\n");
}

void DrawLine(ID3D11DeviceContext* pContext, std::vector<Vector3> nodes, DirectX::XMFLOAT4 color, float thiccness)
{
	if (nodes.size() < 2) return;

	std::vector<LineVertex> vertices;
	celestial::CreateLineVertexBuffer(nodes, color, thiccness, vertices);

	D3D11_MAPPED_SUBRESOURCE vms;
	pContext->Map(lineVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &vms);
	memcpy(vms.pData, vertices.data(), vertices.size() * sizeof(LineVertex));
	pContext->Unmap(lineVertexBuffer, 0);

	UINT stride = sizeof(LineVertex);
	UINT offset = 0;
	pContext->IASetVertexBuffers(0, 1, &lineVertexBuffer, &stride, &offset);
	pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	pContext->IASetInputLayout(lineInputLayout);

	// Set up shaders and constant buffers
	pContext->VSSetShader(lineVertexShader, 0, 0);
	pContext->PSSetShader(pixelShader, 0, 0);
	pContext->VSSetConstantBuffers(0, 1, &constVMatrixBuffer);

	pContext->Draw(vertices.size(), 0);
}

void DrawBox(ID3D11DeviceContext* pContext, std::vector<Vector3> boxPoints, DirectX::XMFLOAT4 color)
{
	if (boxPoints.size() != 8) return;

	std::vector<BoxVertex> boxVertices;

	celestial::CreateBoxVertexBuffer(boxPoints, color, boxVertices);

	D3D11_MAPPED_SUBRESOURCE vms;
	HRESULT pResult = pContext->Map(boxVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &vms);
	memcpy(vms.pData, boxVertices.data(), boxVertices.size() * sizeof(BoxVertex));
	pContext->Unmap(boxVertexBuffer, 0);

	// Set IA stage
	UINT stride = sizeof(BoxVertex);
	UINT offset = 0;
	pContext->IASetVertexBuffers(0, 1, &boxVertexBuffer, &stride, &offset);
	pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	pContext->IASetInputLayout(boxInputLayout);

	// Set up shaders and constant buffers
	pContext->VSSetShader(vertexShader, 0, 0);
	pContext->PSSetShader(pixelShader, 0, 0);
	pContext->VSSetConstantBuffers(0, 1, &constVMatrixBuffer);

	pContext->Draw(boxVertices.size(), 0);
}

void DrawBoxOutline(ID3D11DeviceContext* pContext, std::vector<Vector3> boxPoints, DirectX::XMFLOAT4 color, float thickness)
{
	std::vector<Vector3> outlinePoints;

	outlinePoints.push_back(boxPoints[0]);
	outlinePoints.push_back(boxPoints[1]);
	outlinePoints.push_back(boxPoints[3]);
	outlinePoints.push_back(boxPoints[7]);
	outlinePoints.push_back(boxPoints[5]);
	outlinePoints.push_back(boxPoints[4]);
	outlinePoints.push_back(boxPoints[6]);
	outlinePoints.push_back(boxPoints[7]);
	outlinePoints.push_back(boxPoints[3]);
	outlinePoints.push_back(boxPoints[2]);
	outlinePoints.push_back(boxPoints[0]);
	outlinePoints.push_back(boxPoints[4]);
	outlinePoints.push_back(boxPoints[6]);
	outlinePoints.push_back(boxPoints[2]);
	outlinePoints.push_back(boxPoints[0]);
	outlinePoints.push_back(boxPoints[1]);
	outlinePoints.push_back(boxPoints[5]);

	DrawLine(pContext, outlinePoints, color, thickness);
}

HRESULT celestial::hkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
{
	// Get our current device and context
	ID3D11Device* pDevice = nullptr;
	pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**) &pDevice);

	ID3D11DeviceContext* pContext;
	pDevice->GetImmediateContext(&pContext);

	depthRenderPassIndex = 0;

	if (!pDevice || !pContext)
	{
		printf("[Celestial] ERROR: pDevice or pContext not initialized!\n");
		return ((ocular::SwapChain::PRESENT)ocular::oMethods[ocular::VMT::Present])(pSwapChain, SyncInterval, Flags);
	}

	if (!initImGui)
	{
		// Set our WndProc so we can handle some input and pass it to ImGui
		DXGI_SWAP_CHAIN_DESC sd;
		pSwapChain->GetDesc(&sd);
		window = sd.OutputWindow;
		oWndProc = (WNDPROC)SetWindowLongPtrA(window, GWLP_WNDPROC, (LONG_PTR)WndProc);

		// TODO: figure out why removing this fixes the game freezing on the first frame
		//SetupRenderTarget(pSwapChain);
		// DO NOT DELETE

		// Init our win32 and DX11 ImGui Implementations
		ImGui::CreateContext();
		ImGui_ImplWin32_Init(window);
		ImGui_ImplDX11_Init(pDevice, pContext);
		initImGui = true;

		printf("[Celestial] ImGui initialized.\n");
	}

	if (!initCustomRender)
		InitRendering(pDevice, pContext);

	ID3D11RenderTargetView* pRTV;
	ID3D11DepthStencilView* pDSV;

	HRESULT cdsvRes = pDevice->CreateDepthStencilView(dsRes, NULL, &pDSV);
	HRESULT crtvRes = pDevice->CreateRenderTargetView(rtRes, NULL, &pRTV);

	if (cdsvRes == S_OK && crtvRes == S_OK)
		((ocular::Context::OMSETRENDERTARGETS)ocular::oMethods[ocular::VMT::OMSetRenderTargets])(pContext, 1, &pRTV, pDSV);

	pContext->OMSetBlendState(blendState, NULL, 0xffffffff);
	pContext->OMSetDepthStencilState(depthStencilState, 1);

	Matrix4* viewMatrix = gamedata::GetViewMatrix(processStartPtr);

	VS_CONSTANT_BUFFER VsConstData;
	VsConstData.mWorldViewProj = DirectX::XMFLOAT4X4(viewMatrix->mm);

	D3D11_MAPPED_SUBRESOURCE vms;
	pContext->Map(constVMatrixBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &vms);
	memcpy(vms.pData, &VsConstData, sizeof(VS_CONSTANT_BUFFER));
	pContext->Unmap(constVMatrixBuffer, 0);

	DrawLine(pContext, ppLog.recordingPath.nodes, DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), 0.02f);

	for (int i = 0; i < ppLog.displayedPaths.size(); i++)
	{
		if (selectedPath == ppLog.displayedPaths[i].id)
		{
			DrawLine(pContext, ppLog.displayedPaths[i].nodes, DirectX::XMFLOAT4(0.5f, 0.7f, 0.9f, 1.0f), 0.05f);
			continue;
		}

		if (!pathSoloActive && pathMute[ppLog.displayedPaths[i].id]) continue;
		if (pathSoloActive && !pathSolo[ppLog.displayedPaths[i].id]) continue;

		DrawLine(pContext, ppLog.displayedPaths[i].nodes, DirectX::XMFLOAT4(0.7f, 0.7f, 0.7f, 1.0f), 0.02f);
	}

	for (int i = 0; i < ppLog.comparedPaths.size(); i++)
	{
		if (selectedPath == ppLog.comparedPaths[i].id)
		{
			DrawLine(pContext, ppLog.comparedPaths[i].nodes, DirectX::XMFLOAT4(0.5f, 0.7f, 0.9f, 1.0f), 0.05f);
			continue;
		}

		if (!compSoloActive && compMute[ppLog.comparedPaths[i].id]) continue;
		if (compSoloActive && !compSolo[ppLog.comparedPaths[i].id]) continue;

		if (i == 0) DrawLine(pContext, ppLog.comparedPaths[i].nodes, DirectX::XMFLOAT4(0.9f, 0.7f, 0.5f, 1.0f), 0.05f);
		else DrawLine(pContext, ppLog.comparedPaths[i].nodes, DirectX::XMFLOAT4(	0.0f + i / ((float)ppLog.comparedPaths.size() * 1.43f),
																					0.7f - i / ((float)ppLog.comparedPaths.size() * 1.43f),
																					0.0f,
																					1.0f), 0.02f);
	}

	if (ppLog.triggerState[0] == 2)
	{
		std::vector<Vector3> boxPoints;
		for (int p = 0; p < 8; p++) boxPoints.push_back(ppLog.recordingTrigger[0].points[p]);

		DrawBox(pContext, boxPoints, DirectX::XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f));
		DrawBoxOutline(pContext, boxPoints, DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), 0.03f);
	}

	if (ppLog.triggerState[1] == 2)
	{
		std::vector<Vector3> boxPoints;
		for (int p = 0; p < 8; p++) boxPoints.push_back(ppLog.recordingTrigger[1].points[p]);

		DrawBox(pContext, boxPoints, DirectX::XMFLOAT4(0.85f, 0.55f, 0.27f, 1.0f));
		DrawBoxOutline(pContext, boxPoints, DirectX::XMFLOAT4(1.0f, 0.48f, 0.0f, 1.0f), 0.03f);
	}

	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	//DrawDebug(ImColor(0.9f, 0.3f, 0.7f), 2.0f);
	RenderGUI();

	ImGui::EndFrame();
	ImGui::Render();

	// Set the render target back to main
	pContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	// We return the result of the original method.
	// Here we are casting the original method pointer to the appropriate type so that we can pass the parameters.
	return ((ocular::SwapChain::PRESENT)ocular::oMethods[ocular::VMT::Present])(pSwapChain, SyncInterval, Flags);
}

// Create our custom window proc handler and make sure to pass any events to ImGui
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI celestial::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);

	ImGuiIO& io = ImGui::GetIO();

	if (io.WantCaptureMouse || io.WantCaptureKeyboard) return TRUE;

	if (msg == WM_KEYDOWN)
		KeyPress(wParam);

	return CallWindowProc(oWndProc, hWnd, msg, wParam, lParam);
}

// Update our main render target view pointer if the game happens to change theirs.
void celestial::SetupRenderTarget(IDXGISwapChain* pSwapChain)
{
	ID3D11Texture2D* pBackBuffer;
	pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));

	D3D11_TEXTURE2D_DESC backbufferDesc;
	pBackBuffer->GetDesc(&backbufferDesc);
	bbWidth = backbufferDesc.Width;
	bbHeight = backbufferDesc.Height;

	ID3D11Device* pDevice = nullptr;
	pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**) &pDevice);
	
	if (pBackBuffer)
	{
		pDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView);
		pBackBuffer->Release();
	}

	printf("[Celestial] Set up render targets.\n");
}

// Release our previous render target for safety.
void celestial::CleanupRenderTarget()
{
	if (!g_mainRenderTargetView) return;

	g_mainRenderTargetView->Release();
	g_mainRenderTargetView = NULL;

	printf("[Celestial] Cleaned up render targets.\n");
}

// If the game calls resizeBuffers, our render target will become invalid so let's update it.
HRESULT celestial::hkResizeBuffers(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
{
	CleanupRenderTarget();

	HRESULT res = ((ocular::SwapChain::RESIZEBUFFERS)ocular::oMethods[ocular::VMT::ResizeBuffers])(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);

	if (initImGui)
		SetupRenderTarget(pSwapChain);

	return res;
}

// Games usually call this for each render pass they are doing when drawing.
void celestial::hkOMSetRenderTargets(ID3D11DeviceContext* pDeviceContext, UINT NumViews, ID3D11RenderTargetView* const* ppRenderTargetViews, ID3D11DepthStencilView* pDepthStencilView)
{
	if (ppRenderTargetViews && pDepthStencilView)
	{
		ID3D11Resource* dsvResource;
		pDepthStencilView->GetResource(&dsvResource);

		ID3D11Texture2D* tex = reinterpret_cast<ID3D11Texture2D*>(dsvResource);
		D3D11_TEXTURE2D_DESC texDesc;
		tex->GetDesc(&texDesc);

		// Here we have access to each render target and depth stencil in the game.

		if (texDesc.Width == bbWidth && texDesc.Height == bbHeight && texDesc.BindFlags & D3D11_BIND_DEPTH_STENCIL) {
			if (depthRenderPassIndex == 0) {    // Changed based on what depth pass we are looking for
				(*ppRenderTargetViews)->GetResource(&rtRes);
				pDepthStencilView->GetResource(&dsRes);
			}

			depthRenderPassIndex++;
		}

		dsvResource->Release();
	}

	((ocular::Context::OMSETRENDERTARGETS)ocular::oMethods[ocular::VMT::OMSetRenderTargets])(pDeviceContext, NumViews, ppRenderTargetViews, pDepthStencilView);
}

// takes a list of points and outputs a vertex buffer to draw a line between these points
void celestial::CreateLineVertexBuffer(std::vector<Vector3> nodes, DirectX::XMFLOAT4 color, float lineThickness, std::vector<LineVertex>& destBuffer)
{
	if (nodes.size() < 2) return;
	if (nodes.size() > MAX_PATH_NODES) return;

	for (size_t i = 0; i < nodes.size() - 1; i++)
	{
		LineVertex newVertex[6];
		destBuffer.reserve(6);

		newVertex[0].pos.x = nodes[i].x;
		newVertex[0].pos.y = nodes[i].y;
		newVertex[0].pos.z = nodes[i].z;
		newVertex[0].nextPos.x = nodes[i + 1].x;
		newVertex[0].nextPos.y = nodes[i + 1].y;
		newVertex[0].nextPos.z = nodes[i + 1].z;

		newVertex[1].pos.x = nodes[i + 1].x;
		newVertex[1].pos.y = nodes[i + 1].y;
		newVertex[1].pos.z = nodes[i + 1].z;
		newVertex[1].nextPos.x = nodes[i].x;
		newVertex[1].nextPos.y = nodes[i].y;
		newVertex[1].nextPos.z = nodes[i].z;

		newVertex[0].colour = color;
		newVertex[1].colour = color;

		memcpy(newVertex + 2, newVertex, sizeof(LineVertex));
		memcpy(newVertex + 3, newVertex, sizeof(LineVertex));
		memcpy(newVertex + 4, newVertex + 1, sizeof(LineVertex));
		memcpy(newVertex + 5, newVertex + 1, sizeof(LineVertex));

		newVertex[0].thickness = lineThickness;
		newVertex[1].thickness = -lineThickness;
		newVertex[2].thickness = -lineThickness;
		newVertex[3].thickness = -lineThickness;
		newVertex[4].thickness = -lineThickness;
		newVertex[5].thickness = lineThickness;

		destBuffer.push_back(newVertex[0]);
		destBuffer.push_back(newVertex[1]);
		destBuffer.push_back(newVertex[2]);
		destBuffer.push_back(newVertex[3]);
		destBuffer.push_back(newVertex[4]);
		destBuffer.push_back(newVertex[5]);
	}
}

// please do not read this function I beg you
void celestial::CreateBoxVertexBuffer(std::vector<Vector3> points, DirectX::XMFLOAT4 color, std::vector<BoxVertex>& destBuffer)
{
	if (points.size() != 8)
	{
		printf("[Celestial] ERROR: CreateBoxVertexBuffer needs exactly 8 points!\n");
		Sleep(2000);
		exit(1);
	}
	BoxVertex newVertex[36];
	destBuffer.reserve(36);

	newVertex[0].pos.x = points[0].x;
	newVertex[0].pos.y = points[0].y;
	newVertex[0].pos.z = points[0].z;

	newVertex[1].pos.x = points[2].x;
	newVertex[1].pos.y = points[2].y;
	newVertex[1].pos.z = points[2].z;

	newVertex[2].pos.x = points[1].x;
	newVertex[2].pos.y = points[1].y;
	newVertex[2].pos.z = points[1].z;

	newVertex[3] = newVertex[2];
	newVertex[4] = newVertex[1];

	newVertex[5].pos.x = points[3].x;
	newVertex[5].pos.y = points[3].y;
	newVertex[5].pos.z = points[3].z;

	newVertex[6] = newVertex[0];
	newVertex[7] = newVertex[2];

	newVertex[8].pos.x = points[4].x;
	newVertex[8].pos.y = points[4].y;
	newVertex[8].pos.z = points[4].z;

	newVertex[9] = newVertex[2];

	newVertex[10].pos.x = points[5].x;
	newVertex[10].pos.y = points[5].y;
	newVertex[10].pos.z = points[5].z;

	newVertex[11] = newVertex[8];
	newVertex[12] = newVertex[8];
	newVertex[13] = newVertex[10];

	newVertex[14].pos.x = points[6].x;
	newVertex[14].pos.y = points[6].y;
	newVertex[14].pos.z = points[6].z;

	newVertex[15] = newVertex[10];

	newVertex[16].pos.x = points[7].x;
	newVertex[16].pos.y = points[7].y;
	newVertex[16].pos.z = points[7].z;

	newVertex[17] = newVertex[14];

	newVertex[18] = newVertex[0];
	newVertex[19] = newVertex[14];
	newVertex[20] = newVertex[1];
	newVertex[21] = newVertex[0];
	newVertex[22] = newVertex[8];
	newVertex[23] = newVertex[14];
	newVertex[24] = newVertex[2];
	newVertex[25] = newVertex[5];
	newVertex[26] = newVertex[10];
	newVertex[27] = newVertex[5];
	newVertex[28] = newVertex[16];
	newVertex[29] = newVertex[10];
	newVertex[30] = newVertex[1];
	newVertex[31] = newVertex[16];
	newVertex[32] = newVertex[5];
	newVertex[33] = newVertex[1];
	newVertex[34] = newVertex[14];
	newVertex[35] = newVertex[16];

	for (int i = 0; i < 36; i++)
	{
		newVertex[i].colour = color;
		destBuffer.push_back(newVertex[i]);
	}
}

void GUIPathsTab()
{
	if (!ImGui::BeginTabItem("Paths")) return;

	ImGuiIO& io = ImGui::GetIO();

	if (ImGui::CollapsingHeader("Comparison"))
	{
		for (int i = 0; i < ppLog.comparedPaths.size(); i++)
		{
			compMute.try_emplace(ppLog.comparedPaths[i].id, false);
			compSolo.try_emplace(ppLog.comparedPaths[i].id, false);

			ImGui::PushID(ppLog.comparedPaths[i].id);

			if (compMute[ppLog.comparedPaths[i].id])
			{
				ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0.0f, 0.6f, 0.7f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0.0f, 0.6f, 0.5f));
			}
			else
			{
				ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0.0f, 0.0f, 0.5f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0.0f, 0.0f, 0.3f));
			}

			if (ImGui::Button("M", ImVec2(20.0f, 20.0f)))
			{
				compMute[ppLog.comparedPaths[i].id] ^= true;
			}

			ImGui::PopStyleColor(2);

			ImGui::SameLine();

			if (compSolo[ppLog.comparedPaths[i].id])
			{
				ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0.15f, 0.6f, 0.7f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0.15f, 0.6f, 0.5f));
			}
			else
			{
				ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0.0f, 0.0f, 0.5f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0.0f, 0.0f, 0.3f));
			}

			if (ImGui::Button("S", ImVec2(20.0f, 20.0f)))
			{
				compSolo[ppLog.comparedPaths[i].id] ^= true;

				if (compSolo[ppLog.comparedPaths[i].id])
					++compSoloActive;
				else
					--compSoloActive;
			}

			ImGui::PopStyleColor(2);

			ImGui::SameLine();

			if (selectedPath == ppLog.comparedPaths[i].id)
			{
				ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0.59f, 0.44f, 0.7f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0.59f, 0.44f, 0.5f));
			}
			else
			{
				ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0.0f, 0.0f, 0.5f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0.0f, 0.0f, 0.3f));
			}

			std::string timeMinutes = std::to_string(ppLog.comparedPaths[i].time / 60000);
			std::string timeSeconds = std::to_string((ppLog.comparedPaths[i].time % 60000) / 1000);
			std::string timeMilliseconds = std::to_string(ppLog.comparedPaths[i].time % 1000);

			while (timeMinutes.length() < 2) timeMinutes = "0" + timeMinutes;
			while (timeSeconds.length() < 2) timeSeconds = "0" + timeSeconds;
			while (timeMilliseconds.length() < 3) timeMilliseconds = "0" + timeMilliseconds;

			std::string timeDisplayed = timeSeconds + "." + timeMilliseconds;
			if (timeMinutes != "00") timeDisplayed = timeMinutes + ":" + timeDisplayed;

			if (ImGui::Button(timeDisplayed.c_str(), ImVec2(150.0f, 20.0f)))
			{
				if (selectedPath == ppLog.comparedPaths[i].id)
					selectedPath = 0;
				else
					selectedPath = ppLog.comparedPaths[i].id;
			}

			ImGui::PopStyleColor(2);

			ImGui::SameLine(ImGui::GetWindowWidth() - 50);

			if (ImGui::Button("X", ImVec2(20.0f, 20.0f)))
			{
				ppLog.comparedPaths.erase(ppLog.comparedPaths.begin() + i);
			}

			ImGui::PopID();

			ImGui::Separator();
		}
	}

	if (ImGui::CollapsingHeader("Seperate Paths"))
	{
		for (int i = 0; i < ppLog.displayedPaths.size(); i++)
		{
			pathMute.try_emplace(ppLog.displayedPaths[i].id, false);
			pathSolo.try_emplace(ppLog.displayedPaths[i].id, false);

			ImGui::PushID(ppLog.displayedPaths[i].id);

			if (pathMute[ppLog.displayedPaths[i].id])
			{
				ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0.0f, 0.6f, 0.7f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0.0f, 0.6f, 0.5f));
			}
			else
			{
				ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0.0f, 0.0f, 0.5f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0.0f, 0.0f, 0.3f));
			}

			if (ImGui::Button("M", ImVec2(20.0f, 20.0f)))
			{
				pathMute[ppLog.displayedPaths[i].id] ^= true;
			}

			ImGui::PopStyleColor(2);

			ImGui::SameLine();

			if (pathSolo[ppLog.displayedPaths[i].id])
			{
				ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0.15f, 0.6f, 0.7f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0.15f, 0.6f, 0.5f));
			}
			else
			{
				ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0.0f, 0.0f, 0.5f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0.0f, 0.0f, 0.3f));
			}

			if (ImGui::Button("S", ImVec2(20.0f, 20.0f)))
			{
				pathSolo[ppLog.displayedPaths[i].id] ^= true;

				if (pathSolo[ppLog.displayedPaths[i].id])
					++pathSoloActive;
				else
					--pathSoloActive;
			}

			ImGui::PopStyleColor(2);

			ImGui::SameLine();

			if (selectedPath == ppLog.displayedPaths[i].id)
			{
				ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0.59f, 0.44f, 0.7f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0.59f, 0.44f, 0.5f));
			}
			else
			{
				ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0.0f, 0.0f, 0.5f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0.0f, 0.0f, 0.3f));
			}

			std::string timeMinutes = std::to_string(ppLog.displayedPaths[i].time / 60000);
			std::string timeSeconds = std::to_string((ppLog.displayedPaths[i].time % 60000) / 1000);
			std::string timeMilliseconds = std::to_string(ppLog.displayedPaths[i].time % 1000);

			while (timeMinutes.length() < 2) timeMinutes = "0" + timeMinutes;
			while (timeSeconds.length() < 2) timeSeconds = "0" + timeSeconds;
			while (timeMilliseconds.length() < 3) timeMilliseconds = "0" + timeMilliseconds;

			std::string timeDisplayed = timeSeconds + "." + timeMilliseconds;
			if (timeMinutes != "00") timeDisplayed = timeMinutes + ":" + timeDisplayed;

			if (ImGui::Button(timeDisplayed.c_str(), ImVec2(150.0f, 20.0f)))
			{
				if (selectedPath == ppLog.displayedPaths[i].id)
					selectedPath = 0;
				else
					selectedPath = ppLog.displayedPaths[i].id;
			}

			ImGui::PopStyleColor(2);

			ImGui::SameLine(ImGui::GetWindowWidth() - 50);

			if (ImGui::Button("X", ImVec2(20.0f, 20.0f)))
			{
				ppLog.displayedPaths.erase(ppLog.displayedPaths.begin() + i);
			}

			ImGui::PopID();

			ImGui::Separator();
		}
	}

	ImGui::Separator();

	ImGui::Indent((ImGui::GetWindowSize().x - GUI_BUTTON_SIZE) / 2);

	if (ImGui::Button("Load Comparison", ImVec2(GUI_BUTTON_SIZE, 24.0f)))
	{
		std::string pathsDir = "" + pathlog::GetPathsDirectory();
		nfdchar_t* NDFpathsDir = (nfdchar_t*) calloc(pathsDir.size() + 1, sizeof(nfdchar_t));

		if (!NDFpathsDir)
		{
			printf("[Celestial] ERROR: calloc failed.\n");
			return;
		}

		memcpy(NDFpathsDir, pathsDir.c_str(), pathsDir.size());

		nfdchar_t* outPath = NULL;
		nfdresult_t result = NFD_OpenDialog(NULL, NDFpathsDir, &outPath);

		if (result == NFD_OKAY)
		{
			pathlog::ReadCompFile(ppLog, outPath);

			free(outPath);
		}
		else if (result != NFD_CANCEL)
		{
			printf("[Celestial] ERROR: %s\n", NFD_GetError());
		}
	}

	if (ImGui::Button("Load Seperate Path", ImVec2(GUI_BUTTON_SIZE, 24.0f)))
	{
		std::string pathsDir = "" + pathlog::GetPathsDirectory();
		nfdchar_t* NDFpathsDir = (nfdchar_t*)calloc(pathsDir.size() + 1, sizeof(nfdchar_t));

		if (!NDFpathsDir)
		{
			printf("[Celestial] ERROR: calloc failed.\n");
			return;
		}

		memcpy(NDFpathsDir, pathsDir.c_str(), pathsDir.size());

		nfdchar_t* outPath = NULL;
		nfdresult_t result = NFD_OpenDialog(NULL, NDFpathsDir, &outPath);

		if (result == NFD_OKAY)
		{
			pathlog::ReadPathFile(outPath, ++ppLog.currentPathID, ppLog.displayedPaths);

			free(outPath);
		}
		else if (result != NFD_CANCEL)
		{
			printf("[Celestial] ERROR: %s\n", NFD_GetError());
		}
	}

	if (ImGui::Button("Clear Comparison", ImVec2(GUI_BUTTON_SIZE, 24.0f)))
	{
		ppLog.comparedPaths.clear();
	}

	if (ImGui::Button("Clear Paths", ImVec2(GUI_BUTTON_SIZE, 24.0f)))
	{
		ppLog.displayedPaths.clear();
	}

	ImGui::EndTabItem();
}

void GUIConfigTab()
{
	if (!ImGui::BeginTabItem("Config")) return;

	ImGuiIO& io = ImGui::GetIO();

	if (ImGui::Checkbox("Direct Recording Mode", (bool*)&currentConfig.directMode)) ppLog.direct ^= true;

	if (ImGui::CollapsingHeader("Keybinds"))
	{
		GUIKeybind(io, "Toggle Menu:", currentConfig.toggleWindowKeybind, 0);
		GUIKeybind(io, "Spawn Start Trigger:", currentConfig.startKeybind, 1);
		GUIKeybind(io, "Spawn Stop Trigger:", currentConfig.stopKeybind, 2);
		GUIKeybind(io, "Reset Recording:", currentConfig.resetKeybind, 3);
		GUIKeybind(io, "Clear Paths:", currentConfig.clearKeybind, 4);
	}

	ImGui::Separator();

	ImGui::Indent((ImGui::GetWindowSize().x - GUI_BUTTON_SIZE) / 2);

	if (ImGui::Button("Save Config", ImVec2(GUI_BUTTON_SIZE, 24.0f)))
	{
		config::WriteConfig(currentConfig);
	}

	if (ImGui::Button("Load Config", ImVec2(GUI_BUTTON_SIZE, 24.0f)))
	{
		config::ReadConfig(currentConfig);
	}

	ImGui::Unindent((ImGui::GetWindowSize().x - GUI_BUTTON_SIZE) / 2);

	ImGui::EndTabItem();
}

void celestial::RenderGUI()
{
	if (!initImGui) return;
	if (!currentConfig.showUI) return;

	IM_ASSERT(ImGui::GetCurrentContext() != NULL && "[Celestial] Missing dear imgui context. Refer to examples app!");
	ImGuiIO& io = ImGui::GetIO();

	ImGuiWindowFlags window_flags = 0;

	if (currentConfig.showPLOG)
	{
		ImGui::SetNextWindowSizeConstraints(ImVec2(880.0f, 400.0f), io.DisplaySize);
		if (!ImGui::Begin("PLogMod", (bool*)&currentConfig.showUI, window_flags)) { ImGui::End(); return; }

		ImGui::PushStyleColor(ImGuiCol_Text, (ImVec4)ImColor::HSV(0.5f, 1.0f, 1.0f, 1.0f));

		ImGui::Text(
			"                     .`: -             \n"
			"             .-.///yhymNNs-..``        \n"
			"           ssmmmNNNNNNNMMMNmNy/.``     \n"
			"        -/yMMMMMMNNMMMMMMMMMMMsyo--    \n"
			"       :dNMMMMMMMMMMMMMMMMMMMMNNmdy:`  \n"
			"     :hNMMMMMMMMMMMMMNNmdhdmNMMMNh:    \n"
			"    -NMMMMMMMMMMMNyyso+:::-:/oydMN:    PPPPPPPPPPPPPPPPP   LLLLLLL                 OOOOOOOOO             GGGGGGGGGGGGG\n"
			"    hMMMMMMMMMMMNd:---......---:oNs    P::::::::::::::::P  L:::::L               OO:::::::::OO        GGG::::::::::::G\n"
			"   `MMMMMMMMMMNNNds:--.......---:oN.   P::::::PPPPPP:::::P L:::::L             OO:::::::::::::OO    GG:::::::::::::::G\n"
			"   `dMMMMMMMMMNMNdo/:::/oooo/--.:sm.   PP:::::P     P:::::PL:::::L            O:::::::OOO:::::::O  G:::::GGGGGGGG::::G\n"
			"    :MMMMMMMMMMMm+:------://+/:/++o      P::::P     P:::::PL:::::L            O::::::O   O::::::O G:::::G       GGGGGG\n"
			"    .mMNyyhNMMMNs:-----//:yy+-.o+so      P::::P     P:::::PL:::::L            O:::::O     O:::::OG:::::G              \n"
			"     +Md+:oyyhNmy::-...-/+o:.``:+s/      P::::PPPPPP:::::P L:::::L            O:::::O     O:::::OG:::::G              \n"
			"     `yMo:so//mNy/::--...--.....:::      P:::::::::::::PP  L:::::L            O:::::O     O:::::OG:::::G    GGGGGGGGGG\n"
			"      `mNo:-:/dNdo///:----------:::      P::::PPPPPPPPP    L:::::L            O:::::O     O:::::OG:::::G    G::::::::G\n"
			"       +MMdss+yhhsooo++//::-/oyyo/-      P::::P            L:::::L            O:::::O     O:::::OG:::::G    GGGGG::::G\n"
			"       `mNhyyossssosysooo/----:/+/       P::::P            L:::::L            O:::::O     O:::::OG:::::G        G::::G\n"
			"        ./ssoosyyyso++/:+++oyddho.       P::::P            L:::::L            O::::::O   O::::::O G:::::G       G::::G\n"
			"         +oooosyysso+++//oshmmdo       PP::::::PP          L:::::LLLLLLLLLLLLLO:::::::OOO:::::::O  G:::::GGGGGGGG::::G\n"
			"        -o+o++ooooo++o/:/oyssso:       P::::::::P          L:::::::::::::::::L OO:::::::::::::OO    GG:::::::::::::::G\n"
			"       `++++++/+ooosoos++shhyo-`       P::::::::P          L:::::::::::::::::L   OO:::::::::OO        GGG::::::GGG:::G\n"
			"        ./++++//+ossyyyhsosys-.`       PPPPPPPPPP          LLLLLLLLLLLLLLLLLLL     OOOOOOOOO             GGGGGG   GGGG\n"
			"         `-+////+ssssssyyys+.`         \n"
			"           `.-:::+syhhys+:..`          \n"
			"                 `...-.`````           \n");

		ImGui::PopStyleColor();
	}
	else
	{
		ImGui::SetNextWindowSizeConstraints(ImVec2(300.0f, 300.0f), io.DisplaySize);
		if (!ImGui::Begin("PathLog", (bool*)&currentConfig.showUI, window_flags)) { ImGui::End(); return; }
	}

	ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_None;
	if (ImGui::BeginTabBar("Tabs", tab_bar_flags))
	{
		GUIPathsTab();
		GUIConfigTab();
		//CustomizationTab();
		//DebugTab();
		//CreditsTab();
		ImGui::EndTabBar();
	}

	ImGui::End();
}

void GUIKeybind(ImGuiIO& io, const char* name, uint32_t& keybind, int rebindingIndex)
{
	if (!std::count(currentKeybinds.begin(), currentKeybinds.end(), keybind))
		currentKeybinds.push_back(keybind);

	char keyName[64];
	UINT scanCode = MapVirtualKey(keybind, MAPVK_VK_TO_VSC);

	switch (keybind)
	{
	case VK_LEFT: case VK_UP: case VK_RIGHT: case VK_DOWN:
	case VK_RCONTROL: case VK_RMENU:
	case VK_LWIN: case VK_RWIN: case VK_APPS:
	case VK_PRIOR: case VK_NEXT:
	case VK_END: case VK_HOME:
	case VK_INSERT: case VK_DELETE:
	case VK_DIVIDE:
	case VK_NUMLOCK:
		scanCode |= KF_EXTENDED;
	default:
		GetKeyNameTextA(scanCode << 16, keyName, sizeof(keyName));
	}

	ImGui::Text(name);

	ImGui::SameLine();

	ImGui::Indent((ImGui::GetWindowSize().x - GUI_BUTTON_SIZE));

	if (rebindingKey[rebindingIndex])
	{
		ImGui::Button("... (ESC To Cancel)", ImVec2(GUI_BUTTON_SIZE, 24.0f));

		for (int i = 0; i < IM_ARRAYSIZE(io.KeysDown); i++)
		{
			if (!io.KeysDown[i]) continue;

			rebindingKey[rebindingIndex] = false;

			if (i == VK_ESCAPE) break;
			if (std::count(currentKeybinds.begin(), currentKeybinds.end(), i)) break;

			keybind = i;
			break;
		}

		ImGui::Unindent((ImGui::GetWindowSize().x - GUI_BUTTON_SIZE));
		return;
	}

	if (ImGui::Button(keyName, ImVec2(GUI_BUTTON_SIZE, 24.0f)))
	{
		bool alreadyRebinding = false;

		for (int i = 0; i < KEYBIND_COUNT; i++)
			alreadyRebinding = alreadyRebinding || rebindingKey[i];

		if (!alreadyRebinding)
			rebindingKey[rebindingIndex] = true;
	}

	ImGui::Unindent((ImGui::GetWindowSize().x - GUI_BUTTON_SIZE));
}

void celestial::KeyPress(WPARAM key)
{
	if (key == currentConfig.toggleWindowKeybind)
	{
		currentConfig.showUI ^= 1;
		return;
	}

	if (key == currentConfig.startKeybind)
	{
		if (currentConfig.directMode)
			pathlog::StartRecording(ppLog);
		else
			ppLog.triggerState[0] = 1;
		return;
	}

	if (key == currentConfig.stopKeybind)
	{
		if (currentConfig.directMode)
			pathlog::StopRecording(ppLog);
		else
			ppLog.triggerState[1] = 1;
		return;
	}

	if (key == currentConfig.resetKeybind)
	{
		pathlog::ResetRecording(ppLog);
		return;
	}

	if (key == currentConfig.clearKeybind)
	{
		ppLog.displayedPaths.clear();
		ppLog.comparedPaths.clear();
		ppLog.currentCompFilePath = "";
		return;
	}
}

// DEBUG //

void DrawDebug(ImColor color, float thickness)
{
	ImDrawList* drawList = ImGui::GetForegroundDrawList();

	Matrix4* viewMatrix = gamedata::GetViewMatrix(processStartPtr);

	// Trigger

	Vector2 triggerBoxPoints[8];
	ImVec2 ppPoints[8];

	for (int t = 0; t < 2; t++)
	{
		if (ppLog.triggerState[t] != 2) continue;

		for (int i = 0; i < 8; i++)
		{
			DXWorldToScreen(viewMatrix, ppLog.recordingTrigger[t].points[i], bbWidth, bbHeight, triggerBoxPoints[i]);
			ppPoints[i].x = triggerBoxPoints[i].x;
			ppPoints[i].y = triggerBoxPoints[i].y;
		}

		drawList->AddLine(ppPoints[0], ppPoints[1], color); // //
		drawList->AddLine(ppPoints[0], ppPoints[2], color); // //
		drawList->AddLine(ppPoints[0], ppPoints[4], color); //
		drawList->AddLine(ppPoints[1], ppPoints[3], color); //
		drawList->AddLine(ppPoints[1], ppPoints[5], color); //
		drawList->AddLine(ppPoints[2], ppPoints[3], color); //
		drawList->AddLine(ppPoints[2], ppPoints[6], color); //
		drawList->AddLine(ppPoints[3], ppPoints[7], color); // //
		drawList->AddLine(ppPoints[4], ppPoints[5], color); //
		drawList->AddLine(ppPoints[4], ppPoints[6], color); // //
		drawList->AddLine(ppPoints[5], ppPoints[7], color); //
		drawList->AddLine(ppPoints[6], ppPoints[7], color); //
	}
}