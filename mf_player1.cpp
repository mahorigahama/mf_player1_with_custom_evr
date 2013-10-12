#include "stdafx.h"
#include "DGrph.h"
#include "EvrPres.h"
#include "mf_player1.h"
#include "CustomPresActivateObj.h"

namespace {
	LPCWSTR APP_NAME = L"mf_sample:mf_player1_with_custom_evr_pres";
	LPCWSTR MOVIE_FILE_NAME = L"clock.avi";
}

CMFSample _Module;

struct MyVertex {
	D3DXVECTOR4 Pos;
	D3DXVECTOR4 Color;
	D3DXVECTOR2 Tex;
};

CWndClassInfo& CMyWindow::GetWndClassInfo()
{
	static CWndClassInfo wc = {
		{sizeof(WNDCLASSEX), CS_HREDRAW | CS_VREDRAW, CWindowImplBaseT::StartWindowProc,
		0, 0, NULL, NULL, NULL, (HBRUSH)(COLOR_WINDOW + 1),
		MAKEINTRESOURCE(IDC_MF_PLAYER1), APP_NAME, NULL},
		NULL, NULL, IDC_ARROW, TRUE, 0, _T("")
	};
	return wc;
}

HRESULT CMyWindow::FinalConstruct()
{
	return S_OK;
}

void CMyWindow::FinalRelease()
{
}

LRESULT CMyWindow::OnCreate(UINT, WPARAM, LPARAM, BOOL&)
{
	CRect video_rect;
	GetClientRect(&video_rect);
	m_Static.Create(_T("STATIC"), m_hWnd, video_rect, _T(""), WS_VISIBLE | WS_CHILD | SS_BLACKRECT);
	// Direct3D を初期化する
	CHResult hr = m_DGrph.CreateDevice(m_hWnd);
	// 2D描画に必要な頂点データ、エフェクトを初期化する
	MyVertex vtx[] = {
		{D3DXVECTOR4(  -1,   1, 0, 1), D3DXVECTOR4(0,1,0,1), D3DXVECTOR2(0, 0)},
		{D3DXVECTOR4(   1,   1, 0, 1), D3DXVECTOR4(0,0,1,1), D3DXVECTOR2(1, 0)},
		{D3DXVECTOR4(  -1,  -1, 0, 1), D3DXVECTOR4(1,0,0,1), D3DXVECTOR2(0, 1)},
		{D3DXVECTOR4(   1,  -1, 0, 1), D3DXVECTOR4(1,1,1,1), D3DXVECTOR2(1, 1)},
	};
	D3D10_INPUT_ELEMENT_DESC vertexElements[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0},
		{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D10_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 32, D3D10_INPUT_PER_VERTEX_DATA, 0},
	};
	D3D10_BUFFER_DESC buf_desc = {0};
	buf_desc.ByteWidth = _countof(vtx) * sizeof( MyVertex );
	buf_desc.Usage = D3D10_USAGE_DEFAULT;
	buf_desc.BindFlags = D3D10_BIND_VERTEX_BUFFER;
	buf_desc.CPUAccessFlags = 0;
	buf_desc.MiscFlags = 0;
	D3D10_SUBRESOURCE_DATA subres = {vtx};
	hr = m_DGrph.Device101->CreateBuffer(&buf_desc, &subres, &m_VtxBuffer);
	D3D10_PASS_DESC pass_desc;
	hr = D3DX10CreateEffectFromFile(_T("Simple2D.fx"), NULL, NULL,
		"fx_4_0", D3D10_SHADER_DEBUG | D3D10_SHADER_ENABLE_STRICTNESS, 0, m_DGrph.Device101, NULL, NULL, &m_Effect, NULL, NULL);
	m_Technique = m_Effect->GetTechniqueByName("SimpleRender");
	m_Technique->GetPassByIndex(0)->GetDesc(&pass_desc);
	hr = m_DGrph.Device101->CreateInputLayout(vertexElements, _countof(vertexElements),
		pass_desc.pIAInputSignature, pass_desc.IAInputSignatureSize, &m_InputLayout);
	// レンダーターゲットとビューポートを設定する
	D3D10_TEXTURE2D_DESC backbuffer_desc;
	m_DGrph.BackBuffer->GetDesc(&backbuffer_desc);
	CSize sz(backbuffer_desc.Width, backbuffer_desc.Height);
	m_DGrph.SetRT();
	m_DGrph.SetViewport(sz);
	return 0;
}

LRESULT CMyWindow::OnDestroy(UINT, WPARAM, LPARAM, BOOL&)
{
	ATLTRACE(_T("%S\n"), __FUNCTION__);
	HRESULT hr;
	// Media Session をシャットダウンする
	if (m_MFSession) {
		hr = m_MFSession->Close();
		if (hr == S_OK) {
			// Close は非同期動作なので完了するまで待機する。
			// MF_E_SHUTDOWN のときは何もしない
			bool result;
			result = Concurrency::receive(m_IsMFSessionClosed);
			ATLASSERT(result == true);
		}
	}
	if (m_Source) {
		m_Source->Shutdown();
	}
	if (m_MFSession) {
		m_MFSession->Shutdown();
	}
	// レンダリングループを停止する
	m_RenderingTask.cancel();
	m_RenderingTask.wait();
	// ISurfaceSharing と Direct3D を解放する
	{
		CAutoCritSecLock lock(m_ObjLock);
		m_SurfSharing.Release();
		m_VtxBuffer.Release();
		m_Effect.Release();
		m_InputLayout.Release();
		m_DGrph.ReleaseResources();
	}
	PostQuitMessage(0);
	return 0;
}

LRESULT CMyWindow::OnSize(UINT, WPARAM wparam, LPARAM lparam, BOOL&)
{
	m_Static.MoveWindow(0, 0, LOWORD(lparam), HIWORD(lparam));
	return 0;
}

LRESULT CMyWindow::OnPaint(UINT, WPARAM, LPARAM, BOOL&)
{
	// DO NOTHING.
	// TODO : IMPLEMENT SOMETHING.
	return DefWindowProc();
}

LRESULT CMyWindow::OnMenuStart(UINT , int , HWND, BOOL&)
{
	if (m_MFSession) {
		ATLTRACE(_T("Already initialized.\n"));
		return 0;
	}
	CHResult hr;
	hr = MFCreateMediaSession(NULL, &m_MFSession);
	hr = m_MFSession->BeginGetEvent(this, NULL);

	CComPtr<IUnknown> src;
	MF_OBJECT_TYPE object_type = MF_OBJECT_INVALID;
	CComPtr<IMFSourceResolver> src_resolver;
	hr = MFCreateSourceResolver(&src_resolver);
	hr = src_resolver->CreateObjectFromURL(MOVIE_FILE_NAME, MF_RESOLUTION_MEDIASOURCE, NULL, &object_type, &src);
	ATLASSERT(object_type == MF_OBJECT_MEDIASOURCE);
	m_Source = src;
	ATLASSERT(m_Source != NULL);
	// トポロジを構築する
	CComPtr<IMFPresentationDescriptor> pres_desc;
	DWORD stream_count;
	hr = MFCreateTopology(&m_Topology);
	hr = m_Source->CreatePresentationDescriptor(&pres_desc);
	hr = pres_desc->GetStreamDescriptorCount(&stream_count);
	// 各ストリームごとに、ノードを作成する
	for (DWORD i = 0;i < stream_count;i++) {
		CComPtr<IMFStreamDescriptor> stream_desc;
		CComPtr<IMFActivate> sink_activate;
		CComPtr<IMFTopologyNode> src_node;
		CComPtr<IMFTopologyNode> output_node;
		BOOL selected = FALSE;
		hr = pres_desc->GetStreamDescriptorByIndex(i, &selected, &stream_desc);
		if (!selected) {
			continue;
		}
		hr = CreateMediaSinkActivate(stream_desc, sink_activate.p);
		if (hr == S_OK) {
			AddSourceNode(pres_desc, stream_desc, src_node.p);
			AddOutputNode(sink_activate, output_node.p);
			hr = src_node->ConnectOutput(0, output_node, 0);
		}
	}
	hr = m_MFSession->SetTopology(0, m_Topology);
	return 0;
}

// IMFAsyncCallback::GetParameters
HRESULT CMyWindow::GetParameters( 
	/* [out] */ __RPC__out DWORD *pdwFlags,
	/* [out] */ __RPC__out DWORD *pdwQueue)
{
	return E_NOTIMPL;
}

// IMFAsyncCallback::Invoke
HRESULT CMyWindow::Invoke(IMFAsyncResult *pAsyncResult)
{
	CHResult hr;
	try {
		CComPtr<IMFMediaEvent> media_event;
		MediaEventType me_type;
		hr = m_MFSession->EndGetEvent(pAsyncResult, &media_event);
		hr = media_event->GetType(&me_type);
		OnMFEvent(media_event);
		if (me_type == MESessionClosed) {
			Concurrency::asend(m_IsMFSessionClosed, true);
			// 以降、イベントを待たないので BeginGetEvent は呼ばない。
			return hr;
		}
		hr = m_MFSession->BeginGetEvent(this, NULL);
	}
	catch (CAtlException) {
		if (m_MFSession) {
			return m_MFSession->BeginGetEvent(this, NULL);
		}
		return MF_E_SHUTDOWN;
	}
	return hr;
}

// Media Foundation Event
HRESULT CMyWindow::OnMFEvent(IMFMediaEvent * media_event)
{
	HRESULT hr;
	HRESULT event_status;
	MediaEventType me_type = MEUnknown;
	MF_TOPOSTATUS topo_status = MF_TOPOSTATUS_INVALID;
	hr = media_event->GetType(&me_type);
	hr = media_event->GetStatus(&event_status);
	if (FAILED(event_status)) {
		return 0;
	}
	if (me_type == MESessionTopologyStatus) {
		media_event->GetUINT32(MF_EVENT_TOPOLOGY_STATUS, (UINT32 *)&topo_status);
		if (topo_status == MF_TOPOSTATUS_READY) {
			ATLTRACE(_T("me_type == MESessionTopologyStatus, topo_status = MF_TOPOSTATUS_READY\n"));
			OnTopologyReady();
		}
		else if (topo_status == MF_TOPOSTATUS_STARTED_SOURCE) {
			ATLTRACE(_T("me_type == MESessionTopologyStatus , topo_status = MF_TOPOSTATUS_STARTED_SOURCE\n"));
			hr = MFGetService(m_MFSession, MR_VIDEO_RENDER_SERVICE,
				IID_PPV_ARGS(&m_VideoDisplay));
			hr = m_VideoDisplay->QueryInterface(IID_PPV_ARGS(&m_SurfSharing));
			m_Position = 0;
			m_RenderingTask.run([this]() { RenderLoop();});
		}
		else {
			ATLTRACE(_T("me_type == MESessionTopologyStatus, topo_status = %d\n"), topo_status);
		}
	}
	if (me_type == MESessionStarted) {
		ATLTRACE(_T("MESessionStarted\n"));
	}
	else if (me_type == MESessionPaused) {
		ATLTRACE(_T("MESessionPaused\n"));
	}
	else if (me_type == MESessionStopped) {
		ATLTRACE(_T("MESessionStopped\n"));
		m_RenderingTask.cancel();
		m_RenderingTask.wait();
	}
	else if (me_type == MEEndOfPresentation) {
		ATLTRACE(_T("MEEndOfPresentation\n"));
	}
	else {
		ATLTRACE(_T("me_type = %d event_status = %X\n"), me_type, event_status);
	}
	return 0;
}

HRESULT CMyWindow::OnTopologyReady()
{
	ATLTRACE(_T("%S\n"), __FUNCTION__);
	CHResult hr;
	m_MFSession->GetClock(&m_Clock);
	PROPVARIANT varStart;
	PropVariantInit(&varStart);
	varStart.vt = VT_I8;
	varStart.hVal.QuadPart = 0;
	hr = m_MFSession->Start(NULL, &varStart);
	PropVariantClear(&varStart);
	return hr;
}

HRESULT CMyWindow::CreateMediaSinkActivate(IMFStreamDescriptor * stream_desc,
										   IMFActivate *& activate)
{	
	ATLTRACE(_T("%S\n"), __FUNCTION__);
	CComPtr<IMFMediaTypeHandler> handler;
	GUID major_type = GUID_NULL;
	CHResult hr;
	hr = stream_desc->GetMediaTypeHandler(&handler);
	hr = handler->GetMajorType(&major_type);
	if (major_type == MFMediaType_Audio) {
		hr = MFCreateAudioRendererActivate(&activate);
	}
	else if (major_type == MFMediaType_Video) {
		hr = MFCreateVideoRendererActivate(m_Static.m_hWnd, &activate);
		CComObject<CCustomPresActivateObj> * my_activate_obj;
		CComObject<CCustomPresActivateObj>::CreateInstance(&my_activate_obj);
		PROPVARIANT pval;
		pval.vt = VT_UI8;
		pval.uhVal.QuadPart = (ULONGLONG)(&m_DGrph);
		hr = my_activate_obj->SetItem(__uuidof(CDGrph), pval);
		CComPtr<IUnknown> unk(my_activate_obj);
		hr = activate->SetUnknown(MF_ACTIVATE_CUSTOM_VIDEO_PRESENTER_ACTIVATE, unk);
	}
	else {
		hr = MF_E_INVALIDMEDIATYPE;
	}
	return hr;
}

void CMyWindow::AddSourceNode(IMFPresentationDescriptor * pres_desc,
							  IMFStreamDescriptor * stream_desc, IMFTopologyNode *& node)
{
	ATLTRACE(_T("%S\n"), __FUNCTION__);
	CHResult hr;
	hr = MFCreateTopologyNode(MF_TOPOLOGY_SOURCESTREAM_NODE, &node);
	hr = node->SetUnknown(MF_TOPONODE_SOURCE, m_Source);
	hr = node->SetUnknown(MF_TOPONODE_PRESENTATION_DESCRIPTOR, pres_desc);
	hr = node->SetUnknown(MF_TOPONODE_STREAM_DESCRIPTOR, stream_desc);
	hr = m_Topology->AddNode(node);
}

void CMyWindow::AddOutputNode(IMFActivate * activate, IMFTopologyNode *& node)
{
	ATLTRACE(_T("%S\n"), __FUNCTION__);
	CHResult hr;
	hr = MFCreateTopologyNode(MF_TOPOLOGY_OUTPUT_NODE, &node);
	hr = node->SetObject(activate);
	hr = node->SetUINT32(MF_TOPONODE_STREAMID, 0);
	hr = m_Topology->AddNode(node);
}

/**
* ビデオサンプルをウィンドウへレンダリングする
*/
int CMyWindow::Render()
{
	CHResult hr;
	CAutoCritSecLock lock(m_ObjLock);
	CComPtr<IMFSample> mf_sample;
	if (m_SurfSharing == NULL) {
		return 0;
	}
	// 現在のプレゼンテーション時刻を取得
	LONGLONG clock_time; 
	MFTIME hns_time;
	hr = m_Clock->GetCorrelatedTime(0, &clock_time, &hns_time);
	MFCLOCK_STATE mfc_state;
	m_Clock->GetState(0, &mfc_state);

	LPCRITICAL_SECTION samples_pool_lock;
	m_SurfSharing->GetLock(&samples_pool_lock);
	EnterCriticalSection(samples_pool_lock);
	CComPtr<IMFSample> head_sample;
	LONGLONG sample_time, sample_duration, sample_end_time;
	HANDLE consumed;
	size_t count;
	m_SurfSharing->GetCount(&count);
	while (count > 0) {
		head_sample.Release();
		m_SurfSharing->PeekHead(&head_sample);
		if (head_sample == NULL) {
			// サンプルが1つも届いていない
			break;
		}
		// サンプルが届いている
		UINT64 v;
		head_sample->GetUINT64(ATTR_CONSUMED, &v);
		consumed = (HANDLE)v;
		head_sample->GetSampleTime(&sample_time);
		head_sample->GetSampleDuration(&sample_duration);
		sample_end_time = sample_time + sample_duration;
		if (mfc_state == MFCLOCK_STATE_RUNNING) {
			if (sample_time > clock_time) {
				// このサンプルを使うのは、まだ先
				head_sample.Release();
				/*
				ATLTRACE(_T(" <<*Render clock=%I64d(hns=%I64d) < sample_time=%I64d (too earlier)\n"),
				clock_time, hns_time, sample_time);
				*/
				break;
			}
			if (sample_end_time > clock_time) {
				// このサンプルを使用
				break;
			}
		}
		if (mfc_state == MFCLOCK_STATE_INVALID) {
			// クロックの状態がINVALID。
			// クロックがRUNNINGになる前にこの状態になることがある。
			if (sample_time > 0) {
				// 1フレーム目でない場合、今回はキューに保持したままレンダリングは保留する
				head_sample.Release();
			}
			break;
		}
		// サンプルの期間は経過したので廃棄する
		m_SurfSharing->RemoveHead();
		head_sample.Release();
		SetEvent(consumed);
		// サンプルキューが空であるなら終了
		m_SurfSharing->GetCount(&count);
	}
	if (head_sample == NULL) {
		LeaveCriticalSection(samples_pool_lock);
		return 0;
	}
	ATLTRACE(_T(" << Render clock = %I64d hns_time = %I64d mfc_state = %d, sample_time = %I64d count = %d\n"),
		clock_time, hns_time, mfc_state, sample_time, count);
	ATLASSERT(clock_time >= sample_time);
	m_Position = sample_time;
	// IDirect3DSurface9 を取得
	CComPtr<IMFMediaBuffer> mf_mbuf;
	CComPtr<IDirect3DSurface9> surface;
	head_sample->GetBufferByIndex(0, &mf_mbuf);
	MFGetService(mf_mbuf, MR_BUFFER_SERVICE, IID_PPV_ARGS(&surface));
	// 同期
	D3DLOCKED_RECT locked_rect;
	surface->LockRect(&locked_rect, NULL, D3DLOCK_READONLY);
	surface->UnlockRect();
	// サンプルタイムの文字列を描画
	ID3D10Texture2D * back_buffer = m_DGrph.BackBuffer;
	D3DSURFACE_DESC src_desc;
	D3D10_TEXTURE2D_DESC dst_desc;
	surface->GetDesc(&src_desc);
	back_buffer->GetDesc(&dst_desc);
	{
		CComPtr<ID2D1RenderTarget> d2dtarget;
		CComPtr<ID2D1SolidColorBrush> brush;
		hr = head_sample->GetUnknown(__uuidof(ID2D1RenderTarget), IID_PPV_ARGS(&d2dtarget));
		hr = head_sample->GetUnknown(__uuidof(ID2D1SolidColorBrush), IID_PPV_ARGS(&brush));
		CAtlString text;
		text.Format(_T("sample=%I64d clock=%I64d"), sample_time, clock_time);
		D2D1_RECT_F layout_rect = D2D1::RectF(0, 0, (FLOAT)src_desc.Width, (FLOAT)src_desc.Height);
		m_DGrph.Device101->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_POINTLIST);
		d2dtarget->BeginDraw();
		d2dtarget->DrawText(text, text.GetLength(), m_DGrph.TextFormat, layout_rect, brush);
		hr = d2dtarget->EndDraw();
	}
	// バックバッファへ複写
	D3D10_TECHNIQUE_DESC tech_desc;
	hr = m_Technique->GetDesc(&tech_desc);
	for(UINT p = 0; p < tech_desc.Passes; ++p ) {
		hr = m_Technique->GetPassByIndex(p)->Apply(0);

		m_DGrph.SetRT();
		CComPtr<ID3D10ShaderResourceView1> rs_view;
		hr = head_sample->GetUnknown(__uuidof(ID3D10ShaderResourceView1), IID_PPV_ARGS(&rs_view));
		CComQIPtr<ID3D10ShaderResourceView> rs_view10(rs_view);
		m_DGrph.Device101->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		m_DGrph.Device101->IASetInputLayout(m_InputLayout);
		UINT stride = sizeof(MyVertex);
		UINT offset = 0;
		m_DGrph.Device101->IASetVertexBuffers(0, 1, &m_VtxBuffer.p, &stride, &offset);
		m_DGrph.Device101->PSSetShaderResources(0, 1, &rs_view10.p);
		m_DGrph.Device101->Draw(4, 0);
	}
	m_DGrph.SwapChain->Present(0, 0);
	// 一度レンダリングしたサンプルは破棄する
	m_SurfSharing->RemoveHead();
	SetEvent(consumed);
	LeaveCriticalSection(samples_pool_lock);
	return 0;
}

// レンダリングループ
void CMyWindow::RenderLoop()
{
	DWORD c = 0;
	m_Clock->GetClockCharacteristics(&c);
	ATLASSERT(c & MFCLOCK_CHARACTERISTICS_FLAG_FREQUENCY_10MHZ);
	m_MFIndex = 0;
	ATLTRACE(_T("Begin RenderLoop\n"));
	while (m_RenderingTask.is_canceling() == false) {
		Render();
		Sleep(0);
	}
	ATLTRACE(_T("End RenderLoop\n"));

}

/////////////////////////////////////////////////////////////////////////////////////////////
// CMFSample
bool CMFSample::ParseCommandLine(LPCTSTR lpCmdLine, HRESULT * pnRetCode)
{
	*pnRetCode = S_OK;
	return true;
}

HRESULT CMFSample::PreMessageLoop(int nShowCmd)
{
	CComObject<CMyWindow>::CreateInstance(&m_win);
	m_win->AddRef();
	RECT win_rect = {100, 100, 100 + 640, 100 + 360};
	AdjustWindowRect(&win_rect, WS_OVERLAPPEDWINDOW, TRUE);
	m_win->Create(GetDesktopWindow(), win_rect, APP_NAME, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
	m_win->ShowWindow(nShowCmd);
	m_win->UpdateWindow();
	return S_OK;
}

HRESULT CMFSample::PostMessageLoop()
{
	m_win->Release();
	return S_OK;
}

int APIENTRY _tWinMain(HINSTANCE, HINSTANCE, LPTSTR, int nCmdShow)
{
	HRESULT hr = MFStartup(MF_VERSION);
	if (FAILED(hr)) {
		return hr;
	}
	_Module.WinMain(nCmdShow);
	MFShutdown();
	return 0;
}
