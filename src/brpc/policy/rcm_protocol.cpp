#include "rcm_protocol.h"

RcmProtocol::RcmProtocol(int type, const string str){
    switch(type){
        case STORAGE_TYPE_JSON:
             processJson(str);
            break;
        case STORAGE_TYPE_PARM:
            processDict(str);
            break;
        case STORAGE_TYPE_STRING:
            break;
        case STORAGE_TYPE_NONE:
            break;
        default:
            break;
    }
}

Item::Item(){
    m_item_map = new std::map<string, string>;
}

Item::Item(std::map<string, string> *m){
    m_item_map = m;
} 

Item::Item(const Item &item){
    m_item_map = item.m_item_map;
}

int Item::set(const string key, const string value){
    m_item_map->insert(pair<string, string>(key, value));
    return 0;
}

int Item::set(const string key, int value){
    m_item_imap->insert(pair<string, int>(key, value));
    return 0;
}

string Item::get(const string key){
    std::map<string, string>::iterator map_ite = m_item_map->find(key);
    if(map_ite != m_item_map->end()){
        return map_ite->second;
    }

    auto it2 = m_item_imap->find(key);
    if(it2 != m_item_imap->end()){
        return to_string(it2->second);
    }
    return "";
}


int Item::del(const string key){
    std::map<string, string>::iterator map_ite = m_item_map->find(key);
    if(map_ite != m_item_map->end()){
        m_item_map->erase(map_ite);
        return 0;
    }
    return -1;
}



RcmProtocol::~RcmProtocol(){
    release();
}

int RcmProtocol::processJsonOther(cJSON *node){
    if(NULL == node)
        return -1;
   
    char *data = cJSON_PrintUnformatted(node); 
    string value = data;
    free(data);
    
    int index_start = value.find('"', 0);
    int index_end = value.find('"', value.length() - 1);

    if( string::npos != index_start  && string::npos != index_end){
        value = value.substr(index_start+1, index_end - index_start - 1);
    }
    m_other_map.insert(pair<string, string>(node->string, value));
    return 0;
}

int RcmProtocol::processJsonQuery(cJSON *item){
    if(NULL == item)
        return -1;
    
    size_t array_size = cJSON_GetArraySize(item);
    for(int i = 0; i < array_size; i++){
        cJSON * node = cJSON_GetArrayItem(item, i);

        char *data = cJSON_PrintUnformatted(node);
        string value = data;
        free(data);

        int index_start = value.find('"', 0);
        int index_end = value.find('"', value.length() - 1);
        if( string::npos != index_start  && string::npos != index_end){
            value = value.substr(index_start+1, index_end - index_start - 1);
        }
        m_query_map.insert(pair<string, string>(node->string, value));
        //printf("key:%s value:%s\n",node->string, value.c_str());
    }
    return 0;
}

int RcmProtocol::processJsonMsg(cJSON *item){
    if(NULL == item)
        return -1;

    size_t list_size = cJSON_GetArraySize(item);
    for(int i = 0; i < list_size; i++){
        cJSON * dic_node = cJSON_GetArrayItem(item, i);
        
        if (NULL != dic_node){
            size_t dic_size = cJSON_GetArraySize(dic_node);
            std::map<string, string> *dic_map = new std::map<string, string>;
            for(int j = 0; j < dic_size; j++){
                cJSON * node = cJSON_GetArrayItem(dic_node, j);
               
                char *data = cJSON_PrintUnformatted(node); 
                string value = data;
                free(data);

                int index_start = value.find('"', 0);
                int index_end = value.find('"', value.length() - 1);
                if( string::npos != index_start  && string::npos != index_end){
                    value = value.substr(index_start+1, index_end - index_start - 1);
                }
                dic_map->insert(pair<string, string>(node->string, value));
                //printf("key:%s value:%s\n",node->string, value.c_str());
            }
            if(0 != dic_map->size()){
                m_msg_vector.push_back(dic_map);
            }
        }
    }
    return 0;
}
//{"user_id": "ur:466605798", "recall_num": "6"}
int RcmProtocol::processJson(const string str){
    //{"query":{},"msg":[]}  
    printf("%s\n", str.c_str());  
    cJSON * json = cJSON_Parse(str.c_str());
    if(NULL == json)
        return -1;    

    size_t array_size = cJSON_GetArraySize(json);
    for(int i = 0; i < array_size; i++){
        cJSON * node = cJSON_GetArrayItem(json, i);        
        if(0 ==  strcmp("msg", node->string)){     
            processJsonMsg(node);    
        }else if(0 == strcmp("query", node->string)){                              
            processJsonQuery(node);
        }else{
            processJsonOther(node);
        }
     }     
    
    if(NULL != json)
       cJSON_Delete(json);    
}

//cmd=rcm_feed&uid=466605798&devid=45430001&num=8&write_history=1
int RcmProtocol::processDict(const string str){

    printf("%s\n", str.c_str());  
    std::string strs = str + "&";
    size_t pos = strs.find("&");
    size_t size = strs.size();
    
    size_t index = 0;
    size_t end = 0;

    while (pos != std::string::npos){
        std::string x = strs.substr(0,pos);
        index = x.find("=");
        if(index != std::string::npos){
           string key = x.substr(0,index);
           string value = x.substr(index+1,x.size());
           m_query_map.insert(pair<string, string>(key, value));
        }

        strs = strs.substr(pos+1,size);
        pos = strs.find("&");
    }
    return 0;
}

int RcmProtocol::get(const string key,string &value){

    std::map<string,string>::iterator item_map = m_query_map.find(key);
    if(item_map != m_query_map.end()){
        value = item_map->second;
    }else{
        return -1;
    }
    return 0;
}

int RcmProtocol::get(const string key,int &value){
    string tmp;
    int ret = get(key,tmp);
    if(0 == ret){
        value = atoi(tmp.c_str());
    }
    return ret;    
}

int RcmProtocol::set(const string key,const string value){
    m_query_map.insert(pair<string, string>(key, value));
    return 0;
}

int RcmProtocol::set(const string key,int value){
    m_query_imap.insert(pair<string, int>(key, value));
    return 0;
}

int RcmProtocol::get_other(const string key,string &value){
    std::map<string,string>::iterator item_map = m_other_map.find(key);
    if(item_map != m_other_map.end()){
        value = item_map->second;
    }else{
        return -1;
    }
    return 0;
}

int RcmProtocol::set_other(const string key,const string value){
    m_other_map.insert(pair<string, string>(key, value));
    return 0;
}

int RcmProtocol::delete_other(const string key){
    std::map<string, string>::iterator map_ite = m_other_map.find(key);
    if(map_ite != m_other_map.end()){
        m_other_map.erase(map_ite);
        return 0;
    }
    return -1;
}



int RcmProtocol::get_items_size(){
    return m_msg_vector.size();
}

Item RcmProtocol::get_item(int index){
    //Item item(m_msg_vector[index]);
    return Item(m_msg_vector[index]);
}

int RcmProtocol::add_item(Item &item){
    m_msg_vector.push_back(item.m_item_map);
    return 0;
}


void RcmProtocol::release(){
    std::vector<std::map<string, string> *>::iterator ite_vec= m_msg_vector.begin();    
    
    for(ite_vec; ite_vec != m_msg_vector.end();){
        /*std::map<string, string>::iterator ite_map = (*ite_vec)->begin();
        for(ite_map; ite_map != (*ite_vec)->end(); ite_map++){
            printf("key:%s value:%s\n",ite_map->first.c_str(),ite_map->second.c_str());
        }*/
        std::map<string, string> *m = *ite_vec;
        ite_vec = m_msg_vector.erase(ite_vec);
        delete m;
        m = NULL;
    }
}

int RcmProtocol::toString(string &str){
    cJSON * root =       cJSON_CreateObject();
    cJSON * queryNode =  cJSON_CreateObject();
    cJSON * msgNode =    cJSON_CreateArray();

    cJSON_AddItemToObject(root, "msg", msgNode);
    cJSON_AddItemToObject(root, "query", queryNode);

    std::map<string, string>::iterator map_ite = m_query_map.begin();
    for(map_ite; map_ite != m_query_map.end(); map_ite++){
        cJSON_AddStringToObject(queryNode, map_ite->first.c_str(), map_ite->second.c_str());
    }   

    auto it2 = m_query_imap.begin();
    for(; it2 != m_query_imap.end(); it2++){
        cJSON_AddNumberToObject(queryNode, it2->first.c_str(), it2->second);
    }   
    
    std::vector<std::map<string, string> *>::iterator vec_ite = m_msg_vector.begin();
    for(vec_ite; vec_ite != m_msg_vector.end(); vec_ite++){
        cJSON *next =  cJSON_CreateObject();
        std::map<string, string>::iterator map_ite = (*vec_ite)->begin();
        for(map_ite; map_ite != (*vec_ite)->end(); map_ite++){
            cJSON_AddStringToObject(next, map_ite->first.c_str(), map_ite->second.c_str());
        } 
        cJSON_AddItemToArray(msgNode, next);
    }

    std::map<string, string>::iterator map_item = m_other_map.begin();
    for(map_item; map_item != m_other_map.end(); map_item++){
        cJSON_AddStringToObject(root, map_item->first.c_str(), map_item->second.c_str());
    }   
   
    char *data = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);    
    str = data;
    free(data);
    return 0;
}
int RcmProtocol::get_id(string &str){
    std::string cmd;
    std::string query_id;
    std::string self_pid;
    std::string front_server_id;

    std::map<string,string>::iterator map_ite;

    map_ite = m_query_map.find("cmd");
    if(map_ite == m_query_map.end()){
        return -1;    
    }else{
      cmd = map_ite->second;
    }
    
    map_ite = m_query_map.find("query_id");
    if(map_ite == m_query_map.end()){
        return -1;    
    }else{
      query_id = map_ite->second;
    }
    
    map_ite = m_query_map.find("self_pid");
    if(map_ite == m_query_map.end()){
        return -1;    
    }else{
       self_pid = map_ite->second;
    }
    
    map_ite = m_query_map.find("front_server_id");
    if(map_ite == m_query_map.end()){
        return -1;    
    }else{
       front_server_id  = map_ite->second;
    }

     
    std::map<string,string>::iterator date_map_ite;
    std::map<string,string>::iterator addr_map_ite;

    date_map_ite  = m_query_map.find("query_date");
    addr_map_ite  = m_query_map.find("query_addr");

    if(date_map_ite == m_query_map.end()){
        if(addr_map_ite == m_query_map.end()){
            str = "rcm_"+cmd+"_"+self_pid+"_"+front_server_id+"_"+query_id; 
        }else{
            str = "rcm_"+cmd+"_"+self_pid+"_"+front_server_id+"_"+query_id + "_" + addr_map_ite->second; 
        }
    }else{
        if(addr_map_ite == m_query_map.end()){
            str = cmd+"_"+ date_map_ite->second + "_" +self_pid+"_"+front_server_id+"_"+query_id; 
        }else{
            str = cmd+"_"+ date_map_ite->second + "_" +self_pid+"_"+front_server_id+"_"+query_id + "_" + addr_map_ite->second; 
        }
        
    }
    return 0;
    str = "rcm_"+cmd+"_"+self_pid+"_"+front_server_id+"_"+query_id;
    return 0;
}
/*
int  main(){
    {
        RcmProtocol rcm(STORAGE_TYPE_JSON,"{\"query\":{\"key1\":\"1\",\"key2\":\"hello2\",\"key3\":3},\"msg\":[{\"key3\":3},{\"key4\":4},{\"key5\":\"5\",\"key6\":6},{\"key7\":7,\"key8\":8}]}");
    }
    RcmProtocol rcm(STORAGE_TYPE_JSON,"{\"query\":{\"key1\":\"1\",\"key2\":\"hello2\",\"key3\":3},\"msg\":[{\"key3\":3},{\"key4\":4},{\"key5\":\"5\",\"key6\":6},{\"key7\":7,\"key8\":8}]}");
    string str_value;
    
    int ret = rcm.get("key1",str_value);
    if(0 == ret)
        printf("key:key1 value:%s\n", str_value.c_str());
    
    int int_value = 0;
    ret = rcm.get("key3", int_value);
    if(0 == ret)
        printf("key:key3 value:%d\n",int_value);
    
    
    RcmProtocol rem(STORAGE_TYPE_PARM, "cmd=rcm_feed&uid=466605798&devid=45430001&num=8&write_history=1");
    ret = rem.get("uid", str_value);
    if(0 == ret)
        printf("uid:%s\n", str_value.c_str());
    
    int_value = 0;
    ret = rem.get("write_history", int_value);
    if(0 == ret)
        printf("write_history:%d\n",int_value);
    
    int_value = 0;
    ret = rem.get("num", int_value);
    if(0 == ret)
        printf("num:%d\n",int_value);
    
    rem.set("name", "liuyongcai");
    ret = rem.get("name", str_value);
    if(0 == ret)
        printf("name:%s\n", str_value.c_str());
    

    RcmProtocol rp;
    rp.set("city","shanghai");
    ret = rp.get("city", str_value);
    if(0 == ret)
        printf("city:%s\n", str_value.c_str());
   
    
     int items_size = rcm.get_items_size();
     printf("items_size:%d\n", items_size);
     Item item;
     rcm.add_item(item);
     items_size = rcm.get_items_size();
     printf("items_size:%d\n", items_size);
     item.set("ago","28");
     item.set("sex","man");
     item.set("name","liuyongcai");
     item.set("city","beijing");

     string str;
     rcm.toString(str);
     printf("%s\n", str.c_str());

     printf("------------------------------\n");
     Item it = rcm.get_item(4);
     it.set("score","123456");
     items_size = rcm.get_items_size();
     rcm.toString(str);
     printf("%s\n", str.c_str());
     
    
     printf("%s\n", it.get("city1").c_str());
     
     it.del("score");
     rcm.toString(str);
     printf("%s\n", str.c_str());
    return 0;
}*/
//g++ -o exe rcm_protocol.cpp cJSON.cpp -fno-elide-constructors
