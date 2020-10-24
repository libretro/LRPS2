#pragma once

#include <cassert>
#include <libretro.h>
#include <string>
#include <vector>

extern retro_environment_t environ_cb;

namespace Options
{
void SetVariables();
void CheckVariables();
void Register(const char* id, const char* desc, bool* dirtyPtr);

template <typename T>
class Option
{
public:
	Option(const char* id, const char* name, T initial) = delete;

	Option(const char* id, const char* name,
		   std::vector<std::pair<std::string, T>> list)
		: m_id(id)
		, m_name(name)
		, m_list(list.begin(), list.end())
	{
		Register();
	}

	Option(const char* id, const char* name, std::vector<const char*> list)
		: m_id(id)
		, m_name(name)
	{
		for (auto option : list)
			m_list.push_back({option, (T)m_list.size()});
		Register();
	}
	Option(const char* id, const char* name, T first,
		   std::vector<const char*> list)
		: m_id(id)
		, m_name(name)
	{
		for (auto option : list)
			m_list.push_back({option, first + (int)m_list.size()});
		Register();
	}

	Option(const char* id, const char* name, T first, int count, int step = 1)
		: m_id(id)
		, m_name(name)
	{
		for (T i = first; i < first + count; i += step)
			m_list.push_back({std::to_string(i), i});
		Register();
	}

	bool Updated()
	{
		if (m_dirty)
		{
			m_dirty = false;

			retro_variable var{m_id};
			T value = m_list.front().second;

			if (environ_cb && environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
			{
				for (auto option : m_list)
				{
					if (option.first == var.value)
					{
						value = option.second;
						break;
					}
				}
			}

			if (m_value != value)
			{
				m_value = value;
				return true;
			}
		}
		return false;
	}

	operator T()
	{
		Updated();
		return m_value;
	}

	T Get()
	{
		Updated();
		return m_value;
	}

	template <typename S>
	bool operator==(S value)
	{
		return (T)(*this) == value;
	}

	template <typename S>
	bool operator!=(S value)
	{
		return (T)(*this) != value;
	}

private:
	void Register();

	const char* m_id;
	const char* m_name;
	T m_value;
	bool m_dirty = true;
	std::string m_options;
	std::vector<std::pair<std::string, T>> m_list;
};

template <>
Option<std::string>::Option(const char* id, const char* name,
							std::vector<const char*> list);
template <>
Option<const char*>::Option(const char* id, const char* name,
								   std::vector<const char*> list);

template <>
Option<bool>::Option(const char* id, const char* name, bool initial);

} // namespace Options
