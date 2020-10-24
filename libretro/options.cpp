
#include <libretro.h>

#include "options.h"

namespace Options
{
static std::vector<OptionBase*>& GetOptionList()
{
	/* this ensures that the vector ist constructed first before being accessed */
	static std::vector<OptionBase*> list;
	return list;
}

void OptionBase::Register()
{
	GetOptionList().push_back(this);
}

void SetVariables()
{
	std::vector<retro_variable> vars;
	for (OptionBase* option : GetOptionList())
		if (!option->empty())
			vars.push_back(option->getVariable());

	if(vars.empty())
		return;

	vars.push_back({});
	environ_cb(RETRO_ENVIRONMENT_SET_VARIABLES, (void*)vars.data());
}

void CheckVariables()
{
	bool updated = false;
	if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && !updated)
		return;

	for (OptionBase* option : GetOptionList())
		option->SetDirty();
}

template <>
Option<std::string>::Option(const char* id, const char* name,
							std::vector<const char*> list)
	: OptionBase(id, name)
{
	for (auto option : list)
		push_back(option, option);
}

template <>
Option<const char*>::Option(const char* id, const char* name,
							std::vector<const char*> list)
	: OptionBase(id, name)
{
	for (auto option : list)
		push_back(option, option);
}

template <>
Option<bool>::Option(const char* id, const char* name, bool initial)
	: OptionBase(id, name)
{
	push_back(initial ? "enabled" : "disabled", initial);
	push_back(!initial ? "enabled" : "disabled", !initial);
}

} // namespace Options
