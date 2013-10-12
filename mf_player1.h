#pragma once

#include "resource.h"

class CMyWindow :
	public CWindowImpl<CMyWindow>, public CComObjectRoot,
	public IMFAsyncCallback
{
	// static const UINT WM_MFEVENT = WM_APP + 1;
	BEGIN_COM_MAP(CMyWindow)
		COM_INTERFACE_ENTRY(IMFAsyncCallback)
	END_COM_MAP()
	DECLARE_PROTECT_FINAL_CONSTRUCT();
	BEGIN_MSG_MAP(CMyWindow)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		MESSAGE_HANDLER(WM_SIZE, OnSize)
		MESSAGE_HANDLER(WM_PAINT, OnPaint)
		COMMAND_ID_HANDLER(ID_FILE_START, OnMenuStart)
	END_MSG_MAP()

	CCritSec m_ObjLock;
	CWindow m_Static;
	CComPtr<IMFMediaSession> m_MFSession;
	CComQIPtr<IMFMediaSource> m_Source;
	CComPtr<IMFTopology> m_Topology;
	CComPtr<IMFVideoDisplayControl> m_VideoDisplay;
	CComPtr<ISurfaceSharing> m_SurfSharing;
	CComPtr<IMFClock> m_Clock;
	CDGrph m_DGrph;
	LONGLONG m_Position; // playback position
	Concurrency::task_group m_RenderingTask;
	Concurrency::unbounded_buffer<bool> m_IsMFSessionClosed;
	unsigned int m_MFIndex;
	CComPtr<ID3D10Buffer> m_VtxBuffer;
	CComPtr<ID3D10Effect> m_Effect;
	ID3D10EffectTechnique * m_Technique;
	CComPtr<ID3D10InputLayout> m_InputLayout;
public:
	static CWndClassInfo& GetWndClassInfo();
	HRESULT FinalConstruct();
	void FinalRelease();
	LRESULT OnCreate(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnDestroy(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnSize(UINT, WPARAM wparam, LPARAM lparam, BOOL&);
	LRESULT OnPaint(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnMenuStart(UINT , int , HWND, BOOL&);
	// IMFAsyncCallback
	STDMETHOD(GetParameters)(__RPC__out DWORD *pdwFlags, __RPC__out DWORD *pdwQueue);
	STDMETHOD(Invoke)(IMFAsyncResult *pAsyncResult);
private:
	HRESULT OnMFEvent(IMFMediaEvent * media_event);
	HRESULT OnTopologyReady();
	HRESULT CreateMediaSinkActivate(IMFStreamDescriptor * stream_desc,
		IMFActivate *& activate);
	void AddSourceNode(IMFPresentationDescriptor * pres_desc,
		IMFStreamDescriptor * stream_desc, IMFTopologyNode *& node);
	void AddOutputNode(IMFActivate * activate, IMFTopologyNode *& node);
	int Render();
	void RenderLoop();
};

class CMFSample : public CAtlExeModuleT<CMFSample>
{
	CComObject<CMyWindow> * m_win;
public:
	bool ParseCommandLine(LPCTSTR lpCmdLine, HRESULT * pnRetCode) throw();
	HRESULT PreMessageLoop(int nShowCmd) throw();
	HRESULT PostMessageLoop() throw();
};
