#include "MemoryPatchDatabase.h"
#include <zlib.h>
#include <sstream>
#include <algorithm>

#define LOCAL_FILE_HEADER_SIGNATURE		0x04034b50
#define COMPRESSED_SIZE_INDEX_OFFSET		18
#define UNCOMPRESSED_SIZE_INDEX_OFFSET  	22
#define FILE_NAME_LENGTH_NO_EXTENSION		8
#define FILE_NAME_INDEX_OFFSET			30
#define LOCAL_FILE_HEADER_LENGTH		44
#define MISSING_ENTRY				182
#define MEMORY_PATCH_MAX_SIZE  			32768

static uint32_t uint32_from_bytes_little_endian(uint8_t* byte_array)
{
	return	(byte_array[0]) |
			(byte_array[1] << 8) |
			(byte_array[2] << 16)  |
			(byte_array[3] << 24);
}

static uint16_t uint16_from_bytes_little_endian(uint8_t* byte_array)
{
	return	(byte_array[0]) | (byte_array[1] << 8);
}

MemoryPatchDatabase::MemoryPatchDatabase(uint8_t *s, size_t len)
{
	this->compressed_archive_as_byte_array = s;
	this->archive_length                   = len;
}

std::vector<std::string> MemoryPatchDatabase::GetPatchLines(std::string key)
{
	std::vector<std::string> patch_lines;

	if (entries.count(key) == 1 && entries[key].compressed_data)
	{
		uint8_t uncompressed_data[MEMORY_PATCH_MAX_SIZE];
		int result;

		result = DecompressEntry(key, uncompressed_data);

		if (result == Z_STREAM_END)
		{
			std::string patch(reinterpret_cast<const char*>(uncompressed_data), entries[key].uncompressed_size);
			std::istringstream stream(patch);
			std::string line;
			while (std::getline(stream, line))
				patch_lines.push_back(line);
		}
	}

	return patch_lines;
}

int MemoryPatchDatabase::DecompressEntry(std::string key, uint8_t* dest_buffer)
{
	int result = Z_ERRNO;

	if (entries.count(key) == 1 && entries[key].compressed_data)
	{
		z_stream stream;

		// initialize the stream
		stream.next_in = reinterpret_cast<Bytef*>(entries[key].compressed_data);
		stream.avail_in = entries[key].compressed_size;
		stream.zalloc = Z_NULL;
		stream.zfree = Z_NULL;
		stream.opaque = Z_NULL;
		result = inflateInit2(&stream, -MAX_WBITS);
		
		// decompress the data into dest_buffer
		if (result == Z_OK) {
			stream.next_out = dest_buffer;
			// we're assuming uncompressed_size <= 32768
			stream.avail_out = entries[key].uncompressed_size;
			result = inflate(&stream, Z_NO_FLUSH);
		}
	}
	
	return result;
}

void MemoryPatchDatabase::InitEntries()
{
	InitEntries(this->compressed_archive_as_byte_array, this->archive_length);
}

/*
The widescreen patch database is initialized based on the .ZIP format
specification: https://pkware.cachefly.net/webdocs/casestudies/APPNOTE.TXT.

This works by streaming across the local headers. Because this is only used for
the embedded cheats_ws.zip archive and is not user-facing, we can safely make
the following simplifying assumptions:

(1) The general purpose bit flags are all set to 0
(2) In particular, encryption is not used
(3) Extra fields are not used
(4) Data descriptors are not used
(5) Compression method is deflate
(6) All files inside the archive are filenames of the form XXXXXXX.pnach,
	case insensitive
(7) Maximum allowed file size inside the archive is 32768 bytes

If any of the above assumptions are violated, this code may NOT work, so don't
use this as a general-purpose .ZIP reader.
*/
void MemoryPatchDatabase::InitEntries(uint8_t* compressed_archive_as_byte_array, uint32_t archive_length)
{
	uint32_t header_signature;
	uint32_t compressed_size;
	uint32_t uncompressed_size;
	uint32_t total_compressed_entry_size;

	if (archive_length < LOCAL_FILE_HEADER_LENGTH)
		return; // finished processing the last entry
	
	header_signature = uint32_from_bytes_little_endian(compressed_archive_as_byte_array);
	
	if (header_signature != LOCAL_FILE_HEADER_SIGNATURE)
		return; // finished processing the last entry

	compressed_size = uint32_from_bytes_little_endian(&compressed_archive_as_byte_array[COMPRESSED_SIZE_INDEX_OFFSET]);
	uncompressed_size = uint32_from_bytes_little_endian(&compressed_archive_as_byte_array[UNCOMPRESSED_SIZE_INDEX_OFFSET]);
	total_compressed_entry_size = compressed_size + LOCAL_FILE_HEADER_LENGTH;

	if (archive_length < total_compressed_entry_size)
		return; // archive entry is invalid; no more valid entries to add to database

	std::string file_name(reinterpret_cast<const char*>(&compressed_archive_as_byte_array[FILE_NAME_INDEX_OFFSET]),
		FILE_NAME_LENGTH_NO_EXTENSION);

	std::transform(file_name.begin(), file_name.end(), file_name.begin(), ::toupper);

	entries[file_name].compressed_size = compressed_size;
	entries[file_name].uncompressed_size = uncompressed_size;
	entries[file_name].compressed_data = &compressed_archive_as_byte_array[LOCAL_FILE_HEADER_LENGTH];
	
	// processes the archive, starting from the next entry
	InitEntries(&compressed_archive_as_byte_array[total_compressed_entry_size], archive_length - total_compressed_entry_size);
}
