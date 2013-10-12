#pragma once

// 自動例外送出 HRESULT クラス
class CHResult
{
	HRESULT m_hr;
public:
	CHResult() : m_hr(NOERROR) {}
	CHResult(HRESULT hr) : m_hr(hr) {
		if(FAILED(m_hr)) {
			ATLTRACE(_T("CHResult exception %X.\n"), hr);
			throw CAtlException(m_hr);
		}
	}
	CHResult(DWORD last_error) {
		m_hr = HRESULT_FROM_WIN32(GetLastError());
	}
	operator HRESULT() { return m_hr; }

	/**
	 * CAtlString型にキャストする
	 */
	operator CAtlString () {
		return CHResult::ToString(m_hr);
	}

	/**
	 * HRESULT を CAtlString型にキャストする
	 * @param [in] hr HRESULTコード
	 * @retrun CAtlString 文字列
	 */
	static CAtlString ToString(HRESULT hr) {
		CAtlString err_msg;
		LPTSTR err_txt;
		DWORD dwChars = 0;
		dwChars = FormatMessage(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
			0,
			hr,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPTSTR) &err_txt,
			0,
			0);
		if (dwChars == 0) {
			err_msg = L"An unexpected error occurred.";
		}
		else if (err_txt)
		{
			err_msg = err_txt;
			LocalFree(err_txt);
		}
		return err_msg;
	}
};
