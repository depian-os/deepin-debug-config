#!/bin/bash
# A script to configure the module's debug log level.
# 
# Note: Please do not modify this script directly, 
# as modifying this script will invalidate this script.
set -e

pkg_name="systemd"
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
    line_number2=$(grep -n '^GRUB_CMDLINE_LINUX_DEFAULT=' "$grub_file_backup_nodebug" | cut -d':' -f1)
    # 删除备份文件中的systemd.log-level字段
    # [^ \"]*: 匹配任意不包含空格和双引号的字符，有双引号是因为systemd.log-level字段可能在行最后紧接着双引号
    # \(.*\): 这是一个捕获组，表示任意字符的一个序列，并且将其保存到捕获组中
    # /\1\2/: 这是替换的部分，\1和\2分别表示之前两个捕获组中匹配的内容。这里的意思是用第一个和第二个捕获组的内容来替换匹配到的整个字符串，即删除systemd.log-level配置。
    sed -i "${line_number2}s/\(.*\)systemd\.log-level=[^ \"]*\(.*\)/\1\2/" "$grub_file_backup_nodebug"
    # 使用 sed 命令替换指定行中的多个连续空格为一个空格
    sed -i "${line_number2}s/[[:blank:]]\{2,\}/ /g" "$grub_file_backup_nodebug"
}

# Process the input argument
case "${debug}" in
  "on")
    argument="debug"
    if [[ $(grep -cE 'systemd.log-level='"$argument"'' "$grub_file") -gt 0 ]]; then
        echo "Debug logs are already enabled for ${pkg_name}."
    else
        backup_no_debug_grub_file
        #如果旧grub文件已经有systemd.log-level字段，则删除它
        sed -i "${line_number}s/\(.*\)systemd\.log-level=[^ \"]*\(.*\)/\1\2/" "$grub_file"
        # 添加新的systemd.log-level字段
        sed -i '/^GRUB_CMDLINE_LINUX_DEFAULT=\"/ s/"$/ systemd.log-level='"$argument"' "/' "$grub_file"
        # 解决debug前的空格如果多次设置会不断增长
        sed -i '/^GRUB_CMDLINE_LINUX_DEFAULT=\"/ s/\s\{2,\}systemd.log-level='"$argument"'/ systemd.log-level='"$argument"'/' "$grub_file"
        # 使用 sed 命令替换指定行中的多个连续空格为一个空格
        sed -i "${line_number}s/[[:blank:]]\{2,\}/ /g" "$grub_file"
        update-grub
        echo "Debug logs are enabled for ${pkg_name}."
    fi
    ;;
  "off")
    # Check for existing debug log options
    if [[ $(grep -cE 'systemd.log-level=' "$grub_file") -eq 0 ]]; then
        echo "Debug logs are already disabled for ${pkg_name}."
    else
        # if [[ -f "$grub_file_backup_nodebug" ]]; then
        #     # Restore the original service file
        #     cp "$grub_file_backup_nodebug" "$grub_file"
        # fi
        sed -i "${line_number}s/\(.*\)systemd\.log-level=[^ \"]*\(.*\)/\1\2/" "$grub_file"
        # 使用 sed 命令替换指定行中的多个连续空格为一个空格
        sed -i "${line_number}s/[[:blank:]]\{2,\}/ /g" "$grub_file"
        update-grub
        echo "Debug logs are disabled for ${pkg_name}."
    fi
    ;;
  "debug" | "info" | "notice" | "warning" | "err" | "crit" | "alert" | "emerg")
    if [[ $(grep -cE 'systemd.log-level='"$1"'' "$grub_file") -gt 0 ]]; then
        echo "Debug logs are already set to $1 for ${pkg_name}."
    else
        backup_no_debug_grub_file
        #如果旧grub文件已经有systemd.log-level字段，则删除它
        sed -i "${line_number}s/\(.*\)systemd\.log-level=[^ \"]*\(.*\)/\1\2/" "$grub_file"
        # 添加新的systemd.log-level字段
        sed -i '/^GRUB_CMDLINE_LINUX_DEFAULT=\"/ s/"$/ systemd.log-level='"$1"' "/' "$grub_file"
        # 解决debug前的空格如果多次设置会不断增长
        sed -i '/^GRUB_CMDLINE_LINUX_DEFAULT=\"/ s/\s\{2,\}systemd.log-level='"$1"'/ systemd.log-level='"$1"'/' "$grub_file"
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