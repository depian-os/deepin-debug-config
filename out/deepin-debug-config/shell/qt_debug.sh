#!/bin/bash
# A script to configure the module's debug log level.
# 
# Note: Please do not modify this script directly, 
# as modifying this script will invalidate this script.
set -e

pkg_name="qt"
debug="no"

# 获取当前用户的UID
current_uid=$(id -u)

# 判断是否是Root权限
if [ "$current_uid" -ne 0 ]; then
  echo "You need to have root privileges to run this script."
  exit 1
fi

qt_env_file="/etc/X11/Xsession.d/00deepin-dde-env"
override_file="/etc/X11/Xsession.d/98-deepin-debug-config-qt"

if [ ! -f  "$qt_env_file" ]; then
    echo "ERROR: Cannot find ${qt_env_file}"
    exit 1
fi

# 找到QT_LOGGING_RULES所在的行号
line_number=$(grep -n 'QT_LOGGING_RULES' "$qt_env_file" | cut -d':' -f1)
if [ ! -n "$line_number" ]; then
    echo "ERROR: Cannot find QT_LOGGING_RULES in ${qt_env_file}"
    exit 1
fi

for x in "$@"; do
    case $x in
        debug=*)
            debug=${x#debug=}
            ;;
    esac
done



# 确保override文件存在
mkdir -p "$(dirname "$override_file")"
if [ ! -f "$override_file" ]; then
    touch "$override_file"
else
    > "$override_file"
fi


case "${debug}" in
    "on" | "debug")
        echo "export QT_LOGGING_RULES=\"*.debug=true;*.info=true\"" > "$override_file"
        echo "Debug logs are enabled for ${pkg_name}."
        ;;
    "off" | "warning")
        if [ -f "$override_file" ]; then
            rm -f "$override_file"
            # sed -i '/^export QT_LOGGING_RULES=/d' "$override_file"
            # if [ ! -s "$override_file" ]; then
            #     rm -f "$override_file"
            #     echo "Debug logs are disabled for ${pkg_name}."
            # else
            #     echo "Debug logs are disabled for ${pkg_name}."
            # fi
        fi
        ;;
    *)
        echo "Not support ${debug} parameter: $@"
        exit 1
        ;;
esac
