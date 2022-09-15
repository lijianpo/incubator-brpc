#ifndef _SRC_COMMON_RCM_PROTOCOL_H_
#define _SRC_COMMON_RCM_PROTOCOL_H_
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <map>
#include <vector>
#include "cJSON.h"
using namespace std;


enum storage_type{
    STORAGE_TYPE_NONE=0,
    STORAGE_TYPE_JSON=1,
    STORAGE_TYPE_PARM=2,
    STORAGE_TYPE_STRING=3
};

class Item{
public:
    Item();
    Item(std::map<string, string> *m);
    Item(const Item &item);
    Item &operator=(const Item &rhs){
    }
    int set(const string key, const string value);
    int set(const string key, int value);
    string get(const string key);
    int del(const string key);
public:
    std::map<string,string> *m_item_map;
    std::map<string,int> *m_item_imap;
};

class RcmProtocol{
public:
    RcmProtocol(int type = STORAGE_TYPE_NONE, const string str = "");
    ~RcmProtocol();
public:
    int get(const string key,string &value);
    int get(const string key,int &value);
    
    int set(const string key,const string value);
    int set(const string key,int value);
    
    int toString(string &str);

    int get_id(string &str);
    //connect：连接符 division：分隔符 如cmd=rcm_feed&uid=466605798&devid=45430001&num=8&write_history=1
    //int tostring(string &str,const string connect="=",const string division="&");
   
    int get_other(const string key, string &value);
    int set_other(const string key, const string value);
    int delete_other(const string key);


    Item get_item(int index);
    int add_item(Item &item);
    int delete_item(int index);
    int get_items_size();

    void release();
private:
    int processJson(const string str);
    int processDict(const string str);
    int processJsonQuery(cJSON *item);
    int processJsonMsg(cJSON *item);
    int processJsonOther(cJSON *item);
private:
    //char m_json[1024];
    std::map<string,string> m_other_map;
    std::map<string,string> m_query_map;
    std::map<string,int> m_query_imap;
    std::vector<std::map<string, string> *> m_msg_vector;  
};
#endif
