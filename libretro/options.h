#pragma once

#include <libretro.h>
#include <string>
#include <vector>
#include <type_traits>

extern retro_environment_t environ_cb;

namespace Options
{

enum Groups
{
	OPTIONS_BASE,
	OPTIONS_GFX,
	OPTIONS_EMU,
	OPTIONS_GROUPS_MAX
};

class OptionBase
{
public:
	void SetDirty() { m_dirty = true; }
	retro_variable getVariable() { return {m_id, m_options.c_str()}; }
	virtual bool empty() = 0;

protected:
	OptionBase(const char* id, const char* name, Groups group)
		: m_id(id)
		, m_name(name)
	{
		m_options = m_name;
		m_options.push_back(';');
		Register(group);
	}

	const char* m_id;
	const char* m_name;
	bool m_dirty = true;
	std::string m_options;

private:
	void Register(Groups group);
};

template <typename T, Groups group = OPTIONS_BASE>
class Option : public OptionBase
{
	static_assert(group < OPTIONS_GROUPS_MAX, "invalid option group index");
	Option(Option&) = delete;
	Option(Option&&) = delete;
	Option& operator=(Option&) = delete;

public:
	Option(const char* id, const char* name)
		: OptionBase(id, name, group)
	{
	}

	Option(const char* id, const char* name, T initial)
		: OptionBase(id, name, group)
	{
		push_back(initial ? "enabled" : "disabled", initial);
		push_back(!initial ? "enabled" : "disabled", !initial);
	}

	Option(const char* id, const char* name, std::initializer_list<std::pair<const char*, T>> list)
		: OptionBase(id, name, group)
	{
		for (auto option : list)
			push_back(option.first, option.second);
	}

	Option(const char* id, const char* name, std::initializer_list<const char*> list)
		: OptionBase(id, name, group)
	{
		for (auto option : list)
			push_back(option);
	}

	Option(const char* id, const char* name, T first, std::initializer_list<const char*> list)
		: OptionBase(id, name, group)
	{
		for (auto option : list)
			push_back(option, first + (int)m_list.size());
	}

	Option(const char* id, const char* name, T first, int count, int step = 1)
		: OptionBase(id, name, group)
	{
		for (T i = first; i < first + count; i += step)
			push_back(std::to_string(i), i);
	}

	void push_back(std::string option, T value)
	{
		if (m_list.empty())
		{
			m_options += std::string(" ") + option;
			m_value = value;
		}
		else
			m_options += std::string("|") + option;

		m_list.push_back({option, value});
	}

	template <bool is_str = !std::is_integral<T>() && std::is_constructible<T, const char*>()>
	void push_back(const char* option)
	{
		push_back(option, option);
	}
#if 0
	template <>
	void push_back<false>(const char* option)
	{
		if (m_list.empty())
			push_back(option, 0);
		else
			push_back(option, m_list.back().second + 1);
	}
#endif
	bool Updated()
	{
		if (m_dirty && !m_locked)
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

	void UpdateAndLock()
	{
		m_locked = false;
		Updated();
		m_locked = true;
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
	bool m_locked = false;
	T m_value;
	std::vector<std::pair<std::string, T>> m_list;
};

template <typename T>
using EmuOption = Option<T, OPTIONS_EMU>;
template <typename T>
using GfxOption = Option<T, OPTIONS_GFX>;

void SetVariables();
void CheckVariables();

extern GfxOption<int> upscale_multiplier;
extern GfxOption<std::string> renderer;
} // namespace Options
