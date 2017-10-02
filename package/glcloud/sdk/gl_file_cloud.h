
#ifndef __GL_FILE_CLOUD_H__
#define __GL_FILE_CLOUD_H__

/*
 * 初始化文件上传相关数据，系统启动时调用，仅调用一次
 *
 * 参数：无
 *
 * 返回值：
 * 0：成功。
 * -1：失败
 */
int gl_file_cloud_init(void);

/*
 * 上传指定文件。该调用会阻塞直到文件上传完成并将信息推送到app。
 *
 * 参数：
 * @file: 待上传文件的完整路径。
 * @encrypt: 是否将文件加密后上传。加密耗费cpu资源，如无需求请勿设置。
 *
 * 返回值：
 * 0：成功。
 * 1: 请求token失败
 * -1：有文件正在上传。
 */
int gl_file_cloud_upload(const char *file, char encrypt);

/*
 * 上传指定buffer。该调用会阻塞直到buffer上传完成并将信息推送到app。
 *
 * 参数：
 * @file: 保存的目标文件名，用于标识所上传的buffer。
 * @buf: 待上传buffer指针。
 * @size: buffer大小。
 * @encrypt: 是否将buffer加密后上传。加密耗费cpu资源，如无需求请勿设置。
 *
 * 返回值：
 * 0：成功。
 * -1：出错。
 */
int gl_file_cloud_upload_buffer(const char *file, char encrypt, const char *buf, int size);

/*
 * 读取当前文件上传的进度。
 *
 * 参数：
 * @file_name, 文件名
 *
 * 返回值：
 * 0-100：文件上传进度。
 * -1：没有文件在上传。
 */
int gl_file_cloud_get_upload_percent(char *file_name);

/*
 * 设置上传的token值
 *
 * 参数：
 * @token，token值
 *
 * 返回值：
 * 0 ：设置成功
 * -1：设置失败，token为空
 */
int gl_file_cloud_set_token(char *token);

/*
 * 下载文件到指定路径。该调用会阻塞直到文件下载完成。
 *
 * 参数：
 * @dirname: 文件的保存路径，需为目录路径。
 * @basename: 文件名。
 * @url: 下载地址。
 * @md5: 文件的md5值。
 * @aes_key: 文件的解密密钥，如上传时未加密，下载时传空。
 *
 * 返回值：
 * 0：成功。
 * -1：出错。
 */
int gl_file_cloud_download(const char *dirname, const char *basename, const char *url, const char *md5, char *aes_key);

/*
 * 下载文件到指定buffer。该调用会阻塞直到文件下载完成。
 *
 * 参数：
 * @buf: 文件的下载的目的buffer指针。
 * @size: buffer的大小。
 * @url: 下载地址。
 * @md5: 文件的md5值。
 * @aes_key: 文件的解密密钥，如上传时未加密，下载时传空。
 *
 * 返回值：
 * 0：成功。
 * -1：出错。
 */
int gl_file_cloud_download_buffer(const char *buf, int size, const char *url, const char *md5, const char *aes_key);

#endif


