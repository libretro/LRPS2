/*
* Language Injector by SeventySixx
* According to the provided BIOS path and language option, injects the relative 
* language byte in the nvm BIOS file. If a BIOS is not supported, or if the requested 
* language is already set, it exits without doing nothing. 
*/

#include "language_injector.h"
#include "options_tools.h"
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>

#include "SPU2/Global.h"
#include "ps2/BiosTools.h"

static const char* BIOS_30004R_V6		= "Europev01.60(04/10/2001)Console";  //"PS2 Bios 30004R V6 Pal";
static const char* BIOS_SCPH39001		= "USAv01.60(07/02/2002)Console";     //"scph39001";
static const char* BIOS_SCPH70004_V12	= "Europev02.00(14/06/2004)Console";  //"SCPH-70004_BIOS_V12_PAL_200";


static const uint8_t ADDRESS_SCPH70004_V12	= 0x2c;
static const uint8_t ADDRESS_30004R_V6		= 0x31;
static const uint8_t ADDRESS_SCPH39001		= 0x31;

static const char* LANG_ENGLISH		= "English";
static const char* LANG_FRENCH		= "French";
static const char* LANG_SPANISH		= "Spanish";
static const char* LANG_GERMAN		= "German";
static const char* LANG_ITALIAN		= "Italian";
static const char* LANG_DUTCH		= "Dutch";
static const char* LANG_PORTUGUESE	= "Portuguese";

static const uint8_t BYTE_LANG_ENG = 0x21;
static const uint8_t BYTE_LANG_FRA = 0x22;
static const uint8_t BYTE_LANG_SPA = 0x23;
static const uint8_t BYTE_LANG_GER = 0x24;
static const uint8_t BYTE_LANG_ITA = 0x25;
static const uint8_t BYTE_LANG_DUT = 0x26;
static const uint8_t BYTE_LANG_POR = 0x27;


static const int NUM_BIOS_LANG_ENTRIES = 21;


const bios_lang bios_language[NUM_BIOS_LANG_ENTRIES] = {
	{ BIOS_30004R_V6, LANG_ENGLISH,		ADDRESS_30004R_V6, BYTE_LANG_ENG },
	{ BIOS_30004R_V6, LANG_FRENCH,		ADDRESS_30004R_V6, BYTE_LANG_FRA },
	{ BIOS_30004R_V6, LANG_SPANISH,		ADDRESS_30004R_V6, BYTE_LANG_SPA },
	{ BIOS_30004R_V6, LANG_GERMAN,		ADDRESS_30004R_V6, BYTE_LANG_GER },
	{ BIOS_30004R_V6, LANG_ITALIAN,		ADDRESS_30004R_V6, BYTE_LANG_ITA },
	{ BIOS_30004R_V6, LANG_DUTCH,		ADDRESS_30004R_V6, BYTE_LANG_DUT },
	{ BIOS_30004R_V6, LANG_PORTUGUESE,	ADDRESS_30004R_V6, BYTE_LANG_POR },

	{ BIOS_SCPH70004_V12, LANG_ENGLISH,		ADDRESS_SCPH70004_V12, BYTE_LANG_ENG },
	{ BIOS_SCPH70004_V12, LANG_FRENCH,		ADDRESS_SCPH70004_V12, BYTE_LANG_FRA },
	{ BIOS_SCPH70004_V12, LANG_SPANISH,		ADDRESS_SCPH70004_V12, BYTE_LANG_SPA },
	{ BIOS_SCPH70004_V12, LANG_GERMAN,		ADDRESS_SCPH70004_V12, BYTE_LANG_GER },
	{ BIOS_SCPH70004_V12, LANG_ITALIAN,		ADDRESS_SCPH70004_V12, BYTE_LANG_ITA },
	{ BIOS_SCPH70004_V12, LANG_DUTCH,		ADDRESS_SCPH70004_V12, BYTE_LANG_DUT },
	{ BIOS_SCPH70004_V12, LANG_PORTUGUESE,	ADDRESS_SCPH70004_V12, BYTE_LANG_POR  },

	{ BIOS_SCPH39001, LANG_ENGLISH,		ADDRESS_SCPH39001, BYTE_LANG_ENG },
	{ BIOS_SCPH39001, LANG_FRENCH,		ADDRESS_SCPH39001, BYTE_LANG_FRA },
	{ BIOS_SCPH39001, LANG_SPANISH,		ADDRESS_SCPH39001, BYTE_LANG_SPA },
	{ BIOS_SCPH39001, LANG_GERMAN,		ADDRESS_SCPH39001, BYTE_LANG_GER },
	{ BIOS_SCPH39001, LANG_ITALIAN,		ADDRESS_SCPH39001, BYTE_LANG_ITA },
	{ BIOS_SCPH39001, LANG_DUTCH,		ADDRESS_SCPH39001, BYTE_LANG_DUT },
	{ BIOS_SCPH39001, LANG_PORTUGUESE,	ADDRESS_SCPH39001, BYTE_LANG_POR  },
};



namespace LanguageInjector
{
	
	void Inject(std::string bios_path, const char* language) {


		wxString description;
		wxString bios_file = wxString(bios_path.c_str());
		if (!IsBIOS(bios_file, description)) {
			log_cb(RETRO_LOG_INFO, "Invalid BIOS file: %s \n", bios_path);
			return;
		}
		std::string bios_descr = (std::string)description;

		bios_descr.erase(remove(bios_descr.begin(), bios_descr.end(), ' '), bios_descr.end());

		//log_cb(RETRO_LOG_INFO, "Detected bios description (before): %s \n", ((std::string)description).c_str());
		log_cb(RETRO_LOG_INFO, "Detected BIOS: %s \n", bios_descr.c_str());
		

		//size_t last_dot_index = bios_path.find_last_of(".");
		//std::string rawpath = bios_path.substr(0, last_dot_index);

		size_t last_slash_index = bios_path.find_last_of("\\/");
		std::string bios_name = bios_path.substr(last_slash_index + 1, bios_path.length());
		log_cb(RETRO_LOG_DEBUG, "BIOS file name: %s\n", bios_name.c_str());

		bios_lang lang_data = _GetLanguageDataForBios(bios_descr.c_str(), language);

		if (lang_data.bios_name == NULL) {
			log_cb(RETRO_LOG_INFO, "No injection data found for Bios %s - language: %s \n", bios_name.c_str(), language);
		}
		else
		{
			if (_ModifyNVM(bios_path, lang_data))
				log_cb(RETRO_LOG_INFO, "Injected successfully language data for Bios %s - language: %s \n", bios_name.c_str(), language);
		}

	}


	bios_lang _GetLanguageDataForBios(const char* bios_name, const char* language) {

		for (int i = 0; i < NUM_BIOS_LANG_ENTRIES; i++) {
			if (!strcmp(bios_language[i].bios_name, bios_name) && !strcmp(bios_language[i].language, language)) {
				return bios_language[i];
			}
		}
		return { NULL, NULL, NULL, NULL };

	}


	bool _ModifyNVM(std::string bios_path, bios_lang lang_data) {

		size_t lastindex = bios_path.find_last_of(".");

		std::string rawpath = bios_path.substr(0, lastindex);
		std::string rawext = bios_path.substr(lastindex + 1, bios_path.length());
		std::string nvm_ext = ".nvm";

		if (_FirstUpper(rawext))
			nvm_ext = ".NVM";

		std::string path_nvm = rawpath + nvm_ext;
		//std::string path_nvm_output = rawpath + "_DEBUG_OUT" + nvm_ext;
		std::string path_nvm_output = rawpath + nvm_ext;

		const int BUFFER_SIZE = 1024;
		uint8_t buffer[BUFFER_SIZE];
		std::ifstream infile(path_nvm, std::ifstream::binary);
		infile.read((char*)buffer, BUFFER_SIZE);
		infile.close();
		//log_cb(RETRO_LOG_DEBUG, "File read in buffer\n", buffer);

		if (_ModifyLanguageOptionByte(buffer, lang_data)) {
			std::ofstream outfile(path_nvm_output, std::ofstream::binary);
			outfile.write((char*)buffer, BUFFER_SIZE);                      // write the actual text
			outfile.close();
			return true;
		}
		return false;

	}


	bool _ModifyLanguageOptionByte(uint8_t buffer[], bios_lang lang_data) {

		size_t opt_index = (size_t)lang_data.address * 16;
		log_cb(RETRO_LOG_DEBUG, "Checking language byte, found: %x\n", buffer[opt_index + 1]);

		if (buffer[opt_index + 1] == lang_data.byte_lang) {
			log_cb(RETRO_LOG_INFO, "Language already set to %s, no need to inject language byte\n", lang_data.language);
		}
		else
		{
			buffer[opt_index + 1] = lang_data.byte_lang;
			log_cb(RETRO_LOG_INFO, "Language set to %s\n", lang_data.language);
			int checksum = 0;
			for (size_t i = opt_index; i < opt_index + 15; i++) {
				checksum += buffer[i] % 256;
				//log_cb(RETRO_LOG_DEBUG, "Calc checksum adding value: %x\n", buffer[i]);
			}
			buffer[opt_index + 15] = checksum;
			log_cb(RETRO_LOG_INFO, "checksum set to %x\n", checksum);
			return true;
		}
		return false;
	}


	bool _FirstUpper(const std::string& word) {
		return word.size() && std::isupper(word[0]);
	}



} // closing namespace