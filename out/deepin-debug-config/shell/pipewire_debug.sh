#!/bin/bash
# A script to configure the module's debug log level.
#
# Note: Please do not modify this script directly,
# as modifying this script will invalidate this script.
set -e

readonly pkg_name="pipewire"
readonly conf_dir_pulse="/etc/pipewire/pipewire-pulse.conf.d"
readonly conf_dir_core="/etc/pipewire/pipewire.conf.d"
readonly override_file_pulse="$conf_dir_pulse/deepin-debug.conf"
readonly override_file_core="$conf_dir_core/deepin-debug.conf"

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
        "on" | "off" | "debug" | "err" | "warning" | "info" | "trace")
            return 0
            ;;
        *)
            echo "Usage: $0 debug={on|off|debug|err|warning|info|trace}"
            return 1
            ;;
    esac
}

map_debug_level_to_number() {
    local level=$1
    case "$level" in
        "on") echo 4 ;;
        "debug") echo 4 ;;
        "err") echo 1 ;;
        "warning") echo 2 ;;
        "info") echo 3 ;;
        "trace") echo 5 ;;
    esac
}

write_debug_config() {
    local file_path=$1
    local numeric_level=$2

    cat >"$file_path" <<EOF
context.properties = {
    log.level = $numeric_level
}
EOF
}

remove_debug_config() {
    local file_path=$1
    if [[ -f "$file_path" ]]; then
        rm -f "$file_path"
    fi
}

update_or_delete_pipewire_debug_config() {
    local level=$1

    # Create a directory in any case
    mkdir -p "$conf_dir_pulse"
    mkdir -p "$conf_dir_core"

    case "$level" in
        "off")
            remove_debug_config "$override_file_pulse"
            remove_debug_config "$override_file_core"
            echo "Debug logs are disabled for ${pkg_name}."
            ;;
        *)
            local numeric_level
            numeric_level=$(map_debug_level_to_number "$level")

            write_debug_config "$override_file_pulse" "$numeric_level"
            write_debug_config "$override_file_core" "$numeric_level"

            echo "Debug logs are set to '${level}' for ${pkg_name}."
            ;;
    esac
}

main() {
    if ! can_exec "$@"; then
        exit 1
    fi

    update_or_delete_pipewire_debug_config "$debug_level"
}

main "$@"