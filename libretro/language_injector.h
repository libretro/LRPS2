#include <cstdint>
#include <string>
#pragma once


struct bios_lang {
	const char* bios_name;
	const char* language;
	const uint8_t address;
	const uint8_t byte_lang;
};

namespace LanguageInjector
{
	void Inject(std::string bios_path, const char* language);
	
	
	bool _ModifyNVM(std::string bios_path, bios_lang lang_data);
	bios_lang _GetLanguageDataForBios(const char* bios_name, const char* language);
	bool _ModifyLanguageOptionByte(uint8_t buffer[], bios_lang lang_data);
	bool _FirstUpper(const std::string& word);
	

}


