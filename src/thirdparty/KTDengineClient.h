#pragma once
#include <string>
#include <vector>
#include "taos.h"
#include "taoserror.h"
namespace thirdparty {
	typedef void (*ContinuousQueryCb)(void* param, TAOS_RES*, TAOS_ROW row);


	struct KTDendgineClientConfig
	{
		std::string host;
		std::string user;
		std::string passwd;
		std::string db;
	};

	class KTDEngineQuery
	{
	public:
		KTDEngineQuery(TAOS_STMT* stmt, TAOS_RES* res) :m_res(res), m_fields(NULL), m_numFields(0),m_stmt(stmt) { memset(m_buffer, 0, sizeof(m_buffer)); }

		KTDEngineQuery() :m_res(NULL), m_fields(NULL), m_numFields(0), m_stmt(NULL) { memset(m_buffer, 0, sizeof(m_buffer)); }

		inline bool IsValid() const { return (m_stmt != NULL || m_res != NULL); }

		TAOS_FIELD* GetFields()
		{
			if(m_res != NULL && m_fields == NULL)
				m_fields = taos_fetch_fields(m_res);
			return m_fields;
		}

		int GetFieldCount()
		{
			if (m_res != NULL && m_numFields == 0)
				m_numFields = taos_num_fields(m_res);
			return m_numFields;
		}

		bool BindParams(TAOS_BIND* params)
		{
			if (m_stmt != NULL)
			{
				int rc = taos_stmt_bind_param(m_stmt, params);
				if (rc != 0)
				{
					printf("failed to bind param\n");
					return false;
				}
				rc = taos_stmt_add_batch(m_stmt);
				if (rc != 0)
				{
					printf("failed to bind param\n");
					return false;
				}
				return true;
			}
			return false;
		}

		bool Exec()
		{
			if (m_stmt == NULL && m_res == NULL)
				return false;
			else if (m_stmt != NULL && m_res != NULL)
				Reset();
			else if (m_stmt == NULL && m_res != NULL)
				return true;

			int rc = taos_stmt_execute(m_stmt);
			if (rc != 0) 
			{
				printf("failed to execute statement\n");
				return false;
			}

			m_res = taos_stmt_use_result(m_stmt);
			rc = taos_errno(m_res);
			if (rc != 0)
			{
				printf("errstr:[%s]\n", taos_errstr(m_res));
				return false;
			}

			return true;
		}

		bool Next(std::vector<std::string> &row)
		{
			row.swap(std::vector<std::string>());

			if (GetFields() == NULL)
				return false;

			if (GetFieldCount() < 1)
				return false;
				
			TAOS_ROW tr = NULL;
			if (tr = taos_fetch_row(m_res))
			{
				taos_print_row(m_buffer, tr, m_fields, m_numFields);
				row.push_back(std::string(m_buffer));
				return true;
			}
			return false;
		}		

		void Release()
		{
			if (m_stmt != NULL)
			{
				taos_stmt_close(m_stmt);
				m_stmt = NULL;
			}
			Reset();
		}

	private:
		void Reset()
		{
			if (m_res != NULL)
			{
				taos_free_result(m_res);
				m_res = NULL;
			}
			m_fields = NULL;
			m_numFields = 0;
			memset(m_buffer, 0, sizeof(m_buffer));
		}

	private:
		TAOS_STMT* m_stmt;
		TAOS_RES* m_res;
		TAOS_FIELD* m_fields;
		int m_numFields;
		char m_buffer[256];
	};

	class KTDEngineClient
	{
	public:
		KTDEngineClient()
			:m_taos(NULL)
		{

		}

		bool Connect(const KTDendgineClientConfig& conf)
		{
			m_conf = conf;
			taos_options(TSDB_OPTION_TIMEZONE, "GMT-8");

			m_taos = taos_connect(conf.host.c_str(), conf.user.c_str(), conf.passwd.c_str(), conf.db.c_str(), 0);
			if (m_taos == NULL)
			{
				printf("failed to connect to db, reason:[%s]\n", taos_errstr(m_taos));
				return false;
			}
			return true;
		};

		void Close()
		{
			if (m_taos)
			{
				taos_close(m_taos);
				taos_cleanup();
			}
		};

		KTDEngineQuery Prepare(const std::string& sql)
		{
			TAOS_STMT* stmt = taos_stmt_init(m_taos);
			int rc = taos_stmt_prepare(stmt, sql.c_str(), 0);
			if (rc != 0)
			{
				printf("failed to execute taos_stmt_prepare. code:0x%x\n", rc);
				taos_stmt_close(stmt);
				return KTDEngineQuery();
			}

			return KTDEngineQuery(stmt, NULL);
		};

		KTDEngineQuery Query(const std::string& sql)
		{
			TAOS_RES* res = taos_query(m_taos, sql.c_str());

			if (res == NULL)
				return KTDEngineQuery();

			int rc = taos_errno(res);
			if (rc != 0)
				printf("errstr:[%s]\n", taos_errstr(res));

			return KTDEngineQuery(NULL, res);
		}

		TAOS_STREAM* BeginContinuousQuery(const std::string& sql, ContinuousQueryCb cb, int64_t stime, void* param)
		{
			return taos_open_stream(m_taos, sql.c_str(), cb, stime, param, NULL);
		}

		void EndContinuousQuery(TAOS_STREAM* ts)
		{
			taos_close_stream(ts);
		}

	private:
		TAOS *m_taos;
		KTDendgineClientConfig m_conf;
	};

};
