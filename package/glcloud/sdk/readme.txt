1. 配置好mtk7620的编译器或者修改gen_bin.sh脚本编译器路径
2. 在命令终端进入当前目录
3. 运行 make 命令或者直接运行gen_bin.sh
4. 复制生成可执行文件gelian到run_env目录下
5. demo用使用发送图片作为文件传输，发送的文件放到send_data目录，接收的默认放到recv_data目录。
6. 把run_env目录复制到/tmp/目录下，并进入run_env
7. 运行 route add 255.255.255.255 dev apcli0
8. 运行 ./gelian

删除文件传输功能：
1. 打开main.c，注释SUPPORT_FILE_CLOUD宏
2. 删除gl_file_cloud.c和gl_file_cloud.h
3. 删除qiniu-sdk目录

删除设置控制功能
1. 打开main.c，注释SUPPORT_DEV_CONTROL宏
2. 删除user_dev.c和user_dev.h