#pragma once

#include <cassert>
#include <libretro.h>
#include <string>
#include <vector>

extern retro_environment_t environ_cb;

namespace Options
{
class OptionBase
{
public:
	void SetDirty() { m_dirty = true; }
	retro_variable getVariable() { return {m_id, m_options.c_str()}; }
	virtual bool empty() = 0;

protected:
	OptionBase(const char* id, const char* name)
		: m_id(id)
		, m_name(name)
	{
		m_options = m_name;
		m_options.push_back(';');
		Register();
	}

	const char* m_id;
	const char* m_name;
	bool m_dirty = true;
	std::string m_options;

private:
	void Register();
};

template <typename T>
class Option : private OptionBase
{
public:
	Option(const char* id, const char* name)
		: OptionBase(id, name)
	{
	}

	Option(const char* id, const char* name, T initial);

	Option(const char* id, const char* name,
		   std::vector<std::pair<const char*, T>> list)
		: OptionBase(id, name)
	{
		for (auto option : list)
			push_back(option.first, option.second);
	}

	Option(const char* id, const char* name, std::vector<const char*> list)
		: OptionBase(id, name)
	{
		for (auto option : list)
			push_back(option, (T)m_list.size());
	}

	Option(const char* id, const char* name, T first,
		   std::vector<const char*> list)
		: OptionBase(id, name)
	{
		for (auto option : list)
			push_back(option, first + (int)m_list.size());
	}

	Option(const char* id, const char* name, T first, int count, int step = 1)
		: OptionBase(id, name)
	{
		for (T i = first; i < first + count; i += step)
			push_back(std::to_string(i), i);
	}

	void push_back(const char* name, T value)
	{
		if (m_list.empty())
		{
			m_options += std::string(" ") + name;
			m_value = value;
		}
		else
			m_options += std::string("|") + name;

		m_list.push_back({name, value});
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
		return (T) * this;
	}

	template <typename S>
	bool operator==(S value)
	{
		return (T) * this == value;
	}

	template <typename S>
	bool operator!=(S value)
	{
		return (T) * this != value;
	}

	virtual bool empty() override { return m_list.empty(); }

private:
	T m_value;
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


void SetVariables();
void CheckVariables();

extern Option<int> upscale_multiplier;
} // namespace Options
