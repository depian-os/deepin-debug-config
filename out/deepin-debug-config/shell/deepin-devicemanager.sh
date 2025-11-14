#!/bin/bash

# A script to configure the module's debug log level.
#
# Note: Please do not modify this script directly,
# as modifying this script will invalidate this script.
set -e

app_id="org.deepin.devicemanager"
configuration_id="org.deepin.dtk.loggingrules"

# 获取当前用户的UID
# current_uid=$(id -u)

# 判断是否是Root权限
# if [ "$current_uid" -ne 0 ]; then
#   echo "You need to have root privileges to run this script."
#   exit 1
# fi

for x in "$@"; do
	case $x in
	debug=*)
		debug=${x#debug=}
		;;
	state)
		state=y
		;;
	test)
		test=y
		;;
	esac
done


case "${debug}" in
"debug")
	dde-dconfig --set -a $app_id -r $configuration_id -k rules -v "*.warning=false;*.debug=true;*.info=false" #按需改变分类、添加子模块规则
	exit 0
	;;
"info")
	dde-dconfig --set -a $app_id -r $configuration_id -k rules -v "*.warning=false;*.debug=false;*.info=true" #按需改变分类、添加子模块规则
	exit 0
	;;
"warning")
	dde-dconfig --set -a $app_id -r $configuration_id -k rules -v "*.debug=false;*.info=false;*.warning=true" #按需改变分类、添加子模块规则
	exit 0
	;;
esac

