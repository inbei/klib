
#ifndef _FILE_HPP_
#define _FILE_HPP_

#ifdef WIN32
#include <Windows.h>
#include <io.h> // access
#define PathSeparator '\\'
#else
#include <unistd.h>
#if defined(HPUX)
#include <sys/vfs.h>
#else
#include <sys/statfs.h>
#endif
#define PathSeparator '/'
#endif
#include <string>
#include <stdint.h>
#include <cstdarg>
#include <cstdio>
#include "thread/KError.h"
#include "thread/KBuffer.h"
#include "thread/KAny.h"
#include "thread/KEventObject.h"
#include "util/KTime.h"

namespace klib {
#define HexBlockSize 48
#define HexBufferSize (HexBlockSize * 3 + 2)

#define WriteFile(fd, format, dateStr, date, timestamp) \
    {\
        int tsz = 0;\
        int wsz = 0;\
        if (timestamp)\
        {\
            wsz = fprintf(fd, "%s  ", dateStr.c_str()); \
            if (wsz > 0)\
            {\
                tsz += wsz; \
            }\
        }\
        va_list args;\
        va_start(args, format);\
        wsz = vfprintf(fd, format, args);\
        if (wsz > 0)\
        {\
            tsz += wsz;\
        }\
        va_end(args);\
        fflush(fd);\
        m_totalSize += tsz;\
        return tsz;\
    }


#define MakeMessage(fmt, buf, rsz) \
    {\
        int sz = 100;\
        va_list ap;\
        if ( (buf = (char *) malloc(sz * sizeof(char))) == NULL)\
            return false;\
        while (true) \
        {\
            va_start(ap, fmt);\
            rsz = vsnprintf (buf, sz, fmt, ap);\
            va_end(ap);\
            if (rsz > -1 && rsz < sz)\
                break;\
            sz *= 2;\
            if ((buf = (char *)realloc(buf, sz * sizeof(char))) == NULL)\
                return false;\
        }\
    }

    class KTextFile
    {
    public:
        KTextFile(size_t maxsize = 50/*mb*/, uint16_t duration = 5/*minute*/)
            :m_file(NULL), m_hexBuffer(HexBufferSize),
            m_maxSize(maxsize * 1024 * 1024), m_totalSize(0), m_duration(duration)
        {


        }

        ~KTextFile()
        {
            m_hexBuffer.Release();
            Close();
        }

        virtual void Initialize(const std::string& path, const std::string& filename, bool timestamp = false)
        {
            m_timestamp = timestamp;

            m_path = path;
            m_seedName = filename;

            // get current date
            DateTime date;
            std::string dateStr;
            KTime::NowDateTime("yyyymmddhhnnssccc", dateStr, date);
            BackUp(date, dateStr.substr(0, dateStr.size() - 3), GetMinute(date));
        }

        virtual void Close()
        {
            if (m_file)
            {
                CloseFile();
            }
        }

        void Flush()
        {
            if (m_file)
            {
                fflush(m_file);
            }
        }

        std::string GetFilePath() const
        {
            return m_filePath;
        }

        size_t WriteString(const char* format, ...)
        {
            if (!(format && strlen(format) > 0))
            {
                return 0;
            }

            // 文件被删除了，需要重新打开
            if (!IsExist(m_filePath) && m_file)
            {
                CloseFile();
            }

            std::string dateStr;
            DateTime date;
            CheckBackUp(dateStr, date);

            if (!m_file && !Open("ab+"))
            {
                return 0;
            }

            WriteFile(m_file, format, dateStr, date, m_timestamp);
        }

        size_t WriteHexString(const char* dat, size_t sz)
        {
            std::string hex;
            ToHexString(dat, sz, m_hexBuffer.GetData(), hex);
            return WriteString("%s", hex.c_str());
        }

        static bool Remove(const std::string& pathName)
        {
            if (IsExist(pathName))
            {
                return ::remove(pathName.c_str()) == 0;
            }
            return true;
        }

        static bool ReadAll(const std::string& fp, KBuffer& buf)
        {
            FILE* f = fopen(fp.c_str(), "r");
            if (f != NULL)
            {
                long curpos, length;
                curpos = ftell(f);
                fseek(f, 0L, SEEK_END);
                length = ftell(f);
                fseek(f, curpos, SEEK_SET);
                KBuffer tmp(length);
                size_t sz = fread(tmp.GetData(), 1, tmp.Capacity(), f);
                tmp.SetSize(sz);
                buf = tmp;
                return true;
            }
            return false;
        }

        static float GetDiskFreeRatio(const std::string& path)
        {
            double dratio = -1;
#ifdef WIN32
            ULARGE_INTEGER avail, total, frees;
            if (GetDiskFreeSpaceEx(path.c_str(), &avail, &total, &frees))
            {
                // 
                dratio = (frees.QuadPart / (double)total.QuadPart);
            }
#else 
            struct statfs dinfo;
            if (statfs(const_cast<char*>(path.c_str()), &dinfo) == 0)
            {
                dratio = (dinfo.f_bfree / (double)dinfo.f_blocks);
            }
#endif
            return float(dratio * 100);
        }

        static bool IsDiskFull(const std::string& path)
        {
            if (100 - GetDiskFreeRatio(path) < 0.1)
            {
                return true;
            }
            return false;
        }

        static bool IsExist(const std::string& path)
        {
#ifdef WIN32
            return ((_access(path.c_str(), 0)) != -1);
#else
            return ((access(path.c_str(), 0)) != -1);
#endif
        }

        static bool Exec(const std::string& cmd)
        {
#ifdef WIN32
            std::string wc("cmd /c ");
            wc.append(cmd);
            UINT r = WinExec(wc.c_str(), SW_HIDE);
            if (r > 31)
            {
                return true;
            }
#else 
            if (system(cmd.c_str()) == 0)
            {
                return true;
            }
#endif  
            return false;
        }

        static bool Mkdir(const std::string& path)
        {
            if (IsExist(path))
            {
                return true;
            }
#ifdef WIN32
            if (Exec(std::string("mkdir ") + path))
#else
            if (Exec(std::string("mkdir -p ") + path))
#endif
            {
                int i = 3;
                while (!IsExist(path) && i-- > 0)
                {
                    KTime::MSleep(50);
                }
                return i > 0;
            }
            return false;
        }

        static void ToHexString(const char* dat, size_t sz, char* dst, std::string& hex)
        {
            hex.clear();
            if (sz > 0)
            {
                hex.reserve(sz * 3 + (sz / 30) * 2 + 1);
                char* src = const_cast<char*>(dat);
                size_t rsz = 0;
                do
                {
                    rsz = (sz < HexBlockSize ? sz : HexBlockSize);
                    size_t pos = 0;
                    for (size_t i = 0; i < rsz; ++i)
                    {
                        sprintf(dst + pos, "%02X ", uint8_t(src[i]));
                        pos += 3;
                    }
                    dst[pos++] = '\r';
                    dst[pos++] = '\n';
                    src += rsz;
                    hex.append(dst, pos);
                } while ((sz -= rsz) > 0);
            }
        }

        static void GetFileBaseName(const std::string& name, std::string& suffix, std::string& filename)
        {
            size_t pos = name.find_last_of('.');
            if (pos != std::string::npos)
            {
                suffix = name.substr(pos);
                filename = name.substr(0, pos);
            }
            else
            {
                filename = name;
            }
        }

    protected:
        virtual bool Open(const char* mode)
        {
            if (m_seedName.empty())
            {
                return false;
            }

            if (!m_path.empty() && !Mkdir(m_path))
            {
                return false;
            }

            if (!m_file && !(m_file = fopen(m_filePath.c_str(), mode)))
            {
                return false;
            }

            m_totalSize = Size();
            return true;
        }

        size_t Size()
        {
            if (m_file)
            {
                long curpos, length;
                curpos = ftell(m_file);
                fseek(m_file, 0L, SEEK_END);
                length = ftell(m_file);
                fseek(m_file, curpos, SEEK_SET);
                return length;
            }
            return 0;
        }

        void BackUp(const DateTime& date, const std::string& dateStr, uint16_t minute)
        {
            std::string suffix, baseName;
            if (m_file)
            {
                // close file
                CloseFile();

                // rename current file
                GetFileBaseName(m_currentName, suffix, baseName);
                std::string newPath = m_path + PathSeparator + dateStr.substr(0, dateStr.size() - 2);
                if (!IsExist(newPath))
                    Mkdir(newPath);
                std::string newFilePath = newPath + PathSeparator + baseName + "_" + dateStr + (suffix.empty() ? "" : suffix);
                std::string currentFilePath = m_path + PathSeparator + m_currentName;
                ::rename(currentFilePath.c_str(), newFilePath.c_str());
            }

            // set new name for file
            GetFileBaseName(m_seedName, suffix, baseName);
            m_currentName = baseName + "_" + dateStr + (suffix.empty() ? "" : suffix);
            m_currentMinute = minute;
            m_filePath = m_path + PathSeparator + m_currentName;
        }

        void CheckBackUp(std::string& dateStr, DateTime& date)
        {
            KTime::NowDateTime("yyyymmddhhnnssccc", dateStr, date);
            uint16_t minute = GetMinute(date);
            if (m_totalSize >= m_maxSize || minute != m_currentMinute)
            {
                BackUp(date, dateStr.substr(0, dateStr.size() - 3), minute);
            }
        }

        inline uint16_t GetMinute(const DateTime& date)
        {
            return (date.minute / m_duration * m_duration);
        }

        void CloseFile()
        {
            fclose(m_file);
            m_file = NULL;
            m_totalSize = 0;
        }

    protected:
        FILE* m_file;
        size_t m_totalSize;
        size_t m_maxSize;
        volatile bool m_timestamp;

        std::string m_path;
        std::string m_seedName;
        std::string m_currentName;

        std::string m_filePath;

        uint16_t m_currentMinute;
        uint16_t m_duration;
    private:
        KBuffer m_hexBuffer;
    };

    /*
    线程安全的， 和TextFile类功能一样
    */
    class KTextFileMT :public KTextFile
    {
    public:
        virtual void Initialize(const std::string& path, const std::string& filename, bool timestamp)
        {
            KLockGuard<KMutex> lock(m_fileMutex);
            KTextFile::Initialize(path, filename, timestamp);
        }

        virtual void Close()
        {
            KLockGuard<KMutex> lock(m_fileMutex);
            KTextFile::Close();
        }

        void Flush()
        {
            KLockGuard<KMutex> lock(m_fileMutex);
            KTextFile::Flush();
        }

        size_t Size()
        {
            KLockGuard<KMutex> lock(m_fileMutex);
            return KTextFile::Size();
        }

        size_t WriteString(const char* format, ...)
        {
            KLockGuard<KMutex> lock(m_fileMutex);
            if (!(format && strlen(format) > 0))
            {
                return 0;
            }

            // 文件被删除了，需要重新打开
            if (!IsExist(m_filePath) && m_file)
            {
                CloseFile();
            }

            std::string dateStr;
            DateTime date;
            CheckBackUp(dateStr, date);

            if (!m_file && !Open("ab+"))
            {
                return 0;
            }

            WriteFile(m_file, format, dateStr, date, m_timestamp);
        }

        size_t WriteHexString(const char* dat, size_t sz)
        {
            KLockGuard<KMutex> lock(m_fileMutex);
            return KTextFile::WriteHexString(dat, sz);
        }

    protected:
        KMutex m_fileMutex;
    };

    /*
    异步的，和TextFile类功能一样
    */
    struct FileData
    {
        std::string timestamp;
        KBuffer bdat;
        char* rdat;

        FileData()
            :rdat(NULL)
        {}
    };
    class KTextFileAsyn :public KEventObject<FileData>
    {
    public:
        KTextFileAsyn()
            :KEventObject<FileData>("KTextFileAsyn Thread")
        {

        }

        bool Initialize(const std::string& path, const std::string& filename, bool timestamp)
        {
            m_timestamp = timestamp;
            m_file.Initialize(path, filename, false);
            return KEventObject<FileData>::Start();
        }

        void Close()
        {
            // 等待消息处理完
            while (!KEventObject<FileData>::IsEmpty())
                KTime::MSleep(5);
            KEventObject<FileData>::Stop();
            KEventObject<FileData>::WaitForStop();
            m_file.Close();
        }

        inline void Remove()
        {
            Close();
            m_file.Remove(m_file.GetFilePath());
        }

        bool WriteString(const char* format, ...)
        {
            if (!KEventObject<FileData>::IsRunning())
                return false;

            char* buf = NULL;
            int rsz = 0;
            MakeMessage(format, buf, rsz);
            buf[rsz] = 0;
            FileData fdat;
            fdat.rdat = buf;
            if (m_timestamp)
                KTime::NowDateTime("yyyy-mm-dd hh:nn:ss.ccc", fdat.timestamp);
            if (!Post(fdat))
            {
                free(buf);
                return false;
            }
            return true;
        }

        bool WriteHexString(const char* dat, size_t sz)
        {
            if (!KEventObject<FileData>::IsRunning())
                return false;

            KBuffer buf(sz);
            buf.ApendBuffer(dat, sz);
            FileData fdat;
            fdat.bdat = buf;
            if (!Post(fdat))
            {
                buf.Release();
                return false;
            }
            return true;
        }

    protected:
        virtual void ProcessEvent(const FileData& ev)
        {
            if (ev.rdat != NULL)
            {
                if (m_timestamp)
                    m_file.WriteString("%s %s", ev.timestamp.c_str(), ev.rdat);
                else
                    m_file.WriteString(ev.rdat);
                free(ev.rdat);
            }

            if (ev.bdat.GetSize() > 0)
            {
                m_file.WriteHexString(ev.bdat.GetData(), ev.bdat.GetSize());
                const_cast<FileData&>(ev).bdat.Release();
            }
        }

    private:
        KTextFile m_file;
        bool m_timestamp;
    };
};
#endif
