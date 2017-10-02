//
//  public_data_point_processor.cpp
//
//  Created by jiangtaozhu on 15/8/8.
//  Copyright (c) 2015年 jiangtaozhu. All rights reserved.
//


#include "public_data_point_processor.h"
#include "string.h"
#include "TXNasSDK.h"
#include "TXNasSDKImpl.h"


#define MAX_MP4_FILE_SIZE 1024 * 1024 * 20 //暂定20M
#define NAS_SDK_VERSION  1

extern tx_nas_device_mgr_wrapper*  g_device_mgr_wrapper;

void format_response_data_point_error(int ret, const char* err_msg,tx_data_point* req_dp, tx_data_point* rsp_dp, int subid = -1)
{
	if(err_msg == NULL || req_dp==NULL || rsp_dp == NULL){
		TERROR("format_response_data_point_error: param is null");
		return;
	}

	rsp_dp->id				= req_dp->id;
	rsp_dp->seq             = req_dp->seq;
	rsp_dp->ret_code     = ret;

	//value
	char buf[256] = {0};
	if(subid == -1)	{
		snprintf(buf,256, "{\"ret\":%d,\"msg\":\"%s\"}", ret, err_msg);
	}
	else	{ //任务相关需要加subid
		snprintf(buf,256, "{\"ret\":%d,\"subid\":%d,\"msg\":\"%s\"}", ret,subid,err_msg);
	}

	int buf_len = strlen(buf);
	rsp_dp->value = new char[buf_len+1];
	memset(rsp_dp->value,0,buf_len+1);
	memcpy(rsp_dp->value, buf, buf_len);
}

void format_response_data_point_success(const std::string& rsp, tx_data_point* req_dp, tx_data_point *rsp_dp)
{
	if(req_dp==NULL || rsp_dp == NULL){
		TERROR("format_response_data_point_error: param is null");
		return;
	}

	rsp_dp->id  = req_dp->id;
	rsp_dp->seq              = req_dp->seq;
	rsp_dp->ret_code         = 0;

	int len = rsp.length();
	rsp_dp->value = new char[len+1];
	memset(rsp_dp->value,0,len+1);
	memcpy(rsp_dp->value,rsp.c_str(),len);
}


int public_data_point_processor::on_process(unsigned long long from_client, tx_data_point *req_dp, tx_data_point* rsp_dp)
{
	if(req_dp == NULL || rsp_dp == NULL) {
		return -1;
	}

	 TNOTICE("from_client=%lld, id=%d, seq=%d, val=%s ",from_client, req_dp->id, req_dp->seq,req_dp->value);

	int ret = device_error_code_success;
	if(req_dp->id == 90001){
		ret = process_http_server(from_client,req_dp,rsp_dp);
	}
	else if (req_dp->id == 90002){
		ret = process_task(from_client,req_dp,rsp_dp);
	}
	else if (req_dp->id == 90004){
		ret = process_account(from_client,req_dp,rsp_dp);
	}
	else {        
        TNOTICE("from_client=%lld, data_point__id=%d, data_point_value=%s, unknown public datapoint id.",from_client, req_dp->id, req_dp->value);
		format_response_data_point_error(device_error_code_unknown_property_id,"unknown datapoint id",req_dp,rsp_dp);
		ret = device_error_code_unknown_property_id;
    }
	return ret;	
}

int public_data_point_processor::process_http_server(unsigned long long from_client, tx_data_point *req_dp,tx_data_point* rsp_dp)
{
	int ret = device_error_code_success;

	//1. 判断是否登陆
	if(g_device_mgr_wrapper->notify_user_check_login){

		char user[512] = {0};
		if(g_device_mgr_wrapper->notify_user_check_login(from_client,NULL,user,512) != 0){
			TERROR("tinyid = %llu is not login\n",from_client);
			format_response_data_point_error(device_error_code_not_login,"请重新登录",req_dp,rsp_dp);
			return -1;
		}

	}
	
	//2. 解析请求
	http_server_req request; 

	//如果解析失败就返回错误响应
	if((ret = parse_httpserver_request(req_dp, request)) != device_error_code_success ) {
		format_response_data_point_error(ret,"解析失败",req_dp,rsp_dp);
		return -1;
	}

	//3. 根据type 进行分发，转交到相应的处理逻辑
	if(request.type == 0){  //启动httpserver
		return process_start_http_server(from_client,req_dp,rsp_dp);
	}
	else{                           //停止httpserver
		return process_stop_http_server(from_client,req_dp,rsp_dp);
	}
}

int public_data_point_processor::process_start_http_server(unsigned long long from_client, tx_data_point *req_dp, tx_data_point* rsp_dp)
{
	//启动HttpServer
	format_response_data_point_error(device_error_code_not_impl,"启用局域网功能未实现",req_dp,rsp_dp);
	return 0;
}

int public_data_point_processor::process_stop_http_server(unsigned long long from_client, tx_data_point *req_dp, tx_data_point* rsp_dp)
{
	//停止HttpServer
	format_response_data_point_error(device_error_code_not_impl,"停用局域网功能未实现",req_dp,rsp_dp);
	return 0;
}

int public_data_point_processor::process_task(unsigned long long from_client, tx_data_point *req_dp,tx_data_point *rsp_dp)
{
	int ret = device_error_code_success;

	//1. 判断是否登陆
	if(g_device_mgr_wrapper->notify_user_check_login){

		char user[512] = {0};
		if(g_device_mgr_wrapper->notify_user_check_login(from_client,NULL,user,512) != 0){
			TERROR("tinyid = %llu is not login\n",from_client);
			format_response_data_point_error(device_error_code_not_login,"请重新登录",req_dp,rsp_dp);
			return -1;
		}
	}

	//2. 解析请求
	task_req request;

	//如果解析失败就返回错误响应
	if((ret = parse_task_request(req_dp, request)) != device_error_code_success ) {
		format_response_data_point_error(ret,"解析失败",req_dp,rsp_dp);
		return -1;
	}

	//3. 根据subid 进行分发，转交到原先的处理逻辑
	if (request.subid == task_subid_file_download) //文件下载
	{
		return process_dl(from_client,req_dp,rsp_dp);
	}
	else if(request.subid == task_subid_transfer_cancel){  //取消任务
		return process_cancel_file_transfer(from_client,req_dp,rsp_dp);
	}
	else if(request.subid ==task_subid_transfer_detail ){  //查询任务具体状态
		return process_file_transfer_detail(from_client,req_dp,rsp_dp);
	}
	else{
		ret = device_error_code_request_invalid;
		format_response_data_point_error(ret,"unknow subid",req_dp,rsp_dp);
		return -1;
	}
}

int public_data_point_processor::process_dl(unsigned long long from_client, tx_data_point *req_dp,tx_data_point *rsp_dp)
{
	int ret = 0;
 
    //1. 解析请求
    file_dl_req request;
    file_dl_rsp response;
    
    //如果解析失败就返回错误响应
    request.target_id =from_client;
    if((ret = parse_file_dl_request(req_dp, request)) != 0 ) {
		format_response_data_point_error(ret,"解析失败",req_dp,rsp_dp);
        return -1;
    }

    //2. 处理文件下载
	ret = impl_file_dl(from_client,request,response,req_dp);
	if(ret == device_error_code_success || ret == device_error_code_part_success) { //全部成功或部分成功

		//3. 将响应编码成json
		std::string json_rsp;
		format_file_dl_response(ret, json_rsp, request, response);

		//4. 生成响应
		format_response_data_point_success(json_rsp,req_dp,rsp_dp);
		return -1;
		
	} else{ //所有文件下载均失败
		TERROR("process file download request failure, tinyid = %llu, count = %u\n",from_client, request.dl_item.size());
		format_response_data_point_error(ret,"file download failed",req_dp,rsp_dp);
		return -1;
	}
   
}

int public_data_point_processor::process_cancel_file_transfer(unsigned long long from_client, tx_data_point *req_dp,tx_data_point* rsp_dp)
{
	int ret = device_error_code_request_invalid;

	//1. 解析请求
	cancel_file_transfer_req request;
	cancel_file_transfer_rsp response;

	//如果解析失败就返回错误响应
	if((ret = parse_cancel_file_transfer_request(req_dp, request)) != 0 ) {
		format_response_data_point_error(ret,"请求格式错误",req_dp,rsp_dp);
		return -1;
	}

	//2. 处理取消任务

	bool has_failed = false;           //是否有任务处理失败

	vector<cancel_file_transfer_pair>::iterator  it = request.task_key_list.begin();
	for(; it != request.task_key_list.end(); ++it)
	{
		bool bRet = false;

		//上传任务（未进行，还在队列中，从队列中删除）
		bRet = SendOperationMgr::getInstance()->DeleteSendFileTask(it->task_key);
		if(bRet) 
		{
			it->result = true;
			continue;
		}

		//任务已进行，通过cookie取消
		unsigned long long cookie;
		bRet = file_transfer_record_mgr::getInstance()->get_transfer_cookie_by_task_key(it->task_key,cookie);
		if(!bRet)
		{
			TNOTICE("task_key[%s] %s",it->task_key.c_str(),"未查询到此任务");
			it->result = false;
			has_failed = true;
			continue;
		}

		if(tx_cancel_transfer(cookie) != 0)
		{
			TNOTICE("task_key[%s] %s",it->task_key.c_str(),"取消任务失败");
			it->result = false;
			has_failed = true;
			continue;
		}else{                                           //成功取消
			file_transfer_record* record = file_transfer_record_mgr::getInstance()->get_transfer_record(cookie);
			if(record)			record->set_transfer_cancel();
		}
		it->result = true;
	}

	if(!has_failed){   //全部成功
		ret = 0;        
	}else{                //存在失败
		ret = device_error_code_part_success;
	}

	std::string rsp;
	format_cancel_file_transfer_response(ret, rsp,request,response);
	format_response_data_point_success(rsp,req_dp,rsp_dp);

	return 0;
}

int public_data_point_processor::process_file_transfer_detail(unsigned long long from_client,  tx_data_point *req_dp, tx_data_point* rsp_dp)
{
	int ret = device_error_code_request_invalid;

	file_transfer_detail_req request;
	file_transfer_detail_rsp response;

	//如果解析失败就返回错误响应
	if((ret = parse_file_transfer_detail_request(req_dp, request)) != 0 ) {
		format_response_data_point_error(ret,"请求格式错误",req_dp,rsp_dp);
		return -1;
	}

	//查询所有的已经保存的文件传输任务状态
	unsigned long long cookie;
	bool bRet = file_transfer_record_mgr::getInstance()->get_transfer_cookie_by_task_key(request.task_key,cookie);
	if(!bRet)
	{
		format_response_data_point_error(device_error_code_unknown_error,"未查询到此任务",req_dp,rsp_dp);
		return -1;
	}

	file_transfer_record* record = file_transfer_record_mgr::getInstance()->get_transfer_record(cookie, false); 
	if(record == NULL)
	{
		format_response_data_point_error(device_error_code_unknown_error,"未查询到此任务",req_dp,rsp_dp);
		return -1;
	}

	std::string rsp;
	format_file_transfer_detail_response(rsp,(*record),request,response);
	format_response_data_point_success(rsp,req_dp,rsp_dp);

	return 0;
}


int public_data_point_processor::process_account(unsigned long long from_client, tx_data_point *req_dp,tx_data_point *rsp_dp)
{
	int ret = device_error_code_success;

	//2. 解析请求
	account_req request;
//	account_rsp response;

	//如果解析失败就返回错误响应
	if((ret = parse_account_request(req_dp, request)) != device_error_code_success ) {
		format_response_data_point_error(ret,"解析失败",req_dp,rsp_dp);
		return -1;
	}

	//3. 根据subid 进行分发，转交到原先的处理逻辑
	if (request.subid == account_subid_login)              //登陆
	{
		return process_login(from_client,req_dp,rsp_dp);
	}
	else if(request.subid == account_subid_logout){   //登出
		return process_logout(from_client,req_dp,rsp_dp);
	}
	else if(request.subid ==account_subid_check_login ){  //查询登陆状态
		return process_check_login(from_client,req_dp,rsp_dp);
	}
	else{
		ret = device_error_code_request_invalid;
		format_response_data_point_error(ret,"unknow subid",req_dp,rsp_dp);
		return -1;
	}
}

int public_data_point_processor::process_login(unsigned long long from_client, tx_data_point *req_dp, tx_data_point* rsp_dp)
{
	int ret = device_error_code_success;
	TNOTICE("process_login...");
	//1. 解析请求
	login_req request;
	login_rsp response;

	ret = parse_login_request(req_dp,request);

	if(ret != device_error_code_success){
		format_response_data_point_error(ret, "请求格式错误", req_dp, rsp_dp);
		return -1;
	}

	//2. 登陆设备
	if(g_device_mgr_wrapper && g_device_mgr_wrapper->notify_user_login){
		
		if(request.extra_data.empty()){
			ret = g_device_mgr_wrapper->notify_user_login(from_client,request.usr.c_str(),request.pwd.c_str(), NULL);
		}else{
			ret = g_device_mgr_wrapper->notify_user_login(from_client,request.usr.c_str(),request.pwd.c_str(),request.extra_data.c_str());
		}
		if(ret != 0){
			format_response_data_point_error(device_error_code_login_failure, "用户名或密码错误", req_dp, rsp_dp);
			return -1;
		}else{
			std::string json_rsp;
			format_login_response(json_rsp,request,response);
			format_response_data_point_success(json_rsp,req_dp,rsp_dp);
			return -1;
		}

	}else{
		format_response_data_point_error(device_error_code_request_invalid, "设备不支持登陆", req_dp, rsp_dp);
		return -1;
	}

	return 0;
}

int public_data_point_processor::process_logout(unsigned long long from_client, tx_data_point *req_dp, tx_data_point* rsp_dp)
{
	//1. 解析请求
	int ret = device_error_code_success;
	logout_req request;
	logout_rsp response;

	TNOTICE("process_logout");

	if((ret = parse_logout_request(req_dp,request)) != device_error_code_success){
		format_response_data_point_error(ret, "请求格式错误", req_dp, rsp_dp);
		return -1;
	}

	//2.  登出设备
	if(g_device_mgr_wrapper && g_device_mgr_wrapper->notify_user_logout){

		ret = g_device_mgr_wrapper->notify_user_logout(from_client,(request.extra_data.empty() ? NULL : request.extra_data.c_str()));
		if(ret != 0){
			format_response_data_point_error(device_error_code_logout_failure, "登出失败", req_dp, rsp_dp);
			return -1;
		}else{
			std::string json_rsp;
			format_logout_response(json_rsp,request,response);
			format_response_data_point_success(json_rsp,req_dp,rsp_dp);
			return -1;
		}

	}else{
		format_response_data_point_error(device_error_code_request_invalid, "设备不支持登出", req_dp, rsp_dp);
		return -1;
	}

	return 0;
}

int public_data_point_processor::process_check_login(unsigned long long from_client, tx_data_point *req_dp, tx_data_point* rsp_dp)
{
	int ret =device_error_code_success;

	//1. 解析请求
	check_login_req request;
	check_login_rsp response;

	TNOTICE("process_check_login");

	if((ret = parse_check_login_request(req_dp,request)) != device_error_code_success){
		format_response_data_point_error(ret, "请求格式错误", req_dp, rsp_dp);
		return -1;
	}

	//2. 检查是否登陆
	if(g_device_mgr_wrapper && g_device_mgr_wrapper->notify_user_check_login){

		char user[512] = {0};
		ret = g_device_mgr_wrapper->notify_user_check_login(from_client,(request.extra_data.empty() ? NULL : request.extra_data.c_str()),user,512);
		if(ret != 0){
			response.is_login = false;
		}else{
			response.is_login = true;
			response.usr = user;
		}

		std::string json_rsp;
		format_check_login_response(json_rsp,request,response);
		format_response_data_point_success(json_rsp,req_dp,rsp_dp);
		return -1;

	}else{
		format_response_data_point_error(device_error_code_request_invalid, "设备不支持查询登陆状态", req_dp, rsp_dp);
		return -1;
	}

	return 0;
}


int public_data_point_processor::parse_task_request(tx_data_point* req_dp, task_req& request)
{
	int ret = device_error_code_request_invalid;

	std::string req_json(req_dp->value);
	const char* json = req_json.data();
	// const char* json = "{\"subid\":\"/IMG20150311131342.jpg\","type":"pic"}";
	//{"files":[{"path":"/IMG20150311131342.jpg","type":"pic"}]}

	Document req_d;
	req_d.Parse(json);

	if (req_d.HasParseError() == true)
	{
		TERROR("json is not correct. json=%s", json);
		return ret;
	}

	if(!(req_d.HasMember("subid") && req_d["subid"].IsInt())) {
		TERROR("json is not satisfied fmt. json=%s",json);
		return ret;
	}

	request.subid = req_d["subid"].GetInt();
	ret = device_error_code_success;
	return ret;
}

int public_data_point_processor::parse_file_dl_request(tx_data_point* req_dp, file_dl_req& request)
{
	int ret = device_error_code_request_invalid;

	std::string req_json(req_dp->value);
	const char* json = req_json.data();
	//{"files":[{"path":"/IMG20150311131342.jpg","type":"pic"}]}

	Document req_d;
	req_d.Parse(json);

	if (req_d.HasParseError() == true)
	{
		TERROR("json is not correct. json=%s", json);
		return ret;
	}

	if(!(req_d.HasMember("files") && req_d["files"].IsArray())) {
		TERROR("json is not satisfied fmt. json=%s",json);
		return ret;
	}

	Value& s = req_d["files"];
	for(unsigned int i = 0; i < s.Size(); ++i) {
		file_dl_item item;
		if(s[i].HasMember("path") && s[i]["path"].IsString()
			&& s[i].HasMember("type") && s[i]["type"].IsInt()
			) {
				item.path = s[i]["path"].GetString();
				item.type = s[i]["type"].GetInt();  
		}
		else
		{
			TERROR("json is not satisfied fmt. json=%s",json);
			return ret;
		}

		if(s[i].HasMember("task_key") && s[i]["task_key"].IsString())  //手Q目前是没有task_key的
		{
			item.task_key = s[i]["task_key"].GetString();
		}
		else{
			item.task_key = "mobile_qq_task_key";
		}

		request.dl_item.push_back(file_dl_item(item.path,item.type,item.task_key));

	}

	ret = device_error_code_success;
	return ret;
}
void public_data_point_processor::format_file_dl_response(int ret, std::string& json_rsp, const file_dl_req& request, const file_dl_rsp& response)
{
    StringBuffer sb;
    Writer<StringBuffer> writer(sb);
    
    writer.StartObject();
    writer.String("ret");
    writer.Int(ret);
	writer.String("subid");
	writer.Int(task_subid_file_download);
	
	if(ret == device_error_code_part_success){
		writer.String("failed_tasks");
		writer.StartArray();
		std::vector<file_dl_item>::const_iterator it = request.dl_item.begin();
		for(; it != request.dl_item.end(); ++it)
		{
			if(!it->result){                   //失败
				writer.StartObject();
				writer.String("task_key");
				writer.String(it->task_key.c_str());
				writer.String("path");
				writer.String(it->path.c_str());
				writer.EndObject();
			}
		}
		writer.EndArray();
	}

    writer.EndObject();
    
    json_rsp = sb.GetString();
}

int public_data_point_processor::impl_file_dl(unsigned long long from_client,  file_dl_req& req,file_dl_rsp& response,tx_data_point* dp)
{
	//file_key_to_path()
	int ret = device_error_code_success;

	unsigned int fail_download_count = 0;
	for(std::vector<file_dl_item>::iterator it = req.dl_item.begin();
		it != req.dl_item.end();
		++it) {

			//filekey -> 绝对路径
			char tmp_abspath[1024] = {0};	
			if(g_device_mgr_wrapper->relative2abspath)
			{
				if(g_device_mgr_wrapper->relative2abspath(from_client,it->path.c_str(),tmp_abspath,1024) != 0){
					++ fail_download_count;
					it->result = false;
					TERROR("relative2abspath failed %s",it->path.c_str() );
					continue;
				}
			}
			else{
				TERROR("no relative2abspath!");
				it->result = false;
				continue;
			}

			std::string real_path(tmp_abspath);
			if(!DU_File::isFileExist(real_path)) {
				TERROR("download file failure, %s does not exist\n", real_path.c_str());
				++fail_download_count;
				it->result = false;
				continue;
			}

			it->result = true;

			//生成buffer with file
			std::string buff_with_file;
			{
				StringBuffer sb;
				Writer<StringBuffer> writer(sb);

				writer.StartObject();
				writer.String("datapoint");
				writer.StartArray();
				writer.StartObject();
				writer.String("id");
				writer.Uint(800010);
				writer.String("apiName");
				writer.String("set_data_point");
				writer.String("type");
				writer.String("string");
				writer.String("value");
				writer.String(it->path.c_str());
				writer.String("seq");
				writer.Int(dp->seq);
				writer.String("din");
				writer.Uint64(tx_get_self_din());
				writer.String("task_key");
				writer.String(it->task_key.c_str());
				writer.EndObject();
				writer.EndArray();
				writer.EndObject();
				buff_with_file = sb.GetString();
			}

			//对business name赋值
			std::string business_name ;
			if( it->type == file_type_def_pic ) {
				business_name = "ImgMsg";
			} /*else if(it->type == file_type_def_audio) {
				business_name = "7000-NASDevPushFile";
			} else if (it->type == file_type_def_video) {
				business_name = "7000-NASDevPushFile";
			}*/ else {
				business_name = "7000-NASDevPushFile";
			}

			SendOperationMgr::getInstance()->AddSendFileTask(it->task_key,req.target_id,real_path,buff_with_file,business_name);
	}

	//设置结果码
	if(fail_download_count > 0) {
		if(fail_download_count == req.dl_item.size()) {
			//全部下载失败
			ret = device_error_code_file_not_exist;
		} else {
			ret = device_error_code_part_success;
		}
	}
	return ret;
}

int public_data_point_processor::parse_cancel_file_transfer_request(tx_data_point* data_point, cancel_file_transfer_req& request)
{
	int ret = device_error_code_request_invalid;
	std::string req_json(data_point->value);
	const char* json = req_json.data();
	// const char* json = "{\"task_key\":\"aaaaa\"}";

	Document req_d;
	req_d.Parse(json);

	if (req_d.HasParseError() == true)
	{
		TERROR("json is not correct. json=%s", json);
		return ret;
	}

	if (!(req_d.HasMember("tasks") && req_d["tasks"].IsArray()))
	{
		TERROR("json is not satisfied fmt. json=%s",json);
		return ret;
	}

	Value& s = req_d["tasks"];
	for(unsigned int i = 0; i < s.Size(); ++i) {
		if(s[i].HasMember("task_key") && s[i]["task_key"].IsString())
		{
			cancel_file_transfer_pair p;
			std::string str =  s[i]["task_key"].GetString();
			p.task_key = str;
			p.result = false;
			request.task_key_list.push_back(p);
		}
	}

	ret = device_error_code_success;
	return ret;
}
void public_data_point_processor::format_cancel_file_transfer_response(int ret, std::string& json_rsp, const cancel_file_transfer_req& request, const cancel_file_transfer_rsp& response)
{
	StringBuffer sb;
	Writer<StringBuffer> writer(sb);

	writer.StartObject();
	writer.String("ret");
	writer.Int(ret);

	writer.String("subid");
	writer.Int(task_subid_transfer_cancel);


	if(ret == device_error_code_part_success){
		writer.String("failed_tasks");
		writer.StartArray();
		for(std::vector<cancel_file_transfer_pair>::const_iterator it = request.task_key_list.begin(); it != request.task_key_list.end(); ++it)
		{
			if(! it->result){
				writer.StartObject();
				writer.String("task_key");
				writer.String(it->task_key.c_str());
				writer.EndObject();
			}
		}
		writer.EndArray();
	}
	
	writer.EndObject();

	json_rsp = sb.GetString();
}

int public_data_point_processor::parse_file_transfer_detail_request(tx_data_point* data_point, file_transfer_detail_req& request)
{
	int ret = device_error_code_request_invalid;
	std::string req_json(data_point->value);
	const char* json = req_json.data();
	// const char* json = "{\"task_key\":\"aaaaa\"}";

	Document req_d;
	req_d.Parse(json);

	if (req_d.HasParseError() == true)
	{
		TERROR("json is not correct. json=%s", json);
		return ret;
	}

	if(req_d.HasMember("task_key") && req_d["task_key"].IsString())
	{
		request.task_key = req_d["task_key"].GetString();
	}
	else
	{
		return ret;
	}


	ret = device_error_code_success;
	return ret;
}

void public_data_point_processor::format_file_transfer_detail_response(std::string& json_rsp, const file_transfer_record&record, const file_transfer_detail_req& request, const file_transfer_detail_rsp& response)
{
	//{"ret":0,"":"http://www.baidu.com","size":1024,"type":1}

	StringBuffer sb;
	Writer<StringBuffer> writer(sb);

	writer.StartObject();
	writer.String("ret");
	writer.Int(0);

	writer.String("subid");
	writer.Int(task_subid_transfer_detail);

	writer.String("task_key");
	writer.String(record._task_key.c_str());

	writer.String("file");
	writer.String(record._file_path.c_str());

	writer.String("status");
	writer.Int(record._transfer_status);

	writer.String("progress");
	writer.Uint64(record._transfered_bytes);

	writer.String("max");
	writer.Uint64(record._total_bytes);	

	writer.EndObject();
	json_rsp = sb.GetString();
}

int public_data_point_processor::parse_httpserver_request(tx_data_point* data_point, http_server_req& req)
{
	int ret = device_error_code_success;

	//解析响应
	//req.md5.clear();
	//req.force = false;
	std::string req_json(data_point->value);
	const char* json = req_json.data();
	// const char* json = "{\"port\":1024}";

	Document req_d;
	req_d.Parse(json);

	if (req_d.HasParseError()) {
		TERROR("json is not correct. json=%s\n", json);
		ret = device_error_code_request_invalid;
		return ret;
	}

	if (!(req_d.HasMember("type") && req_d["type"].IsInt())) {
		TERROR("json is not satisfied fmt. json=%s\n",json);
		ret = device_error_code_request_invalid;
		return ret;
	}

	req.type = req_d["type"].GetInt();

	if(req.type == 1)  //停止http，必须有port
	{
		if (!(req_d.HasMember("port") && req_d["port"].IsInt())) {
			TERROR("json is not satisfied fmt. json=%s\n",json);
			ret = device_error_code_request_invalid;
			return ret;
		}
		req.port =  req_d["port"].GetInt();
	}
	return ret;
}

int public_data_point_processor::parse_account_request(tx_data_point* req_dp, account_req& request)
{
	int ret = device_error_code_request_invalid;

	std::string req_json(req_dp->value);
	const char* json = req_json.data();

	Document req_d;
	req_d.Parse(json);

	if (req_d.HasParseError() == true)
	{
		TERROR("json is not correct. json=%s", json);
		return ret;
	}

	if(!(req_d.HasMember("subid") && req_d["subid"].IsInt())) {
		request.subid = account_subid_check_login;        //默认为account_subid_check_login，查询状态
	}else{
		request.subid = req_d["subid"].GetInt();
	}

	

	ret = device_error_code_success;
	return ret;
}

int public_data_point_processor::parse_login_request(tx_data_point* req_dp, login_req& request)
{
	std::string req_json(req_dp->value);
	const char* json = req_json.data();
	// const char* json = "{\"usr\":\"admin",\"pwd\":"123456",\"extra_data\":\"{\"auto_login\":1}\"}";

	TNOTICE("req_json=%s", req_json.c_str());

	Document req_d;
	req_d.Parse(json);
	
	if (req_d.HasParseError() == true)
	{
		TERROR("json is not correct. json=%s", json);
		return device_error_code_request_invalid;
	}

	if (!(req_d.HasMember("usr") && req_d["usr"].IsString()
		&& req_d.HasMember("pwd") && req_d["pwd"].IsString()))
	{
		TERROR("json is not satisfied fmt. json=%s",json);
		return device_error_code_request_invalid;
	}

	request.usr = req_d["usr"].GetString();;
	request.pwd = req_d["pwd"].GetString();

	if(req_d.HasMember("extra_data") && req_d["extra_data"].IsString()){
		request.extra_data = req_d["extra_data"].GetString();

	}else{
		request.extra_data = "";
	}


	return 0;
}

void public_data_point_processor::format_login_response(std::string& json_rsp, const login_req& request, const login_rsp& response)
{
	StringBuffer sb;
	Writer<StringBuffer> writer(sb);

	writer.StartObject();
	writer.String("ret");
	writer.Int(0);
	writer.String("usr");
	writer.String(request.usr.c_str());
	writer.EndObject();

	json_rsp = sb.GetString();
}



int public_data_point_processor::parse_logout_request(tx_data_point* req_dp, logout_req& request)
{
	if(req_dp->value == NULL)  return device_error_code_success;

	std::string req_json(req_dp->value);
	const char* json = req_json.data();
	// const char* json = "{\"extra_data\":\"{\"auto_login\":1}\"}";

	TNOTICE("req_json=%s", req_json.c_str());

	Document req_d;
	req_d.Parse(json);

	if (req_d.HasParseError() == true)
	{
		TERROR("json is not correct. json=%s", json);
		return device_error_code_success;
	}

	if(req_d.HasMember("extra_data") && req_d["extra_data"].IsString())
	{
		request.extra_data = req_d["extra_data"].GetString();
	}

	return device_error_code_success;
}


void public_data_point_processor::format_logout_response(std::string& json_rsp, const logout_req& request, const logout_rsp& response)
{
	StringBuffer sb;
	Writer<StringBuffer> writer(sb);

	writer.StartObject();
	writer.String("ret");
	writer.Int(0);
	writer.EndObject();

	json_rsp = sb.GetString();
}


int public_data_point_processor::parse_check_login_request(tx_data_point* req_dp, check_login_req& request)
{
	if(req_dp->value == NULL)  return device_error_code_success;

	std::string req_json(req_dp->value);
	const char* json = req_json.data();
	// const char* json = "{\"extra_data\":\"{\"auto_login\":1}\"}";

	TNOTICE("req_json=%s", req_json.c_str());

	Document req_d;
	req_d.Parse(json);

	if (req_d.HasParseError() == true)
	{
		TERROR("json is not correct. json=%s", json);
		return device_error_code_success;
	}

	if(req_d.HasMember("extra_data") && req_d["extra_data"].IsString())
	{
		request.extra_data = req_d["extra_data"].GetString();
	}

	return device_error_code_success;
}

void public_data_point_processor::format_check_login_response(std::string& json_rsp, const check_login_req& request, const check_login_rsp& response)
{
	StringBuffer sb;
	Writer<StringBuffer> writer(sb);

	writer.StartObject();
	writer.String("ret");
	writer.Int(0);
	writer.String("is_login");
	writer.Bool(response.is_login);
	if(!response.usr.empty()){
		writer.String("usr");
		writer.String(response.usr.c_str());
	}
	writer.String("version");
	writer.Int(NAS_SDK_VERSION);
	writer.EndObject();

	json_rsp = sb.GetString();
}
