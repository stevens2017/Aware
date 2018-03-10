#ifndef _AL_DB_H_
#define _AL_DB_H_

#include<vector>
#include<map>
#include <functional>
#include "sqlite3.h"

class al_db{
public:
    al_db():trans_id(-1){};
    int begin(int64_t id=-1);
    int commit(void);
    void  rollback(void);
    int insert(std::map<std::string, std::string>& map);
    int insert_with_sql(std::string& sql);
    int update(std::map<std::string, std::string>& map);
    int update_with_sql(std::string& sql);
    int del(std::map<std::string, std::string>& map);
    int del_with_sql(std::string& sql);
    int select(std::vector<std::string>& field, std::map<std::string, std::string>& map, std::function<void (int /*field count*/, char** /*values*/)> out);
    int select(std::string sql, std::function<void (int /*field count*/, char** /*values*/)> out);
    ~al_db();
private:
    int64_t trans_id;
public:
    static int64_t newid();
    static sqlite3* m_sqlite3;
};

#define SQUENCE_TABLE  "squence"
#define CONFIGURE_TABLE "aware"
#define HWADDR_TABLE "hwaddr"


#endif /*_AL_DB_H_*/

