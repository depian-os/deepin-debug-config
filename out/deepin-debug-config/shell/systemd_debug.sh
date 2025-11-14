#!/bin/bash
# A script to configure the module's debug log level.
#
# Note: Please do not modify this script directly,
# as modifying this script will invalidate this script.
set -e

readonly pkg_name="systemd"
readonly systemd_dir="/etc/systemd/system.conf.d"
readonly user_dir="/etc/systemd/user.conf.d"
readonly systemd_override_file="$systemd_dir/98-deepin-debug-systemd_system.conf"
readonly user_override_file="$user_dir/98-deepin-debug-systemd_user.conf"
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
        "on" | "off" | "warning" | "debug" | "info" | "notice" | "err" | "crit" | "alert" | "emerg")
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
    local override_file=$2

    if [[ "$level" == "off" ]]; then
        if [[ -f "$override_file" ]]; then
            rm -f "$override_file"
        fi
        return
    fi

    if [[ "$level" == "on" ]]; then
        level="debug"
    fi

    echo "[Manager]" > "$override_file"
    echo "LogLevel=$level" >> "$override_file"
}

main() {
    if ! can_exec "$@"; then
        exit 1
    fi
    mkdir -p "$systemd_dir" "$user_dir"
    update_or_delete_log_level "$debug" "$systemd_override_file"
    update_or_delete_log_level "$debug" "$user_override_file"

    echo "Debug logs are set to '${debug}' for ${pkg_name}."
}

main "$@"