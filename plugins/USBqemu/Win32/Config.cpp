#include <stdlib.h>

#include "../USB.h"

#include "CfgHelpers.h"

void CALLBACK USBsetSettingsDir( const char* dir )
{
	CfgSetSettingsDir(dir);
}
