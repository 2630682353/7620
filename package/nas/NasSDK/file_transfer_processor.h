#ifndef FILE_TRANSFER_PROCESSOR_H
#define FILE_TRANSFER_PROCESSOR_H

#include <string>
#include <map>

#include "TXFileTransfer.h"
#include "utils.h"

class file_transfer_processor: public DU_Singleton<file_transfer_processor>
{
public:
	void on_process(const tx_file_transfer_info* ft_info, unsigned long long cookie);

private:
	int upload_process(const tx_file_transfer_info* ft_info, unsigned long long cookie);
	int download_process(const tx_file_transfer_info* ft_info, unsigned long long cookie);
	int c2c_in_process(const tx_file_transfer_info* ft_info, unsigned long long cookie);
	int c2c_out_process(const tx_file_transfer_info* ft_info, unsigned long long cookie);

	DU_ThreadLock _lock;
};

#endif // PROCESS_FT_H
