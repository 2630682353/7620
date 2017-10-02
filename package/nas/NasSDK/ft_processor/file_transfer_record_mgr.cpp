//
//  file_transfer_record_mgr.cpp
//  用于记录文件传输的记录
//  Created by mingweiliu, jiangtaozhu on 15/6/12.
//  Copyright (c) 2015年 mingweiliu, jiangtaozhu. All rights reserved.
//

#include "file_transfer_record_mgr.h"

bool file_transfer_record_mgr::init(const std::string& record_file_path)
{
    bool ret = true;
    
    _record_file_path = record_file_path;
    
    //恢复数据
    recovery();
    
    return ret;
}

bool file_transfer_record_mgr::is_record_existed(unsigned long long cookie)
{
	DU_ThreadLock::Lock lock(*this);

	transfer_record_list_t::iterator it = _transfer_record_list.find(cookie);
	if(it == _transfer_record_list.end()) {
		return false;
	} else {
		return true;
	}

	return true;
}
void file_transfer_record_mgr::set_transfer_begin(unsigned long long cookie, unsigned long long total_bytes, const std::string& file_path, int transfer_type, unsigned long long target_id, const std::string& task_key, const std::string& account)
{
	DU_ThreadLock::Lock lock(*this);
	transfer_record_list_t::iterator it = _transfer_record_list.find(cookie);
	if(it != _transfer_record_list.end()) {
		it->second._transfer_status = transfer_status_processing;
		it->second._total_bytes = total_bytes;
		it->second._file_path = file_path;
		it->second._transfer_begin = time(NULL);
		it->second._transfer_type = transfer_type;
		it->second._target_id = target_id;
		it->second._task_key = task_key;
		it->second._account = account;
		it->second._err_code = 0;
		it->second._last_time = 0;
		return;
	}
	TNOTICE("Cannot find the record, cookie=%llu file_path=%s task_key=%s",cookie,file_path.c_str(),task_key.c_str());
}

void file_transfer_record_mgr::set_transfer_end(unsigned long long cookie, const std::string& file_key)
{
	DU_ThreadLock::Lock lock(*this);

	transfer_record_list_t::iterator it = _transfer_record_list.find(cookie);
	if(it != _transfer_record_list.end()) {
		it->second._transfer_status = transfer_status_finish;
		it->second._file_key = file_key;
		it->second._transfer_end = time(NULL);
		it->second._transfered_bytes = it->second._total_bytes;
		it->second._err_code = 0;
	}
}
void file_transfer_record_mgr::set_transfer_error(unsigned long long cookie, int err)
{
	DU_ThreadLock::Lock lock(*this);

	transfer_record_list_t::iterator it = _transfer_record_list.find(cookie);
	if(it != _transfer_record_list.end()) {
		it->second._transfer_status = transfer_status_error;
		it->second._transfer_end =time(NULL);
		it->second._err_code = err;
	}
}
void file_transfer_record_mgr::set_transfer_cancel(unsigned long long cookie)
{
	DU_ThreadLock::Lock lock(*this);

	transfer_record_list_t::iterator it = _transfer_record_list.find(cookie);
	if(it != _transfer_record_list.end()) {
		it->second._transfer_status = transfer_status_cancel;
		it->second._transfer_end =time(NULL);
		it->second._err_code = 0;
	}
}
void file_transfer_record_mgr::set_transfer_progress(unsigned long long cookie, unsigned long long transfered_bytes)
{
	DU_ThreadLock::Lock lock(*this);

	transfer_record_list_t::iterator it = _transfer_record_list.find(cookie);
	if(it != _transfer_record_list.end()) {
		it->second._transfered_bytes = transfered_bytes;
		it->second._err_code = 0;
	}
}
bool file_transfer_record_mgr::is_transfer_success(unsigned long long cookie)
{
	DU_ThreadLock::Lock lock(*this);

	transfer_record_list_t::iterator it = _transfer_record_list.find(cookie);
	if(it != _transfer_record_list.end()) {
		return it->second._total_bytes <= it->second._transfered_bytes;
	}

	return false;
}

bool file_transfer_record_mgr::is_need_send_notify(unsigned long long cookie, unsigned long long progress)
{
	DU_ThreadLock::Lock lock(*this);

	transfer_record_list_t::iterator it = _transfer_record_list.find(cookie);
	if(it != _transfer_record_list.end()) {
		if( it->second._total_bytes > 10 && progress -  it->second._transfered_bytes >=  it->second._total_bytes / 10) {
			return  true;
		}
	}

	return false;
}

file_transfer_record_t* file_transfer_record_mgr::get_transfer_record(unsigned long long cookie, bool auto_create)
{
    DU_ThreadLock::Lock lock(*this);
    file_transfer_record_t* record = NULL;
    
    transfer_record_list_t::iterator it = _transfer_record_list.find(cookie);
    if(it == _transfer_record_list.end()) {
        if(auto_create) {
            _transfer_record_list.insert(std::make_pair(cookie, file_transfer_record_t()));
            it = _transfer_record_list.find(cookie);
            record = &(it->second);
            record->_cookie = cookie;
        }
    } else {
        record = &(it->second);
    }
    
    return record;
}

bool file_transfer_record_mgr::get_transfer_record_copy(unsigned long long cookie,file_transfer_record_t& record)
{
	DU_ThreadLock::Lock lock(*this);

	transfer_record_list_t::iterator it = _transfer_record_list.find(cookie);
	if(it != _transfer_record_list.end()) {
		record._cookie = it->second._cookie;                           //cookie
		record._err_code = it->second._err_code;                      //保存出错信息
		record._total_bytes = it->second._total_bytes;               //总字节数
		record._transfer_status = it->second._transfer_status;    //传输状态
		record._transfer_type = it->second._transfer_type;         //传输类型
		record._transfered_bytes = it->second._transfered_bytes;//已传输字节数
		record._task_key = it->second._task_key;                          //任务的key
		record._file_path = it->second._file_path;                          //文件路径     
		record._file_key = it->second._file_key;                              //文件key
		record._transfer_begin = it->second._transfer_begin;        //开始时间戳
		record._transfer_end = it->second._transfer_end;              //结束时间戳
		record._target_id = it->second._target_id;                         //对端的targetid
		record._account = it->second._account;                            //用户名
		return true;
	}

	return false;
}



file_transfer_record_t* file_transfer_record_mgr::add_transfer_record(unsigned long long cookie, std::string task_key)
{
	DU_ThreadLock::Lock lock(*this);
	file_transfer_record_t* record = NULL;

	//确定cookie是否已存在，已存在则删除
	transfer_record_list_t::iterator it = _transfer_record_list.find(cookie);
	if(it != _transfer_record_list.end()) { 
		_transfer_record_list.erase(it);
	}

	//查找task_key是否已存在，已存在则删除
	 it= _transfer_record_list.begin();
	for(; it != _transfer_record_list.end(); ++it)
		{
			if(it->second._task_key == task_key)
			{		
				_transfer_record_list.erase(it);
				break;
			}
		}

	//新建record
	_transfer_record_list.insert(std::make_pair(cookie, file_transfer_record_t()));
	it = _transfer_record_list.find(cookie);
	record = &(it->second);
	record->_cookie = cookie;

	return record;
}



bool file_transfer_record_mgr::get_transfer_cookie_by_task_key(std::string task_key, unsigned long long& cookie)
{
    DU_ThreadLock::Lock lock(*this);
    transfer_record_list_t::iterator it = _transfer_record_list.begin();
    for(; it != _transfer_record_list.end(); ++it)
    {
        if(it->second._task_key == task_key)
        {
            cookie = it->first;
            return true;
        }
    }

    return false;
}

#define SECOND_COUNT_IN_ONE_MONTH 2592000 // 30 * 24 * 60 * 60 = 
#define SECOND_COUNT_IN_ONE_THREE_DAY 259200 // 3 * 24 * 60 * 60 = 
void file_transfer_record_mgr::persistence()
{
    DU_ThreadLock::Lock lock(*this);
    std::string file_content;
	time_t now = time(NULL);

    for(transfer_record_list_t::iterator it = _transfer_record_list.begin();
        it != _transfer_record_list.end();) {

			//保存记录的时候，删除三天前的传输记录
			if(
				(now - it->second._transfer_begin) > SECOND_COUNT_IN_ONE_THREE_DAY     ||
				(now - it->second._transfer_end) > SECOND_COUNT_IN_ONE_THREE_DAY
			){
				TNOTICE("persistence transfer record, [ Expired! ] cookie=%llu, task_key=%s, file = %s, key size = %u, status = %d, byte=%llu/%llu, begin = %lu, end = %lu\n",it->second._cookie,it->second._task_key.c_str(), it->second._file_path.c_str(), it->second._file_key.size(), it->second._transfer_status, it->second._transfered_bytes, it->second._total_bytes, it->second._transfer_begin, it->second._transfer_end);
				it = _transfer_record_list.erase(it);
			}
			else{
				file_content.append(it->second.to_buffer());
				TNOTICE("persistence transfer record,  cookie=%llu, task_key=%s, file = %s, key size = %u, status = %d, byte=%llu/%llu, begin = %lu, end = %lu\n",it->second._cookie,it->second._task_key.c_str(),  it->second._file_path.c_str(), it->second._file_key.size(), it->second._transfer_status, it->second._transfered_bytes, it->second._total_bytes, it->second._transfer_begin, it->second._transfer_end);
				++it;
			}
    }
    
    DU_File::save2file(_record_file_path, file_content);
	printf("save transfer records Complete!\n");
}

void file_transfer_record_mgr::recovery()
{
    DU_ThreadLock::Lock lock(*this);
    
    std::string file_content = DU_File::load2str(_record_file_path);
    
    if(file_content.empty()) return; //空的就不做了
    
    const char* p = &file_content[0];
    while (true) {
        int total_size = 0;
        memcpy(&total_size, p, sizeof(int));
        
        //读取全部的内容
        std::string buffer;
        buffer.resize(total_size);
        memcpy(&buffer[0], p, total_size);
        
        //处理数据
        file_transfer_record_t record;
        record.from_buffer(buffer);
        _transfer_record_list.insert(std::make_pair(record._cookie, record));
        printf("recovery transfer record, file = %s, key size = %u, status = %d, errcode = %d, byte=%llu/%llu, begin = %lu, end = %lu\n", record._file_path.c_str(), record._file_key.size(), record._transfer_status,record._err_code, record._transfered_bytes, record._total_bytes, record._transfer_begin, record._transfer_end);
        
        //偏移指针
        p += total_size;
        
        if (p - &file_content[0] >= file_content.size()) {
            printf("recovery file transfer record finish\n");
            break;
        }
    }
}

std::string file_transfer_record_mgr::get_key_from_buffer_with_file(const char* buffer_with_file, int buffer_len)
{
    std::string task_key ;
    
    if(buffer_with_file == NULL || buffer_len == 0) return task_key;
    
    std::string buffer(buffer_with_file, buffer_len);
    
    const char* json = buffer.data();
    
    Document req_d;
    req_d.Parse(json);
    
    if (req_d.HasParseError()) {
        printf("json is not correct. json=%s\n", json);
        return task_key;
    }
    
    if(req_d.HasMember("task_key") && req_d["task_key"].IsString()) {
        //如果有task_key,那么就获取task_key
        task_key = req_d["task_key"].GetString();
    }
    
    return task_key;
}

void file_transfer_record_mgr::parse_buffer_with_file(ftn_file_upload_buffer_with_file& data, const char* buffer_with_file, int buffer_len)
{
    //{"task_key":"123456","dest_path":"/a/b/c"}
    if(buffer_with_file == NULL || buffer_len == 0) return ;
    
    std::string buffer(buffer_with_file, buffer_len);
    
    const char* json = buffer.data();
    
    Document req_d;
    req_d.Parse(json);
    
    if (req_d.HasParseError()) {
        printf("json is not correct. json=%s\n", json);
        return;
    }
    
    if(req_d.HasMember("task_key") && req_d["task_key"].IsString()) {
        //如果有task_key,那么就获取task_key
        data._task_key = req_d["task_key"].GetString();
    }
    
    if(req_d.HasMember("dest_path") && req_d["dest_path"].IsString()) {
        //如果有dest_path,那么就获取dest_path
        data._dest_path = req_d["dest_path"].GetString();
    }
}

void file_transfer_record_mgr::get_all_file_transfer_record(unsigned long long tinyid, const std::string& account, std::vector<file_transfer_record*>& records)
{
    for(transfer_record_list_t::iterator it = _transfer_record_list.begin(); it != _transfer_record_list.end(); ++it) {
        if(it->second._target_id == tinyid
           && it->second._account == account) {
            records.push_back(&(it->second));
        }
    }
}