/**
* =============================================================================
* Source Python
* Copyright (C) 2012 Source Python Development Team.  All rights reserved.
* =============================================================================
*
* This program is free software; you can redistribute it and/or modify it under
* the terms of the GNU General Public License, version 3.0, as published by the
* Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
* FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
* details.
*
* You should have received a copy of the GNU General Public License along with
* this program.  If not, see <http://www.gnu.org/licenses/>.
*
* As a special exception, the Source Python Team gives you permission
* to link the code of this program (as well as its derivative works) to
* "Half-Life 2," the "Source Engine," and any Game MODs that run on software
* by the Valve Corporation.  You must obey the GNU General Public License in
* all respects for all other code used.  Additionally, the Source.Python
* Development Team grants this exception to all derivative works.
*/

//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------
#include <stdlib.h>
#include <string>

#include "dyncall.h"

#include "memory_hooks.h"
#include "memory_tools.h"
#include "utility/wrap_macros.h"
#include "utility/sp_util.h"


DCCallVM* g_pCallVM = dcNewCallVM(4096);
extern std::map<CHook *, std::map<DynamicHooks::HookType_t, std::list<PyObject *> > > g_mapCallbacks;

CHookManager* g_pHookMngr = new CHookManager;

//-----------------------------------------------------------------------------
// CPointer class
//-----------------------------------------------------------------------------
CPointer::CPointer(unsigned long ulAddr /* = 0 */)
{
	m_ulAddr = ulAddr;
}

void CPointer::SetStringPtr(char* szText, int iOffset /* = 0 */)
{
	if (!IsValid())
		BOOST_RAISE_EXCEPTION(PyExc_ValueError, "Pointer is NULL.")

	strcpy(*(char **) (m_ulAddr + iOffset), szText);
}

const char * CPointer::GetStringArray(int iOffset /* = 0 */)
{
	if (!IsValid())
		BOOST_RAISE_EXCEPTION(PyExc_ValueError, "Pointer is NULL.")

	return (const char *) m_ulAddr + iOffset;
}

void CPointer::SetStringArray(char* szText, int iOffset /* = 0 */)
{
	if (!IsValid())
		BOOST_RAISE_EXCEPTION(PyExc_ValueError, "Pointer is NULL.")

	strcpy((char *) (m_ulAddr + iOffset), szText);
}

CPointer* CPointer::GetPtr(int iOffset /* = 0 */)
{
	if (!IsValid())
		BOOST_RAISE_EXCEPTION(PyExc_ValueError, "Pointer is NULL.")

	return new CPointer(*(unsigned long *) (m_ulAddr + iOffset));
}

void CPointer::SetPtr(CPointer* ptr, int iOffset /* = 0 */)
{
	if (!IsValid())
		BOOST_RAISE_EXCEPTION(PyExc_ValueError, "Pointer is NULL.")

	*(unsigned long *) m_ulAddr = ptr->m_ulAddr;
}

int CPointer::Compare(object oOther, unsigned long ulNum)
{
	unsigned long ulOther = ExtractPyPtr(oOther);
	if (!m_ulAddr || !ulOther)
		BOOST_RAISE_EXCEPTION(PyExc_ValueError, "At least one pointer is NULL.")

	return memcmp((void *) m_ulAddr, (void *) ulOther, ulNum);
}

bool CPointer::IsOverlapping(object oOther, unsigned long ulNumBytes)
{
	unsigned long ulOther = ExtractPyPtr(oOther);
	if (m_ulAddr <= ulOther)
		return m_ulAddr + ulNumBytes > ulOther;
       
	return ulOther + ulNumBytes > m_ulAddr;
}

CPointer* CPointer::SearchBytes(object oBytes, unsigned long ulNumBytes)
{
	if (!m_ulAddr)
		BOOST_RAISE_EXCEPTION(PyExc_ValueError, "Pointer is NULL.")

	unsigned long iByteLen = len(oBytes);
	if (ulNumBytes < iByteLen)
		BOOST_RAISE_EXCEPTION(PyExc_ValueError, "Search range is too small.")

	unsigned char* base  = (unsigned char *) m_ulAddr;
	unsigned char* end   = (unsigned char *) (m_ulAddr + ulNumBytes - (iByteLen - 1));
	unsigned char* bytes = NULL;
	PyArg_Parse(oBytes.ptr(), "y", &bytes);

	while (base < end)
	{
		unsigned long i = 0;
		for(; i < iByteLen; i++)
		{
			if (base[i] == '\x2A')
				continue;

			if (bytes[i] != base[i])
				break;
		}

		if (i == iByteLen)
			return new CPointer((unsigned long) base);

		base++;
	}
	return NULL;
}

void CPointer::Copy(object oDest, unsigned long ulNumBytes)
{
	unsigned long ulDest = ExtractPyPtr(oDest);
	if (!m_ulAddr || ulDest)
		BOOST_RAISE_EXCEPTION(PyExc_ValueError, "At least one pointer is NULL.")

	if (IsOverlapping(oDest, ulNumBytes))
		BOOST_RAISE_EXCEPTION(PyExc_ValueError, "Pointers are overlapping!")

	memcpy((void *) ulDest, (void *) m_ulAddr, ulNumBytes);
}

void CPointer::Move(object oDest, unsigned long ulNumBytes)
{
	unsigned long ulDest = ExtractPyPtr(oDest);
	if (!m_ulAddr || ulDest == 0)
		BOOST_RAISE_EXCEPTION(PyExc_ValueError, "At least one pointer is NULL.")

	memmove((void *) ulDest, (void *) m_ulAddr, ulNumBytes);
}

CPointer* CPointer::GetVirtualFunc(int iIndex)
{
	if (!IsValid())
		BOOST_RAISE_EXCEPTION(PyExc_ValueError, "Pointer is NULL.")

	void** vtable = *(void ***) m_ulAddr;
	if (!vtable)
		return new CPointer();

	return new CPointer((unsigned long) vtable[iIndex]);
}

CFunction* CPointer::MakeFunction(Convention_t eConv, tuple args, ReturnType_t return_type)
{
	if (!IsValid())
		BOOST_RAISE_EXCEPTION(PyExc_ValueError, "Pointer is NULL.")

	return new CFunction(m_ulAddr, eConv, args, return_type);
}

CFunction* CPointer::MakeVirtualFunction(int iIndex, Convention_t eConv, tuple args, ReturnType_t return_type)
{
	return GetVirtualFunc(iIndex)->MakeFunction(eConv, args, return_type);
}

//-----------------------------------------------------------------------------
// CFunction class
//-----------------------------------------------------------------------------
CFunction::CFunction(unsigned long ulAddr, Convention_t eConv, tuple args, ReturnType_t return_type)
{
	m_ulAddr = ulAddr;
	m_eConv = eConv;
	m_Args = args;
	m_ReturnType = return_type;
}

object CFunction::Call(tuple args, dict kw)
{
	if (!IsValid())
		BOOST_RAISE_EXCEPTION(PyExc_ValueError, "Function pointer is NULL.")

	if (len(args) != len(m_Args))
		BOOST_RAISE_EXCEPTION(PyExc_ValueError, "Number of passed arguments is not equal to the required number.")

	// Reset VM and set the calling convention
	dcReset(g_pCallVM);
	dcMode(g_pCallVM, GetDynCallConvention(m_eConv));

	// Loop through all passed arguments and add them to the VM
	for(int i=0; i < len(args); i++)
	{
		object arg = args[i];
		switch(extract<Argument_t>(m_Args[i]))
		{
			case DC_SIGCHAR_BOOL:      dcArgBool(g_pCallVM, extract<bool>(arg)); break;
			case DC_SIGCHAR_CHAR:      dcArgChar(g_pCallVM, extract<char>(arg)); break;
			case DC_SIGCHAR_UCHAR:     dcArgChar(g_pCallVM, extract<unsigned char>(arg)); break;
			case DC_SIGCHAR_SHORT:     dcArgShort(g_pCallVM, extract<short>(arg)); break;
			case DC_SIGCHAR_USHORT:    dcArgShort(g_pCallVM, extract<unsigned short>(arg)); break;
			case DC_SIGCHAR_INT:       dcArgInt(g_pCallVM, extract<int>(arg)); break;
			case DC_SIGCHAR_UINT:      dcArgInt(g_pCallVM, extract<unsigned int>(arg)); break;
			case DC_SIGCHAR_LONG:      dcArgLong(g_pCallVM, extract<long>(arg)); break;
			case DC_SIGCHAR_ULONG:     dcArgLong(g_pCallVM, extract<unsigned long>(arg)); break;
			case DC_SIGCHAR_LONGLONG:  dcArgLongLong(g_pCallVM, extract<long long>(arg)); break;
			case DC_SIGCHAR_ULONGLONG: dcArgLongLong(g_pCallVM, extract<unsigned long long>(arg)); break;
			case DC_SIGCHAR_FLOAT:     dcArgFloat(g_pCallVM, extract<float>(arg)); break;
			case DC_SIGCHAR_DOUBLE:    dcArgDouble(g_pCallVM, extract<double>(arg)); break;
			case DC_SIGCHAR_POINTER:   dcArgPointer(g_pCallVM, ExtractPyPtr(arg)); break;
			case DC_SIGCHAR_STRING:    dcArgPointer(g_pCallVM, (unsigned long) (void *) extract<char *>(arg)); break;
			default: BOOST_RAISE_EXCEPTION(PyExc_ValueError, "Unknown argument type.")
		}
	}

	// Call the function
	switch(m_ReturnType)
	{
		case DC_SIGCHAR_VOID:      dcCallVoid(g_pCallVM, m_ulAddr); break;
		case DC_SIGCHAR_BOOL:      return object(dcCallBool(g_pCallVM, m_ulAddr));
		case DC_SIGCHAR_CHAR:      return object(dcCallChar(g_pCallVM, m_ulAddr));
		case DC_SIGCHAR_UCHAR:     return object((unsigned char) dcCallChar(g_pCallVM, m_ulAddr));
		case DC_SIGCHAR_SHORT:     return object(dcCallShort(g_pCallVM, m_ulAddr));
		case DC_SIGCHAR_USHORT:    return object((unsigned short) dcCallShort(g_pCallVM, m_ulAddr));
		case DC_SIGCHAR_INT:       return object(dcCallInt(g_pCallVM, m_ulAddr));
		case DC_SIGCHAR_UINT:      return object((unsigned int) dcCallInt(g_pCallVM, m_ulAddr));
		case DC_SIGCHAR_LONG:      return object(dcCallLong(g_pCallVM, m_ulAddr));
		case DC_SIGCHAR_ULONG:     return object((unsigned long) dcCallLong(g_pCallVM, m_ulAddr));
		case DC_SIGCHAR_LONGLONG:  return object(dcCallLongLong(g_pCallVM, m_ulAddr));
		case DC_SIGCHAR_ULONGLONG: return object((unsigned long long) dcCallLongLong(g_pCallVM, m_ulAddr));
		case DC_SIGCHAR_FLOAT:     return object(dcCallFloat(g_pCallVM, m_ulAddr));
		case DC_SIGCHAR_DOUBLE:    return object(dcCallDouble(g_pCallVM, m_ulAddr));
		case DC_SIGCHAR_POINTER:   return object(CPointer(dcCallPointer(g_pCallVM, m_ulAddr)));
		case DC_SIGCHAR_STRING:    return object((const char *) dcCallPointer(g_pCallVM, m_ulAddr));
		default: BOOST_RAISE_EXCEPTION(PyExc_TypeError, "Unknown return type.")
	}
	return object();
}

object CFunction::CallTrampoline(tuple args, dict kw)
{
	if (!IsValid())
		BOOST_RAISE_EXCEPTION(PyExc_ValueError, "Function pointer is NULL.")

	CHook* pHook = g_pHookMngr->FindHook((void *) m_ulAddr);
	if (!pHook)
		BOOST_RAISE_EXCEPTION(PyExc_ValueError, "Function was not hooked.")

	return CFunction((unsigned long) pHook->m_pTrampoline, m_eConv, m_Args, m_ReturnType).Call(args, kw);
}

handle<> CFunction::AddHook(DynamicHooks::HookType_t eType, PyObject* pCallable)
{
	if (!IsValid())
		BOOST_RAISE_EXCEPTION(PyExc_ValueError, "Function pointer is NULL.")

	// Generate the argument string
	char* szParams = extract<char*>(eval("lambda args, ret: ''.join(map(chr, args)) + ')' + chr(ret)")(m_Args, m_ReturnType));
	puts(szParams);

	// Hook the function
	CHook* pHook = g_pHookMngr->HookFunction((void *) m_ulAddr, m_eConv, strdup(szParams));

	// Add the hook handler. If it's already added, it won't be added twice
	pHook->AddCallback(eType, (void *) &SP_HookHandler);

	// Add the callback to our map
	g_mapCallbacks[pHook][eType].push_back(pCallable);

	// Return the callback, so we can use this method as a decorator
	return handle<>(borrowed(pCallable));
}

void CFunction::RemoveHook(DynamicHooks::HookType_t eType, PyObject* pCallable)
{
	if (!IsValid())
		BOOST_RAISE_EXCEPTION(PyExc_ValueError, "Function pointer is NULL.")
		
	CHook* pHook = g_pHookMngr->FindHook((void *) m_ulAddr);
	if (!pHook)
		return;

	g_mapCallbacks[pHook][eType].remove(pCallable);
}