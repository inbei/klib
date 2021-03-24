#ifdef WIN32
#include <windows.h>
#else
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#endif
#include <stdio.h>
#include <string>
#include <stdint.h>
namespace klib
{
    class KSharedMemory
    {
    public:
        KSharedMemory()
            :m_shmBuf(NULL)
        {
        }

        bool Create(const std::string& key, size_t len)
        {
            // �����ڴ��ʶ�� ���������ڴ�  //
#ifdef WIN32
            int hashKey = HashCode(key);
            printf("str:[%s] hash code:[%d]\n", key.c_str(), hashKey);
            // ������ͼ��һ���������ڴ�ӳ���ļ�����  //
            m_hMap = OpenFileMapping(FILE_MAP_ALL_ACCESS, 0, key.c_str());
            if (NULL == m_hMap)
                m_hMap = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, len, key.c_str());
            if (NULL == m_hMap)
                return false;
            // ӳ������һ����ͼ���õ�ָ�����ڴ��ָ�룬������������� //
            m_shmBuf = MapViewOfFile(m_hMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
            return true;
#else
            int hashKey = HashCode(key);
            m_shmId = shmget((key_t)hashKey, len, 0666 | IPC_CREAT);
            if (m_shmId == -1)
            {
                printf("shmget failed\n");
                return false;
            }

            m_shmBuf = shmat(m_shmId, 0, 0);
            if (m_shmBuf == (void*)-1)
            {
                printf("shmat failed\n");
                return false;
            }
            return true;
#endif
        };

        inline void* GetBuffer() const { return m_shmBuf; }

        bool Detach()
        {
#ifdef WIN32
            return UnmapViewOfFile(m_shmBuf);
#else
            // �ѹ����ڴ�ӵ�ǰ�����з��� //
            if (shmdt(m_shmBuf) == -1)
            {
                printf("shmdt failed\n");
                return false;
            }
            return true;
#endif
        };

        void Release()
        {
#ifdef WIN32
            CloseHandle(m_hMap);
#else
            // ɾ�������ڴ� //   
            if (shmctl(m_shmId, IPC_RMID, 0) == -1)
            {
                printf("shmctl(IPC_RMID) failed\n");
            }
#endif
        };

        int32_t HashCode(const std::string& s)
        {
            // BKDR Hash Function
            unsigned int seed = 131; // 31 131 1313 13131 131313 etc..
            unsigned int hash = 0;
            const char* t = s.c_str();
            while (*t)
            {
                hash = hash * seed + (*t++);
            }

            return (hash & 0x7FFFFFFF);
        };

    private:
#ifdef WIN32
        LPVOID m_shmBuf;
        HANDLE m_hMap;
#else

        int m_shmId;
        void* m_shmBuf;

#endif
    };

};
