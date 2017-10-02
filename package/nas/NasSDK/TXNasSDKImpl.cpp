//
//  TXNasSDKImpl.cpp
//
//  Created by jiangtaozhu on 15/8/8.
//  Copyright (c) 2015年 jiangtaozhu. All rights reserved.
//

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "TXNasSDK.h"
#include "TXNasSDKImpl.h"
#include "TXSDKCommonDef.h"
#include "TXDeviceSDK.h"
#include "public_data_point_processor.h"
#include "file_transfer_processor.h"
#include "file_key_mgr_sdk.h"
#include "file_transfer_record_mgr.h"


#include <vector>
#include <string>

#define DEVICE_STATUS_ONLINE 11
#define DEVICE_STATUS_OFFLINE 21

#define  NOTIFY_TYPE_PROGRESS 1
#define  NOTIFY_TYPE_COMPLETE 2

#define BUSINESS_NAME_NAS_FILE_MUSIC "8000-NASDevMusicFile" // 手Q向设备发音乐      （音乐文件）
#define BUSINESS_NAME_NAS_FILE_VIDEO "8001-NASDevVideoFile" // 手Q向设备发视频(较大的文件)
#define BUSINESS_NAME_NAS_FILE_DOC "8002-NASDevDocumentFile" // 手Q向设备发文档
#define BUSINESS_NAME_NAS_FILE_COMMON "8003-NASDevCommonFile" // 手Q向设备发其他文件



tx_device_info*                  g_device_info;
tx_device_notify*               g_device_notify;
tx_init_path*                      g_init_path;
tx_file_transfer_notify*       g_file_transfer_nofity;
tx_data_point_notify*         g_data_point_notify;
tx_nas_device_mgr_wrapper*    g_device_mgr_wrapper;



task_mgr g_task_mgr;
bool g_bExitNasSDK = false;
DU_ThreadPool g_pool_sdk;
binder_info_t g_binder_list_internal; //已绑定的用户列表


void notify_file_transfer_progress(unsigned long long cookie, unsigned long long progress, unsigned long long max_progress);
void notify_file_transfer_complete(unsigned long long cookie, const std::string& file_key, int errcode, int status);

void on_send_data_point_callback_internal(unsigned int cookie, unsigned long long from_client, int err_code)
{
	TNOTICE("send_data_point_callback cookie[%u] from_client [%llu] err_code[%d]",cookie,from_client, err_code);
}

void on_report_data_point_callback_internal(unsigned int cookie, int err_code)
{
	TNOTICE("on_report_data_point_callback_internal cookie[%u] err_code[%d]",cookie, err_code);
}

void SendOperationMgr::AddReportDataPoint(unsigned long long tinyid, unsigned int propertyid, const std::string& notify,int type)
{
	DU_ThreadLock::Lock lock(m_lock_report_dp);

	tx_data_point* dp = new tx_data_point;
	dp->id = propertyid;

	int len = notify.length();
	dp->value = new char[len + 1];
	memset( dp->value,0,len+1);
	memcpy(dp->value, notify.c_str(),len);

	dp->seq = propertyid;
	dp->ret_code = 0;

	m_vecReportDpToSend.push_back(dp);
}

#define MAX_REPORT_DATA_POINT_SIZE_IN_BYTE 500

//一次性发送多个，且确保不会超过一个datapoint数据包的大小
void SendOperationMgr::SendReportDataPoints()
{
	DU_ThreadLock::Lock lock(m_lock_report_dp);
	int cnt = m_vecReportDpToSend.size();
	if(cnt == 0)	return;

	////////////////////////////////////////////////////////////////////////////
	//合成一个大的datapoint （且确保不会超过 一个datapoint数据包的大小）

	int i = 0;
	std::string strValueToSend("{\"subid\":1004,\"tasks\":[");	
	for (i=0; i<cnt; ++i)
	{		
		if(m_vecReportDpToSend[i]->value){

			std::string strTmp = strValueToSend + m_vecReportDpToSend[i]->value;
			if(strTmp.length() > MAX_REPORT_DATA_POINT_SIZE_IN_BYTE){
				break;
			}else{
				TNOTICE("Report Datapoint: %d/%d -> id[%d] value[%s] seq[%d]", i+1, cnt, m_vecReportDpToSend[i]->id,m_vecReportDpToSend[i]->value,m_vecReportDpToSend[i]->seq);
				if(i !=0 )	strValueToSend.append(",");
				strValueToSend.append(m_vecReportDpToSend[i]->value);
				delete[] m_vecReportDpToSend[i]->value;
				m_vecReportDpToSend[i]->value = NULL;
			}		
		}

		delete m_vecReportDpToSend[i];
		m_vecReportDpToSend[i] = NULL;
	}
	strValueToSend.append("]}");

	m_vecReportDpToSend.erase(m_vecReportDpToSend.begin(),m_vecReportDpToSend.begin() + i );

	TNOTICE("BigDP value: %s",strValueToSend.c_str());

	tx_data_point bigDP;
	bigDP.id				= 90002;
	bigDP.ret_code     = 0;
	bigDP.seq             = 90002;
	bigDP.value          = (char*) strValueToSend.c_str();

	////////////////////////////////////////////////////////////////////////////
	//发送datapoint

	unsigned int cookie = 0;
	tx_report_data_points(&bigDP,1,&cookie,on_report_data_point_callback_internal);
	TNOTICE("Report Finished: cookie[%u] count[%d]",cookie,i);

}

void SendOperationMgr::AddSendFileTask(std::string task_key, unsigned long long target_id, std::string file_path, std::string buff_with_file,std::string bussiness_name)
{
	DU_ThreadLock::Lock lock(m_lock_send_file);

	NasSendFileTask* task = new NasSendFileTask;
	task->task_key = task_key;
	task->target_id  = target_id;
	task->file_path  = file_path;
	task->buff_with_file = buff_with_file;
	task->bussiness_name = bussiness_name;

	m_vecFileTaskToSend.push_back(task);
}
void SendOperationMgr::SendFileTasks()
{
	DU_ThreadLock::Lock lock(m_lock_send_file);
	if(m_vecFileTaskToSend.empty()) return;

	unsigned long long cookie = 0;
	int dl_ret = tx_send_file_to(m_vecFileTaskToSend[0]->target_id, (char*)m_vecFileTaskToSend[0]->file_path.c_str(),&cookie,(char*)m_vecFileTaskToSend[0]->buff_with_file.c_str(), (int)m_vecFileTaskToSend[0]->buff_with_file.length(),  (char*)m_vecFileTaskToSend[0]->bussiness_name.c_str());	
	TNOTICE("Send File To:  target = %llu, file = %s, business name = %s, dl_ret = %d, cookie = %llu\n", m_vecFileTaskToSend[0]->target_id,m_vecFileTaskToSend[0]->file_path.c_str(), m_vecFileTaskToSend[0]->bussiness_name.c_str(), dl_ret, cookie);
	file_transfer_record_mgr::getInstance()->add_transfer_record(cookie,m_vecFileTaskToSend[0]->task_key);
	file_transfer_record_mgr::getInstance()->set_transfer_begin(cookie,DU_File::getFileSize(m_vecFileTaskToSend[0]->file_path),m_vecFileTaskToSend[0]->file_path,transfer_type_c2c_out,m_vecFileTaskToSend[0]->target_id,m_vecFileTaskToSend[0]->task_key,std::string("nassdk"));
	
	//删除数据，并从队列中移除
	delete m_vecFileTaskToSend[0];
	m_vecFileTaskToSend[0] = NULL;
	m_vecFileTaskToSend.erase(m_vecFileTaskToSend.begin());
}

bool SendOperationMgr::DeleteSendFileTask(std::string task_key)
{
	DU_ThreadLock::Lock lock(m_lock_send_file);

	int cnt = m_vecFileTaskToSend.size();
	if(cnt == 0) return false;

	for (int i=0;i<cnt;++i){
		if(m_vecFileTaskToSend[i]->task_key == task_key){
			delete m_vecFileTaskToSend[i];
			m_vecFileTaskToSend[i] = NULL;
			m_vecFileTaskToSend.erase(m_vecFileTaskToSend.begin()+i);
			TNOTICE("DeleteSendFileTask task_key=%s",task_key.c_str());
			return true;
		}
	}
	return false;
}

void SendOperationMgr::AddAckDataPoint(unsigned long long target_id, unsigned int id, unsigned int seq, unsigned int ret_code, std::string value)
{
	DU_ThreadLock::Lock lock(m_lock_ack_dp);

	NasAckDataPointItem item;
	item.target_id					= target_id;
	item.datapoint.id				= id;
	item.datapoint.seq			= seq;
	item.datapoint.ret_code	= ret_code;

	if(value.empty()){
		item.datapoint.value = NULL;
	}else{
		int len = value.length();
		item.datapoint.value = new char[len + 1];
		memset( item.datapoint.value,0,len+1);
		memcpy(item.datapoint.value, value.c_str(),len);
	}

	m_vecAckDpToSend.push_back(item);

}
void SendOperationMgr::SendAckDataPoints()
{
	DU_ThreadLock::Lock lock(m_lock_ack_dp);
	int cnt = m_vecAckDpToSend.size();
	if(cnt == 0) return;

	unsigned int cookie = 0;
	tx_ack_data_points(m_vecAckDpToSend[0].target_id,&(m_vecAckDpToSend[0].datapoint),1,&cookie,on_send_data_point_callback_internal);
	TNOTICE("SendDataPoint: cookie[%u] target[%llu] id[%d] value[%s] seq[%d]", cookie, m_vecAckDpToSend[0].target_id, m_vecAckDpToSend[0].datapoint.id, m_vecAckDpToSend[0].datapoint.value, m_vecAckDpToSend[0].datapoint.seq);

	//释放内存
	if(m_vecAckDpToSend[0].datapoint.value){
		delete[] m_vecAckDpToSend[0].datapoint.value;
		m_vecAckDpToSend[0].datapoint.value = NULL;
	}
	m_vecAckDpToSend.erase(m_vecAckDpToSend.begin());
}


bool init_file_upload_key_mgr_sdk()
{
	std::string file_upload_key_path("./.file_upload_key_cache_sdk");
    return FILE_UPLOAD_KEY_MGR_SDK->init(file_upload_key_path.c_str(), 86400*6);
}


bool init_file_transfer_record_mgr_sdk()
{
	std::string file_transfer_record("./.file_transfer_record_sdk");
    return file_transfer_record_mgr::getInstance()->init(file_transfer_record);
}


tx_file_transfer_info* dup_file_transfer_info(const tx_file_transfer_info* transfer_info)
{
	if(transfer_info == NULL) return NULL;

	tx_file_transfer_info* dup_info = new tx_file_transfer_info;

	memcpy(dup_info, transfer_info, sizeof(tx_file_transfer_info));
	return dup_info;
}

void free_file_transfer_info(const tx_file_transfer_info* transfer_info)
{
	if(transfer_info != NULL) {
		delete transfer_info;
	}
}

//对datapoint进行拷贝
tx_data_point* dup_data_point(const tx_data_point* data_points, int dp_count)
{
	if(dp_count <= 0 || data_points == NULL) return  NULL;
	tx_data_point* request = new tx_data_point[dp_count];
	memset(request,0,sizeof(tx_data_point)*dp_count);

	for(int i = 0; i < dp_count; ++i) {

		request[i].id				= data_points[i].id;
		request[i].seq			= data_points[i].seq;
		request[i].ret_code	= data_points[i].ret_code;

		//value
		int val_len = strlen(data_points[i].value);
		request[i].value = new char[val_len+1];
		memset(request[i].value,0,val_len+1);
		strncpy(request[i].value,data_points[i].value,val_len); 
	}

	return request;
}

void free_data_point(tx_data_point* data_points, int dp_count)
{
	if(data_points == NULL) return;

	for(int i = 0; i < dp_count; ++i) {

		if(&data_points[i] == NULL) continue;
		//value
		if(data_points[i].value != NULL) {delete [] (data_points[i].value); data_points[i].value = NULL; }      
	}

	//删除整个数组
	delete [] data_points;
	data_points = NULL;
}



/**
 * 处理公共datapoint
 */
void process_internal_data_point(unsigned long long from_client, tx_data_point * data_points, int data_points_count)
{
	if(data_points == NULL) return;

	for (int i = 0; i < data_points_count; ++i) {
		if(&data_points[i] == NULL) continue;

		//处理datapoint
		tx_data_point rsp_dp;
		public_data_point_processor::getInstance()->on_process(from_client,&data_points[i],&rsp_dp);
		if(rsp_dp.value != NULL){

			TNOTICE("ACK Internal DataPoint: id[%d] value[%s] seq[%d]",rsp_dp.id,rsp_dp.value,rsp_dp.seq);
			SendOperationMgr::getInstance()->AddAckDataPoint(from_client,rsp_dp.id, rsp_dp.seq,rsp_dp.ret_code,std::string(rsp_dp.value));
			delete[] rsp_dp.value;
			rsp_dp.value = NULL;

		}
	}
	
	//处理完后，释放内存
	for (int k = 0; k< data_points_count; ++k) {
		if(&data_points[k] == NULL) continue;
		if(data_points[k].value != NULL) {
			delete [] (data_points[k].value); 
			data_points[k].value = NULL; 
		} 
	}
	delete [] data_points;
	data_points = NULL;
}


/**
 * 收到data_point数据回调
 */
void on_receive_data_point_internal(unsigned long long from_client, tx_data_point * data_points, int data_points_count)
{
	TNOTICE("on_receive_data_point_internal: from_client=%lld, data_points_count=%d", from_client, data_points_count);

	//内部处理掉公共dp
	if(data_points == NULL) return;
	for(int i=0; i<data_points_count; ++i){
		if(&data_points[i] == NULL) continue;

		//如果是公共datapoint，内部处理掉
		if(    data_points[i].id == 90001     //局域网支持
			|| data_points[i].id == 90002     //任务管理
			|| data_points[i].id == 90004     //账号登陆
		)
		{	    
			DU_Functor<void, TL::TLMaker<unsigned long long , tx_data_point * , int >::Result> tf(process_internal_data_point);
			DU_FunctorWrapper<
				DU_Functor<void, TL::TLMaker<unsigned long long , tx_data_point * , int>::Result> > tfw(tf, from_client,  dup_data_point(&data_points[i], 1), 1);
			g_pool_sdk.exec(tfw);
		}

		//不是公共datapoint，调用用户的回调
		else{
			if(g_data_point_notify && g_data_point_notify->on_receive_data_point){
				g_data_point_notify->on_receive_data_point(from_client,&data_points[i],1);
			}
		}	
	}   
}


/**
 *   处理文件传输
 */
void process_internal_file_transfer(unsigned long long cookie, unsigned long long tinyid, int err_code, tx_file_transfer_info* ft_info)
{
	// 这个 transfer_cookie 是SDK内部执行文件传输（接收或发送） 任务 保存的一个标记，在回调完这个函数后，transfer_cookie 将失效
	// 可以根据 transfer_cookie 查询文件的信息
	if (err_code == 0 && (ft_info != NULL))
	{
		file_transfer_processor::getInstance()->on_process(ft_info,cookie);
	}
	else
	{
		if(ft_info)
			TERROR("file transfer_complete error=%d, transfer_type [%d], bussiness_name [%s], file path [%s]", err_code, ft_info->transfer_type, ft_info->bussiness_name, ft_info->file_path);
		else
			TERROR("file transfer_complete error=%d, ft_info is null", err_code);
	}

	//执行完成了，删除transferinfo
	free_file_transfer_info(ft_info);
}

/**
 * 收到c2c 文件回调
 */
void on_file_in_come_internal(unsigned long long transfer_cookie, const tx_ccmsg_inst_info * inst_info,const tx_file_transfer_info * tran_info)
{
	if(inst_info == NULL){
		TERROR("on file in come, transfer_cookie=%lld, inst_info is null \n", transfer_cookie);
		return;
	}

	TNOTICE("on file in come, transfer_cookie=%lld, target_id [%lld], productid [%d]\n", transfer_cookie, inst_info->target_id, inst_info->productid);

	//将发送者的信息保存起来
	g_task_mgr.add_c2c_in_task(transfer_cookie,inst_info->target_id);
	
	if(tran_info != NULL){
		if( tran_info->file_path == NULL ||
			(strlen(tran_info->file_path) == 0 && tran_info->file_size == 0)
		) {                                                  //NFC 模式下，没有tran_info信息
			TNOTICE("Query ftn transfer info failure, maybe it's nfc, cookie ＝ %llu\n", transfer_cookie);
		} else {
			//FTN传输，获取bufferwithfile
			ftn_file_upload_buffer_with_file data;
			data._task_key = tran_info->file_path;
			data._dest_path = "/from_mobile_qq_so_dest_path_not_defined/";    //手Q上传的，没有指定目录
			file_transfer_record_mgr::getInstance()->parse_buffer_with_file(data, tran_info->buff_with_file, tran_info->buff_length);
			
			if(!data._task_key.empty()) {
				file_transfer_record_mgr::getInstance()->add_transfer_record(transfer_cookie, data._task_key); //新建
				file_transfer_record_mgr::getInstance()->set_transfer_begin(transfer_cookie,tran_info->file_size, data._dest_path, tran_info->transfer_type, inst_info->target_id, data._task_key, std::string("nassdk"));
				TNOTICE("file in come task, task key = %s, cookie = %llu, file = %s, size = %llu, dest = %s, account = %s\n", data._task_key.c_str(), transfer_cookie, tran_info->file_path, tran_info->file_size, data._dest_path.c_str(), "nassdk");
			}
		}
	}else{
		TNOTICE("tran_info is null");
	}

	//调用用户的文件回调
	if(g_file_transfer_nofity && g_file_transfer_nofity->on_file_in_come)
		g_file_transfer_nofity->on_file_in_come(transfer_cookie,inst_info,tran_info);

}

/**
 * 文件传输进度回调
 */
void on_transfer_progress_internal(unsigned long long transfer_cookie, unsigned long long transfer_progress, unsigned long long max_transfer_progress)
{
	TNOTICE("ontransfer progess, transfer_cookie=%lld, progress=%lld/%lld", transfer_cookie, transfer_progress, max_transfer_progress);

	//通知进度
	notify_file_transfer_progress(transfer_cookie, transfer_progress, max_transfer_progress);

	//调用用户的回调
	if(g_file_transfer_nofity && g_file_transfer_nofity->on_transfer_progress)
		g_file_transfer_nofity->on_transfer_progress(transfer_cookie,transfer_progress,max_transfer_progress);
}

/**
 * 文件传输完成后的结果通知
 */
void on_transfer_complete_internal(unsigned long long transfer_cookie, int err_code, tx_file_transfer_info* trans_info)
{
	TNOTICE("on transfer complete, transfer_cookie=%lld error_code=%d", transfer_cookie,err_code);

	//通知文件传输完成 (包括成功和失败)

	if(trans_info == NULL || err_code != 0)	{    //失败
		TERROR("on transfer complete, trans_info is null or err_code != 0");
		notify_file_transfer_complete(transfer_cookie, std::string(""), err_code,transfer_status_finish); //直接失败了
		return;
	}else	{                                                         //文件传输完成了，但是不一定后续处理也成功，所以算还在进行中                                               
		//notify_file_transfer_complete(transfer_cookie, std::string(trans_info->file_key, trans_info->key_length), err_code,transfer_status_processing); // 1 表示进行中，任务还未完成
	}

	tx_file_transfer_info* info = dup_file_transfer_info(trans_info);  


	DU_Functor<void, TL::TLMaker<unsigned long long, unsigned long long, int, tx_file_transfer_info*>::Result> tf(process_internal_file_transfer);
	DU_FunctorWrapper<
		DU_Functor<void, TL::TLMaker<unsigned long long, unsigned long long, int, tx_file_transfer_info*>::Result> > tfw(tf, transfer_cookie, transfer_cookie, err_code, info);
	g_pool_sdk.exec(tfw);


	//调用用户的回调
	if(g_file_transfer_nofity && g_file_transfer_nofity->on_transfer_complete)
		g_file_transfer_nofity->on_transfer_complete(transfer_cookie,err_code,trans_info);
}

void on_get_binder_list_result_callback_internal(tx_binder_info * pBinderList, int nCount)
{
	if(pBinderList == NULL){
		TERROR("on_get_binder_list_result_callback_internal: pBinderList is null");
		return;
	}

	for(int i = 0;i < nCount; ++i) {
		TNOTICE("Get binder: [%d], id = %lld, type = %d, uin = %lld", i, pBinderList[i].tinyid, pBinderList[i].type, pBinderList[i].uin);
	}

	g_binder_list_internal.dup(pBinderList, nCount); //深拷贝
}


/**
 * 登录完成的通知，errcode为0表示登录成功，其余请参考全局的错误码表
 */
void on_login_complete_internal(int errcode) 
{
	//如果用户注册了回调，则调用用户的回调函数
	if(g_device_notify && g_device_notify->on_login_complete)
	{
		g_device_notify->on_login_complete(errcode);
	}

	//如果用户没有注册，则使用本SDK提供的
	else
	{
		if(errcode != 0) {
			TERROR("Login failure, error = %d\n", errcode);
		} else {
			TNOTICE("Login success, error = %d\n", errcode);
		}
	}
}

/**
 * 在线状态变化通知， 状态（status）取值为 11 表示 在线， 取值为 21 表示  离线
 * old是前一个状态，new是变化后的状态（当前）
 */
void on_online_status_internal(int old_status, int new_status) 
{
	//设备在线时，初始化文件传输回调和datapoint回调
	if(new_status == DEVICE_STATUS_ONLINE)
	{
		TNOTICE("device online, new status = %d, old status = %d, din = %llu\n", new_status, old_status, tx_get_self_din());
		int count = 0;
		tx_get_binder_list(NULL, &count, on_get_binder_list_result_callback_internal);
	}

	else if(new_status == DEVICE_STATUS_OFFLINE)
	{
		TNOTICE("device offline, new status = %d, old status = %d, din = %llu\n", new_status, old_status, tx_get_self_din());
	}


	//如果用户注册了回调，则调用用户的回调函数
	if(g_device_notify && g_device_notify->on_online_status)
	{
		g_device_notify->on_online_status(old_status,new_status);
	}
}


// 绑定列表变化通知
void on_binder_list_change_internal(int error_code, tx_binder_info * pBinderList, int nCount)
{
	int i = 0;
	for (i = 0; i < nCount; ++i )
	{
		printf(">>   binder uin[%llu], nick_name[%s]\n", pBinderList[i].uin, pBinderList[i].nick_name);
	}

	g_binder_list_internal.free();
	if(pBinderList && nCount > 0){
		g_binder_list_internal.dup(pBinderList, nCount); //深拷贝
	}
	
	//如果用户注册了回调，则调用用户的回调函数
	if(g_device_notify && g_device_notify->on_binder_list_change)
	{
		g_device_notify->on_binder_list_change(error_code,pBinderList,nCount);
	}

}


//数据存盘
void tx_nas_sync()
{
	//保存transfer_record
	file_transfer_record_mgr::getInstance()->persistence();

	//缩略图存盘
	FILE_UPLOAD_KEY_MGR_SDK->sync();
}



int tx_nas_init_device(tx_device_info *info, tx_device_notify *notify, tx_init_path* init_path)
{
	///////////////////////////////////////////////////////////////////////////////////
	//保存参数

	//device_info
	if(info == NULL){
		TERROR("device_info cannot be null"); 
		return err_invalid_param;
	}
	g_device_info = (tx_device_info*)malloc(sizeof(tx_device_info));
	if(!g_device_info) return err_mem_alloc;
	memcpy(g_device_info,info,sizeof(tx_device_info));

	//device_notify
	if(notify){
		g_device_notify = (tx_device_notify*)malloc(sizeof(tx_device_notify));
		if(!g_device_notify) return err_mem_alloc;
		memcpy(g_device_notify,notify,sizeof(tx_device_notify));
	}else{
		g_device_notify = NULL;
	}

	//init_path
	if(init_path == NULL){
		TERROR("init_path cannot be null"); return err_invalid_param;
	}
	g_init_path = (tx_init_path*)malloc(sizeof(tx_init_path));
	if(!g_init_path) return err_mem_alloc;
	memcpy(g_init_path,init_path,sizeof(tx_init_path));


	//tx_init_device
	tx_device_notify notify_internal      = {0};
	notify_internal.on_login_complete     = on_login_complete_internal;
	notify_internal.on_online_status      = on_online_status_internal;
	notify_internal.on_binder_list_change = on_binder_list_change_internal;

	int ret = tx_init_device(info,&notify_internal,init_path);
	if(ret != err_null)
	{
		TERROR("tx_init_device error [ret:%d]",ret);
	}

	return ret;
}

int tx_nas_init_file_transfer(tx_file_transfer_notify* notify, char * path_recv_file)
{
	//file_transfer_notify
	if(notify){
		g_file_transfer_nofity = (tx_file_transfer_notify*)malloc(sizeof(tx_file_transfer_notify));
		if(!g_file_transfer_nofity) return err_mem_alloc;
		memcpy(g_file_transfer_nofity,notify,sizeof(tx_file_transfer_notify));
	}else{
		g_file_transfer_nofity = NULL;
	}

	if(path_recv_file == NULL){
		return err_invalid_param;
	}

	//初始化文件传输
	tx_file_transfer_notify file_transfer_nofity_internal = {0};
	file_transfer_nofity_internal.on_file_in_come 		= on_file_in_come_internal;
	file_transfer_nofity_internal.on_transfer_progress 	= on_transfer_progress_internal;
	file_transfer_nofity_internal.on_transfer_complete 	= on_transfer_complete_internal;


	std::string strRecvPath = path_recv_file;
	TNOTICE("strRecvPath = [%s]",strRecvPath.c_str());
	if(!DU_File::makeDirRecursive(strRecvPath)){
		TERROR("创建临时文件接收目录失败 %s",strRecvPath.c_str());
		return nas_err_invalid_file_transfer_tmp_path;
	}

	int ret = tx_init_file_transfer(file_transfer_nofity_internal,path_recv_file);

	if(ret != 0) return ret;

	tx_reg_file_transfer_filter(BUSINESS_NAME_NAS_FILE_MUSIC, file_transfer_nofity_internal);
	tx_reg_file_transfer_filter(BUSINESS_NAME_NAS_FILE_VIDEO, file_transfer_nofity_internal);
	tx_reg_file_transfer_filter(BUSINESS_NAME_NAS_FILE_DOC, file_transfer_nofity_internal);
	tx_reg_file_transfer_filter(BUSINESS_NAME_NAS_FILE_COMMON, file_transfer_nofity_internal);
	tx_reg_file_transfer_filter("FileMsg", file_transfer_nofity_internal);

	return ret;
}

int tx_nas_init_data_point(const tx_data_point_notify *notify)
{
	if(notify){
		g_data_point_notify = (tx_data_point_notify*)malloc(sizeof(tx_data_point_notify));
		if(!g_data_point_notify) return err_mem_alloc;
		memcpy(g_data_point_notify,notify,sizeof(tx_data_point_notify));
	}else{
		g_data_point_notify = NULL;
	}

	//初始化datapoint
	tx_data_point_notify data_point_notify_internal = {0};
	data_point_notify_internal.on_receive_data_point = on_receive_data_point_internal;
	int ret = tx_init_data_point(&data_point_notify_internal);

	return ret;
}

 int tx_nas_init(tx_nas_device_mgr_wrapper* device_mgr_wrapper)
 {
	 if(device_mgr_wrapper == NULL){
		 TERROR("device_mgr_wrapper cannot be null");
		 return err_invalid_param;
	 }
	 g_device_mgr_wrapper = (tx_nas_device_mgr_wrapper*)malloc(sizeof(tx_nas_device_mgr_wrapper));
	 if(!g_device_mgr_wrapper) return err_mem_alloc;
	 memcpy(g_device_mgr_wrapper,device_mgr_wrapper,sizeof(tx_nas_device_mgr_wrapper));

	 //初始化file upload key
	 if(!init_file_upload_key_mgr_sdk()) {
		 TERROR("Init file upload key manager failed\n");
		 return err_failed;
	 }

	 //初始化文件传输历史列表
	 if(!init_file_transfer_record_mgr_sdk()) {
		 TERROR("Init file transfer record manager failed\n");
		 return err_failed;
	 } 

	 //初始化线程池
	 try {
		 g_pool_sdk.init(3);
		 g_pool_sdk.start();
	 } catch (DU_ThreadPool_Exception& e) {
		 TERROR("init thread pool failure, erorr = %s\n", e.what());
		 return err_failed;
	 }  

	 g_bExitNasSDK = false;

	 //开一个线程，当做程序的定时器，定期检测缩略图是否过期、保存数据
	 DU_Functor<void, TL::TLMaker<int>::Result> tf(nas_internal_timer);
	 DU_FunctorWrapper<
		 DU_Functor<void, TL::TLMaker<int>::Result> > tfw(tf,1);
	 g_pool_sdk.exec(tfw);

	 return 0;
 }


int tx_nas_uninit()
{
	g_bExitNasSDK = true;

	g_pool_sdk.stop();
	g_pool_sdk.waitForAllDone();
	
	//保存数据
	tx_nas_sync();

	//释放内存
	if(g_device_info) {	
		free(g_device_info);
		g_device_info = NULL;
	}
	if(g_device_notify) {
		free(g_device_notify);
		g_device_notify = NULL;
	}
	if(g_init_path) {
		free(g_init_path);
		g_init_path = NULL;
	}
	if(g_file_transfer_nofity){
		free(g_file_transfer_nofity);
		g_file_transfer_nofity = NULL;
	}

	if(g_data_point_notify) {
		free(g_data_point_notify);
		g_data_point_notify = NULL;
	}

	tx_exit_device();

	TNOTICE("NasSDK uninit success!");
	return err_null;
}

int tx_nas_upload_file(const char* abspath,const char* filekey)
{
	if(abspath == NULL || filekey == NULL) return -1;

	if(DU_File::isFileExist(abspath)){
		std::string md5 = "";//DU_MD5::md5str(DU_File::load2str(abspath)); //获取文件md5
		std::string url    = FILE_UPLOAD_KEY_MGR_SDK->get_file_upload_key(filekey,md5);
		if(!url.empty()){
			TERROR("Debug: --> Alread uploaded\n");
			return err_null;         //已经上传过
		}

		unsigned long long cookie = 0;
		tx_upload_file(transfer_channeltype_MINI, transfer_filetype_image, abspath, &cookie);
		if(cookie == 0){
			TERROR("upload image failed, file=%s",abspath);
			return err_failed;
		}
		else{
			TNOTICE("upload image success, file=%s",abspath);
			g_task_mgr.add_upload_task(cookie,0,0,0,NULL,abspath,filekey,transfer_channeltype_MINI,true);
			DU_ThreadControl::sleep(1000); 
			return err_null;
		}
	}
	else{
		TERROR("Image does not exist, image = %s\n", abspath);
		return nas_err_file_not_exist;
	}

	return err_failed;
}

int tx_nas_get_url(const char* filekey, char* url, int len)
{
	if(filekey == NULL) return err_invalid_param;
	//TNOTICE("get_url: filekey=%s\n",filekey);
	std::string md5 = "";//DU_MD5::md5str(DU_File::load2str(filekey));
	std::string strUrl = FILE_UPLOAD_KEY_MGR_SDK->get_file_upload_key(filekey,md5);
	if(strUrl.empty()) return nas_err_file_not_exist;
	//TNOTICE("get_url: strUrl=%s\n",strUrl.c_str());
	int lenUrl = strUrl.length();
	if(len < (lenUrl + 1)) return err_buffer_notenough;
	
	memset(url,0,len);
	memcpy(url,strUrl.c_str(),lenUrl);
	return err_null;
}

int tx_nas_delete_uploaded_file(const char* filekey)
{
	if(filekey == NULL) return err_invalid_param;

	FILE_UPLOAD_KEY_MGR_SDK->remove_file_upload_key(filekey);

	return err_null;
}

void tx_nas_refresh()
{
	//缩略图过期管理
	std::vector<string> expired_files;               //过期的缩略图
	FILE_UPLOAD_KEY_MGR_SDK->clean(expired_files);

	if(!expired_files.empty()){
		std::vector<string>::const_iterator it = expired_files.begin();
		for(; it != expired_files.end();++it){
			TNOTICE("thumbnail file [%s] expired, now upload again",(*it).c_str());
			tx_nas_upload_file((*it).c_str(),(*it).c_str());
		}
	}else{
		//TNOTICE("No thumbnail file expired!");
	}
}

//检测缩略图是否过期
void nas_internal_timer(int code)
{
	time_t now = time(NULL);
	time_t last_check_time_thumnail  = now;
	time_t last_check_time_sync = now;
	time_t last_check_time_report_dp = now;
	time_t last_check_time_send_file = now;
	time_t last_check_time_ack_dp = now;

	while(false == g_bExitNasSDK) {
		now = time(NULL);

		//检测缩略图是否过期
		if((now - last_check_time_thumnail) > (3600)){                     //不会访问文件，1小时 检测一次
			tx_nas_refresh();
			last_check_time_thumnail = now;
		}	

		//保存数据
		if((now - last_check_time_sync) > (36000)){            //会访问文件，10小时，保存一次
			tx_nas_sync();
			last_check_time_sync = now;
		}

		//检查发送文件的队列，如果有文件，则发送队首的文件
		if((now - last_check_time_send_file) > 2){            //两秒发送文件
			SendOperationMgr::getInstance()->SendFileTasks();
			last_check_time_send_file = now;
		}

		//检测是否有report_datapoint需要发送
		if((now - last_check_time_report_dp) > 2){             
			SendOperationMgr::getInstance()->SendReportDataPoints();
			last_check_time_report_dp = now;
		}

		//检测是否有ack_datapoint需要发送
		if((now - last_check_time_ack_dp) > 1){             
			SendOperationMgr::getInstance()->SendAckDataPoints();
			last_check_time_ack_dp = now;
		}
		
		DU_ThreadControl::sleep(1000);
	}
}

/////////////////////////////////////////////////////////////////////////////////////

void task_mgr::add_c2c_in_task(unsigned long long cookie, unsigned long long from_client)
{
	DU_ThreadLock::Lock lock(_list_lock);
	if(_c2c_in_list.find(cookie) != _c2c_in_list.end()) {
		rm_c2c_in_task(cookie); //删除原先的cookie
	}

	_c2c_in_list.insert(std::make_pair(cookie, from_client));
}
void task_mgr::add_c2c_out_task(unsigned long long cookie, unsigned long long from_client)
{
	DU_ThreadLock::Lock lock(_list_lock);
	if(_c2c_out_list.find(cookie) != _c2c_out_list.end()) {
		rm_c2c_out_task(cookie); //删除原先的cookie
	}
	_c2c_out_list.insert(std::make_pair(cookie, from_client));
}
void task_mgr::add_upload_task(unsigned long long cookie, unsigned long long from_client, unsigned int id, unsigned int seq, const char* relative_path, const char* abs_path, const char* filekey, int transfer_type, bool pre_upload)
{
	DU_ThreadLock::Lock lock(_list_lock);
	if(_upload_list.find(cookie) != _upload_list.end()) {
		rm_upload_task(cookie); //删除原先的cookie
	}
	upload_cookie_param_t param;
	param._from_client = from_client;
	param._id = id;
	param._seq = seq;
	if(relative_path != NULL) param._relative_path = relative_path;
	if(abs_path != NULL) param._abs_path = abs_path;
	if(filekey != NULL) param._filekey = filekey;
	param._transfer_type = transfer_type;
	param._pre_upload = pre_upload;

	_upload_list.insert(std::make_pair(cookie, param));

}
void task_mgr::add_download_task(unsigned long long cookie, unsigned long long from_client)
{
	DU_ThreadLock::Lock lock(_list_lock);
	if(_download_list.find(cookie) != _download_list.end()) {
		rm_download_task(cookie); //删除原先的cookie
	}

	_download_list.insert(std::make_pair(cookie, from_client));
}

//获取一个cookie list的内容
unsigned long long task_mgr::get_c2c_in_task(unsigned long long cookie)
{
	DU_ThreadLock::Lock lock(_list_lock);
	unsigned long long sender = 0;
	if(_c2c_in_list.find(cookie) != _c2c_in_list.end()) {
		sender = _c2c_in_list[cookie];
	}    
	return sender;
}
unsigned long long task_mgr::get_c2c_out_task(unsigned long long cookie)
{
	DU_ThreadLock::Lock lock(_list_lock);
	unsigned long long sender = 0;
	if(_c2c_out_list.find(cookie) != _c2c_out_list.end()) {
		sender = _c2c_out_list[cookie];
	}

	return sender;
}
task_mgr::upload_cookie_param_t* task_mgr::get_upload_task(unsigned long long cookie)
{
	DU_ThreadLock::Lock lock(_list_lock);
	upload_cookie_param_t* param = NULL;
	if(_upload_list.find(cookie) != _upload_list.end()) {
		param = &_upload_list[cookie];
	}

	return param;
}
unsigned long long task_mgr::get_download_task(unsigned long long cookie)
{
	DU_ThreadLock::Lock lock(_list_lock);
	unsigned long long sender = 0;
	if(_download_list.find(cookie) != _download_list.end()) {
		sender = _download_list[cookie];
	}

	return sender;
}

//删除一个cookie list内容
void task_mgr::rm_c2c_in_task(unsigned long long cookie)
{
	DU_ThreadLock::Lock lock(_list_lock);
	if(_c2c_in_list.find(cookie) != _c2c_in_list.end()) {
		_c2c_in_list.erase(cookie);
	}
}
void task_mgr::rm_c2c_out_task(unsigned long long cookie)
{
	DU_ThreadLock::Lock lock(_list_lock);
	if(_c2c_out_list.find(cookie) != _c2c_out_list.end()) {
		_c2c_out_list.erase(cookie);
	}
}
void task_mgr::rm_upload_task(unsigned long long cookie)
{
	DU_ThreadLock::Lock lock(_list_lock);
	if(_upload_list.find(cookie) != _upload_list.end()) {
		_upload_list.erase(cookie);
	}
}
void task_mgr::rm_download_task(unsigned long long cookie)
{
	DU_ThreadLock::Lock lock(_list_lock);
	if(_download_list.find(cookie) != _download_list.end()) {
		_download_list.erase(cookie);
	}
}

//////////////////////////////////////////////////////////////////

void notify_file_transfer_progress(unsigned long long cookie, unsigned long long progress, unsigned long long max_progress)
{
	file_transfer_record_t* record = file_transfer_record_mgr::getInstance()->get_transfer_record(cookie);
	if(record == NULL){
		TERROR("[notify_file_transfer_progress] get file transfer record failure, cookie = %llu\n", cookie);
		return;
	}

	//检查是否需要通知
	if(record->is_need_send_notify(progress)) {
		TNOTICE("send file transfer notify, tinyid = %llu, cookie = %llu, task_key = %s\n", record->_target_id, cookie, record->_task_key.c_str());
		//编码消息内容
		StringBuffer sb;
		Writer<StringBuffer> writer(sb);

		//{"task_key":"123456","file":"test.jpg","progress":12345,"max":123450}
		writer.StartObject();

		writer.String("subid");
		writer.Int(task_subid_transfer_progress);

		writer.String("task_key");
		writer.String(record->_task_key.c_str());

		//writer.String("file");
		//writer.String(record->_file_path.c_str());

		writer.String("progress");
		writer.Uint64(progress);

		writer.String("max");
		writer.Uint64(max_progress);

		writer.String("status");
		writer.Int(1);                    //1表示任务进行中 2 表示任务完成

		writer.String("ret");
		writer.Int(0);                    //0 表示没有错误，其它为具体错误

		writer.EndObject();

		std::string notify = sb.GetString();
		SendOperationMgr::getInstance()->AddReportDataPoint(record->_target_id,90002,notify,NOTIFY_TYPE_PROGRESS);
	}

	//设置进度
	file_transfer_record_mgr::getInstance()->set_transfer_progress(cookie,progress);
}

void notify_file_transfer_complete(unsigned long long cookie, const std::string& file_key, int errcode, int status)
{
	file_transfer_record_t record;
	bool bExisted = file_transfer_record_mgr::getInstance()->get_transfer_record_copy(cookie,record);     //这里只有ftn传输才会加进来，在onfileincome里面会判断
	if(!bExisted) {
		TERROR("[notify_file_transfer_complete] get file transfer record failure, maybe this is a upload thumbnail procedure , cookie = %llu\n", cookie);
		return;
	}

	if(file_key.empty() || errcode != 0 ){      //出错
		file_transfer_record_mgr::getInstance()->set_transfer_error(cookie,errcode);
	}
	else{
		file_transfer_record_mgr::getInstance()->set_transfer_end(cookie,file_key);
	}

	//检查是否需要通知
	TNOTICE("send file transfer notify complete !, tinyid = %llu, cookie = %llu, task_key = %s, err_code = %d\n", record._target_id, cookie, record._task_key.c_str(),record._err_code);

	//编码消息内容
	StringBuffer sb;
	Writer<StringBuffer> writer(sb);

	//{"task_key":"123456","file":"test.jpg","progress":12345,"max":123450,"ret":1234}
	writer.StartObject();

	writer.String("subid");
	writer.Int(task_subid_transfer_progress);

	writer.String("task_key");
	writer.String(record._task_key.c_str());

	//writer.String("file");
	//writer.String(record._file_path.c_str());

	writer.String("progress");
	writer.Uint64(record._total_bytes);

	writer.String("max");
	writer.Uint64(record._total_bytes);

	writer.String("ret");
	writer.Int(errcode);         //0 表示没有错误，其它为具体错误

	writer.String("status");
	writer.Int(status);                    //1表示任务进行中 2 表示任务完成

	writer.EndObject();

	std::string notify = sb.GetString();

	SendOperationMgr::getInstance()->AddReportDataPoint(record._target_id,90002,notify,NOTIFY_TYPE_COMPLETE);
}