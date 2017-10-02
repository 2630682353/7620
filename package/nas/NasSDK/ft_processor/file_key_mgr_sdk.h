//
//  file_key_mgr.h
//
//  Created by jiangtaozhu on 15/8/1.
//  Copyright (c) 2015年 jiangtaozhu. All rights reserved.
//

#ifndef __device__file_key_mgr_sdk__
#define __device__file_key_mgr_sdk__

//#include "callback.h"
#include <map>
#include <string.h>
//#include "device_error.h"
#include "utils.h"


//用于记录上传到文件通道的所有文件的key，可以快速的获取用于下载的url
class file_upload_key_mgr_sdk : public DU_Singleton<file_upload_key_mgr_sdk>
{
public:
    typedef struct key_info
    {
        std::string _url; //文件的key
        time_t _ctime; //增加的时间戳
        std::string _md5;
		std::string _abspath; //文件的绝对路径
        key_info() {};
        key_info(const std::string& url, const std::string& md5, const std::string& abspath) {
            _url = url;
            _ctime = time(NULL);
            _md5 = md5;
			_abspath = abspath;
        }
        key_info(const std::string& url, time_t ctime, const std::string& md5,const std::string& abspath) {
            _url = url;
            _ctime = ctime;
            _md5 = md5;
			_abspath = abspath;
        }
    }key_info_t;
    
    typedef struct mmap_key_info
    {
        //用于将文件结构化到mmap中
        char _file[512];
		char _filekey[512];
        char _url[1024];
        time_t _ctime;
        char _md5[64];
    }mmap_key_info_t;
    
    virtual ~file_upload_key_mgr_sdk(){sync();}
    
    //从mmap中读取已经上传的key信息
    bool init(const std::string& mmap_file_name, unsigned int expire);
    
    //std::map<real_path, key_info_t>
    typedef std::map<std::string, key_info_t> file_upload_key_list_t;
    
    //新增一个文件对应的
    int add_file_upload_key(const std::string& file_key, const std::string& file_real_path, const std::string& url, const std::string& md5);
    
    //获取一个文件对应的key
    std::string get_file_upload_key(const std::string& file_real_path, const std::string& md5);
    
    //删除一个文件对应的key
    void remove_file_upload_key(const std::string& file_key);
    
    //固化到磁盘
    void sync();
    
    //进行清理,清理掉x天前的数据
    void clean(std::vector<string>& expired_files);
    
private:
    file_upload_key_list_t _fuk_list;
    
    std::string _mmap_file_name; //mmap文件的名称
    
    unsigned int _expire; //key超时时长，单位是秒
    
    DU_Mmap _mmap; //用于读取或者写入mmap内容
    
    DU_ThreadLock _lock;
};

#define FILE_UPLOAD_KEY_MGR_SDK file_upload_key_mgr_sdk::getInstance()


#endif /* defined(__device__file_preview_callback__) */
