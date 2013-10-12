#include "StdAfx.h"
#include "EvrPres.h"

namespace {
	const DWORD MFSAMPLE_TIMEOUT = 100;
	size_t BUFFER_SIZE = 10;
	const LPCTSTR MFVP_TXT[] = {
		_T("MFVP_MESSAGE_FLUSH"),
		_T("MFVP_MESSAGE_INVALIDATEMEDIATYPE"),
		_T("MFVP_MESSAGE_PROCESSINPUTNOTIFY"),
		_T("MFVP_MESSAGE_BEGINSTREAMING"),
		_T("MFVP_MESSAGE_ENDSTREAMING"),
		_T("MFVP_MESSAGE_ENDOFSTREAM"),
		_T("MFVP_MESSAGE_STEP"),
		_T("MFVP_MESSAGE_CANCELSTEP"),
	};
}

HRESULT CEvrPres::FinalConstruct()
{
	m_RenderState = RENDER_STATE_SHUTDOWN;
	::MFCreateMediaType(&m_MediaType);
	m_EOS = false;
	m_DGrph = NULL;
	return S_OK;
}

void CEvrPres::FinalRelease()
{
	ReleaseResources();
}

// シャットダウン済みかどうか (プロパティ)
HRESULT CEvrPres::get_IsShutdowned() const
{
	return m_RenderState == RENDER_STATE_SHUTDOWN ? MF_E_SHUTDOWN : S_OK;
}

// アクティブかどうか (プロパティ)
bool CEvrPres::get_IsActive() const
{
	return ((m_RenderState == RENDER_STATE_STARTED) || (m_RenderState == RENDER_STATE_PAUSED));
}

// IMFGetService
HRESULT CEvrPres::GetService(REFGUID guidService, REFIID riid, LPVOID * ppvObject)
{
	ATLENSURE_RETURN_HR(ppvObject, E_POINTER);
	if (riid == __uuidof(IDirect3DDeviceManager9)) {
		ATLENSURE_RETURN_HR(m_DGrph->DevManager, E_UNEXPECTED);
		*ppvObject = m_DGrph->DevManager;
		m_DGrph->DevManager->AddRef();
		return S_OK;
	}
	return QueryInterface(riid, ppvObject);
}

// IMFTopologyServiceLookupClient
HRESULT CEvrPres::InitServicePointers(IMFTopologyServiceLookup * pLookup)
{
	ATLENSURE_RETURN_HR(pLookup != NULL, E_POINTER);
	CAutoCritSecLock lock(m_ObjLock);
	CHResult hr;
	DWORD obj_count;
	ATLENSURE_RETURN_HR(IsActive == false, MF_E_INVALIDREQUEST);
	m_Clock.Release();
	m_Mixer.Release();
	m_MediaEventSink.Release();
	obj_count = 1; 
	hr = pLookup->LookupService(MF_SERVICE_LOOKUP_GLOBAL, 0, 
		MR_VIDEO_MIXER_SERVICE, IID_PPV_ARGS(&m_Mixer), &obj_count);
	CComQIPtr<IMFVideoDeviceID> vid_dev_id(m_Mixer);
	IID dev_id = GUID_NULL;
	hr = vid_dev_id->GetDeviceID(&dev_id);
	if (dev_id != __uuidof(IDirect3DDevice9)) {
		return MF_E_INVALIDREQUEST;
	}
	obj_count = 1;
	hr = pLookup->LookupService(MF_SERVICE_LOOKUP_GLOBAL, 0,
		MR_VIDEO_RENDER_SERVICE, IID_PPV_ARGS(&m_MediaEventSink), &obj_count);
	// クロックを取得する。(無い場合もある。)
	obj_count = 1;
	pLookup->LookupService(MF_SERVICE_LOOKUP_GLOBAL, 0,
		MR_VIDEO_RENDER_SERVICE, IID_PPV_ARGS(&m_Clock), &obj_count);
	m_RenderState = RENDER_STATE_STOPPED;
	SetMixerSourceRect();
	return S_OK;
}

HRESULT CEvrPres::ReleaseServicePointers(void)
{
	{
		CAutoCritSecLock lock(m_ObjLock);
		m_RenderState = RENDER_STATE_SHUTDOWN;
	}
	Flush();
	SetMediaType(NULL);
	m_Clock.Release();
	m_Mixer.Release();
	m_MediaEventSink.Release();
	return S_OK;
}

// IMFVideoDeviceID
/**
 * デバイスIDを返す
 * 標準EVRミキサーを使う場合は __uuidof(IDirect3DDevice9) でなければならないです。
 * http://msdn.microsoft.com/en-us/library/ms704630(VS.85).aspx
 */
HRESULT CEvrPres::GetDeviceID(IID * pDeviceID)
{
	ATLTRACE(L"%S\n", __FUNCTION__);
	ATLENSURE_RETURN_HR(pDeviceID != NULL, E_POINTER);
	*pDeviceID = __uuidof(IDirect3DDevice9);
	return S_OK;
}

// IMFVideoPresenter

/**
 * EVR からメッセージを受信したとき
 */
HRESULT CEvrPres::ProcessMessage(MFVP_MESSAGE_TYPE eMessage, ULONG_PTR ulParam)
{
	HRESULT hr;
	CAutoCritSecLock lock(m_ObjLock); // メッセージ処理中はスレッドセーフにする
	// if (eMessage != MFVP_MESSAGE_PROCESSINPUTNOTIFY) {
		ATLTRACE(L"%I64d %S msg = %s(%d)\n",
			::MFGetSystemTime(), __FUNCTION__, MFVP_TXT[eMessage], eMessage);
	// }
	ATLENSURE_RETURN_HR(SUCCEEDED(IsShutdowned), IsShutdowned);
	struct func_table {
		MFVP_MESSAGE_TYPE msg;
		std::function<HRESULT (void)> func;
	};
	const std::array<func_table, 9> funcs = { {
		// Flush all pending samples.
		{MFVP_MESSAGE_FLUSH, [this]() -> HRESULT { return Flush(); }},
		// Renegotiate the media type with the mixer.
		{MFVP_MESSAGE_INVALIDATEMEDIATYPE, [this]() -> HRESULT { return RenegotiateMediaType(); }},
		// The mixer received a new input sample. 
		{MFVP_MESSAGE_PROCESSINPUTNOTIFY, [this]() -> HRESULT { return ProcessInputNotify(); }},
		// Streaming is about to start.
		{MFVP_MESSAGE_BEGINSTREAMING, [this]() -> HRESULT { return BeginStreaming(); }},
		// Streaming has ended. (The EVR has stopped.)
		{MFVP_MESSAGE_ENDSTREAMING, [this]() -> HRESULT { return EndStreaming(); }},
		// All input streams have ended.
		{MFVP_MESSAGE_ENDOFSTREAM, [this]() -> HRESULT { m_EOS = true; return CheckEndOfStream(); }},
		// Frame-stepping is starting.
		{MFVP_MESSAGE_STEP, [this, ulParam]() -> HRESULT { return PrepareFrameStep((DWORD)(ulParam & 0xffffffff)); }},
		// Cancels frame-stepping.
		{MFVP_MESSAGE_CANCELSTEP, [this]() -> HRESULT { return E_NOTIMPL; }},
	} };
	m_InputNotify = false;
	auto it = std::find_if(funcs.begin(), funcs.end(),
		[eMessage](const func_table & a) -> bool { return eMessage == a.msg; });
	if (it != funcs.end()) {
		hr = it->func();
	}
	else {
		// 未知のメッセージ
		hr = E_INVALIDARG;
		ATLENSURE_THROW(false, E_INVALIDARG);
	}
	return hr;
}

// 現在のメディアタイプを返す
HRESULT CEvrPres::GetCurrentMediaType(IMFVideoMediaType ** ppMediaType)
{
	ATLTRACE(L"%S\n", __FUNCTION__);
	ATLENSURE_RETURN_HR(ppMediaType, E_POINTER);
	*ppMediaType = NULL;
	CAutoCritSecLock lock(m_ObjLock);
	ATLENSURE_RETURN_HR(SUCCEEDED(IsShutdowned), IsShutdowned);
	if (m_MediaType == NULL) {
		return MF_E_NOT_INITIALIZED;
	}
	HRESULT hr = m_MediaType.QueryInterface<IMFVideoMediaType>(ppMediaType);
	return hr;
}

// IMFClockStateSink
/*
1.Set the presenter state to started. 
2.If the llClockStartOffset is not PRESENTATION_CURRENT_POSITION, flush the presenter's queue of samples. (This is equivalent to receiving an MFVP_MESSAGE_FLUSH message.) 
3.If a previous frame-step request is still pending, process the request (see Frame Stepping). Otherwise, try to process output from the mixer (see Processing Output). 
*/
HRESULT CEvrPres::OnClockStart(MFTIME hnsSystemTime, LONGLONG llClockStartOffset)
{
	ATLTRACE(L"%S Begin\n", __FUNCTION__);
	CAutoCritSecLock lock(m_ObjLock);
	ATLENSURE_RETURN_HR(SUCCEEDED(IsShutdowned), IsShutdowned);
	HRESULT hr = S_OK;
	if (IsActive) {
		m_RenderState = RENDER_STATE_STARTED;
		// If the clock position changes while the clock is active, it 
		// is a seek request. We need to flush all pending samples.
		if (llClockStartOffset != PRESENTATION_CURRENT_POSITION) {
			Flush();
		}
	}
	else {
		m_RenderState = RENDER_STATE_STARTED;
		// The clock has started from the stopped state. 
		// Possibly we are in the middle of frame-stepping OR have samples waiting 
		// in the frame-step queue. Deal with these two cases first:
		// hr = StartFrameStep();
	}
	// Now try to get new output samples from the mixer.
	ProcessOutputLoop();
	ATLTRACE(L"%S End\n", __FUNCTION__);
	return hr;
}

/*
1.Set the presenter state to stopped. 
2.Flush the presenter's queue of samples. 
3.Cancel any pending frame-step operation. 
*/
HRESULT CEvrPres::OnClockStop(MFTIME hnsSystemTime)
{
	ATLTRACE(L"%S\n", __FUNCTION__);
	CAutoCritSecLock lock(m_ObjLock);
	ATLENSURE_RETURN_HR(SUCCEEDED(IsShutdowned), IsShutdowned);
	if (m_RenderState == RENDER_STATE_STOPPED) {
		return S_OK;
	}
	m_RenderState = RENDER_STATE_STOPPED;
	Flush();
	// If we are in the middle of frame-stepping, cancel it now.
	/*
	if (m_FrameStep.state != FRAMESTEP_NONE) {
	CancelFrameStep();
	}
	*/
	return S_OK;
}

// 停止
HRESULT CEvrPres::OnClockPause(MFTIME hnsSystemTime)
{
	ATLTRACE(L"%S\n", __FUNCTION__);
	CAutoCritSecLock lock(m_ObjLock);
	ATLENSURE_RETURN_HR(SUCCEEDED(IsShutdowned), IsShutdowned);
	m_RenderState = RENDER_STATE_PAUSED;
	return S_OK;
}

/*
Treat the same as OnClockStart but do not flush the queue of samples.
*/
HRESULT CEvrPres::OnClockRestart(MFTIME hnsSystemTime)
{
	ATLTRACE(L"%S\n", __FUNCTION__);
	CAutoCritSecLock lock(m_ObjLock);
	ATLENSURE_RETURN_HR(SUCCEEDED(IsShutdowned), IsShutdowned);
	ATLASSERT(m_RenderState == RENDER_STATE_PAUSED);
	m_RenderState = RENDER_STATE_STARTED;
	ProcessOutputLoop();
	return S_OK;
}

/**
 * クロックレートを設定する
 * (未実装です)
 */
HRESULT CEvrPres::OnClockSetRate(MFTIME hnsSystemTime, float flRate)
{
	ATLTRACE(L"%S\n", __FUNCTION__);
	CAutoCritSecLock lock(m_ObjLock);
	ATLENSURE_RETURN_HR(SUCCEEDED(IsShutdowned), IsShutdowned);
	return S_OK;
}

// IMFVideoDisplayControl
HRESULT CEvrPres::GetNativeVideoSize(SIZE *pszVideo, SIZE *pszARVideo)
{
	ATLTRACE(L"%S\n", __FUNCTION__);
	return E_NOTIMPL;
}

HRESULT CEvrPres::GetIdealVideoSize(SIZE *pszMin, SIZE *pszMax)
{
	ATLTRACE(L"%S\n", __FUNCTION__);
	return E_NOTIMPL;
}

HRESULT CEvrPres::SetVideoPosition(const MFVideoNormalizedRect *pnrcSource, const LPRECT prcDest)
{
	CAutoCritSecLock lock(m_ObjLock);
	ATLTRACE(L"%S\n", __FUNCTION__);
	ATLENSURE_RETURN_HR(SUCCEEDED(IsShutdowned), IsShutdowned);
	HRESULT hr;
	ATLASSERT(pnrcSource == NULL);
	if (prcDest) {
		hr = RenegotiateMediaType();
		if (hr == MF_E_TRANSFORM_TYPE_NOT_SET) {
			// ミキサーの出力タイプが確定していないので、
			// メディアタイプを確定できない。
			ATLTRACE(L"hr = MF_E_TRANSFORM_TYPE_NOT_SET\n");
			return S_OK;
		}
	}
	return hr;
}

HRESULT CEvrPres::GetVideoPosition(MFVideoNormalizedRect *pnrcSource, LPRECT prcDest)
{
	return E_NOTIMPL;
}

HRESULT CEvrPres::SetAspectRatioMode(DWORD dwAspectRatioMode)
{
	return E_NOTIMPL;
}

HRESULT CEvrPres::GetAspectRatioMode(DWORD *pdwAspectRatioMode)
{
	return E_NOTIMPL;
}

HRESULT CEvrPres::SetVideoWindow(HWND hwndVideo)
{
	// 何もしない
	return S_OK;
}

HRESULT CEvrPres::GetVideoWindow(HWND *phwndVideo)
{
	// 何もしない
	return S_OK;
}

HRESULT CEvrPres::RepaintVideo(void)
{
	// 何もしない。このプレゼンタ自体、画面描画は行わない。
	return S_OK;
}

HRESULT CEvrPres::GetCurrentImage(BITMAPINFOHEADER *pBih, BYTE **pDib, DWORD *pcbDib, LONGLONG *pTimeStamp)
{
	return E_NOTIMPL;
}

HRESULT CEvrPres::SetBorderColor(COLORREF Clr)
{
	return E_NOTIMPL;
}

HRESULT CEvrPres::GetBorderColor(COLORREF *pClr)
{
	return E_NOTIMPL;
}

HRESULT CEvrPres::SetRenderingPrefs(DWORD dwRenderFlags)
{
	return E_NOTIMPL;
}

HRESULT CEvrPres::GetRenderingPrefs(DWORD *pdwRenderFlags)
{
	return E_NOTIMPL;
}

HRESULT CEvrPres::SetFullscreen(BOOL fFullscreen)
{
	return E_NOTIMPL;
}

HRESULT CEvrPres::GetFullscreen(BOOL *pfFullscreen)
{
	return E_NOTIMPL;
}

// ISurfaceSharing
HRESULT CEvrPres::GetLock(LPCRITICAL_SECTION * lock)
{
	(*lock) = &m_SamplesPoolLock.m_sec;
	return S_OK;
}

HRESULT CEvrPres::PeekHead(IMFSample ** mf_sample)
{
	if (m_QueuedSamples.GetCount() == 0) {
		(*mf_sample) = NULL;
		return S_FALSE;
	}
	(*mf_sample) = m_QueuedSamples.GetHead().p;
	(*mf_sample)->AddRef();
	return S_OK;
}

HRESULT CEvrPres::GetCount(size_t * count)
{
	(*count) = m_QueuedSamples.GetCount();
	return S_OK;
}

HRESULT CEvrPres::RemoveHead()
{
	m_QueuedSamples.RemoveHead();
	return S_OK;
}

/**
 * 初期化
 */
void CEvrPres::Init(IMFAttributes * attr)
{
	PROPVARIANT val;
	attr->GetItem(__uuidof(CDGrph), &val);
	m_DGrph = (CDGrph *)val.uhVal.QuadPart;
	PropVariantClear(&val);
}

/**
 * ソース画像の矩形を全体に設定する
 */
HRESULT CEvrPres::SetMixerSourceRect()
{
	ATLENSURE_RETURN_HR(m_Mixer, E_UNEXPECTED);
	HRESULT hr;
	CComPtr<IMFAttributes> attr;
	hr = m_Mixer->GetAttributes(&attr);
	MFVideoNormalizedRect nrc_src = {0, 0, 1, 1};
	hr = attr->SetBlob(VIDEO_ZOOM_RECT, (const UINT8*)&nrc_src, sizeof(nrc_src));
	return hr;
}

/**
 * メディアタイプをこのプレゼンタに設定する
 * @param [in] mt メディアタイプ
 */
HRESULT CEvrPres::SetMediaType(IMFMediaType * mt)
{
	if (mt == NULL) {
		ATLTRACE(L"%S(mt==NULL)\n", __FUNCTION__);
		m_MediaType.Release();
		ReleaseResources();
		return S_OK;
	}
	ATLTRACE(L"%S\n", __FUNCTION__);
	ATLENSURE_RETURN_HR(SUCCEEDED(IsShutdowned), IsShutdowned);
	DWORD flags;
	if (m_MediaType) {
		HRESULT is_equal = m_MediaType->IsEqual(mt, &flags);
		if (is_equal == S_OK) {
			// DO NOTHING
			return S_OK;
		}
		m_MediaType.Release();
	}
	ReleaseResources();
	CHResult hr;
	try {
		m_SamplesPool.SetCount(BUFFER_SIZE);
		for (size_t n = 0;n < m_SamplesPool.GetCount(); n++) {
			CComPtr<IMFSample> obj;
			hr = m_DGrph->CreateMFSample(mt, &obj);
			HANDLE e = CreateEvent(NULL, TRUE, TRUE, NULL);
			obj->SetUINT64(ATTR_CONSUMED, (UINT64)e);
			m_SamplesPool.SetAt(n, obj);
			m_Consumed.push_back(e);
		}
		ATLASSERT(m_SamplesPool.GetCount() == m_Consumed.size());
	}
	catch (CAtlException &e) {
		m_SamplesPool.RemoveAll();
		return e.m_hr;
	}
	m_MediaType.Attach(mt);
	mt->AddRef();
	return hr;
}

// EVRにイベントを通知
HRESULT CEvrPres::NotifyEvent(long EventCode, LONG_PTR Param1, LONG_PTR Param2)
{
	HRESULT hr;
	ATLTRACE(L"%S\n", __FUNCTION__);
	if (m_MediaEventSink) {
		hr = m_MediaEventSink->Notify(EventCode, Param1, Param2);
	}
	else {
		ATLASSERT(false);
		return E_FAIL;
	}
	return hr;
}

// ミキサーの出力タイプの設定を試みる
HRESULT CEvrPres::RenegotiateMediaType()
{
	ATLTRACE(L"%S\n", __FUNCTION__);
	HRESULT hr = S_OK;
	CComPtr<IMFMediaType> mixer_type;
	CComPtr<IMFMediaType> optimal_type;
	CComPtr<IMFVideoMediaType> video_type;
	ATLENSURE_RETURN_HR(m_Mixer != NULL, MF_E_INVALIDREQUEST);
	DWORD type_index = 0;
	while (hr != MF_E_NO_MORE_TYPES) {
		mixer_type.Release();
		hr = m_Mixer->GetOutputAvailableType(0, type_index++, &mixer_type);
		if (FAILED(hr)) {
			break;
		}
		CHResult nego_hr;
		try {
			nego_hr = IsMediaTypeSupported(mixer_type);
			optimal_type.Release();
			nego_hr = CreateOptimalVideoType(mixer_type, &optimal_type);
			nego_hr = m_Mixer->SetOutputType(0, optimal_type, MFT_SET_TYPE_TEST_ONLY);
			nego_hr = SetMediaType(optimal_type);
			nego_hr = m_Mixer->SetOutputType(0, optimal_type, 0);
			hr = S_OK;
			break;
		}
		catch (CAtlException) {
			SetMediaType(NULL);
		}
	}
	return hr;
}

/**
* メディアタイプがサポートされている?
*/
HRESULT CEvrPres::IsMediaTypeSupported(IMFMediaType * mt)
{
	ATLENSURE_RETURN_HR(mt != NULL, E_POINTER);
	CHResult hr;
	try {
		// 非圧縮?
		BOOL compressed;
		hr = mt->IsCompressedFormat(&compressed);
		if (compressed) {
			return MF_E_INVALIDMEDIATYPE;
		}
		// フォーマットは D3DFMT_X8R8G8B8?
		GUID sub_type;
		hr = mt->GetGUID(MF_MT_SUBTYPE, &sub_type);
		D3DFORMAT format = (D3DFORMAT)sub_type.Data1;
		if (format != D3DFMT_X8R8G8B8) {
			return MF_E_INVALIDMEDIATYPE;
		}
		// プログレッシブ?
		UINT32 interlace_mode;
		hr = mt->GetUINT32(MF_MT_INTERLACE_MODE, (UINT32*)&interlace_mode);
		if (interlace_mode != MFVideoInterlace_Progressive) {
			return MF_E_INVALIDMEDIATYPE;
		}
	}
	catch (CAtlException &e) {
		return e.m_hr;
	}
	return hr;
}

/**
 * より適したフォーマットを作成する
 */
HRESULT CEvrPres::CreateOptimalVideoType(IMFMediaType * propsed_mt, IMFMediaType ** optimal_mt)
{
	CHResult hr;
	ATLENSURE_RETURN_HR(propsed_mt != NULL, E_POINTER);
	ATLENSURE_RETURN_HR(optimal_mt != NULL, E_POINTER);
	try {
		CRect outupt_rect;
		MFVideoArea displayArea = {0};
		hr = MFCreateMediaType(optimal_mt);
		hr = propsed_mt->CopyAllItems(*optimal_mt);
		hr = MFSetAttributeRatio(*optimal_mt, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
		hr = (*optimal_mt)->SetUINT32(MF_MT_YUV_MATRIX, MFVideoTransferMatrix_BT709);
		hr = (*optimal_mt)->SetUINT32(MF_MT_TRANSFER_FUNCTION, (UINT32)MFVideoTransFunc_709);
		hr = (*optimal_mt)->SetUINT32(MF_MT_VIDEO_PRIMARIES, (UINT32)MFVideoPrimaries_BT709);
		hr = (*optimal_mt)->SetUINT32(MF_MT_VIDEO_NOMINAL_RANGE, (UINT32)MFNominalRange_16_235);
		hr = (*optimal_mt)->SetUINT32(MF_MT_VIDEO_LIGHTING, (UINT32)MFVideoLighting_dim);
		hr = (*optimal_mt)->SetUINT32(MF_MT_PAN_SCAN_ENABLED, (UINT32)FALSE);
	}
	catch (CAtlException &e) {
		return e.m_hr;
	}
	return hr;
}

/**
 * ストリーミングが開始した
 */
HRESULT CEvrPres::BeginStreaming()
{
	ATLTRACE(_T("%S\n"), __FUNCTION__);
	HRESULT hr = S_OK;
	// 何もしない
	m_QueuingTask.run([this]() { QueuingTask();});
	return hr;
}

/**
 * ストリーミング終了、EVRが停止した
 */
HRESULT CEvrPres::EndStreaming()
{
	ATLTRACE(_T("%S\n"), __FUNCTION__);
	HRESULT hr = S_OK;
	ReleaseResources();
	m_QueuingTask.cancel();
	m_QueuingTask.wait();
	return hr;
}

/**
 * MFVP_MESSAGE_PROCESSINPUTNOTIFY 受信時の処理
 */
HRESULT CEvrPres::ProcessInputNotify()
{
	m_InputNotify = true;
	HRESULT hr;
	// メディアタイプが未設定ならMF_E_TRANSFORM_TYPE_NOT_SETを返す
	if (m_MediaType == NULL) {
		return MF_E_TRANSFORM_TYPE_NOT_SET;
	}
	hr = ProcessOutputLoop();
	return hr;
}

HRESULT CEvrPres::ProcessOutputLoop()
{
	HRESULT hr;
	do {
		hr = ProcessOutput();
	} while (hr == S_OK);
	if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
		// ミキサーからの入力データが無い。EOSか確認する。
		CheckEndOfStream();
		return S_OK;
	}
	return S_OK;
}

/**
 * Mixer からサンプルを取得する
 */
HRESULT CEvrPres::ProcessOutput()
{
	HRESULT hr = S_OK;
	MFTIME hns_time;
	//ATLTRACE(_T("%S\n"), __FUNCTION__);
	if (m_Mixer == NULL) {
		return MF_E_INVALIDREQUEST;
	}
	if (m_SamplesPool.GetCount() == 0) {
		return MF_E_TRANSFORM_TYPE_NOT_SET;
	}

	// 空きMFSampleを検索
	DWORD status;
	CComPtr<IMFSample> mf_sample;
	MFTIME start = ::MFGetSystemTime();
	size_t sample_num;
	// 空きサンプル待ち
	DWORD wait_result;
	wait_result = WaitForMultipleObjects(m_Consumed.size(),
		&m_Consumed.at(0), FALSE, MFSAMPLE_TIMEOUT);
	if (wait_result == WAIT_TIMEOUT) {
		// 空きサンプルが見つからなくてタイムアウト
		// (空きサンプルがある状態でreturnするので想定外の動作)
		ATLASSERT(false);
		return S_FALSE;
	}
	ATLASSERT(wait_result >= WAIT_OBJECT_0 &&
		wait_result < (WAIT_OBJECT_0 + m_Consumed.size()));
	sample_num = wait_result - WAIT_OBJECT_0;
	/*
	if (!m_InputNotify) {
		// 可能な限りProcessOutputする
		do {
			MFT_OUTPUT_DATA_BUFFER data_buffer = {0};
			data_buffer.pSample = mf_sample;
			hr = m_Mixer->ProcessOutput(0, 1, &data_buffer, &status);
		} while (hr == S_OK);
		return S_FALSE;
	}
	*/
	{
		CAutoCritSecLock pool_lock(m_SamplesPoolLock);
		MFT_OUTPUT_DATA_BUFFER data_buffer = {0};
		mf_sample = m_SamplesPool.GetAt(sample_num);
		if (mf_sample == NULL) {
			return E_FAIL;
		}
		data_buffer.pSample = mf_sample;
		LONGLONG mixer_start_time, mixer_end_time;
		if (m_Clock) {
			m_Clock->GetCorrelatedTime(0, &mixer_start_time, &hns_time);
		}
		hr = m_Mixer->ProcessOutput(0, 1, &data_buffer, &status);
		if (FAILED(hr)) {
			if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
				return hr;
			}
			else if (hr == MF_E_TRANSFORM_TYPE_NOT_SET) {
				hr = RenegotiateMediaType();
				return hr;
			}
			else if (hr == MF_E_TRANSFORM_STREAM_CHANGE) {
				// メディアタイプが変更された
				SetMediaType(NULL);
			}
			else {
				return hr;
			}
		}

		if (m_Clock) {
			MFCLOCK_STATE mfc_state;
			m_Clock->GetState(0, &mfc_state);
			m_Clock->GetCorrelatedTime(0, &mixer_end_time, &hns_time);
			const LONGLONG latency = mixer_end_time - mixer_start_time;
			if (mfc_state == MFCLOCK_STATE_RUNNING) {
				NotifyEvent(EC_PROCESSING_LATENCY, (LONG_PTR)&latency, 0);
				ATLTRACE(_T("  ** LATENCY = %I64d\n"), latency);
			}
		}

		ResetEvent(m_Consumed.at(sample_num));
		LONGLONG sample_time, sample_duration;
		m_QueuedSamples.AddTail(mf_sample);
		{
			// 取得したサンプルタイムをデバッグ出力
			mf_sample->GetSampleTime(&sample_time);
			mf_sample->GetSampleDuration(&sample_duration);
			size_t count;
			GetCount(&count);
			ATLTRACE(_T(" >> Process sample_time = %I64d (dur = %I64d) (render_state=%d) (count=%d)\n"), sample_time, sample_duration, m_RenderState, count);
		}
	} // leave critsec automatically (m_SamplesPoolLock);
	size_t count;
	do {
		Sleep(0);
		{
			CAutoCritSecLock pool_lock(m_SamplesPoolLock);
			GetCount(&count);
		}
	} while (count == m_SamplesPool.GetCount());
	ATLASSERT(count < m_SamplesPool.GetCount());
	return hr;
}

// EOS を確認する
HRESULT CEvrPres::CheckEndOfStream()
{
	if (!m_EOS) {
		// MFVP_MESSAGE_ENDOFSTREAM を受信していない
		return S_OK;
	}
	ATLTRACE(_T("%S\n"), __FUNCTION__);
	// TODO : ミキサーに入力が残っている
	// TODO : レンダリングする残りのサンプルがある

	// 完了しているのでEC_COMPLETEを通知する
	NotifyEvent(EC_COMPLETE, (LONG_PTR)S_OK, 0);
	m_EOS = false;
	return S_OK;
}

// フラッシュ
HRESULT CEvrPres::Flush()
{
	ATLTRACE(_T("%S (%d left)\n"), __FUNCTION__, m_QueuedSamples.GetCount());
	CAutoCritSecLock pool_lock(m_SamplesPoolLock);
	// キューされているサンプルを全て消化
	while (m_QueuedSamples.GetCount() > 0) {
		CComPtr<IMFSample> head_sample;
		PeekHead(&head_sample);
		if (head_sample) {
			UINT64 v;
			head_sample->GetUINT64(ATTR_CONSUMED, &v);
			HANDLE consumed = (HANDLE)v;
			SetEvent(consumed);
		}
		RemoveHead();
	}
	return S_OK;
}

// リソース解放
void CEvrPres::ReleaseResources()
{
	std::for_each(m_Consumed.begin(), m_Consumed.end(), [](HANDLE & h) { CloseHandle(h); });
	m_Consumed.clear();
	m_QueuedSamples.RemoveAll();
	m_SamplesPool.RemoveAll();
}

// フレームステップ MFVP_MESSAGE_STEP
HRESULT CEvrPres::PrepareFrameStep(DWORD step)
{
	CHResult hr;
	ATLTRACE(L"%S\n", __FUNCTION__);
	ATLASSERT(step > 0);
	// http://msdn.microsoft.com/en-us/library/ms698964(VS.85).aspx
	ATLENSURE_RETURN_HR(m_RenderState == RENDER_STATE_STARTED, MF_E_INVALIDREQUEST);
	HRESULT proc_hr = ProcessOutput();
	if (proc_hr == S_OK) {
		NotifyEvent(EC_STEP_COMPLETE, (LONG_PTR)FALSE, 0);
	}
	return S_OK;
}

// キューイングタスク
void CEvrPres::QueuingTask()
{
	ATLTRACE(_T("Begin QueuingTask\n"));
	while (m_QueuingTask.is_canceling() == false) {

		Sleep(0);
	}
	ATLTRACE(_T("End QueuingTask\n"));
}
