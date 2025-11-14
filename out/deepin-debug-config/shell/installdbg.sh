#!/bin/bash
# A script to configure the module's debug log level.
#
# Note: Please do not modify this script directly,
# as modifying this script will invalidate this script.
set -e

readonly dbgconf="/usr/share/deepin-debug-config/deepin-debug-config.d/dbg.conf"
DEBUG_PACKAGES=()

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
}

get_debug_package_list() {
    local package_name
    local package_version
    local pkg_array=()
    local debug_packages=()

    package_name="$1"
    package_version=$(dpkg -s "${package_name}" 2>/dev/null | grep '^Version:' | awk '{print $2}')
    if [ -z "$package_version" ]; then
        echo "Cannot find package_version for ${package_name}"
        return 1
    fi

    readarray -t pkg_array < <(dpkg -l | grep '^ii' | grep -w "${package_version}" | awk '{print $2}' | cut -d ':' -f 1 | grep -v dbgsym)

    echo "Number of packages: ${#pkg_array[@]}"

    for package in "${pkg_array[@]}"; do
        debug_package="${package}-dbgsym"
        if ! dpkg -l "${debug_package}" &>/dev/null; then
            debug_packages+=("${debug_package}=${package_version}")
        else
            echo "${debug_package} already installed"
        fi
    done

    DEBUG_PACKAGES=("${debug_packages[@]}")
    return 0
}

install_debug_packages() {
    local source_conf="$1"
    local apt_append_arg=""
    local package_spec package version candidate_version
    local to_install=()

    if [ -n "$source_conf" ]; then
        apt_append_arg="-c $source_conf"
    fi

    if [ ${#DEBUG_PACKAGES[@]} -eq 0 ]; then
        echo "No dbgsym packages to install."
        return 1
    fi

    for package_spec in "${DEBUG_PACKAGES[@]}"; do
        package=$(echo "$package_spec" | cut -d '=' -f 1)
        version=$(echo "$package_spec" | cut -d '=' -f 2)

        # Get all available versions
        available_versions=$(LANG=C apt-cache policy $apt_append_arg "$package" 2>/dev/null | awk '/Version table:/,0' | grep -v 'Version table:' | grep -v http | awk '{print $1}')

        while IFS= read -r candidate_version; do
            if [ "$candidate_version" == "$version" ]; then
                to_install+=("$package=$version")
                echo "Found matching version: $package=$version"
                break
            fi
        done <<< "$available_versions"
    done

    if [ ${#to_install[@]} -eq 0 ]; then
        echo "No matching dbgsym packages available to install."
        return 2
    fi

    echo "Installing dbgsym packages: ${to_install[*]}"
    if ! apt-get install -y $apt_append_arg "${to_install[@]}"; then
        echo "Failed to install some dbgsym packages."
        return 1
    fi

    echo "All dbgsym packages installed successfully."
}

pkgs_is_dev_or_sysv() {
    local package_spec package
    for package_spec in "${DEBUG_PACKAGES[@]}"; do
        package=$(echo "$package_spec" | cut -d '=' -f 1)

        if ! [[ "$package" =~ -(dev|sysv|dev-dbgsym|sysv-dbgsym)$ ]]; then
            return 1
        fi
    done

    return 0
}

install_dbg() {
    local package_name
    local get_from_source ret_val

    package_name="$1"
    get_from_source=0

    echo "Start to install dbgsym packages for ${package_name}"

    if ! get_debug_package_list "$package_name"; then
        return 1
    fi

    if [ ${#DEBUG_PACKAGES[@]} -eq 0 ]; then
        return 0
    fi

    if [ -f "$dbgconf" ]; then
        if apt update -c "$dbgconf"; then
            get_from_source=1
        fi
    fi

    if [ $get_from_source -eq 1 ]; then
        if install_debug_packages "$dbgconf"; then
            echo "install from dbg source sucess!"
            return 0
        fi
    fi
    echo "install from dbg source failed,now try to install from main source!"
    install_debug_packages
    ret_val=$?
    if [ $ret_val -ne 0 ]; then
        if [ $ret_val -eq 2 ] && pkgs_is_dev_or_sysv; then
            echo "All packages are dev or sysv packages,skip install!"
            return 0
        fi
        echo "Failed to install some dbgsym packages."
        return 1
    fi
    return 0
}

main() {
  if ! can_exec "$@"; then
    exit 1
  fi

  if ! install_dbg "$@"; then
    exit 1
  fi
}

main "$@"

