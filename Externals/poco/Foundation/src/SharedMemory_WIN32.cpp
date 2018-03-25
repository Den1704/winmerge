//
// SharedMemoryImpl.cpp
//
// $Id: //poco/1.4/Foundation/src/SharedMemory_WIN32.cpp#1 $
//
// Library: Foundation
// Package: Processes
// Module:  SharedMemoryImpl
//
// Copyright (c) 2007, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// Permission is hereby granted, free of charge, to any person or organization
// obtaining a copy of the software and accompanying documentation covered by
// this license (the "Software") to use, reproduce, display, distribute,
// execute, and transmit the Software, and to prepare derivative works of the
// Software, and to permit third-parties to whom the Software is furnished to
// do so, all subject to the following:
// 
// The copyright notices in the Software and this entire statement, including
// the above license grant, this restriction and the following disclaimer,
// must be included in all copies of the Software, in whole or in part, and
// all derivative works of the Software, unless such copies or derivative
// works are solely in the form of machine-executable object code generated by
// a source language processor.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
// SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
// FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.
//


#include "Poco/SharedMemory_WIN32.h"
#include "Poco/Exception.h"
#include "Poco/File.h"
#if defined (POCO_WIN32_UTF8)
#include "Poco/UnicodeConverter.h"
#endif
#include "Poco/UnWindows.h"


namespace Poco {


SharedMemoryImpl::SharedMemoryImpl(const std::string& name, std::size_t size, SharedMemory::AccessMode mode, const void*, bool):
	_name(name),
	_memHandle(INVALID_HANDLE_VALUE),
	_fileHandle(INVALID_HANDLE_VALUE),
	_size(size),
	_mode(PAGE_READONLY),
	_address(0)
{
	LARGE_INTEGER mySize;
	mySize.QuadPart = _size;
	if (mode == SharedMemory::AM_WRITE)
		_mode = PAGE_READWRITE;

#if defined (POCO_WIN32_UTF8)
	std::wstring utf16name;
	UnicodeConverter::toUTF16(_name, utf16name);
	_memHandle = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, _mode, mySize.HighPart, mySize.LowPart, utf16name.c_str());
#else
	_memHandle = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, _mode, mySize.HighPart, mySize.LowPart, _name.c_str());
#endif

	if (!_memHandle)
		throw SystemException("Cannot create shared memory object", _name);

	map();
}


SharedMemoryImpl::SharedMemoryImpl(const Poco::File& file, SharedMemory::AccessMode mode, const void*, std::size_t size):
	_name(file.path()),
	_memHandle(INVALID_HANDLE_VALUE),
	_fileHandle(INVALID_HANDLE_VALUE),
	_size(size),
	_mode(PAGE_READONLY),
	_address(0)
{
	DWORD shareMode = FILE_SHARE_READ;
	DWORD fileMode  = GENERIC_READ;

	if (mode == SharedMemory::AM_WRITE)
	{
		_mode = PAGE_READWRITE;
		fileMode |= GENERIC_WRITE;
	}

#if defined (POCO_WIN32_UTF8)
	_fileHandle = CreateFileW(file.wpath().c_str(), fileMode, shareMode, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
#else
	_fileHandle = CreateFileA(_name.c_str(), fileMode, shareMode, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
#endif

	if (_fileHandle == INVALID_HANDLE_VALUE)
	{
		DWORD dwError = GetLastError();
		if (dwError == ERROR_FILE_NOT_FOUND || dwError == ERROR_PATH_NOT_FOUND)
			throw FileNotFoundException(_name);
		throw OpenFileException("Cannot open memory mapped file", _name);
	}

	LARGE_INTEGER liFileSize = {0};
	GetFileSizeEx(_fileHandle, &liFileSize);
	if (_size == 0 || _size > liFileSize.QuadPart)
		_size = liFileSize.QuadPart;
	else
		liFileSize.QuadPart = _size;
	_memHandle = CreateFileMapping(_fileHandle, NULL, _mode, liFileSize.HighPart, liFileSize.LowPart, NULL);
	if (!_memHandle)
	{
		CloseHandle(_fileHandle);
		_fileHandle = INVALID_HANDLE_VALUE;
		throw SystemException("Cannot map file into shared memory", _name);
	}
	map();
}


SharedMemoryImpl::~SharedMemoryImpl()
{
	unmap();
	close();
}

namespace
{
	bool canRead(void *p)
	{
		__try
		{
			char a = *reinterpret_cast<const char *>(p);
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return false;
		}
		return true;
	}
}


void SharedMemoryImpl::map()
{
	DWORD access = FILE_MAP_READ;
	if (_mode == PAGE_READWRITE)
		access = FILE_MAP_WRITE;
	LPVOID addr = MapViewOfFile(_memHandle, access, 0, 0, static_cast<SIZE_T>(_size));
	if (!addr)
		throw SystemException("Cannot map shared memory object", _name);
	if (_size == 1 && !canRead(addr))
	{
		UnmapViewOfFile(addr);
		throw SystemException("Cannot map shared memory object", _name);
	}
	_address = static_cast<char*>(addr);
}


void SharedMemoryImpl::unmap()
{
	if (_address)
	{
		UnmapViewOfFile(_address);
		_address = 0;
		return;
	}
}


void SharedMemoryImpl::close()
{
	if (_memHandle != INVALID_HANDLE_VALUE)
	{
		CloseHandle(_memHandle);
		_memHandle = INVALID_HANDLE_VALUE;
	}

	if (_fileHandle != INVALID_HANDLE_VALUE)
	{
		CloseHandle(_fileHandle);
		_fileHandle = INVALID_HANDLE_VALUE;
	}
}


} // namespace Poco
