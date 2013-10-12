#include "StdAfx.h"
#include "CustomPresActivateObj.h"

HRESULT CCustomPresActivateObj::FinalConstruct()
{
	HRESULT hr = MFCreateAttributes(&m_Attr, 1);
	return hr;
}

void CCustomPresActivateObj::FinalRelease()
{
}

// IMFActivate
HRESULT CCustomPresActivateObj::ActivateObject(REFIID riid, void **ppv)
{
	ATLENSURE_RETURN_HR(riid == __uuidof(IMFVideoPresenter), MF_E_CANNOT_CREATE_SINK);
	ATLENSURE_RETURN_HR(ppv != NULL, E_POINTER);
	if (m_CustomPres != NULL) {
		CComQIPtr<IMFVideoPresenter> mfvp(m_CustomPres);
		*ppv = mfvp;
		mfvp.p->AddRef();
		return S_OK;
	}
	CComObject<CEvrPres> * object;
	CComObject<CEvrPres>::CreateInstance(&object);
	// プレゼンタに自身の属性を引数として渡す
	object->Init(m_Attr);
	m_CustomPres.Attach(object);
	m_CustomPres.p->AddRef();
	CComQIPtr<IMFVideoPresenter> mfvp(object);
	if (mfvp == NULL) {
		return MF_E_CANNOT_CREATE_SINK;
	}
	*ppv = mfvp;
	mfvp.p->AddRef();
	return S_OK;
}

HRESULT CCustomPresActivateObj::ShutdownObject(void)
{
	m_CustomPres.Release();
	return S_OK;
}

HRESULT CCustomPresActivateObj::DetachObject(void)
{
	m_CustomPres.Detach();
	return S_OK;
}

//IMFAttributes
HRESULT CCustomPresActivateObj::GetItem(
	__RPC__in REFGUID guidKey,
	__RPC__inout_opt PROPVARIANT *pValue)
{
	return m_Attr->GetItem(guidKey, pValue);
}

HRESULT CCustomPresActivateObj::GetItemType(
	__RPC__in REFGUID guidKey,
	__RPC__out MF_ATTRIBUTE_TYPE *pType)
{
	return m_Attr->GetItemType(guidKey, pType);
}

HRESULT CCustomPresActivateObj::CompareItem( 
	__RPC__in REFGUID guidKey,
	__RPC__in REFPROPVARIANT Value,
	__RPC__out BOOL *pbResult)
{
	return m_Attr->CompareItem(guidKey, Value, pbResult);
}

HRESULT CCustomPresActivateObj::Compare(
	__RPC__in_opt IMFAttributes *pTheirs,
	MF_ATTRIBUTES_MATCH_TYPE MatchType,
	__RPC__out BOOL *pbResult)
{
	return m_Attr->Compare(pTheirs, MatchType, pbResult);
}

HRESULT CCustomPresActivateObj::GetUINT32(
	__RPC__in REFGUID guidKey,
	__RPC__out UINT32 *punValue)
{
	return m_Attr->GetUINT32(guidKey, punValue);
}

HRESULT CCustomPresActivateObj::GetUINT64(
	__RPC__in REFGUID guidKey,
	__RPC__out UINT64 *punValue)
{
	return m_Attr->GetUINT64(guidKey, punValue);
}

HRESULT CCustomPresActivateObj::GetDouble(
	__RPC__in REFGUID guidKey,
	__RPC__out double *pfValue)
{
	return m_Attr->GetDouble(guidKey, pfValue);
}

HRESULT CCustomPresActivateObj::GetGUID(
	__RPC__in REFGUID guidKey,
	__RPC__out GUID *pguidValue)
{
	return m_Attr->GetGUID(guidKey, pguidValue);
}

HRESULT CCustomPresActivateObj::GetStringLength(
	__RPC__in REFGUID guidKey,
	__RPC__out UINT32 *pcchLength)
{
	return m_Attr->GetStringLength(guidKey, pcchLength);
}

HRESULT CCustomPresActivateObj::GetString(
	__RPC__in REFGUID guidKey,
	__RPC__out_ecount_full(cchBufSize) LPWSTR pwszValue,
	UINT32 cchBufSize,
	__RPC__inout_opt UINT32 *pcchLength)
{
	return m_Attr->GetString(guidKey, pwszValue, cchBufSize, pcchLength);
}

HRESULT CCustomPresActivateObj::GetAllocatedString( 
	__RPC__in REFGUID guidKey,
	__RPC__deref_out_ecount_full_opt(( *pcchLength + 1 ) ) LPWSTR *ppwszValue,
	__RPC__out UINT32 *pcchLength)
{
	return m_Attr->GetAllocatedString(guidKey, ppwszValue, pcchLength);
}

HRESULT CCustomPresActivateObj::GetBlobSize(
	__RPC__in REFGUID guidKey,
	__RPC__out UINT32 *pcbBlobSize)
{
	return m_Attr->GetBlobSize(guidKey, pcbBlobSize);
}

HRESULT CCustomPresActivateObj::GetBlob(
	__RPC__in REFGUID guidKey,
	__RPC__out_ecount_full(cbBufSize) UINT8 *pBuf,
	UINT32 cbBufSize,
	__RPC__inout_opt UINT32 *pcbBlobSize)
{
	return m_Attr->GetBlob(guidKey, pBuf, cbBufSize, pcbBlobSize);
}

HRESULT CCustomPresActivateObj::GetAllocatedBlob(
	__RPC__in REFGUID guidKey,
	__RPC__deref_out_ecount_full_opt(*pcbSize) UINT8 **ppBuf,
	__RPC__out UINT32 *pcbSize)
{
	return m_Attr->GetAllocatedBlob(guidKey, ppBuf, pcbSize);
}

HRESULT CCustomPresActivateObj::GetUnknown( 
	__RPC__in REFGUID guidKey,
	__RPC__in REFIID riid,
	__RPC__deref_out_opt LPVOID *ppv)
{
	return m_Attr->GetUnknown(guidKey, riid, ppv);
}

HRESULT CCustomPresActivateObj::SetItem( 
	__RPC__in REFGUID guidKey,
	__RPC__in REFPROPVARIANT Value)
{
	return m_Attr->SetItem(guidKey, Value);
}

HRESULT CCustomPresActivateObj::DeleteItem(
	__RPC__in REFGUID guidKey)
{
	return m_Attr->DeleteItem(guidKey);
}

HRESULT CCustomPresActivateObj::DeleteAllItems(void)
{
	return m_Attr->DeleteAllItems();
}

HRESULT CCustomPresActivateObj::SetUINT32( 
	__RPC__in REFGUID guidKey,
	UINT32 unValue)
{
	return m_Attr->SetUINT32(guidKey, unValue);
}

HRESULT CCustomPresActivateObj::SetUINT64(
	__RPC__in REFGUID guidKey,
	UINT64 unValue)
{
	return m_Attr->SetUINT64(guidKey, unValue);
}

HRESULT CCustomPresActivateObj::SetDouble(
	__RPC__in REFGUID guidKey,
	double fValue)
{
	return m_Attr->SetDouble(guidKey, fValue);
}

HRESULT CCustomPresActivateObj::SetGUID(
	__RPC__in REFGUID guidKey,
	__RPC__in REFGUID guidValue)
{
	return m_Attr->SetGUID(guidKey, guidValue);
}

HRESULT CCustomPresActivateObj::SetString(
	__RPC__in REFGUID guidKey,
	__RPC__in_string LPCWSTR wszValue)
{ 
	return m_Attr->SetString(guidKey, wszValue);
}

HRESULT CCustomPresActivateObj::SetBlob(
	__RPC__in REFGUID guidKey,
	__RPC__in_ecount_full(cbBufSize) const UINT8 *pBuf,
	UINT32 cbBufSize)
{
	return m_Attr->SetBlob(guidKey, pBuf, cbBufSize);
}

HRESULT CCustomPresActivateObj::SetUnknown(
	__RPC__in REFGUID guidKey,
	__RPC__in_opt IUnknown *pUnknown)
{
	return m_Attr->SetUnknown(guidKey, pUnknown);
}

HRESULT CCustomPresActivateObj::LockStore(void)
{
	return m_Attr->LockStore();
}

HRESULT CCustomPresActivateObj::UnlockStore(void)
{
	return m_Attr->UnlockStore();
}

HRESULT CCustomPresActivateObj::GetCount(__RPC__out UINT32 *pcItems)
{
	return m_Attr->GetCount(pcItems);
}

HRESULT CCustomPresActivateObj::GetItemByIndex(
	UINT32 unIndex,
	__RPC__out GUID *pguidKey,
	__RPC__inout_opt PROPVARIANT *pValue)
{
	return m_Attr->GetItemByIndex(unIndex, pguidKey, pValue);
}

HRESULT CCustomPresActivateObj::CopyAllItems(__RPC__in_opt IMFAttributes *pDest)
{
	return m_Attr->CopyAllItems(pDest);
}
