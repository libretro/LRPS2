
#include <libretro.h>

#include "options.h"

namespace Options
{
static std::vector<retro_variable>* optionsList;
static std::vector<bool*>* dirtyPtrList;

template <typename T>
void Option<T>::Register()
{
	if (!optionsList)
		optionsList = new std::vector<retro_variable>;
	if (!dirtyPtrList)
		dirtyPtrList = new std::vector<bool*>;

	if (!m_options.empty())
		return;

	m_options = m_name;
	m_options.push_back(';');
	for (auto& option : m_list)
	{
		if (option.first == m_list.begin()->first)
			m_options += std::string(" ") + option.first;
		else
			m_options += std::string("|") + option.first;
	}
	optionsList->push_back({m_id, m_options.c_str()});
	dirtyPtrList->push_back(&m_dirty);
//	Updated();
	m_value = m_list.front().second;
	m_dirty = true;
}

void SetVariables()
{
	if (optionsList->empty())
		return;
	optionsList->push_back({});
	environ_cb(RETRO_ENVIRONMENT_SET_VARIABLES, (void*)optionsList->data());
}

void CheckVariables()
{
	bool updated = false;
	if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && !updated)
		return;

	for (bool* ptr : *dirtyPtrList)
		*ptr = true;
}

template <>
Option<std::string>::Option(const char* id, const char* name,
							std::vector<const char*> list)
	: m_id(id)
	, m_name(name)
{
	for (auto option : list)
		m_list.push_back({option, option});
	Register();
}

template <>
Option<const char*>::Option(const char* id, const char* name,
							std::vector<const char*> list)
	: m_id(id)
	, m_name(name)
{
	for (auto option : list)
		m_list.push_back({option, option});
	Register();
}

template <>
Option<bool>::Option(const char* id, const char* name, bool initial)
	: m_id(id)
	, m_name(name)
{
	m_list.push_back({initial ? "enabled" : "disabled", initial});
	m_list.push_back({!initial ? "enabled" : "disabled", !initial});
	Register();
}
//static Option<std::string> test2("pcsx2_test2", "Test2", {{"aa", "hh"}, {"bb", "hho"}});

template void Option<std::string>::Register();
template void Option<const char*>::Register();
template void Option<bool>::Register();
} // namespace Options
