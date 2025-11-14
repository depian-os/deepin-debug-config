#!/bin/bash
# A script to configure the module's debug log level.
# 
# Note: Please do not modify this script directly, 
# as modifying this script will invalidate this script.
set -e

pkg_name="dde"
debug="no"

dde_env_file="/etc/profile.d/deepin-system-upgrade-tool.sh"
dde_env_override_dir="/etc/X11/Xsession.d"
dde_env_override_file="$dde_env_override_dir/98-deepin-debug-config-dde"

# 确保 override 文件的目录存在
mkdir -p "$dde_env_override_dir"

for x in "$@"; do
        case $x in
        debug=*)
                debug=${x#debug=}
                ;;
        esac
done


# 检查传入的参数
case "${debug}" in
    "on")
        # 如果传入的参数是 "on"
        # 删除已存在的 system-upgrade-tool.sh 脚本
        if [ -f "${dde_env_file}" ]; then
            rm "${dde_env_file}"
        fi
        # 创建新的 system-upgrade-tool.sh 脚本并写入内
        echo '#!/bin/bash' > "${dde_env_file}"
        echo 'export DDE_DEBUG_LEVEL=debug' >> "${dde_env_file}"
        if [ -f "$dde_env_override_file" ]; then
            # 文件存在，检查是否包含 DDE_DEBUG_LEVEL
            if grep -qE '^export DDE_DEBUG_LEVEL=.*' "$dde_env_override_file"; then
                sed -i 's/^export DDE_DEBUG_LEVEL=.*/export DDE_DEBUG_LEVEL=debug/' "$dde_env_override_file"
            else
                echo 'export DDE_DEBUG_LEVEL=debug' >> "$dde_env_override_file"
            fi
        else
            # 文件不存在，创建并写入
            echo 'export DDE_DEBUG_LEVEL=debug' > "$dde_env_override_file"
        fi
        echo "Debug logs are enabled for ${pkg_name}."
        ;;
    "off" | "warning")
        # 如果传入的参数是 "off"
        # 删除已存在的 system-upgrade-tool.sh 脚本
        if [ -f "${dde_env_file}" ]; then
            rm "${dde_env_file}"
            echo "Removed ${dde_env_file}"
        fi
        echo '#!/bin/bash' > "${dde_env_file}"
        echo 'export DDE_DEBUG_LEVEL=warning' >> "${dde_env_file}"

        # 如果传入的参数是 "off"
        if [ -f "$dde_env_override_file" ]; then
            rm -rf "${dde_env_override_file}"
        #     # 文件存在，检查是否包含 DDE_DEBUG_LEVEL
        #     if grep -qE '^export DDE_DEBUG_LEVEL=.*' "$dde_env_override_file"; then
        #         sed -i 's/^export DDE_DEBUG_LEVEL=.*/export DDE_DEBUG_LEVEL=warning/' "$dde_env_override_file"
        #     else
        #         # 不包含 DDE_DEBUG_LEVEL，检查文件是否为空
        #         echo 'export DDE_DEBUG_LEVEL=warning' >> "$dde_env_override_file"
        #     fi
        # else
        #     echo 'export DDE_DEBUG_LEVEL=warning' > "$dde_env_override_file"
        fi
        ;;
    "debug" | "info" | "error" | "fatal" | "disable")
        # 如果传入的参数是 "warning"
        # 删除已存在的 system-upgrade-tool.sh 脚本（如果存在）
        if [ -f "${dde_env_file}" ]; then
            rm "${dde_env_file}"
        fi
        # 创建新的 system-upgrade-tool.sh 脚本并写入内容
        echo '#!/bin/bash' > "${dde_env_file}"
        echo "export DDE_DEBUG_LEVEL=${debug}" >> "${dde_env_file}"

        if [ -f "$dde_env_override_file" ]; then
            # 文件存在，检查是否包含 DDE_DEBUG_LEVEL
            if grep -qE '^export DDE_DEBUG_LEVEL=.*' "$dde_env_override_file"; then
                sed -i "s/^export DDE_DEBUG_LEVEL=.*/export DDE_DEBUG_LEVEL=${debug}/" "$dde_env_override_file"
            else
                # 不包含 DDE_DEBUG_LEVEL，检查文件是否为空
                echo "export DDE_DEBUG_LEVEL=${debug}" >> "$dde_env_override_file"
            fi
        else
            echo "export DDE_DEBUG_LEVEL=${debug}" > "$dde_env_override_file"
        fi
        ;;
    *)
        # 如果传入的参数不是 "on", "off", 或 "warning"
        echo "Usage: $debug {on|off|debug|info|warning|error|fatal|disable}"
        exit 1
        ;;
esac
