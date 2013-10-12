 #include "StdAfx.h"
#include "DGrph.h"

CDGrph::CDGrph() : m_Window(NULL)
{
}

HRESULT CDGrph::CreateDevice(HWND window)
{
	CHResult hr;
	CAutoCritSecLock lock(m_ObjLock);
	m_Window = window;
	// Direct3D10.1
	DXGI_SWAP_CHAIN_DESC dscd;
	CreateDXGISwapDesc(dscd);
	hr = D3D10CreateDeviceAndSwapChain1(NULL, D3D10_DRIVER_TYPE_HARDWARE,
		NULL,
#ifdef _DEBUG
		D3D10_CREATE_DEVICE_DEBUG | D3D10_CREATE_DEVICE_BGRA_SUPPORT,
#else
		D3D10_CREATE_DEVICE_BGRA_SUPPORT,
#endif
		D3D10_FEATURE_LEVEL_10_0, D3D10_1_SDK_VERSION, &dscd,
		&m_SwapChain, &m_Device101);
	hr = m_SwapChain->GetBuffer(0, IID_PPV_ARGS(&m_BackBuffer));
	hr = m_Device101->CreateRenderTargetView(m_BackBuffer, NULL, &m_RTView);
	// Direct3D9Ex
	hr = Direct3DCreate9Ex(D3D_SDK_VERSION, &m_D3D9Ex);
	D3DPRESENT_PARAMETERS d3d9pp;
	CreatePresParam9(d3d9pp);
	hr = m_D3D9Ex->CreateDeviceEx(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window,
		D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED,
		&d3d9pp, NULL, &m_Device9Ex);
	// D3D9 Device Manager
	hr = DXVA2CreateDirect3DDeviceManager9(&m_DeviceResetToken, &m_DevManager);
	hr = m_DevManager->ResetDevice(m_Device9Ex, m_DeviceResetToken);
	// Direct2D, DirectWrite
	hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED, &m_D2D1Factory);
	hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
		__uuidof(IDWriteFactory), (IUnknown **)&m_DWriteFactory);
	hr = m_DWriteFactory->CreateTextFormat(L"Consolas", NULL,
		DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
		DWRITE_FONT_STRETCH_NORMAL, 50.f, L"en-US", &m_TextFormat);
	return S_OK;
}

// メディアサンプル(IMFSample) を作成する
HRESULT CDGrph::CreateMFSample(IMFMediaType * mt, IMFSample ** mf_sample)
{
	CHResult hr;
	ATLENSURE_RETURN_HR(m_Window, MF_E_INVALIDREQUEST);
	ATLENSURE_RETURN_HR(mt, MF_E_UNEXPECTED);
	ATLENSURE_RETURN_HR(mf_sample, MF_E_UNEXPECTED);
	CAutoCritSecLock lock(m_ObjLock);
	// サーフェイスフォーマットを決める
	UINT32 width, height;
	hr = MFGetAttributeSize(mt, MF_MT_FRAME_SIZE, &width, &height);
	// note : SUBTYPEのData1はD3DFORMATと同じ値
	// http://msdn.microsoft.com/en-us/library/aa370819(VS.85).aspx
	GUID sub_type;
	hr = mt->GetGUID(MF_MT_SUBTYPE, &sub_type);
	D3DFORMAT d3dformat = (D3DFORMAT)sub_type.Data1;
	ATLASSERT(d3dformat == D3DFMT_X8R8G8B8);
	// サーフェイス作成
	SharedSurface surf;
	hr = CreateSharedSurface(D3DFMT_A8R8G8B8, CSize(width, height), surf);
	hr = MFCreateVideoSampleFromSurface(surf.Surface9, mf_sample);
	hr = (*mf_sample)->SetUnknown(__uuidof(ID3D10Texture2D), surf.Texture10);
	hr = (*mf_sample)->SetUnknown(__uuidof(ID3D10ShaderResourceView1), surf.SRView);
	hr = (*mf_sample)->SetUnknown(__uuidof(IDXGISurface), surf.DxgiSurf);
	hr = (*mf_sample)->SetUnknown(__uuidof(ID2D1RenderTarget) , surf.D2DTarget);
	hr = (*mf_sample)->SetUnknown(__uuidof(ID2D1SolidColorBrush), surf.Brush);
	return S_OK;
}

// アダプタでハードウェア アクセラレーション デバイス タイプが使用可能かどうかを検証する.
// http://msdn.microsoft.com/ja-jp/library/cc324297.aspx
HRESULT CDGrph::CheckFormat(D3DFORMAT format)
{
	CHResult hr;
	D3DDISPLAYMODE mode;
	hr = m_D3D9Ex->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &mode);
	hr = m_D3D9Ex->CheckDeviceType(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
		mode.Format, format, TRUE);
	return hr;
}

// Direct3D10.1 - Direct3D9Ex 共有サーフェイスを作成する
HRESULT CDGrph::CreateSharedSurface(D3DFORMAT format, const CSize & size, SharedSurface & surface)
{
	ATLENSURE_RETURN_HR(m_Device9Ex, E_UNEXPECTED);
	ATLENSURE_RETURN_HR(m_Device101, E_UNEXPECTED);
	ATLENSURE_RETURN_HR(m_D2D1Factory, E_UNEXPECTED);
	CHResult hr;
	HANDLE shared_handle = NULL;
	hr = m_Device9Ex->CreateRenderTarget(size.cx, size.cy, format,
		D3DMULTISAMPLE_NONE, 0, TRUE, &surface.Surface9, &shared_handle);
	hr = m_Device101->OpenSharedResource(shared_handle, IID_PPV_ARGS(&surface.Texture10));
	surface.DxgiSurf = surface.Texture10;
	D2D1_RENDER_TARGET_PROPERTIES props =
		D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,
		D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED));
	hr = m_D2D1Factory->CreateDxgiSurfaceRenderTarget(surface.DxgiSurf, &props, &surface.D2DTarget);
	hr = surface.D2DTarget->CreateSolidColorBrush(D2D1::ColorF(0xffffff, 1.0f), &surface.Brush);
	// Shader Resource View
	D3D10_TEXTURE2D_DESC src_desc10;
	surface.Texture10->GetDesc(&src_desc10);
	D3D10_SHADER_RESOURCE_VIEW_DESC1 view_desc = {src_desc10.Format, D3D10_1_SRV_DIMENSION_TEXTURE2D};
	view_desc.Texture2D.MipLevels = src_desc10.MipLevels;
	view_desc.Texture2D.MostDetailedMip = 0;
	hr = Device101->CreateShaderResourceView1(surface.Texture10, &view_desc, &surface.SRView);
	ATLASSERT((src_desc10.BindFlags & D3D10_BIND_SHADER_RESOURCE) != 0);
	ATLASSERT((src_desc10.MiscFlags & D3D10_RESOURCE_MISC_SHARED) != 0);
	return hr;
}

// DXGI_SWAP_CHAIN_DESCを作成する
void CDGrph::CreateDXGISwapDesc(DXGI_SWAP_CHAIN_DESC &dscd) const
{
	ZeroMemory(&dscd, sizeof(DXGI_SWAP_CHAIN_DESC));
	dscd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	dscd.SwapEffect = DXGI_SWAP_EFFECT_SEQUENTIAL;
	dscd.BufferCount = 1;
	dscd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	dscd.OutputWindow = m_Window;
	dscd.SampleDesc.Count = 1;
	dscd.Windowed = TRUE;
}

// D3DPRESENT_PARAMETERS を作成する
void CDGrph::CreatePresParam9(D3DPRESENT_PARAMETERS &d3d9pp) const
{
	ZeroMemory(&d3d9pp, sizeof(D3DPRESENT_PARAMETERS));
	d3d9pp.Windowed = TRUE;
	d3d9pp.hDeviceWindow = m_Window;
	d3d9pp.SwapEffect = D3DSWAPEFFECT_COPY;
	d3d9pp.BackBufferFormat = D3DFMT_A8R8G8B8;
	d3d9pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
}

void CDGrph::ReleaseResources()
{
	m_D3D9Ex.Release();
	m_Device9Ex.Release();
	m_Device101.Release();
	m_SwapChain.Release();
	m_BackBuffer.Release();
	m_D2D1Factory.Release();
	m_DWriteFactory.Release();
	m_DevManager.Release();
	m_RTView.Release();
	m_TextFormat.Release();
}

HRESULT CDGrph::SetRT()
{
	m_Device101->OMSetRenderTargets(1, &m_RTView.p, NULL);
	return S_OK;
}

HRESULT CDGrph::SetViewport(const SIZE &sz)
{
	D3D10_VIEWPORT v;
	v.TopLeftX = 0;
	v.TopLeftY = 0;
	v.Width  = sz.cx;
	v.Height = sz.cy;
	v.MinDepth = 0.0f;
	v.MaxDepth = 1.0f;
	m_Device101->RSSetViewports(1, &v);
	return S_OK;
}
