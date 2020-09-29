#include "stdafx.h"
#include "FileBackupHelper.h"
#include "define.h"

bool IsBackupDirectory(_In_ PCUNICODE_STRING directory)
{
	// no counted version of wcsstr :(

	ULONG maxSize = 1024;
	if (directory->Length > maxSize) {
		return false;
	}

	auto copy = (WCHAR*)ExAllocatePoolWithTag(PagedPool, maxSize + sizeof(WCHAR), DRIVER_TAG);
	if (!copy) {
		return false;
	}

	RtlZeroMemory(copy, maxSize + sizeof(WCHAR));
	auto count = directory->Length / sizeof(WCHAR);
	wcsncpy_s(copy, 1 + maxSize / sizeof(WCHAR), directory->Buffer, count);
	_wcslwr_s(copy, count);
	bool doBackup = wcsstr(copy, L"\\pictures\\") || wcsstr(copy, L"\\documents\\");
	ExFreePoolWithTag(copy, DRIVER_TAG);

	return doBackup;
}

NTSTATUS BackupFile(_In_ PUNICODE_STRING FileName, _In_ PCFLT_RELATED_OBJECTS FltObjects) 
{
	HANDLE hTargetFile = nullptr;
	HANDLE hSourceFile = nullptr;
	IO_STATUS_BLOCK ioStatus;
	auto status = STATUS_SUCCESS;
	void* buffer = nullptr;

	// get source file size
	LARGE_INTEGER fileSize;
	status = FsRtlGetFileSize(FltObjects->FileObject, &fileSize);
	if (!NT_SUCCESS(status) || fileSize.QuadPart == 0) {
		return status;
	}

	do {
		// open source file
		OBJECT_ATTRIBUTES sourceFileAttr;
		InitializeObjectAttributes(&sourceFileAttr, FileName,
			OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, nullptr, nullptr);

		status = FltCreateFile(
			FltObjects->Filter,		// filter object
			FltObjects->Instance,	// filter instance
			&hSourceFile,			// resulting handle
			FILE_READ_DATA | SYNCHRONIZE, // access mask
			&sourceFileAttr,		// object attributes
			&ioStatus,				// resulting status
			nullptr, FILE_ATTRIBUTE_NORMAL, 	// allocation size, file attributes
			FILE_SHARE_READ | FILE_SHARE_WRITE,		// share flags
			FILE_OPEN,		// create disposition
			FILE_SYNCHRONOUS_IO_NONALERT | FILE_SEQUENTIAL_ONLY, // create options (sync I/O)
			nullptr, 0,				// extended attributes, EA length
			IO_IGNORE_SHARE_ACCESS_CHECK);	// flags

		if (!NT_SUCCESS(status))
			break;

		// open target file
		UNICODE_STRING targetFileName;
		const WCHAR backupStream[] = L":backup";
		targetFileName.MaximumLength = FileName->Length + sizeof(backupStream);
		targetFileName.Buffer = (WCHAR*)ExAllocatePoolWithTag(PagedPool, targetFileName.MaximumLength, DRIVER_TAG);
		if (targetFileName.Buffer == nullptr)
			return STATUS_INSUFFICIENT_RESOURCES;

		RtlCopyUnicodeString(&targetFileName, FileName);
		RtlAppendUnicodeToString(&targetFileName, backupStream);

		OBJECT_ATTRIBUTES targetFileAttr;
		InitializeObjectAttributes(&targetFileAttr, &targetFileName,
			OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, nullptr, nullptr);

		status = FltCreateFile(
			FltObjects->Filter,		// filter object
			FltObjects->Instance,	// filter instance
			&hTargetFile,			// resulting handle
			GENERIC_WRITE | SYNCHRONIZE, // access mask
			&targetFileAttr,		// object attributes
			&ioStatus,				// resulting status
			nullptr, FILE_ATTRIBUTE_NORMAL, 	// allocation size, file attributes
			0,		// share flags
			FILE_OVERWRITE_IF,		// create disposition
			FILE_SYNCHRONOUS_IO_NONALERT | FILE_SEQUENTIAL_ONLY, // create options (sync I/O)
			nullptr, 0,		// extended attributes, EA length
			0 /*IO_IGNORE_SHARE_ACCESS_CHECK*/);	// flags

		ExFreePool(targetFileName.Buffer);

		if (!NT_SUCCESS(status))
			break;

		// allocate buffer for copying purposes
		ULONG size = 1 << 21;	// 2 MB
		buffer = ExAllocatePoolWithTag(PagedPool, size, DRIVER_TAG);
		if (!buffer) {
			status = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}

		// loop - read from source, write to target
		LARGE_INTEGER offset = { 0 };		// read
		LARGE_INTEGER writeOffset = { 0 };	// write

		ULONG bytes;
		auto saveSize = fileSize;
		while (fileSize.QuadPart > 0) {
			status = ZwReadFile(
				hSourceFile,
				nullptr,	// optional KEVENT
				nullptr, nullptr,	// no APC
				&ioStatus,
				buffer,
				(ULONG)min((LONGLONG)size, fileSize.QuadPart),	// # of bytes
				&offset,	// offset
				nullptr);	// optional key
			if (!NT_SUCCESS(status))
				break;

			bytes = (ULONG)ioStatus.Information;

			// write to target file
			status = ZwWriteFile(
				hTargetFile,	// target handle
				nullptr,		// optional KEVENT
				nullptr, nullptr, // APC routine, APC context
				&ioStatus,		// I/O status result
				buffer,			// data to write
				bytes, // # bytes to write
				&writeOffset,	// offset
				nullptr);		// optional key

			if (!NT_SUCCESS(status))
				break;

			// update byte count and offsets
			offset.QuadPart += bytes;
			writeOffset.QuadPart += bytes;
			fileSize.QuadPart -= bytes;
		}

		FILE_END_OF_FILE_INFORMATION info;
		info.EndOfFile = saveSize;
		NT_VERIFY(NT_SUCCESS(ZwSetInformationFile(hTargetFile, &ioStatus, &info, sizeof(info), FileEndOfFileInformation)));
	} while (false);

	if (buffer)
		ExFreePool(buffer);
	if (hSourceFile)
		FltClose(hSourceFile);
	if (hTargetFile)
		FltClose(hTargetFile);

	return status;
}