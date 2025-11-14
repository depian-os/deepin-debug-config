#!/bin/bash
# A script to configure the module's debug log level.
#
# Note: Please do not modify this script directly,
# as modifying this script will invalidate this script.
readonly pkg_name="network-manager"
readonly conf_dir="/etc/NetworkManager/conf.d"
readonly override_file="$conf_dir/99-deepin-debug-config.conf"
debug="no"

is_root() {
    local current_uid
    current_uid=$(id -u)

    if [ "$current_uid" -ne 0 ]; then
        echo "You need to have root privileges to run this script."
        return 1
    fi
    return 0
}

can_exec() {
    if ! is_root; then
        return 1
    fi
    if [ $# -ne 1 ]; then
        echo "Invalid argument"
        return 1
    fi
    for x in "$@"; do
        case $x in
            debug=*)
                debug=${x#debug=}
                ;;
        esac
    done
    case "${debug}" in
        "on" | "off" | "debug" | "warning" | "TRACE" | "DEBUG" | "INFO" | "WARN" | "ERR" | "OFF")
            return 0
            ;;
        *)
            echo "Not support ${debug} parameter: $*"
            return 1
            ;;
    esac
}

update_or_delete_log_level() {
    local level=$1
    local conf_file=$2

    if [[ "$level" == "off" ]]; then
        if [[ -f "$conf_file" ]]; then
            rm -f "$conf_file"
        fi
        return
    fi

    case "$level" in
        on|debug)
            level="TRACE"
            ;;
        warning)
            level="WARN"
            ;;
    esac

    {
        echo "[logging]"
        echo "level=$level"
    } > "$conf_file"
}

main() {
    if ! can_exec "$@"; then
        exit 1
    fi
    mkdir -p "$conf_dir"
    update_or_delete_log_level "$debug" "$override_file"

    echo "Debug logs are set to '${debug}' for ${pkg_name}."
}

main "$@"