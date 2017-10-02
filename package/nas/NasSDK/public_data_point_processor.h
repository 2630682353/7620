#ifndef	_PUBLIC_DATA_POINT_PROCESSOR_H
#define _PUBLIC_DATA_POINT_PROCESSOR_H

#include "utils.h"
#include "file_transfer_record_mgr.h"
#include "TXSDKCommonDef.h"
#include "TXDeviceSDK.h"

class public_data_point_processor : public DU_Singleton<public_data_point_processor>
{
public:
	int on_process(unsigned long long from_client, tx_data_point *req_dp, tx_data_point* rsp_dp);
private:

	//局域网
	int process_http_server(unsigned long long from_client, tx_data_point *req_dp,tx_data_point* rsp_dp);
	int process_start_http_server(unsigned long long from_client,  tx_data_point *req_dp, tx_data_point* rsp_dp);
	int process_stop_http_server(unsigned long long from_client,  tx_data_point *req_dp, tx_data_point* rsp_dp);

	//任务管理
	int process_task(unsigned long long from_client, tx_data_point *req_dp, tx_data_point* rsp_dp);
    int process_dl(unsigned long long from_client, tx_data_point *req_dp, tx_data_point* rsp_dp);
	int process_cancel_file_transfer(unsigned long long from_client,  tx_data_point *req_dp, tx_data_point* rsp_dp);
	int process_file_transfer_detail(unsigned long long from_client,  tx_data_point *req_dp, tx_data_point* rsp_dp);

	//账号管理
	int process_account(unsigned long long from_client, tx_data_point *req_dp, tx_data_point* rsp_dp);
	int process_login(unsigned long long from_client, tx_data_point *req_dp, tx_data_point* rsp_dp);
	int process_logout(unsigned long long from_client, tx_data_point *req_dp, tx_data_point* rsp_dp);
	int process_check_login(unsigned long long from_client, tx_data_point *req_dp, tx_data_point* rsp_dp);
	

protected:

	//解析httpserver
	int parse_httpserver_request(tx_data_point* req_dp, http_server_req& req);

	////////////////////////////////////////////////////////////////////

	//解析任务
	int parse_task_request(tx_data_point* req_dp, task_req& request);

	//文件下载
	int parse_file_dl_request(tx_data_point* req_dp, file_dl_req& request);
	void format_file_dl_response(int ret, std::string& json_rsp, const file_dl_req& request, const file_dl_rsp& response);
	int impl_file_dl(unsigned long long from_client,file_dl_req& request,file_dl_rsp& response,tx_data_point* req_dp);

	//取消任务
	int parse_cancel_file_transfer_request(tx_data_point* data_point, cancel_file_transfer_req& request);
	void format_cancel_file_transfer_response(int ret, std::string& json_rsp, const cancel_file_transfer_req& request, const cancel_file_transfer_rsp& response);
	int impl_cancel_file_transfer(std::string task_key);

	//获取任务详情
	int parse_file_transfer_detail_request(tx_data_point* data_point, file_transfer_detail_req& request);
	void format_file_transfer_detail_response(std::string& json_rsp, const file_transfer_record&record, const file_transfer_detail_req& request, const file_transfer_detail_rsp& response);

	////////////////////////////////////////////////////////////////////
	
	//账号管理
	int parse_account_request(tx_data_point* req_dp, account_req& request);

	int parse_login_request(tx_data_point* req_dp, login_req& request);
	void format_login_response(std::string& json_rsp, const login_req& request, const login_rsp& response);

	int parse_logout_request(tx_data_point* req_dp, logout_req& request);
	void format_logout_response(std::string& json_rsp, const logout_req& request, const logout_rsp& response);

	int parse_check_login_request(tx_data_point* req_dp, check_login_req& request);
	void format_check_login_response(std::string& json_rsp, const check_login_req& request, const check_login_rsp& response);
};

#endif // FILE_PROCESSOR_H