#!/bin/bash

# 获取命令行参数中的日志级别
# 默认日志级别
log_level="info"
config_file="/etc/dde-cooperation/config/log.conf"
daemon_config_file="/etc/dde-cooperation-daemon/config/log.conf"

# 检查目录是否存在，如果不存在则创建
if [ ! -d "/etc/dde-cooperation/config" ]; then
    mkdir -p /etc/dde-cooperation/config
fi

# 检查配置文件是否存在，如果不存在则创建并写入日志级别
if [ ! -f "$config_file" ]; then
    echo "[General]" > "$config_file"
    echo "g_minLogLevel=2" >> "$config_file"
fi

# 检查目录是否存在，如果不存在则创建
if [ ! -d "/etc/dde-cooperation-daemon/config" ]; then
    mkdir -p /etc/dde-cooperation-daemon/config
fi

if [ ! -f "$daemon_config_file" ]; then
    echo "[General]" > "$daemon_config_file"
    echo "g_minLogLevel=2" >> "$daemon_config_file"
fi

# 解析命令行参数
for arg in "$@"; do
       case "$arg" in
       debug=*)
            # 获取 debug 参数的值
            log_level=${arg#debug=}
            ;;
	state)
	      state=y
		;;
	test)
		test=y
		;;
    esac
done

# 根据用户输入设置 g_minLogLevel 的值
case $log_level in
    "debug")
        sed -i 's/g_minLogLevel=.*/g_minLogLevel=0/' $config_file
        sed -i 's/g_minLogLevel=.*/g_minLogLevel=0/' $daemon_config_file
        ;;
    "on")
        sed -i 's/g_minLogLevel=.*/g_minLogLevel=0/' $config_file
        sed -i 's/g_minLogLevel=.*/g_minLogLevel=0/' $daemon_config_file
        ;;
    "off")
        sed -i 's/g_minLogLevel=.*/g_minLogLevel=2/' $config_file
        sed -i 's/g_minLogLevel=.*/g_minLogLevel=2/' $daemon_config_file
        ;;
    "info")
        sed -i 's/g_minLogLevel=.*/g_minLogLevel=1/' $config_file
        sed -i 's/g_minLogLevel=.*/g_minLogLevel=1/' $daemon_config_file
        ;;
    "warning")
        sed -i 's/g_minLogLevel=.*/g_minLogLevel=2/' $config_file
        sed -i 's/g_minLogLevel=.*/g_minLogLevel=2/' $daemon_config_file        
        ;;
    "error")
        sed -i 's/g_minLogLevel=.*/g_minLogLevel=3/' $config_file
        sed -i 's/g_minLogLevel=.*/g_minLogLevel=3/' $daemon_config_file
        ;;
    *)
        echo "无效的日志级别" $args
        exit 1
        ;;
esac

echo "dde-cooperation log level is $(grep 'g_minLogLevel' $config_file)"
