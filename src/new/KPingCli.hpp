#ifndef _PING_HPP_
#define _PING_HPP_
#include <string>
#include <sstream>
#include <iostream>

class KPingCli
{
public:
	enum {pnull, pwindows,  paix, plinux, phpux};
	static void CmdOutput(const std::string &cmd,  std::string &output)
	{
		char buffer[513] = {0};
#if defined(WIN32)
		FILE *file = _popen(cmd.c_str(), "r");
#else
		FILE *file = popen(cmd.c_str(), "r");
#endif
		if (NULL != file)
		{
			while (NULL != fgets(buffer, 512, file))
			{
				output += buffer;
			}
#if defined(WIN32)
			_pclose(file);
#else
			pclose(file);
#endif
		}
	}
	static int GetPlatform()
	{
		static int pf = pnull;
		if (pf == pnull)
		{
#if defined(WIN32)
			pf = pwindows;
#else
			std::string result;
			CmdOutput("uname", result);
			if (result.find("AIX") != std::string::npos)
			{
				pf = paix;
			}
			else if (result.find("HP-UX") != std::string::npos)
			{
				pf = phpux;
			}
			else if (result.find("Linux") != std::string::npos)
			{
				pf = plinux;
			}
			else
			{
				pf = pnull;
			}
#endif
		}
		return pf;
	}
	static const std::string Ping(const std::string & ip)
	{
		int pf = GetPlatform();
		//ping 一次，超时时间为1秒
		switch (pf)
		{
		case plinux:
			{
				return std::string("ping -c 1 -W 1 ") + ip;
			}
		case paix:
			{
				return std::string("ping -c 1 -w 1 ") + ip;
			}
		case pwindows:
			{
				return std::string("cmd /c ping -n 1 -w 1000 ") + ip;
			}
		case phpux:
			{
				return std::string("ping ") + ip + " -n 1 -m 1";
			}
		default:
			return std::string();
		}
	}
	static bool Available(const std::string & ip)
	{
#if defined(WIN32)
		static std::string key("来自");
#else
		static std::string key("from");
#endif
		std::string result;
		CmdOutput(Ping(ip), result);
		return (result.find(key) != std::string::npos);
	}
	static size_t GetPmtu(const std::string &host)
	{
		static size_t MaxMtu = 1500;
		// ping -n 1 -w 1000 www.baidu.com -f -l 1500
		// ping一次超时1000毫秒强制不分片数据包大小1500字节
		std::string cmd (Ping(host));
		size_t lastFalseMtu = MaxMtu;
		size_t curTrueMtu = MaxMtu;
		size_t maxTrueMtu = 0;
		int count = 0;
		bool success = true;
		do
		{
			++count;
			std::ostringstream os;
			os << cmd << " -f -l " << curTrueMtu;
			std::string pcmd = os.str();
			std::cout << pcmd << std::endl;
			std::string result;
			CmdOutput(pcmd, result);
			//std::cout << result << std::endl;
			if(result.find("TTL") != std::string::npos)
			{
				maxTrueMtu = (curTrueMtu > maxTrueMtu?curTrueMtu:maxTrueMtu);
				curTrueMtu += (lastFalseMtu - curTrueMtu) / 2;
			}
			else /*if (result.find("DF") != std::string::npos)*/
			{
				if (curTrueMtu == 0)
				{
					break;
				}
				lastFalseMtu = curTrueMtu;
				curTrueMtu = maxTrueMtu;
			}
			/*else
			{
				// ping failed
				success = false;
				break;
			}*/
		}while(lastFalseMtu - curTrueMtu > 1);
		std::cout << "count: " << count << ",pmtu is " << maxTrueMtu << std::endl;
		return maxTrueMtu;
	}
};

#endif