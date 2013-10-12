#pragma once

class SharedSurface
{
public:
	CComPtr<IDirect3DSurface9> Surface9;
	CComPtr<ID3D10Texture2D> Texture10;
	CComQIPtr<IDXGISurface> DxgiSurf;
	CComPtr<ID2D1RenderTarget> D2DTarget;
	CComPtr<ID2D1SolidColorBrush> Brush;
	CComPtr<ID3D10ShaderResourceView1> SRView;
};

class __declspec(uuid("2A7D5E8A-8DE1-4e0a-B10F-ECA8DCE9C86E")) CDGrph
{
	CCritSec m_ObjLock;
	HWND m_Window;
	CComPtr<IDirect3D9Ex> m_D3D9Ex;
	CComPtr<IDirect3DDevice9Ex> m_Device9Ex;
	CComPtr<ID3D10Device1> m_Device101;
	CComPtr<IDXGISwapChain> m_SwapChain;
	CComPtr<ID3D10Texture2D> m_BackBuffer;
	CComPtr<ID2D1Factory> m_D2D1Factory;
	CComPtr<IDWriteFactory> m_DWriteFactory;
	CComPtr<IDWriteTextFormat> m_TextFormat;
	CComPtr<ID3D10RenderTargetView> m_RTView;
	// Direct3Dデバイスを共有するために必要
	UINT m_DeviceResetToken;
	CComPtr<IDirect3DDeviceManager9> m_DevManager;
public:
	CDGrph(void);
	HRESULT CreateDevice(HWND window);
	void ReleaseResources();
	HRESULT CreateMFSample(IMFMediaType * mt, IMFSample ** mf_sample);
	HRESULT CheckFormat(D3DFORMAT format);
	HRESULT CreateSharedSurface(D3DFORMAT format, const CSize & size, SharedSurface & surface);
	HRESULT SetRT();
	HRESULT SetViewport(const SIZE &sz);

	__declspec(property(get = get_D3D9Ex)) IDirect3D9Ex * D3D9Ex;
	IDirect3D9Ex * get_D3D9Ex() const { return m_D3D9Ex; }
	
	__declspec(property(get = get_Device9Ex)) IDirect3DDevice9Ex * Device9Ex;
	IDirect3DDevice9Ex * get_Device9Ex() const { return m_Device9Ex; }
	
	__declspec(property(get = get_DevManager)) IDirect3DDeviceManager9 * DevManager;
	IDirect3DDeviceManager9 * get_DevManager() const { return m_DevManager; }

	__declspec(property(get = get_Device101)) ID3D10Device1 * Device101;
	ID3D10Device1 * get_Device101() const { return m_Device101; }
	
	__declspec(property(get = get_BackBuffer)) ID3D10Texture2D * BackBuffer;
	ID3D10Texture2D * get_BackBuffer() const { return m_BackBuffer; }
	
	__declspec(property(get = get_SwapChain)) IDXGISwapChain * SwapChain;
	IDXGISwapChain * get_SwapChain() const { return m_SwapChain; }
	
	__declspec(property(get = get_D2D1Factory)) ID2D1Factory * D2D1Factory;
	ID2D1Factory * get_D2D1Factory() const { return m_D2D1Factory; }
	
	__declspec(property(get = get_DWriteFactory)) IDWriteFactory * DWriteFactory;
	IDWriteFactory * get_DWriteFactory() const { return m_DWriteFactory; }

	__declspec(property(get = get_TextFormat)) IDWriteTextFormat * TextFormat;
	IDWriteTextFormat * get_TextFormat() const { return m_TextFormat; }

	__declspec(property(get = get_RTView)) ID3D10RenderTargetView * RTView;
	ID3D10RenderTargetView * get_RTView() const { return m_RTView; }
private:
	void CreateDXGISwapDesc(DXGI_SWAP_CHAIN_DESC &dscd) const;
	void CreatePresParam9(D3DPRESENT_PARAMETERS &pp) const;
};
