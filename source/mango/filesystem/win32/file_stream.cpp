/*
    MANGO Multimedia Development Platform
    Copyright (C) 2012-2016 Twilight Finland 3D Oy Ltd. All rights reserved.
*/
#include <mango/core/string.hpp>
#include <mango/core/exception.hpp>
#include <mango/filesystem/file.hpp>

#define ID "FileStream: "

namespace mango
{

    // -----------------------------------------------------------------
    // FileHandle
    // -----------------------------------------------------------------

	struct FileHandle
	{
        std::string m_filename;
		HANDLE m_handle;
		uint64 m_size;

		FileHandle(const std::string& filename, HANDLE handle)
		: m_filename(filename), m_handle(handle), m_size(0)
		{
			// cache file size
	        LARGE_INTEGER integer;
	        BOOL status = GetFileSizeEx(m_handle, &integer);
	        if (status)
	        {
	            m_size = static_cast<size_t>(integer.QuadPart);
	        }
		}

		~FileHandle()
		{
            CloseHandle(m_handle);
		}

        const std::string& filename() const
        {
            return m_filename;
        }

	    uint64 size() const
	    {
			return m_size;
	    }

	    uint64 offset() const
	    {
    	    LARGE_INTEGER dist = { 0 };
	        LARGE_INTEGER result = { 0 };
	        BOOL status = SetFilePointerEx(m_handle, dist, &result, FILE_CURRENT);
			MANGO_UNREFERENCED_PARAMETER(status);
	        return result.QuadPart;
	    }

	    void seek(uint64 distance, DWORD method)
	    {
	        LARGE_INTEGER dist;
	        dist.QuadPart = distance;
	        BOOL status = SetFilePointerEx(m_handle, dist, NULL, method);
			MANGO_UNREFERENCED_PARAMETER(status);
	    }

	    void read(void* dest, size_t size)
	    {
	        DWORD bytes_read;
	        BOOL status = ReadFile(m_handle, dest, static_cast<DWORD>(size), &bytes_read, NULL);
			MANGO_UNREFERENCED_PARAMETER(status);
			MANGO_UNREFERENCED_PARAMETER(bytes_read);
	    }

	    void write(const void* data, size_t size)
	    {
	        DWORD bytes_written;
	        BOOL status = WriteFile(m_handle, data, static_cast<DWORD>(size), &bytes_written, NULL);
			MANGO_UNREFERENCED_PARAMETER(status);
			MANGO_UNREFERENCED_PARAMETER(bytes_written);
	    }
	};

    // -----------------------------------------------------------------
    // FileStream
    // -----------------------------------------------------------------

    FileStream::FileStream(const std::string& filename, OpenMode mode)
        : m_handle(nullptr)
    {
        DWORD access;
        DWORD disposition;

        switch (mode)
        {
            case READ:
                access = GENERIC_READ;
                disposition = OPEN_EXISTING;
                break;

            case WRITE:
                access = GENERIC_WRITE;
                disposition = CREATE_ALWAYS;
                break;

            default:
                MANGO_EXCEPTION(ID"Incorrect OpenMode.");
                break;
        }

        // TODO: mode parameter
        HANDLE handle = CreateFileW(u16_fromBytes(filename).c_str(), access, 0, NULL, disposition, FILE_ATTRIBUTE_NORMAL, NULL);
        if (handle == INVALID_HANDLE_VALUE)
        {
            MANGO_EXCEPTION(ID"CreateFileW() failed.");
        }

		m_handle = new FileHandle(filename, handle);
    }

    FileStream::~FileStream()
    {
		delete m_handle;
    }

    const std::string& FileStream::filename() const
    {
        return m_handle->filename();
    }

    uint64 FileStream::size() const
    {
        return m_handle->size();
    }

    uint64 FileStream::offset() const
    {
        return m_handle->offset();
    }

    void FileStream::seek(uint64 distance, SeekMode mode)
    {
        DWORD method;

        switch (mode)
        {
            case BEGIN:
                method = FILE_BEGIN;
                break;

            case CURRENT:
                method = FILE_CURRENT;
                break;

            case END:
                method = FILE_END;
                break;

            default:
                MANGO_EXCEPTION(ID"Invalid seek mode.");
        }

		m_handle->seek(distance, method);
    }

    void FileStream::read(void* dest, size_t size)
    {
		m_handle->read(dest, size);
    }

    void FileStream::write(const void* data, size_t size)
    {
		m_handle->write(data, size);
    }

} // namespace mango
