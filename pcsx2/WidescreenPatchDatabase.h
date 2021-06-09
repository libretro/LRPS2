#pragma once

#include <unordered_map>
#include <string>
#include <vector>

class WidescreenPatchDatabase
{
public:
	WidescreenPatchDatabase();
	WidescreenPatchDatabase(WidescreenPatchDatabase& other) = delete;
	void operator= (const WidescreenPatchDatabase&) = delete;
	static WidescreenPatchDatabase* GetSingleton();

	struct Patch
	{
		uint32_t compressed_size = 0;
		uint32_t uncompressed_size = 0;
		uint8_t* compressed_data;
	};

	std::vector<std::string> GetPatchLines(std::string key);
private:
	void InitEntries();
	void InitEntries(uint8_t* compressed_archive_as_byte_array, uint32_t archive_length);
	int DecompressEntry(std::string key, uint8_t* dest_buffer);

	std::unordered_map<std::string, Patch> entries;
	uint8_t* compressed_archive_as_byte_array;
	uint32_t archive_length;
};

static WidescreenPatchDatabase* widescreen_patch_db_ = nullptr;

uint32_t uint32_from_bytes_little_endian(uint8_t* byte_array);
uint16_t uint16_from_bytes_little_endian(uint8_t* byte_array);
