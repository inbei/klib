#pragma once

#ifndef __INIFILE_HPP__
#define __INIFILE_HPP__
#include <string>
#include <map>
#include <vector>
#include "util/KStringUtility.h"

namespace klib
{
    class KIniFile
    {
    public:
        bool ParseFile(const std::string& filepath)
        {
            FILE* fd = fopen(filepath.c_str(), "r");
            if (fd)
            {
                std::string line;
                std::string group;
                while (!feof(fd))
                {
                    if (ReadLine(fd, line))
                    {
                        line = KStringUtility::TrimString(line);
                        if (line[0] == '#')
                        {
                            continue;
                        }
                        if (line.size() > 2)
                        {
                            if (line[0] == '[' && line[line.size() - 1] == ']')// group
                            {
                                group = line.substr(1, line.size() - 2);
                            }
                            else
                            {
                                std::vector<std::string> param;
                                KStringUtility::SplitString(line, "=", param, true);
                                if (param.size() == 2)
                                {
                                    std::map<std::string, std::string>& params = m_iniDat[group];
                                    param[0] = KStringUtility::TrimString(param[0]);
                                    param[1] = KStringUtility::TrimString(param[1]);
                                    params[param[0]] = param[1];
                                }
                            }
                        }
                    }
                }
                fclose(fd);
                return true;
            }
            return false;
        }

        bool GetValue(const std::string& group, const std::string& key, std::string& val)
        {
            std::map<std::string, std::map<std::string, std::string> >::iterator it = m_iniDat.find(group);
            if (it != m_iniDat.end())
            {
                std::map<std::string, std::string>::iterator nit = it->second.find(key);
                if (nit != it->second.end())
                {
                    val = nit->second;
                    return true;
                }
            }
            return false;
        }

        const std::map<std::string, std::map<std::string, std::string> >& GetData() const { return m_iniDat; }


    private:
        bool ReadLine(FILE* fd, std::string& line)
        {
            line.clear();
            char c;
            while (!feof(fd) && (c = getc(fd)) != '\n')
            {
                line.push_back(c);
            }
            return !line.empty();
        }


    private:
        std::map<std::string, std::map<std::string, std::string> > m_iniDat;
    };
};
#endif
