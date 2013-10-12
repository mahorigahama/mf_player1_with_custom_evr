#pragma once

#include "DGrph.h"

// {2A7DB4BD-C552-4475-9EBC-CA1192D56E3F}
static const GUID ATTR_CONSUMED = 
{ 0x2a7db4bd, 0xc552, 0x4475, { 0x9e, 0xbc, 0xca, 0x11, 0x92, 0xd5, 0x6e, 0x3f } };

__interface __declspec(uuid("{AD4A803A-06DC-45f5-983E-DAE5D1EB10E8}")) ISurfaceSharing : IUnknown
{
	HRESULT STDMETHODCALLTYPE GetLock(LPCRITICAL_SECTION * lock);
	HRESULT STDMETHODCALLTYPE PeekHead(IMFSample ** mf_sample);
	HRESULT STDMETHODCALLTYPE GetCount(size_t * count);
	HRESULT STDMETHODCALLTYPE RemoveHead();
};

enum RENDER_STATE
{
	RENDER_STATE_STARTED = 1,
	RENDER_STATE_STOPPED,
	RENDER_STATE_PAUSED,
	RENDER_STATE_SHUTDOWN,
};

// EVR Presenter coclass
class CEvrPres : public CComObjectRoot,
	public IMFGetService,
	public IMFTopologyServiceLookupClient,
	public IMFVideoDeviceID,
	public IMFVideoPresenter, // inherits from IMFClockStateSink
	public IMFVideoDisplayControl,
	public ISurfaceSharing
{
public:
	DECLARE_PROTECT_FINAL_CONSTRUCT()
	BEGIN_COM_MAP(CEvrPres)
		COM_INTERFACE_ENTRY(IMFGetService)
		COM_INTERFACE_ENTRY(IMFTopologyServiceLookupClient)
		COM_INTERFACE_ENTRY(IMFVideoDeviceID)
		COM_INTERFACE_ENTRY(IMFVideoPresenter)
		COM_INTERFACE_ENTRY(IMFClockStateSink)
		COM_INTERFACE_ENTRY(IMFVideoDisplayControl)
		COM_INTERFACE_ENTRY(ISurfaceSharing)
	END_COM_MAP()
private: 
	CCritSec m_ObjLock;
	CCritSec m_SamplesPoolLock;
	RENDER_STATE m_RenderState;
	CInterfaceArray<IMFSample>	m_SamplesPool;
	CInterfaceList<IMFSample>	m_QueuedSamples;
	std::vector<HANDLE>			m_Consumed;
	CComPtr<IMFTransform>		m_Mixer;
	CComPtr<IMediaEventSink>	m_MediaEventSink;
	CComPtr<IMFClock>			m_Clock;
	CComPtr<IMFMediaType>		m_MediaType;
	bool m_EOS;
	bool m_InputNotify;
	CDGrph * m_DGrph;

	Concurrency::task_group m_QueuingTask;
	// Concurrency::unbounded_buffer<bool> m_IsMFSessionClosed;
public:
	HRESULT FinalConstruct();
	void FinalRelease();
	__declspec(property(get=get_IsShutdowned)) HRESULT IsShutdowned;
	HRESULT get_IsShutdowned() const;
	__declspec(property(get=get_IsActive)) bool IsActive;
	bool get_IsActive() const;
	// IMFGetService
	HRESULT STDMETHODCALLTYPE GetService(REFGUID guidService, REFIID riid, LPVOID * ppvObject);
	// IMFTopologyServiceLookupClient
	HRESULT STDMETHODCALLTYPE InitServicePointers(IMFTopologyServiceLookup *pLookup);
	HRESULT STDMETHODCALLTYPE ReleaseServicePointers(void);
	// IMFVideoDeviceID
	HRESULT STDMETHODCALLTYPE GetDeviceID(IID * pDeviceID);
	// IMFVideoPresenter
	STDMETHOD(ProcessMessage)(MFVP_MESSAGE_TYPE eMessage, ULONG_PTR ulParam);
	STDMETHOD(GetCurrentMediaType)(IMFVideoMediaType** ppMediaType);
	// IMFClockStateSink
	HRESULT STDMETHODCALLTYPE OnClockStart(MFTIME hnsSystemTime, LONGLONG llClockStartOffset) override;
	HRESULT STDMETHODCALLTYPE OnClockStop(MFTIME hnsSystemTime);
	HRESULT STDMETHODCALLTYPE OnClockPause(MFTIME hnsSystemTime);
	HRESULT STDMETHODCALLTYPE OnClockRestart(MFTIME hnsSystemTime);
	HRESULT STDMETHODCALLTYPE OnClockSetRate(MFTIME hnsSystemTime, float flRate);
	// IMFVideoDisplayControl
	HRESULT STDMETHODCALLTYPE GetNativeVideoSize(SIZE *pszVideo, SIZE *pszARVideo);
	HRESULT STDMETHODCALLTYPE GetIdealVideoSize(SIZE *pszMin, SIZE *pszMax);
	HRESULT STDMETHODCALLTYPE SetVideoPosition(const MFVideoNormalizedRect *pnrcSource, const LPRECT prcDest);
	HRESULT STDMETHODCALLTYPE GetVideoPosition(MFVideoNormalizedRect *pnrcSource, LPRECT prcDest);
	HRESULT STDMETHODCALLTYPE SetAspectRatioMode(DWORD dwAspectRatioMode);
	HRESULT STDMETHODCALLTYPE GetAspectRatioMode(DWORD *pdwAspectRatioMode);
	HRESULT STDMETHODCALLTYPE SetVideoWindow(HWND hwndVideo);
	HRESULT STDMETHODCALLTYPE GetVideoWindow(HWND *phwndVideo);
	HRESULT STDMETHODCALLTYPE RepaintVideo(void);
	HRESULT STDMETHODCALLTYPE GetCurrentImage(BITMAPINFOHEADER *pBih, BYTE **pDib, DWORD *pcbDib, LONGLONG *pTimeStamp);
	HRESULT STDMETHODCALLTYPE SetBorderColor(COLORREF Clr);
	HRESULT STDMETHODCALLTYPE GetBorderColor(COLORREF *pClr);
	HRESULT STDMETHODCALLTYPE SetRenderingPrefs(DWORD dwRenderFlags);
	HRESULT STDMETHODCALLTYPE GetRenderingPrefs(DWORD *pdwRenderFlags);
	HRESULT STDMETHODCALLTYPE SetFullscreen(BOOL fFullscreen);
	HRESULT STDMETHODCALLTYPE GetFullscreen(BOOL *pfFullscreen);
	// ISurfaceSharing
	HRESULT STDMETHODCALLTYPE GetLock(LPCRITICAL_SECTION * lock);
	HRESULT STDMETHODCALLTYPE PeekHead(IMFSample ** mf_sample);
	HRESULT STDMETHODCALLTYPE GetCount(size_t * count);
	HRESULT STDMETHODCALLTYPE RemoveHead();
	// このクラス固有のメソッド
	void Init(IMFAttributes * attr);
	HRESULT SetMixerSourceRect();
	HRESULT SetMediaType(IMFMediaType * mt);
	HRESULT NotifyEvent(long EventCode, LONG_PTR Param1, LONG_PTR Param2);
	HRESULT RenegotiateMediaType();
	HRESULT IsMediaTypeSupported(IMFMediaType * mt);
	HRESULT CreateOptimalVideoType(IMFMediaType * propsed_mt, IMFMediaType ** optimal_mt);
	HRESULT BeginStreaming();
	HRESULT EndStreaming();
	HRESULT ProcessInputNotify();
	HRESULT ProcessOutputLoop();
	HRESULT ProcessOutput();
	HRESULT CheckEndOfStream();
	HRESULT Flush();
	void ReleaseResources();
	HRESULT PrepareFrameStep(DWORD step);
	void QueuingTask();
};
