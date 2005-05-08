/*
 *	This class implements a pure IStream object
 *	and can be used for many purposes.
 *
 *	The main reason for implementing this was
 *	a cleaner implementation of IShellLink which
 *	needs to be able to load lnks from an IStream
 *	interface so it was obvious to capsule the file
 *	access in an IStream to.
 *
 * Copyright 1999 Juergen Schmied
 * Copyright 2003 Mike McCormack for CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdarg.h>
#include <string.h>

#define COBJMACROS

#include "windef.h"
#include "winbase.h"
#include "winerror.h"
#include "winuser.h"
#include "wingdi.h"
#include "shlobj.h"
#include "wine/debug.h"
#include "shell32_main.h"

WINE_DEFAULT_DEBUG_CHANNEL(shell);

#define STGM_ACCESS_MODE(stgm)   ((stgm)&0x0000f)
#define STGM_SHARE_MODE(stgm)    ((stgm)&0x000f0)
#define STGM_CREATE_MODE(stgm)   ((stgm)&0x0f000)

typedef struct
{	
	const IStreamVtbl	*lpvtst;
	DWORD			ref;
	HANDLE			handle;
} ISHFileStream;

/**************************************************************************
*  IStream_fnQueryInterface
*/
static HRESULT WINAPI IStream_fnQueryInterface(IStream *iface, REFIID riid, LPVOID *ppvObj)
{
	ISHFileStream *This = (ISHFileStream *)iface;

	TRACE("(%p)->(\n\tIID:\t%s,%p)\n",This,debugstr_guid(riid),ppvObj);

	*ppvObj = NULL;

	if(IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IStream))
		*ppvObj = This;

	if(*ppvObj)
	{
		IStream_AddRef((IStream*)*ppvObj);
		TRACE("-- Interface: (%p)->(%p)\n",ppvObj,*ppvObj);
		return S_OK;
	}
	TRACE("-- Interface: E_NOINTERFACE\n");
	return E_NOINTERFACE;
}

/**************************************************************************
*  IStream_fnAddRef
*/
static ULONG WINAPI IStream_fnAddRef(IStream *iface)
{
	ISHFileStream *This = (ISHFileStream *)iface;
	ULONG refCount = InterlockedIncrement(&This->ref);

	TRACE("(%p)->(count=%lu)\n", This, refCount - 1);

	return refCount;
}

/**************************************************************************
*  IStream_fnRelease
*/
static ULONG WINAPI IStream_fnRelease(IStream *iface)
{
	ISHFileStream *This = (ISHFileStream *)iface;
	ULONG refCount = InterlockedDecrement(&This->ref);

	TRACE("(%p)->(count=%lu)\n", This, refCount + 1);

	if (!refCount)
	{
		TRACE(" destroying SHFileStream (%p)\n",This);
		CloseHandle(This->handle);
		HeapFree(GetProcessHeap(),0,This);
	}
	return refCount;
}

static HRESULT WINAPI IStream_fnRead (IStream * iface, void* pv, ULONG cb, ULONG* pcbRead)
{
	ISHFileStream *This = (ISHFileStream *)iface;

	TRACE("(%p)->(%p,0x%08lx,%p)\n",This, pv, cb, pcbRead);

	if ( !pv )
		return STG_E_INVALIDPOINTER;

	if ( ! ReadFile( This->handle, pv, cb, pcbRead, NULL ) )
		return S_FALSE;

	return S_OK;
}

static HRESULT WINAPI IStream_fnWrite (IStream * iface, const void* pv, ULONG cb, ULONG* pcbWritten)
{
       DWORD dummy_count;
	ISHFileStream *This = (ISHFileStream *)iface;

	TRACE("(%p)\n",This);

	if( !pv )
		return STG_E_INVALIDPOINTER;

       /* WriteFile() doesn't allow to specify NULL as write count pointer */
       if (!pcbWritten)
               pcbWritten = &dummy_count;

	if( ! WriteFile( This->handle, pv, cb, pcbWritten, NULL ) )
		return E_FAIL;

	return S_OK;
}

static HRESULT WINAPI IStream_fnSeek (IStream * iface, LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER* plibNewPosition)
{
	DWORD pos, newposlo, newposhi;

	ISHFileStream *This = (ISHFileStream *)iface;

	TRACE("(%p)\n",This);

	pos = dlibMove.QuadPart; /* FIXME: truncates */
	newposhi = 0;
	newposlo = SetFilePointer( This->handle, pos, &newposhi, dwOrigin );
	if( newposlo == INVALID_SET_FILE_POINTER )
		return E_FAIL;

	plibNewPosition->QuadPart = newposlo | ( (LONGLONG)newposhi<<32);

	return S_OK;
}

static HRESULT WINAPI IStream_fnSetSize (IStream * iface, ULARGE_INTEGER libNewSize)
{
	ISHFileStream *This = (ISHFileStream *)iface;

	TRACE("(%p)\n",This);

	if( ! SetFilePointer( This->handle, libNewSize.QuadPart, NULL, FILE_BEGIN ) )
		return E_FAIL;

	if( ! SetEndOfFile( This->handle ) )
		return E_FAIL;

	return S_OK;
}
static HRESULT WINAPI IStream_fnCopyTo (IStream * iface, IStream* pstm, ULARGE_INTEGER cb, ULARGE_INTEGER* pcbRead, ULARGE_INTEGER* pcbWritten)
{
	ISHFileStream *This = (ISHFileStream *)iface;

	TRACE("(%p)\n",This);

	return E_NOTIMPL;
}
static HRESULT WINAPI IStream_fnCommit (IStream * iface, DWORD grfCommitFlags)
{
	ISHFileStream *This = (ISHFileStream *)iface;

	TRACE("(%p)\n",This);

	return E_NOTIMPL;
}
static HRESULT WINAPI IStream_fnRevert (IStream * iface)
{
	ISHFileStream *This = (ISHFileStream *)iface;

	TRACE("(%p)\n",This);

	return E_NOTIMPL;
}
static HRESULT WINAPI IStream_fnLockRegion (IStream * iface, ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType)
{
	ISHFileStream *This = (ISHFileStream *)iface;

	TRACE("(%p)\n",This);

	return E_NOTIMPL;
}
static HRESULT WINAPI IStream_fnUnlockRegion (IStream * iface, ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType)
{
	ISHFileStream *This = (ISHFileStream *)iface;

	TRACE("(%p)\n",This);

	return E_NOTIMPL;
}
static HRESULT WINAPI IStream_fnStat (IStream * iface, STATSTG*   pstatstg, DWORD grfStatFlag)
{
	ISHFileStream *This = (ISHFileStream *)iface;

	TRACE("(%p)\n",This);

	return E_NOTIMPL;
}
static HRESULT WINAPI IStream_fnClone (IStream * iface, IStream** ppstm)
{
	ISHFileStream *This = (ISHFileStream *)iface;

	TRACE("(%p)\n",This);

	return E_NOTIMPL;
}

static const IStreamVtbl stvt =
{
	IStream_fnQueryInterface,
	IStream_fnAddRef,
	IStream_fnRelease,
	IStream_fnRead,
	IStream_fnWrite,
	IStream_fnSeek,
	IStream_fnSetSize,
	IStream_fnCopyTo,
	IStream_fnCommit,
	IStream_fnRevert,
	IStream_fnLockRegion,
	IStream_fnUnlockRegion,
	IStream_fnStat,
	IStream_fnClone
};

/**************************************************************************
 *   CreateStreamOnFile()
 *
 *   similar to CreateStreamOnHGlobal
 */
HRESULT CreateStreamOnFile (LPCWSTR pszFilename, DWORD grfMode, IStream ** ppstm)
{
	ISHFileStream*	fstr;
	HANDLE		handle;
	DWORD		access = GENERIC_READ, creat;

	if( grfMode & STGM_TRANSACTED )
		return E_INVALIDARG;

	switch( STGM_ACCESS_MODE( grfMode ) )
	{
	case STGM_READ:
		access = GENERIC_READ;
		break;
	case STGM_WRITE:
	case STGM_READWRITE:
		access = GENERIC_WRITE | GENERIC_READ;
		break;
	default:
		return STG_E_INVALIDFLAG;
	}

	switch( STGM_CREATE_MODE( grfMode ) )
	{
	case STGM_CREATE:
		creat = CREATE_ALWAYS;
		break;
	case STGM_FAILIFTHERE:
		creat = OPEN_EXISTING;
		break;
	default:
		return STG_E_INVALIDFLAG;
	}

	handle = CreateFileW( pszFilename, access,
			FILE_SHARE_READ, NULL, creat, 0, NULL );
	if( handle == INVALID_HANDLE_VALUE )
		return HRESULT_FROM_WIN32(GetLastError());

	fstr = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(ISHFileStream));
	if( !fstr )
		return E_OUTOFMEMORY;
	fstr->lpvtst = &stvt;
	fstr->ref = 1;
	fstr->handle = handle;

	(*ppstm) = (IStream*)fstr;

	return S_OK;
}
