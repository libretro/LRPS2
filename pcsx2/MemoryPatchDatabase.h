#pragma once

#include <unordered_map>
#include <string>
#include <vector>

class MemoryPatchDatabase
{
public:
	MemoryPatchDatabase(uint8_t *s, size_t len);
	MemoryPatchDatabase(MemoryPatchDatabase& other) = delete;
	void operator= (const MemoryPatchDatabase&) = delete;

	struct Patch
	{
		uint32_t compressed_size = 0;
		uint32_t uncompressed_size = 0;
		uint8_t* compressed_data;
	};

	std::vector<std::string> GetPatchLines(std::string key);
	void InitEntries();
	void InitEntries(uint8_t* compressed_archive_as_byte_array, uint32_t archive_length);
private:
	int DecompressEntry(std::string key, uint8_t* dest_buffer);

	std::unordered_map<std::string, Patch> entries;
	uint8_t* compressed_archive_as_byte_array;
	uint32_t archive_length;
};
