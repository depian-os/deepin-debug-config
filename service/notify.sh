#!/bin/bash
failmsg="Debug mode configuration failed"
failmsg_zh="调试模式配置失败"

rebootmsg="The debug mode configuration is completed, please restart the system to take effect."
rebootmsg_zh="调试模式配置完成，请重启系统生效"

sucessmsg="The debug mode configuration is completed and has taken effect."
sucessmsg_zh="调试模式配置完成，已经生效"

if [ $1 == "SetDebug" ]; then
	msg="${2}msg"
fi

# 根据语言环境拼接变量名并获取对应的值
if [[ $LANG == *"zh_"* ]]; then
    # 中文环境
    msg="${msg}_zh"
fi

notify-send -i preferences-system -a dde-control-center "${!msg}"
