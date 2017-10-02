//
//  TXNasSDKImpl.h
//
//  Created by jiangtaozhu on 15/8/8.
//  Copyright (c) 2015年 jiangtaozhu. All rights reserved.
//

#ifndef _NAS_SDK_IMPL_H
#define _NAS_SDK_IMPL_H

#include "TXDataPoint.h"
#include <map>
#include "utils.h"


typedef enum tag_enum_task_subid{
	task_subid_file_download = 1001,
	task_subid_transfer_cancel = 1002,
	task_subid_transfer_detail = 1003,
	task_subid_transfer_progress = 1004
}enum_task_subid;


typedef enum tag_enum_account_subid{
	account_subid_login = 1001,
	account_subid_logout = 1002,
	account_subid_check_login = 1003
}enum_account_subid;


//定期刷新
void nas_refresh();

void nas_internal_timer(int code);


typedef struct NasAckDataPointItem{
	unsigned long long target_id;
	tx_data_point datapoint;
}NasAckDataPointItem;

typedef struct tag_SendFileTask{
	std::string task_key;
	unsigned long long target_id;
	std::string file_path;                  //绝对路径
	std::string buff_with_file;
	std::string bussiness_name;
}NasSendFileTask;


/****************************************************************************************************************
  * SendOperationMgr 用于解决几个接口发送频率限制问题
  ****************************************************************************************************************/

class SendOperationMgr : public DU_Singleton<SendOperationMgr>
{
public:
	SendOperationMgr()	{};
	virtual ~SendOperationMgr(){};

	//ReportDataPoint
	void AddReportDataPoint(unsigned long long tinyid, unsigned int propertyid, const std::string& notify,int type);    //进度通知
	void SendReportDataPoints();

	//AckDataPoint
	void AddAckDataPoint(unsigned long long target_id, unsigned int id, unsigned int seq, unsigned int ret_code, std::string value);
	void SendAckDataPoints();

	//SendFileTo
	void AddSendFileTask(std::string task_key, unsigned long long target_id, std::string file_path, std::string buff_with_file,std::string bussiness_name);
	void SendFileTasks();
	bool DeleteSendFileTask(std::string task_key);

private:
	DU_ThreadLock m_lock_report_dp;
	DU_ThreadLock m_lock_ack_dp;
	DU_ThreadLock m_lock_send_file;
	DU_ThreadLock m_lock_upload_file;

	std::vector<tx_data_point*> m_vecReportDpToSend;				//ReportDataPoint 队列，用于进度/完成 通知
	std::vector<NasSendFileTask*> m_vecFileTaskToSend;				//文件队列，用于发送文件给QQ
	std::vector<NasAckDataPointItem> m_vecAckDpToSend;        //AckDataPoint 队列，用于发送响应datapoint给QQ
};

class task_mgr
{
	//c2c in消息的cookie list
	typedef std::map<unsigned long long, unsigned long long> c2c_in_cookie_list_t;

	//c2c out消息的cookie list
	typedef std::map<unsigned long long, unsigned long long > c2c_out_cookie_list_t;
	
public:
	//upload消息的cookie list
	typedef struct upload_cookie_param {
		unsigned long long _from_client;
		unsigned int       _id; //datapoint属性id
		unsigned int        _seq;
		std::string        _abs_path; //文件的实际路径
		std::string		_filekey; //文件的filekey，用于缩略图的上传
		std::string        _relative_path; //文件的相对路径
		int                _transfer_type; //小文件通道orFTN
		bool               _pre_upload; //是否是预先上传
	}upload_cookie_param_t;
	typedef std::map<unsigned long long, upload_cookie_param_t> upload_cookie_list_t;

	//download消息的cookie list
	typedef std::map<unsigned long long, unsigned long long> download_cookie_list_t;

public:

	task_mgr(){};
	virtual ~task_mgr(){};

	//增加一个cookielist
	void add_c2c_in_task(unsigned long long cookie, unsigned long long from_client);
	void add_c2c_out_task(unsigned long long cookie, unsigned long long from_client);
	void add_upload_task(unsigned long long cookie, unsigned long long from_client, unsigned int id, unsigned int seq, const char* relative_path, const char* abs_path, const char* filekey, int transfer_type, bool pre_upload);
	void add_download_task(unsigned long long cookie, unsigned long long from_client);

	//获取一个cookie list的内容
	unsigned long long get_c2c_in_task(unsigned long long cookie);
	unsigned long long get_c2c_out_task(unsigned long long cookie);
	upload_cookie_param_t* get_upload_task(unsigned long long cookie);
	unsigned long long get_download_task(unsigned long long cookie);

	//删除一个cookie list内容
	void rm_c2c_in_task(unsigned long long cookie);
	void rm_c2c_out_task(unsigned long long cookie);
	void rm_upload_task(unsigned long long cookie);
	void rm_download_task(unsigned long long cookie);

private:
	//保存文件传输的cookie和信息的对应关系
	c2c_in_cookie_list_t _c2c_in_list;
	c2c_out_cookie_list_t _c2c_out_list;
	upload_cookie_list_t _upload_list;
	download_cookie_list_t _download_list;
	DU_ThreadLock _list_lock;
};

#endif // 
