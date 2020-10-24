
#include <libretro.h>

#include "options.h"

namespace Options
{
static std::vector<OptionBase*>& GetOptionList()
{
	static std::vector<OptionBase*> list;
	return list;
}

void OptionBase::Register()
{
	GetOptionList().push_back(this);
}

void SetVariables()
{
	if (GetOptionList().empty())
		return;

	std::vector<retro_variable> vars;
	for (OptionBase* option : GetOptionList())
		if (!option->empty())
			vars.push_back(option->getVariable());

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
		m_list.push_back({option, option});
}

template <>
Option<const char*>::Option(const char* id, const char* name,
							std::vector<const char*> list)
	: OptionBase(id, name)
{
	for (auto option : list)
		m_list.push_back({option, option});
}

template <>
Option<bool>::Option(const char* id, const char* name, bool initial)
	: OptionBase(id, name)
{
	m_list.push_back({initial ? "enabled" : "disabled", initial});
	m_list.push_back({!initial ? "enabled" : "disabled", !initial});
}

} // namespace Options
