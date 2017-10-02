//
//  file_key_mgr.cpp
//
//  Created by jiangtaozhu on 15/8/1.
//  Copyright (c) 2015年 jiangtaozhu. All rights reserved.
//

#include "file_key_mgr_sdk.h"
#include <vector>

bool file_upload_key_mgr_sdk::init(const std::string& mmap_file_name, unsigned int expire)
{
    bool ret = false;
    
    _mmap_file_name = mmap_file_name;
    _expire = expire;
    
    //1. 先获取文件长度
    if(!DU_File::isFileExist(mmap_file_name)) {
        TNOTICE("Mmap file is empty, file = %s\n", mmap_file_name.c_str());

        ret = true;
        return ret;
    }
    
    size_t file_len = DU_File::getFileSize(mmap_file_name);
    if(file_len == 0) {
        TNOTICE("Mmap file is empty, file = %s\n", mmap_file_name.c_str());
        ret = true;
        return ret;
    }
    
    try {
        //2. 从mmap文件中读取
        _mmap.mmap(mmap_file_name.c_str(), file_len);
        
        //3. 读取文件内容
        void* start = _mmap.getPointer();
        for(char* p = (char*)start; (p + sizeof(mmap_key_info_t)) <= (char*)start + file_len; p += sizeof(mmap_key_info_t)) {
            //循环的读取文件内容
            mmap_key_info_t* key_info = (mmap_key_info_t*)p;
            _fuk_list[key_info->_filekey] = key_info_t(key_info->_url, key_info->_ctime, key_info->_md5,key_info->_file);
            //TNOTICE("read file upload key cache, filekey = %s, file = %s,  url = %s, ctime = %lu, md5 = %s\n", key_info->_filekey, _fuk_list[key_info->_filekey]._abspath.c_str(), _fuk_list[key_info->_filekey]._url.c_str(), _fuk_list[key_info->_filekey]._ctime, _fuk_list[key_info->_filekey]._md5.c_str());
        }
        
        _mmap.munmap();
        
        TNOTICE("read file upload key success, total = %u\n", _fuk_list.size());
        
        ret = true;
    } catch (DU_Mmap_Exception& e) {
		TERROR("Read mmap failure, error = %s\n", e.what());
    }
    
    return ret;
}

int file_upload_key_mgr_sdk::add_file_upload_key(const std::string& file_key, const std::string& file_real_path, const std::string& url, const std::string& md5)
{
    int ret = -1;
    
    DU_ThreadLock::Lock lock(_lock);
    _fuk_list.insert(std::make_pair(file_key, key_info_t(url, md5,file_real_path)));
    
    return ret;
}

std::string file_upload_key_mgr_sdk::get_file_upload_key(const std::string& file_key, const std::string& md5)
{
    std::string s;
    DU_ThreadLock::Lock lock(_lock);
    if(_fuk_list.find(file_key) != _fuk_list.end()) {
        //检查是否超时
        time_t now = time(NULL);
        if(now - _fuk_list[file_key]._ctime > _expire) {
            TNOTICE("file key info expire, now = %lu, ctime = %lu, expire = %u\n", now, _fuk_list[file_key]._ctime, _expire);
            _fuk_list.erase(file_key);
            
        } else {
            if(md5.empty()) return _fuk_list[file_key]._url;
            if(_fuk_list[file_key]._md5 == md5) { //md5值相同才不传输文件
                s = _fuk_list[file_key]._url;
            } else {
                //不相同，那么就删掉原来的
                _fuk_list.erase(file_key);
            }
        }
    } else {
		//TNOTICE("find file key failure, filekey = %s, total = %lu\n", file_key.c_str(), _fuk_list.size());
    }
    
    return s;
}

void file_upload_key_mgr_sdk::remove_file_upload_key(const std::string& file_key)
{
    DU_ThreadLock::Lock lock(_lock);
    if(_fuk_list.find(file_key) != _fuk_list.end()) {
        _fuk_list.erase(file_key);
    }
}

void file_upload_key_mgr_sdk::sync()
{
	//TNOTICE("file_upload_key_mgr_sdk::sync");
    DU_ThreadLock::Lock lock(_lock);
    //先删除原有的文件
    if(_fuk_list.size() == 0) return;
    
    //写入文件中
    try {
        DU_File::removeFile(_mmap_file_name, false);
        
        //计算总大小
        size_t buf_len = _fuk_list.size() * sizeof(mmap_key_info_t);
        _mmap.mmap(_mmap_file_name.c_str(), buf_len);
        
        void* mp = _mmap.getPointer();
        
        mmap_key_info_t* key_info = (mmap_key_info_t*)mp;
        memset(key_info, 0, sizeof(mmap_key_info_t));
        
        int i = 0;
        for(file_upload_key_list_t::iterator it = _fuk_list.begin();
            it != _fuk_list.end();
            ++it, ++i) {
			strncpy(key_info[i]._filekey, it->first.c_str(), sizeof(key_info[i]._filekey) - 1);
            strncpy(key_info[i]._file, it->second._abspath.c_str(), sizeof(key_info[i]._file) - 1);
            strncpy(key_info[i]._url, it->second._url.c_str(), sizeof(key_info[i]._url) - 1);
            key_info[i]._ctime = it->second._ctime;
            strncpy(key_info[i]._md5, it->second._md5.c_str(), sizeof(key_info[i]._md5) - 1);
            //TNOTICE("sync: filekey=%s url=%s",key_info[i]._filekey,key_info[i]._url);
        }
        _mmap.msync();
        _mmap.munmap();
    } catch (DU_Mmap_Exception& e) {
        TERROR("mmap file failure, error = %s\n", e.what());
    }
}

void file_upload_key_mgr_sdk::clean(std::vector<string>& expired_files)
{
    DU_ThreadLock::Lock lock(_lock);
    //先删除原有的文件
    if(_fuk_list.size() == 0) return;
    
    //遍历整个map，将超过x天的内容清理掉
    time_t now = time(NULL);
    for (file_upload_key_list_t::iterator it = _fuk_list.begin(); it != _fuk_list.end();) {
        if(now - it->second._ctime > _expire) {
			expired_files.push_back(it->first);
            it = _fuk_list.erase(it);
        } else {
            ++it;
        }
    }
}
