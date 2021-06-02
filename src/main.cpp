// KlibTest.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include "util/KTime.h"
#include "new/KOdbcClient.h"
#include "thread/KPthread.h"
#include "tcp/KWebsocketClient.hpp"
//#include "tcp/KOpenSSL.h"

int WsWorker(int j)
{
    klib::KWebsocketClient wc;
    /*klib::KOpenSSLConfig conf;
    conf.caFile = "E:\\openssl\\ca.crt";
    conf.certFile = "E:\\openssl\\client.crt";
    conf.privateKeyFile = "E:\\openssl\\client_rsa_private.pem.unsecure";*/
    if (wc.Start("172.28.70.55:9000",/* conf, */true))
    {
        while (!wc.IsConnected())
            klib::KTime::MSleep(100);

        //klib::KTime::MSleep(3600000);
        for (int i = 0; i < 1000; ++i)
        {
            klib::KWebsocketMessage msg;
            msg.Initialize("hello");
            klib::KBuffer buf;
            msg.Serialize(buf);
            std::vector<klib::KBuffer> bufs;
            bufs.push_back(buf);
            if (!wc.Send(bufs))
                buf.Release();

            klib::KTime::MSleep(1000);
            /*if (i % 999 == 0)
                wc.Disconnect();*/
            //klib::KTime::MSleep(1000);
        }
        klib::KTime::MSleep(3600000);
        wc.Stop();
        wc.WaitForStop();
    }
    return 0;
}


void TestWebsocket()
{
    std::map<int, klib::KPthread*> threads;
    for (int j = 0; j < 49; ++j)
    {
        klib::KPthread* t = new klib::KPthread("");
        if (t->Run(WsWorker, 1))
        {
            std::cout << "thread " << j << std::endl;
        }
    }
    std::map<int, klib::KPthread*>::iterator it = threads.begin();
    while (it != threads.end())
    {
        it->second->Join();
        ++it;
    }
}


void TestOdbc()
{
    /*klib::KOdbcConfig config(klib::db2);
    config.drvname = "IBM DATA SERVER DRIVER for ODBC - E:/software/snack2/db/clidriver";
    config.host = "172.25.11.21";
    config.passwd = "mics@db2";
    config.username = "db2inst1";
    config.dbname = "micsltz";*/
    

    /*klib::KOdbcConfig config(klib::oracle);
    config.drvname = "Oracle in instantclient_18_5";
    config.host = "10.41.10.11";
    config.passwd = "L18mics";
    config.username = "mics";
    config.dbname = "orapdb19";*/

    klib::KOdbcConfig config(klib::sqlite);
    config.drvname = "Devart ODBC Driver for SQLite";
    config.dbname = "e:\\sqlite.db";

    /*klib::KOdbcConfig config(klib::mysql);
    config.drvname = "MySQL ODBC 8.0 ANSI Driver";
    config.host = "172.25.11.102";
    config.passwd = "mics";
    config.username = "mics";
    config.dbname = "micssta";*/


    klib::KOdbcClient c(config);
    if (c.Connect())
    {
        //klib::KOdbcSql sql = c.Prepare("select * from dba_profiles where profile='MICS_PROFILE'");
        klib::KOdbcSql sql = c.Prepare("select * from company");
        if (!sql.IsValid()) return;
        //sql.BindParam("%d", 5);
        bool rc = sql.Execute();

        /*klib::KOdbcSql sql2 = c.Prepare("alter user mics profile mics_profile");
        if (!sql2.IsValid()) return;
        bool rc2 = sql2.Execute();*/

        const klib::KOdbcRow& header = sql.GetRow();

        std::vector<klib::KOdbcValue>::const_iterator it = header.begin();
        while (it != header.end())
        {
            std::cout << it->GetFieldName() << ",";
            ++it;
        }
        std::cout << std::endl;
        std::string sval;
        double dval;
        int64_t ival;
        bool bval;
        char* binval = new char[1024]();
        while (sql.Next())
        {
            const klib::KOdbcRow& rw = sql.GetRow();
            std::vector<klib::KOdbcValue>::const_iterator it = rw.begin();
            while (it != rw.end())
            {
                if (it->StringVal(sval))
                    std::cout << sval << ",";
                else if (it->IntegerVal(ival))
                    std::cout << ival << ",";
                else if (it->DoubleVal(dval))
                    std::cout << dval << ",";
                else if (it->GetBool(bval))
                    std::cout << bval << ",";
                else if (it->BinaryVal(binval))
                    std::cout << "binary,";
                else if (it->IsNull())
                    std::cout << "null,";
                else
                    std::cout << "unexpected,";

                ++it;
            }
            std::cout << std::endl;
        }

        std::cout << "connected\n";
        c.Disconnect();
    }
}


#include <chrono>

#define  Interval_Microseconds 1000
#define  BUFFER_SIZE 10000000
#define  TEST_TIMES 10000

void SleepSelectUS(SOCKET s, int64_t usec)
{
    struct timeval tv;
    fd_set dummy;
    FD_ZERO(&dummy);
    FD_SET(s, &dummy);
    tv.tv_sec = usec / 1000000L;
    tv.tv_usec = usec % 1000000L;
    select(0, 0, 0, &dummy, &tv);
    DWORD err = GetLastError();
    if (err != 0)
        printf("Error : %d", err);
}

void Precision_select()
{
    std::string buffer;
    buffer.assign(BUFFER_SIZE, 0);
    buffer.clear();
    int i = TEST_TIMES;
    uint64_t total_used = 0;
    WORD wVersionRequested = MAKEWORD(1, 0);
    WSADATA wsaData;
    WSAStartup(wVersionRequested, &wsaData);

    SOCKET s = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    while (i) {
        i--;
        std::chrono::steady_clock::time_point time_begin = std::chrono::steady_clock::now();
        SleepSelectUS(s, Interval_Microseconds);
        std::chrono::steady_clock::time_point time_end = std::chrono::steady_clock::now();
        char tmp[128] = { 0 };
        uint64_t used = std::chrono::duration_cast<std::chrono::microseconds>(time_end - time_begin).count();
        snprintf(tmp, 128, "%s Sleep %d us, time used : %lld us\n", __FUNCTION__, Interval_Microseconds, used);
        total_used += used;
        buffer += tmp;
    }
    closesocket(s);
    printf("%s", buffer.c_str());
    printf("%s Sleep %d us, avatar %lld us\n\n", __FUNCTION__, Interval_Microseconds, total_used / TEST_TIMES);

}

int main()
{
    TestOdbc();

    TestWebsocket();
    //TestOdbc();

    klib::KTime::MSleep(3600000);
    std::cout << "Hello World!\n";
}

// 运行程序: Ctrl + F5 或调试 >“开始执行(不调试)”菜单
// 调试程序: F5 或调试 >“开始调试”菜单

// 入门使用技巧: 
//   1. 使用解诀方案资源管理器窗口添加/管理文件
//   2. 使用团队资源管理器窗口连接到源代码管理
//   3. 使用输出窗口查看生成输出和其他消息
//   4. 使用错误列表窗口查看错误
//   5. 转到“项目”>“添加新项”以创建新的代码文件，或转到“项目”>“添加现有项”以将现有代码文件添加到项目
//   6. 将来，若要再次打开此项目，请转到“文件”>“打开”>“项目”并选择 .sln 文件
