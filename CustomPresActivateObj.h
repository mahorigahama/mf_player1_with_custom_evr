#pragma once

#include "EvrPres.h"

//////////////////////////////////////////////////////////////////
// CCustomPresActivateObj
// http://msdn.microsoft.com/en-us/library/ms703039(VS.85).aspx
class CCustomPresActivateObj :
	public CComObjectRoot,
	public IMFActivate // inherits from IMFAttributes
{
	CComPtr<CEvrPres> m_CustomPres;
	CComPtr<IMFAttributes> m_Attr; // contained
	BEGIN_COM_MAP(CCustomPresActivateObj)
		COM_INTERFACE_ENTRY(IMFActivate)
		COM_INTERFACE_ENTRY(IMFAttributes)
	END_COM_MAP()
public:
	HRESULT FinalConstruct();
	void FinalRelease();
	// IMFActivate
	STDMETHOD(ActivateObject)(REFIID riid, void **ppv);
	STDMETHOD(ShutdownObject)(void);
	STDMETHOD(DetachObject)(void);
	//IMFAttributes
	STDMETHOD(GetItem)(
		__RPC__in REFGUID guidKey,
		__RPC__inout_opt PROPVARIANT *pValue);
	STDMETHOD(GetItemType)( 
		__RPC__in REFGUID guidKey,
		__RPC__out MF_ATTRIBUTE_TYPE *pType) ;
	STDMETHOD(CompareItem)( 
		__RPC__in REFGUID guidKey,
		__RPC__in REFPROPVARIANT Value,
		__RPC__out BOOL *pbResult);
	STDMETHOD(Compare)( 
		__RPC__in_opt IMFAttributes *pTheirs,
		MF_ATTRIBUTES_MATCH_TYPE MatchType,
		__RPC__out BOOL *pbResult) ;
	STDMETHOD(GetUINT32)( 
		__RPC__in REFGUID guidKey,
		__RPC__out UINT32 *punValue) ;
	STDMETHOD(GetUINT64)( 
		__RPC__in REFGUID guidKey,
		__RPC__out UINT64 *punValue) ;
	STDMETHOD(GetDouble)( 
		__RPC__in REFGUID guidKey,
		__RPC__out double *pfValue) ;
	STDMETHOD(GetGUID)( 
		__RPC__in REFGUID guidKey,
		__RPC__out GUID *pguidValue) ;
	STDMETHOD(GetStringLength)( 
		__RPC__in REFGUID guidKey,
		__RPC__out UINT32 *pcchLength) ;
	STDMETHOD(GetString)( 
		__RPC__in REFGUID guidKey,
		__RPC__out_ecount_full(cchBufSize) LPWSTR pwszValue,
		UINT32 cchBufSize,
		__RPC__inout_opt UINT32 *pcchLength) ;

	STDMETHOD(GetAllocatedString)( 
		__RPC__in REFGUID guidKey,
		__RPC__deref_out_ecount_full_opt(( *pcchLength + 1 ) ) LPWSTR *ppwszValue,
		__RPC__out UINT32 *pcchLength) ;
	STDMETHOD(GetBlobSize)( 
		__RPC__in REFGUID guidKey,
		__RPC__out UINT32 *pcbBlobSize);
	STDMETHOD(GetBlob)( 
		__RPC__in REFGUID guidKey,
		__RPC__out_ecount_full(cbBufSize) UINT8 *pBuf,
		UINT32 cbBufSize,
		__RPC__inout_opt UINT32 *pcbBlobSize);

	STDMETHOD(GetAllocatedBlob)( 
		__RPC__in REFGUID guidKey,
		__RPC__deref_out_ecount_full_opt(*pcbSize) UINT8 **ppBuf,
		__RPC__out UINT32 *pcbSize);
	STDMETHOD(GetUnknown)( 
		__RPC__in REFGUID guidKey,
		__RPC__in REFIID riid,
		__RPC__deref_out_opt LPVOID *ppv);
	STDMETHOD(SetItem)( 
		__RPC__in REFGUID guidKey,
		__RPC__in REFPROPVARIANT Value);
	STDMETHOD(DeleteItem)( 
		__RPC__in REFGUID guidKey);
	STDMETHOD(DeleteAllItems)(void);
	STDMETHOD(SetUINT32)( 
		__RPC__in REFGUID guidKey,
		UINT32 unValue);
	STDMETHOD(SetUINT64)(
		__RPC__in REFGUID guidKey,
		UINT64 unValue);
	STDMETHOD(SetDouble)( 
		__RPC__in REFGUID guidKey,
		double fValue);
	STDMETHOD(SetGUID)( 
		__RPC__in REFGUID guidKey,
		__RPC__in REFGUID guidValue);
	STDMETHOD(SetString)( 
		__RPC__in REFGUID guidKey,
		__RPC__in_string LPCWSTR wszValue);
	STDMETHOD(SetBlob)( 
		__RPC__in REFGUID guidKey,
		__RPC__in_ecount_full(cbBufSize) const UINT8 *pBuf,
		UINT32 cbBufSize);
	STDMETHOD(SetUnknown)( 
		__RPC__in REFGUID guidKey,
		__RPC__in_opt IUnknown *pUnknown);
	STDMETHOD(LockStore)(void);
	STDMETHOD(UnlockStore)(void);
	STDMETHOD(GetCount)( 
		__RPC__out UINT32 *pcItems);
	STDMETHOD(GetItemByIndex)( 
		UINT32 unIndex,
		__RPC__out GUID *pguidKey,
		__RPC__inout_opt PROPVARIANT *pValue);
	STDMETHOD(CopyAllItems)( 
		__RPC__in_opt IMFAttributes *pDest);
};
