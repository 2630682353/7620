//
//  file_transfer_record_mgr.h
//
//  Created by mingweiliu, jiangtaozhu on 15/6/12.
//  Copyright (c) 2015年 mingweiliu, jiangtaozhu. All rights reserved.
//

#ifndef __onespace__file_transfer_record_mgr__
#define __onespace__file_transfer_record_mgr__

#include "utils.h"
//#include "device_api.h"

typedef enum enum_transfer_status {
    transfer_status_not_start = 0, //未开始
    transfer_status_processing = 1, //处理中
    transfer_status_finish = 2, //已完成
	transfer_status_error = 3,  //出错
	transfer_status_cancel = 4, //被取消了
}transfer_status_t;

typedef enum tag_task_error{
	transfer_error_code_success = 0,               //成功
	transfer_error_code_failed = -1,                //失败
	transfer_error_code_not_found = -9998,  //任务未找到
	transfer_error_code_not_login = -9999,    //用户未登录
}task_error_code_t;



//文件传输记录
typedef struct file_transfer_record
{
    unsigned long long _cookie; //cookie
    int _transfer_status; //传输状态
    unsigned long long _transfered_bytes; //已传输字节数
    unsigned long long _total_bytes; //总字节数
    std::string _file_path; //文件路径
    std::string _file_key; //文件key
    time_t _transfer_begin; //开始时间戳
    time_t _transfer_end; //结束时间戳
    int _transfer_type; //传输类型
    std::string _task_key; //任务的key
    unsigned long long _target_id; //对端的targetid
    std::string _account; //用户名
	int _err_code;           //保存出错信息

	time_t _last_time;   //上次发送进度的时间，用于两秒一个进度
    
    file_transfer_record() {
        _cookie = 0;
        _transfer_status = transfer_status_not_start;
        _transfered_bytes = 0;
        _total_bytes = 0;
        _transfer_begin = 0;
        _transfer_end = 0;
        _target_id = 0;
		_err_code = 0;
		_last_time = 0;
    }
    
    void set_transfer_begin(unsigned long long total_bytes, const std::string& file_path, int transfer_type, unsigned long long target_id, const std::string& task_key, const std::string& account) {
        _transfer_status = transfer_status_processing;
        _total_bytes = total_bytes;
        _file_path = file_path;
        _transfer_begin = time(NULL);
        _transfer_type = transfer_type;
        _target_id = target_id;
        _task_key = task_key;
        _account = account;
		_err_code = 0;
    }
    
    void set_transfer_end(const std::string& file_key) {
        _transfer_status = transfer_status_finish;
        _file_key = file_key;
        _transfer_end = time(NULL);
        _transfered_bytes = _total_bytes;
		_err_code = 0;
    }

	void set_transfer_error(int err){
		_transfer_status = transfer_status_error;
		_transfer_end =time(NULL);
		_err_code = err;
	}

	void set_transfer_cancel(){
		_transfer_status = transfer_status_cancel;
		_transfer_end =time(NULL);
		_err_code = 0;
	}
    
    void set_transfer_progress(unsigned long long transfered_bytes) {
        _transfered_bytes = transfered_bytes;
		_err_code = 0;
    }
    
    bool is_transfer_success() {
        return _total_bytes <= _transfered_bytes;
    }
    
    bool is_need_send_notify(unsigned long long progress) {
        bool ret = false;
		time_t now = time(NULL);

		if((now-_last_time) > 3) {    
			_last_time = now;
			ret = true;
		}

//         if(_total_bytes > 1048576 && progress - _transfered_bytes >= _total_bytes / 10) {     // 1M: 1024*1024 = 1048576
//             ret = true;
//         }

        return ret;
    }
    
    std::string to_buffer() {
        //将数据转换成buffer
        std::string buffer;
        
        char tmp_buf[4096] = {0};
        int total_size = 0;
        char* p = tmp_buf + sizeof(int);
        
        memcpy(p, &_cookie, sizeof(_cookie));
        p += sizeof(_cookie);
        
        memcpy(p, &_transfer_status, sizeof(_transfer_status));
        p += sizeof(_transfer_status);
        
        memcpy(p, &_transfered_bytes, sizeof(_transfered_bytes));
        p += sizeof(_transfered_bytes);
        
        memcpy(p, &_total_bytes, sizeof(_total_bytes));
        p += sizeof(_total_bytes);

        memcpy(p, &_transfer_begin, sizeof(time_t));
        p += sizeof(time_t);
        
        memcpy(p, &_transfer_end, sizeof(time_t));
        p += sizeof(time_t);
        
        memcpy(p, &_transfer_type, sizeof(_transfer_type));
        p += sizeof(_transfer_type);
        
        memcpy(p, &_target_id, sizeof(_target_id));
        p += sizeof(_target_id);

		memcpy(p, &_err_code, sizeof(_err_code));
		p += sizeof(_err_code);
        
        //file path
        int path_size = _file_path.length();
        memcpy(p, &path_size, sizeof(path_size));
        p += sizeof(path_size);
        if(path_size > 0)
        {
            memcpy(p, _file_path.c_str(), path_size);
            p += path_size;
        }        
        
        //file key
        int key_size = _file_key.length();
        memcpy(p, &key_size, sizeof(key_size));
        p += sizeof(key_size);

        if(key_size > 0)
        {
            memcpy(p, _file_key.c_str(), key_size);
            p += key_size;
        }            
        
        //account
        int account_size = _account.length();
        memcpy(p, &account_size, sizeof(account_size));
        p += sizeof(account_size);
        if(account_size > 0)
        {
            memcpy(p, _account.c_str(), account_size);
            p += account_size;
        }  


        
        //拷贝长度
        total_size = p - tmp_buf;
        memcpy(tmp_buf, &total_size, sizeof(total_size));
        
        buffer.assign(tmp_buf, total_size);
        return buffer;
    }
    
    void from_buffer(const std::string& buffer) {
        const char* p = buffer.c_str();
        
        p += sizeof(int); //跳过前四个字节
        
        memcpy(&_cookie, p, sizeof(_cookie));
        p += sizeof(_cookie);
        
        memcpy(&_transfer_status, p, sizeof(_transfer_status));
        p += sizeof(_transfer_status);
        
        memcpy(&_transfered_bytes, p, sizeof(_transfered_bytes));
        p += sizeof(_transfered_bytes);
        
        memcpy(&_total_bytes, p, sizeof(_total_bytes));
        p += sizeof(_total_bytes);

        memcpy(&_transfer_begin, p, sizeof(time_t));
        p += sizeof(time_t);
        
        memcpy(&_transfer_end, p, sizeof(time_t));
        p += sizeof(time_t);
        
        memcpy(&_transfer_type, p, sizeof(_transfer_type));
        p += sizeof(_transfer_type);
        
        memcpy(&_target_id, p, sizeof(_target_id));
        p += sizeof(_target_id);

		memcpy(&_err_code, p, sizeof(_err_code));
		p += sizeof(_err_code);
        
        int path_size = 0;
        memcpy(&path_size, p, sizeof(path_size));
        p += sizeof(path_size);
        if(path_size > 0) {
            _file_path.assign(p, path_size);
            p += path_size;
        }
        
        int key_size = 0;
        memcpy(&key_size, p, sizeof(key_size));
        p += sizeof(key_size);
        if(key_size > 0) {
            _file_key.assign(p, key_size);
            p += key_size;
        }     
        
        int account_size = 0;
        memcpy(&account_size, p, sizeof(account_size));
        p += sizeof(account_size);
        if(account_size > 0) {            
            _account.assign(p, account_size);
            p += account_size;
        }
    }
}file_transfer_record_t;


//传输记录管理器
class file_transfer_record_mgr : public DU_Singleton<file_transfer_record_mgr>, public DU_ThreadLock
{
public:
    typedef std::map<unsigned long long, file_transfer_record_t> transfer_record_list_t; //文件传输记录列表,主键可以是key或者path
    
    //初始化
    bool init(const std::string& record_file_path);
    
    //析构
    virtual ~file_transfer_record_mgr(){persistence();}

	//判断一个cookie是否存在
	bool is_record_existed(unsigned long long cookie);

	//获取一个记录的副本，存在返回true，不存在返回false
	bool get_transfer_record_copy(unsigned long long cookie,file_transfer_record_t& record);

	//设置record 状态
	void set_transfer_begin(unsigned long long cookie, unsigned long long total_bytes, const std::string& file_path, int transfer_type, unsigned long long target_id, const std::string& task_key, const std::string& account);
    void set_transfer_end(unsigned long long cookie, const std::string& file_key);
	void set_transfer_error(unsigned long long cookie, int err);
	void set_transfer_cancel(unsigned long long cookie);
	void set_transfer_progress(unsigned long long cookie, unsigned long long transfered_bytes);
	bool is_transfer_success(unsigned long long cookie);
	bool is_need_send_notify(unsigned long long cookie, unsigned long long progress);

    //获取一个记录,如果设置了auto_create记录就自动新增
	file_transfer_record_t* get_transfer_record(unsigned long long cookie, bool auto_create = false);
		
	//添加一条record
    file_transfer_record_t* add_transfer_record(unsigned long long cookie, std::string task_key);

    bool get_transfer_cookie_by_task_key(std::string task_key,unsigned long long& cookie);
    
    //将记录写到磁盘上
    void persistence();
    
    //从磁盘上恢复数据
    void recovery();
    
    //解析buffer with file
    std::string get_key_from_buffer_with_file(const char* buffer_with_file, int buffer_len);
    
    //解析buffer with file
    void parse_buffer_with_file(ftn_file_upload_buffer_with_file& data, const char* buffer_with_file, int buffer_len);
    
    //获取所有的记录
    void get_all_file_transfer_record(unsigned long long tinyid, const std::string& account, std::vector<file_transfer_record*>& records);
    
private:
    
    transfer_record_list_t _transfer_record_list; //传输记录列表
    std::string _record_file_path; //文件传输记录的文件名称
    
    DU_Mmap _mmap;
};

#endif /* defined(__onespace__file_transfer_record_mgr__) */
