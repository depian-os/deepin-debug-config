#!/bin/bash
# A script to configure the module's debug log level.
#
# Note: Please do not modify this script directly,
# as modifying this script will invalidate this script.
set -e

readonly pkg_name="bluetooth"
readonly deepin_debug_config_dir="/etc/deepin/deepin-debug-config"
readonly bluetooth_debug_file="${deepin_debug_config_dir}/deepin-bluetoothd.conf"

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
        "on" | "off" | "debug" | "warning")
            return 0
            ;;
        *)
            echo "Usage: $0 debug={on|off|debug|warning}"
            return 1
            ;;
    esac
}

update_or_delete_bluetooth_debug_config() {
    local level=$1

    # Delete existing conf files
    if [ -f "${bluetooth_debug_file}" ]; then
        rm "${bluetooth_debug_file}"
    fi

    mkdir -p "${deepin_debug_config_dir}"

    case "${level}" in
        "on" | "debug")
            echo "OParameter=-d" > "${bluetooth_debug_file}"
            echo "Debug logs are enabled for ${pkg_name}."
            ;;

        "off" | "warning")
            echo "Debug logs are disabled for ${pkg_name}."
            ;;
    esac
}

main() {
    if ! can_exec "$@"; then
        exit 1
    fi

    update_or_delete_bluetooth_debug_config "$debug"
}

main "$@"
