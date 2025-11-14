#!/bin/bash
# A script to configure the module's debug log level.
#
# Note: Please do not modify this script directly,
# as modifying this script will invalidate this script.
set -e

readonly pkg_name="pulseaudio"
readonly conf_dir="/etc/pulse/daemon.conf.d"
readonly override_file="$conf_dir/99-deepin-debug-config.conf"

debug_level="no"

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
                debug_level=${x#debug=}
                ;;
        esac
    done


    case "${debug_level}" in
        "on" | "off" | "debug" | "info" | "notice" | "err" | "crit" | "alert" | "emerg" | "warning")
            return 0
            ;;
        *)
            echo "Not support debug level: ${debug_level}"
            return 1
            ;;
    esac
}

update_override_file() {
    local level=$1

    # Create directory in any case
    mkdir -p "$conf_dir"

    # If set to off, delete the configuration file.
    if [[ "$level" == "off" ]]; then
        if [[ -f "$override_file" ]]; then
            rm -f "$override_file"
        fi
        return
    fi

    if [[ "$level" == "on" ]]; then
        level="debug"
    fi

    # Overwrite the new log-level
    echo "log-level = $level" >"$override_file"
}

main() {
    if ! can_exec "$@"; then
        exit 1
    fi

    update_override_file "$debug_level"

    echo "Debug logs are set to '${debug_level}' for ${pkg_name}."
}

main "$@"