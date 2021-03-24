#pragma once
#ifndef __CSVFILE__HPP_
#define __CSVFILE__HPP_
#include <string>
#include <vector>
#include <stdio.h>
#include "util/KStringUtility.h"
/**
csv文件解析类
**/
namespace klib {
    class KCsvFile
    {
    public:
        /************************************
        * Method:    解析csv文件
        * Returns:   
        * Parameter: filepath
        *************************************/
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

                        std::vector<std::string> cols;
                        KStringUtility::SplitString(line, ",", cols, true);
                        if (cols.size() > 3)
                        {
                            m_csvDat.push_back(cols);
                        }
                    }
                }
                fclose(fd);
                return true;
            }
            return false;
        }

        /************************************
        * Method:    获取第rindex行的数据
        * Returns:   
        * Parameter: rindex
        * Parameter: row
        *************************************/
        bool GetRowData(size_t rindex, std::vector<std::string>& row)
        {
            if (rindex < m_csvDat.size())
            {
                row = m_csvDat[rindex];
                return true;
            }
            return false;
        }

        /************************************
        * Method:    获取行数
        * Returns:   
        *************************************/
        size_t GetRowCount()
        {
            return m_csvDat.size();
        }


    private:
        /************************************
        * Method:    读取一行数据
        * Returns:   
        * Parameter: fd
        * Parameter: line
        *************************************/
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
        std::vector<std::vector<std::string> > m_csvDat;
    };
};
#endif
