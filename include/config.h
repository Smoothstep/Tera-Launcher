#ifndef __CONFIG_H__
#define __CONFIG_H__

#pragma once

#include <boost/property_tree/ptree.hpp>
#include <boost/optional.hpp>
#include <boost/property_tree/ini_parser.hpp>

typedef boost::optional<boost::property_tree::basic_ptree<std::string, std::string, std::less<std::string> >& > optional_pt;

class CConfig
{
protected:
	std::string m_LastError;

private:
	boost::property_tree::ptree m_Config;

public:
	bool LoadConfig(const std::string& strConfigFile)
	{
		if (!m_Config.empty())
		{
			m_Config.clear();
		}

		std::ifstream ifConfig(strConfigFile, std::ios::in | std::ios::binary);

		if(!ifConfig.is_open())
		{
			m_LastError = "Unable to open config file.";
			return false;
		}

		std::stringstream ss;
		ss << ifConfig.rdbuf();

		try
		{
			boost::property_tree::ini_parser::read_ini(ss, m_Config);
		}
		catch (const boost::property_tree::ini_parser_error& error)
		{
			m_LastError = error.message();
			return false;
		}

		return true;
	}

	template<typename T>
	bool GetValue(const std::string& strKey, T& value)
	{
		optional_pt pt = boost::property_tree::ptree::get_child_optional(strKey);

		if (!pt)
		{
			m_LastError = "No such child in config: " + strKey;
			return false;
		}

		boost::optional<T> val = pt->get_value_optional();

		if (!val)
		{
			m_LastError = "Cast failed for key's value: " + strKey;
			return false;
		}

		value = val.value();

		return true;
	}

	std::string& GetLastError()
	{
		return m_LastError;
	}
};


#endif