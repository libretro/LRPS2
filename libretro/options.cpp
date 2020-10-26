
#include <libretro.h>
#include "options.h"

namespace Options
{
static std::vector<OptionBase*>& GetOptionGroup(Groups group)
{
	/* this garentees that 'list' is constructed first before being accessed other global constructors.*/
	static std::vector<OptionBase*> list[OPTIONS_GROUPS_MAX];
	return list[group];
}

void OptionBase::Register(Groups group)
{
	GetOptionGroup(group).push_back(this);
}

void SetVariables()
{
	std::vector<retro_variable> vars;
	for (int grp = 0; grp < OPTIONS_GROUPS_MAX; grp++)
		for (OptionBase* option : GetOptionGroup((Groups)grp))
			if (!option->empty())
				vars.push_back(option->getVariable());

	if (vars.empty())
		return;

	vars.push_back({});
	environ_cb(RETRO_ENVIRONMENT_SET_VARIABLES, (void*)vars.data());
}

void CheckVariables()
{
	bool updated = false;
	if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && !updated)
		return;

	for (int grp = 0; grp < OPTIONS_GROUPS_MAX; grp++)
		for (OptionBase* option : GetOptionGroup((Groups)grp))
			option->SetDirty();
}
} // namespace Options
