#pragma once
#include <string>
#include <vector>
#include <stdint.h>
#include "taos.h"

namespace thirdparty {
	typedef void (*ContinuousQueryCb)(void*, void*, void**);
	
	struct KTDendgineConfig
	{
		std::string host;
		std::string user;
		std::string passwd;
		std::string db;
	};

	struct KTDengineValue
	{
		union 
		{
			char* strVal;
			int64_t ival;
			uint64_t uval;
			double dval;
		} value;

		size_t size;
		int type;

		KTDengineValue()
			:value(),size(0),type(TSDB_DATA_TYPE_NULL)
		{

		}
	};

	class KTDEngineQuery
	{
	public:
		KTDEngineQuery(void* stmt, void* res);

		KTDEngineQuery();

		inline bool IsValid() const { return (m_stmt != NULL || m_res != NULL); }

		taosField* GetFields();

		int GetFieldCount();

		bool BindParams(TAOS_BIND* params);

		bool Exec();

		bool Next(std::vector<KTDengineValue>& row);

		void Release();

	private:
		void Reset();

	private:
		void* m_stmt;
		void* m_res;
		taosField* m_fields;
		int m_numFields;
	};

	class KTDEngineClient
	{
	public:
		KTDEngineClient()
			:m_taos(NULL)
		{

		}

		bool Connect(const KTDendgineConfig& conf);

		void Close();

		KTDEngineQuery Prepare(const std::string& sql);

		KTDEngineQuery Select(const std::string& sql);

		bool Exec(const std::string& sql);

		int BeginContinuousQuery(const std::string& sql, ContinuousQueryCb cb, int64_t stime, void* param);

		void EndContinuousQuery(int);

	private:
		static void ContinuousQueryStopCb(void* param);

	private:
		void *m_taos;
		KTDendgineConfig m_conf;

	};

};
