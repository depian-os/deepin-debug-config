#!/bin/bash
# A script to configure the module's debug log level.
# 
# Note: Please do not modify this script directly, 
# as modifying this script will invalidate this script.
set -e

pkg_name="initramfs"
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
    # 删除备份文件中的debug字段
    sed -i "${line_number}s/\(.*\) debug[^ \"]*\(.*\)/\1\2/" "$grub_file_backup_nodebug"
    # 使用 sed 命令替换指定行中的多个连续空格为一个空格
    sed -i "${line_number}s/[[:blank:]]\{2,\}/ /g" "$grub_file_backup_nodebug"
}

# Process the input argument
case "${debug}" in
  "on")
    if [[ $(grep -cE ' debug' "$grub_file") -gt 0 ]]; then
        echo "Debug logs are already disabled for ${pkg_name}."
    else
        argument="debug"
        backup_no_debug_grub_file
        # 删除备份文件中的debug字段
        sed -i "${line_number}s/\(.*\) debug[^ \"]*\(.*\)/\1\2/" "$grub_file"
        # 添加新的debug字段
        sed -i '/^GRUB_CMDLINE_LINUX_DEFAULT=\"/ s/"$/ '"$argument"' "/' "$grub_file"
        # 解决debug前的空格如果多次设置会不断增长
        sed -i '/^GRUB_CMDLINE_LINUX_DEFAULT=\"/ s/\s\{2,\}'"$argument"'/ '"$argument"'/' "$grub_file"
        # 使用 sed 命令替换指定行中的多个连续空格为一个空格
        sed -i "${line_number}s/[[:blank:]]\{2,\}/ /g" "$grub_file"
        update-grub
        echo "Debug logs are enabled for ${pkg_name}."
    fi
    ;;
  "off" | "warning")
    # Check for existing debug log options
    if [[ $(grep -cE ' debug' "$grub_file") -eq 0 ]]; then
        echo "Debug logs are already disabled for ${pkg_name}."
    else
        # if [[ -f "$grub_file_backup_nodebug" ]]; then
        #     # Restore the original service file
        #     cp "$grub_file_backup_nodebug" "$grub_file"
        # fi
        sed -i "${line_number}s/\(.*\) debug[^ \"]*\(.*\)/\1\2/" "$grub_file"
        # 使用 sed 命令替换指定行中的多个连续空格为一个空格
        sed -i "${line_number}s/[[:blank:]]\{2,\}/ /g" "$grub_file"
        update-grub
        echo "Debug logs are disabled for ${pkg_name}."
    fi
    ;;
  "debug")
    if [[ $(grep -cE ' '"$1"'' "$grub_file") -gt 0 ]]; then
        echo "Debug logs are already set to $1 for ${pkg_name}."
    else
        backup_no_debug_grub_file
        #如果旧grub文件已经有debug字段，则删除它
        sed -i "${line_number}s/\(.*\) debug[^ \"]*\(.*\)/\1\2/" "$grub_file"
        # 添加新的debug字段
        sed -i '/^GRUB_CMDLINE_LINUX_DEFAULT=\"/ s/"$/ '"$1"' "/' "$grub_file"
        # 解决debug前的空格如果多次设置会不断增长
        sed -i '/^GRUB_CMDLINE_LINUX_DEFAULT=\"/ s/\s\{2,\}'"$1"'/ '"$1"'/' "$grub_file"
        # 使用 sed 命令替换指定行中的多个连续空格为一个空格
        sed -i "${line_number}s/[[:blank:]]\{2,\}/ /g" "$grub_file"
        update-grub
        echo "${pkg_name} log level set to $1."
    fi
    ;;
  *)
    echo "Not support ${debug} parameter: $@"
    exit 1;
    ;;
esac
