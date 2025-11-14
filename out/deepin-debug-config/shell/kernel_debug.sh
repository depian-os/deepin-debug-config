#!/bin/bash
# A script to configure the module's debug log level.
# 
# Note: Please do not modify this script directly, 
# as modifying this script will invalidate this script.
set -e

pkg_name="kernel"
debug="no"

# 获取当前用户的UID
current_uid=$(id -u)

# 判断是否是Root权限
if [ "$current_uid" -ne 0 ]; then
  echo "You need to have root privileges to run this script."
  exit 1
fi

grub_file="/etc/default/grub"
grub_file_backup_nodebug="/etc/default/grub.bak-off-debug"

if [ ! -f  "$grub_file" ]; then
    echo "ERROR: Cannot find ${grub_file}"
    exit 1
fi


# 找到GRUB_CMDLINE_LINUX_DEFAULT所在的行号
line_number=$(grep -n '^GRUB_CMDLINE_LINUX_DEFAULT=' "$grub_file" | cut -d':' -f1)
if [ ! -n "$line_number" ]; then
    echo "ERROR: Cannot find GRUB_CMDLINE_LINUX_DEFAULT in ${grub_file}"
    exit 1
fi

for x in "$@"; do
        case $x in
        debug=*)
                debug=${x#debug=}
                ;;
        esac
done

# 备份grub_file
backup_no_debug_grub_file() {
    if [[ ! -f "$grub_file_backup_nodebug" ]]; then
        cp "$grub_file" "$grub_file_backup_nodebug"
    fi
    sed -i "${line_number}s/\(.*\)loglevel=[^ \"]*\(.*\)/\1\2/" "$grub_file_backup_nodebug"
    # 使用 sed 命令替换指定行中的多个连续空格为一个空格
    sed -i "${line_number}s/[[:blank:]]\{2,\}/ /g" "$grub_file_backup_nodebug"
}

# Process the input argument
case "${debug}" in
  "on" | "debug")
    if [[ $(grep -cE 'loglevel=7' "$grub_file") -gt 0 ]]; then
        echo "Debug logs are already enabled for ${pkg_name}."
    else
        argument="7"
        backup_no_debug_grub_file
        #如果旧grub文件已经有loglevel=字段，则删除它
        sed -i "${line_number}s/\(.*\)loglevel=[^ \"]*\(.*\)/\1\2/" "$grub_file"
        # 添加新的loglevel字段
        sed -i '/^GRUB_CMDLINE_LINUX_DEFAULT=\"/ s/"$/ loglevel='"$argument"' "/' "$grub_file"
        # 解决debug前的空格如果多次设置会不断增长
        sed -i '/^GRUB_CMDLINE_LINUX_DEFAULT=\"/ s/\s\{2,\}loglevel='"$argument"'/ loglevel='"$argument"'/' "$grub_file"
        # 使用 sed 命令替换指定行中的多个连续空格为一个空格
        sed -i "${line_number}s/[[:blank:]]\{2,\}/ /g" "$grub_file"
        update-grub
        echo "Debug logs are enabled for ${pkg_name}."
    fi
    ;;
  "off" | "warning")
    # Check for existing debug log options
    if [[ $(grep -cE 'loglevel=' "$grub_file") -eq 0 ]]; then
        echo "Debug logs are already disabled for ${pkg_name}."
    else
        # if [[ -f "$grub_file_backup_nodebug" ]]; then
        #     # Restore the original service file
        #     cp "$grub_file_backup_nodebug" "$grub_file"
        # fi
        sed -i "${line_number}s/\(.*\)loglevel=[^ \"]*\(.*\)/\1\2/" "$grub_file"
        # 使用 sed 命令替换指定行中的多个连续空格为一个空格
        sed -i "${line_number}s/[[:blank:]]\{2,\}/ /g" "$grub_file"
        update-grub
        echo "Debug logs are disabled for ${pkg_name}."
    fi
    ;;
  "7" | "6" | "5" | "4" | "3" | "2" | "1" | "0")
    if [[ $(grep -cE 'loglevel='"$1"'' "$grub_file") -gt 0 ]]; then
        echo "Debug logs are already set to '$1' for ${pkg_name}."
    else
        backup_no_debug_grub_file
        #如果旧grub文件已经有loglevel=字段，则删除它
        sed -i "${line_number}s/\(.*\)loglevel=[^ \"]*\(.*\)/\1\2/" "$grub_file"
        # 添加新的loglevel字段，这里有个小问题
        sed -i '/^GRUB_CMDLINE_LINUX_DEFAULT=\"/ s/"$/ loglevel='"$1"' "/' "$grub_file"
        # 解决debug前的空格如果多次设置会不断增长
        sed -i '/^GRUB_CMDLINE_LINUX_DEFAULT=\"/ s/\s\{2,\}loglevel='"$1"'/ loglevel='"$1"'/' "$grub_file"
        # 使用 sed 命令替换指定行中的多个连续空格为一个空格
        sed -i "${line_number}s/[[:blank:]]\{2,\}/ /g" "$grub_file"
        update-grub
        echo "Debug logs are set to '$1' for ${pkg_name}."
    fi
    ;;
  *)
    echo "Not support ${debug} parameter: $@"
    exit 1;
    ;;
esac
