#include <string>

namespace PCSX2_ui
{
	std::string GzipIsoIndexTemplate = "$(f).pindex.tmp";
	bool EnableSpeedHacks					= true;
	bool EnableGameFixes					= false;
	bool EnablePresets						= true;
	int PresetIndex							= 2;
	bool McdCompressNTFS					= true;

	// [MemoryCards]
	bool Slot1_Enable						= true;
	std::string Slot1_Filename				= "";
	bool Slot2_Enable						= false;
	std::string Slot2_Filename				= "";

	bool Multitap1_Slot2_Enable				= false;
	std::string Multitap1_Slot2_Filename	= "Mcd-Multitap1-Slot02.ps2";
	bool Multitap1_Slot3_Enable				= false;
	std::string Multitap1_Slot3_Filename	= "Mcd-Multitap1-Slot03.ps2";
	bool Multitap1_Slot4_Enable				= false;
	std::string Multitap1_Slot4_Filename	= "Mcd-Multitap1-Slot04.ps2";
	bool Multitap2_Slot2_Enable				= false;
	std::string Multitap2_Slot2_Filename	= "Mcd-Multitap2-Slot02.ps2";
	bool Multitap2_Slot3_Enable				= false;
	std::string Multitap2_Slot3_Filename	= "Mcd-Multitap2-Slot03.ps2";
	bool Multitap2_Slot4_Enable				= false;
	std::string Multitap2_Slot4_Filename	= "Mcd-Multitap2-Slot04.ps2";

	// [Folders] - not still useful
	bool UseDefaultBios						= true;
	bool UseDefaultSavestates				= true;
	bool UseDefaultMemoryCards				= true;
	bool UseDefaultPluginsFolder			= true;
	bool UseDefaultCheats					= true;
	bool UseDefaultCheatsWS					= true;
	std::string Bios						= "";
	std::string Savestates					= "";
	std::string MemoryCards					= "";
	std::string Cheats						= "";
	std::string CheatsWS					= "";
	std::string RunDisc						= "";

	// [Filenames]
	std::string GS							= "Built-in";
	std::string BIOS						= "";
	
}
