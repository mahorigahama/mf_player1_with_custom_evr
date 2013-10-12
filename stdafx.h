// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <windows.h>

// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>

// ATL
// #define _ATL_DEBUG_INTERFACES
// #define _ATL_DEBUG_QI
#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS
#include <atlbase.h>
#include <atlwin.h>
#include <atlstr.h>
#include <atltypes.h>
#include <atlcoll.h>

// TODO: reference additional headers your program requires here
#include "hresult.h"

// Typedefs
typedef CComMultiThreadModel::AutoCriticalSection CCritSec;
typedef CComCritSecLock<CComMultiThreadModel::AutoCriticalSection> CAutoCritSecLock;

// Direct3D9Ex
#ifdef _DEBUG
#define D3D_DEBUG_INFO
#endif
#include <d3d9.h>
#pragma comment(lib, "d3d9.lib")
#include <dxerr.h>
#pragma comment(lib, "dxerr.lib")
#pragma comment(lib, "dxguid.lib")

// Direct3D10.1
#include <d3d10_1.h>
#include <d3dX10.h>
#include <dxgi.h>
#pragma comment(lib, "d3d10_1.lib")
#pragma comment(lib, "d3dx10.lib")
#pragma comment(lib, "dxgi.lib")

#include <dxerr.h>
#pragma comment(lib, "dxerr.lib")

// Direct2D
#include <d2d1.h>
#include <d2d1helper.h>
#pragma comment(lib, "d2d1.lib")

// DirectWrite
#include <Dwrite.h>
#pragma comment(lib, "Dwrite.lib")

// DXVA2.0
#include <dxva2api.h>
#pragma comment(lib, "Dxva2.lib")

// Media Foundation
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "evr.lib")
#pragma comment(lib, "strmiids.lib")
#pragma comment(lib, "mfuuid.lib")
#include <mfapi.h>
#include <mfidl.h>
#include <mferror.h>
#include <evr9.h>
#include <evr.h>
#include <evcode.h> // EVR event codes (IMediaEventSink)

#include <ppl.h>
#include <agents.h>
#include <algorithm>
#include <array>
#include <memory>
