#include <iostream>
#include <string>
#include "sqlite3.h"
#include "al_errmsg.h"
#include "al_db.h"
//#include "al_to_main.h"

#define DB_CONF "aware"

sqlite3* al_db::m_sqlite3=NULL;

static int al_init_squence_table();

/*
 * return code: 0 initial success, 1 already initial, others failed
 * opt: check or initial db
 */
extern "C" int al_db_initial(const char* dir, int opt){

    char bufp[1024];
    int rc;
    
    sprintf(bufp, "%s/%s.db", dir, DB_CONF);

    rc=sqlite3_open_v2(bufp, &al_db::m_sqlite3, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX, NULL);
    if( rc != SQLITE_OK ){
        return -1;
    }else{
        /*check tables*/
        if( opt == 1 ){
            if( (rc=al_init_squence_table()) == -1 ){
                return -1;
            }
        }
    }
    
    return rc;
}

int al_db_close(){
    sqlite3_close_v2(al_db::m_sqlite3);
}

int al_db::begin(int64_t id){
    trans_id=id;
#if 0
    int rc = sqlite3_exec(al_db::m_sqlite3, "BEGIN TRANSACTION;", 0, 0, NULL);
    if( rc != SQLITE_OK ){
        errno = AL_DB_BEGIN_TRANS_FAILED;
        return -1;
    }else{
        return 0;
    }
#endif
    return 0;
}

int al_db::commit(void){
    trans_id=-1;
#if 0
    char* pmsg=NULL;
    int rc = sqlite3_exec( al_db::m_sqlite3, "COMMIT;", 0, 0, &pmsg);
    if( rc != SQLITE_OK ){
        fprintf(stderr, "Commit failed, msg:%s", pmsg);
        sqlite3_free(pmsg);
        return -1;
    }else{
        return 0;
    }
#endif
    return 0;
}

void al_db::rollback(void){
    int error_code=errno;

    if( trans_id == -1 ){
        return;
    }
    
    char* pmsg=NULL;
    char buf[1024];
    int rc;
    errno = 0;
    sprintf(buf, "delete from aware where key2='%d'", trans_id);
    rc = sqlite3_exec( al_db::m_sqlite3, buf, 0, 0, &pmsg);
    if( rc != SQLITE_OK ){
        fprintf(stderr, "Commit failed, msg:%s", pmsg);
        sqlite3_free(pmsg);
    }

    if( error_code ) {
       errno=error_code;
    }
}

int64_t al_db::newid(){
    char* errmsg=NULL;
    al_db db;

    std::string sql=std::string("insert into main.")+std::string(SQUENCE_TABLE)+std::string(" ( blank) values (\" \")");
    int rc = sqlite3_exec( al_db::m_sqlite3, sql.c_str(), 0, 0, &errmsg);
    if( rc != SQLITE_OK ){
        sqlite3_free((void*)errmsg);
        return -1;
    }else{
	return sqlite3_last_insert_rowid(al_db::m_sqlite3);
    }

    return -1;
}

al_db::~al_db(){
    rollback();
}


static int sequence_check_callback(void *status, int argc, char **argv, char **azColName){
    if( argc != 1){
        return -1;
    }
    *(int*)status = atoi(argv[0]);
    return 0;
}

static int al_init_squence_table(){

    char* errmsg=NULL;
    int status;
    
    std::string sqsql=std::string("SELECT CASE WHEN tbl_name = \"")+SQUENCE_TABLE+ std::string("\" THEN 1 ELSE 0 END FROM sqlite_master WHERE tbl_name = \"")+SQUENCE_TABLE+std::string("\" AND type = \"table\"");
    std::string sqsqlc=std::string("CREATE  TABLE  ")+SQUENCE_TABLE+std::string(" ( id INTEGER PRIMARY KEY  AUTOINCREMENT  NOT NULL  UNIQUE , blank VARCHAR NOT NULL )");
    std::string sqsqla=std::string("CREATE  TABLE ")+CONFIGURE_TABLE+std::string(" (id INTEGER PRIMARY KEY  AUTOINCREMENT  NOT NULL  UNIQUE  DEFAULT 0, value VARCHAR(256) NOT NULL , is_live BOOL NOT NULL  DEFAULT 1, key1 VARCHAR(32) NOT NULL , key2 VARCHAR(32), key3 VARCHAR(32), key4 VARCHAR(32), key5 VARCHAR(32), key6 VARCHAR(32), key7 VARCHAR(32), key8 VARCHAR(32), key9 VARCHAR(32), key10 VARCHAR(32), key11 VARCHAR(32), key12 VARCHAR(32), key13 VARCHAR(32), key14 VARCHAR(32), key15 VARCHAR(32), key16 VARCHAR(32), key17 VARCHAR(32), key18 VARCHAR(32), key19 VARCHAR(32), key20 VARCHAR(32))");
    std::string sqsqld=std::string("CREATE  TABLE ")+HWADDR_TABLE+std::string(" (id INTEGER PRIMARY KEY  AUTOINCREMENT  NOT NULL  UNIQUE  DEFAULT 0, ifname VARCHAR(256) NOT NULL , hwaddr VARCHAR(32) NOT NULL)");
    
    status=0;
    int ret = sqlite3_exec(al_db::m_sqlite3, sqsql.c_str(), sequence_check_callback, &status, &errmsg);
    if( ret != SQLITE_OK ){
        std::cout<<"Sql:"<<sqsql<<"Error code:"<<ret<<" Error message:"<< errmsg<<std::endl;
        return -1;
    }else{
        if(status == 0){
            ret = sqlite3_exec(al_db::m_sqlite3, sqsqlc.c_str(), NULL, NULL, &errmsg);
            if( ret != SQLITE_OK ){
                std::cout<<"Sql:"<<sqsqlc<<"Error code:"<<ret<<" error message:"<< errmsg<<std::endl;
                return -1;
            }else{
                    ret = sqlite3_exec(al_db::m_sqlite3, sqsqla.c_str(), NULL, NULL, &errmsg);
                    if( ret != SQLITE_OK ){
                        std::cout<<"Sql:"<<sqsqla<<" error message:"<< errmsg<<std::endl;
                        return -1;
                    }else{
                        ret = sqlite3_exec(al_db::m_sqlite3, sqsqld.c_str(), NULL, NULL, &errmsg);
                        if( ret != SQLITE_OK ){
                            std::cout<<"Sql:"<<sqsqla<<" error message:"<< errmsg<<std::endl;
                            return -1;
                        }else{
                            return 1;
                        }
                    }
            }
        }
    }

    return 0;
}

int al_db::insert(std::map<std::string, std::string>& map){

    std::string sql_key("insert into aware( ");
    std::string sql_val(" values(");
    for(std::map<std::string, std::string>::iterator i=map.begin(); i != map.end(); i++){
        sql_key += i->first+",";
        sql_val += std::string("\"")+i->second+std::string("\",");
    }

    int pos=sql_key.rfind(',');
    sql_key=sql_key.substr(0, pos); 
    pos=sql_val.rfind(',');
    sql_val=sql_val.substr(0, pos);
    sql_key+=")"+sql_val+")";

    char *errmsg;
    int ret = sqlite3_exec(al_db::m_sqlite3, sql_key.c_str(), NULL, NULL, &errmsg);
    if( ret != SQLITE_OK ){
        std::cout<<"Sql:"<<sql_key<<" error message:"<< errmsg<<std::endl;
        return -1;
    }

    return 0;
}

int al_db::insert_with_sql(std::string& sql){
    char *errmsg;
    int ret = sqlite3_exec(al_db::m_sqlite3, sql.c_str(), NULL, NULL, &errmsg);
    if( ret != SQLITE_OK ){
        return -1;
    }

    return 0;
}

int al_db::update(std::map<std::string, std::string>& map){
    
    std::string sql("update aware set ");
    sql += " value = \""+map["value"]+"\" where ";
    
    for(std::map<std::string, std::string>::iterator i=map.begin(); i != map.end(); i++){
        if( i->first.compare("value") == 0 ){
            continue;
        }else{
            sql += i->first+std::string("=\"")+i->second+std::string("\" and ");
        }
    }

    int pos=sql.rfind("and");
    sql=sql.substr(0, pos);

    char *errmsg;
    int ret = sqlite3_exec(al_db::m_sqlite3, sql.c_str(), NULL, NULL, &errmsg);
    if( ret != SQLITE_OK ){
        std::cout<<"Sql:"<<sql<<" error message:"<< errmsg<<std::endl;
        return -1;
    }

    return 0;
}

int al_db::update_with_sql(std::string& sql){
    char *errmsg;
    int ret = sqlite3_exec(al_db::m_sqlite3, sql.c_str(), NULL, NULL, &errmsg);
    if( ret != SQLITE_OK ){
        std::cout<<"Sql:"<<sql<<" error message:"<< errmsg<<std::endl;
        return -1;
    }
    return 0;
}

int al_db::del(std::map<std::string, std::string>& map){
    
    std::string sql("delete from aware where ");
    
    for(std::map<std::string, std::string>::iterator i=map.begin(); i != map.end(); i++){
        sql += i->first+std::string("=\"")+i->second+std::string("\" and ");
    }

    int pos=sql.rfind("and");
    sql=sql.substr(0, pos);

    char *errmsg;
    int ret = sqlite3_exec(al_db::m_sqlite3, sql.c_str(), NULL, NULL, &errmsg);
    if( ret != SQLITE_OK ){
        std::cout<<"Sql:"<<sql<<" error message:"<< errmsg<<std::endl;
        return -1;
    }

    return 0;
}

int al_db::del_with_sql(std::string& sql){
        
    char *errmsg;
    int ret = sqlite3_exec(al_db::m_sqlite3, sql.c_str(), NULL, NULL, &errmsg);
    if( ret != SQLITE_OK ){
        return -1;
    }

    return 0;
    
}

static int select_callback(void *out, int argc, char **argv, char **azColName){

    auto outf=(std::function<int (int, char**)>*)out;
    if( outf != NULL ){
        (*outf)(argc, argv);
    }

    return 0;
}

int al_db::select(std::vector<std::string>& field, std::map<std::string, std::string>& map,  std::function<void (int /*field count*/, char** /*values*/)> out){
    
    std::string sql("select ");

    if( field.size() != 0 ){
        for(auto i=field.begin(); i!=field.end(); i++){
            sql+=*i+", ";
        }
        int pos=sql.rfind(',');
        sql=sql.substr(0, pos);
    }else{
        sql +="* ";
    }
    
    sql+=std::string(" from aware where ");
    
    for(auto i=map.begin(); i != map.end(); i++){
        sql += i->first+std::string("=\"")+i->second+std::string("\" and ");
    }
    sql += "is_live=1";
    
    std::cout<<"Query Sql:"<<sql<<std::endl;
    
    char *errmsg;
    int ret = sqlite3_exec(al_db::m_sqlite3, sql.c_str(),select_callback, &out,  &errmsg);
    if( ret != SQLITE_OK ){
        std::cout<<"Sql:"<<sql<<" error message:"<< errmsg<<std::endl;
        return -1;
    }

    return 0;
}

int al_db::select(std::string sql, std::function<void (int /*field count*/, char** /*values*/)> out){
    
    std::cout<<"Query Sql:"<<sql<<std::endl;
    
    char *errmsg;
    int ret = sqlite3_exec(al_db::m_sqlite3, sql.c_str(),select_callback, &out,  &errmsg);
    if( ret != SQLITE_OK ){
        std::cout<<"Sql:"<<sql<<" error message:"<< errmsg<<std::endl;
        return -1;
    }

    return 0;
}

