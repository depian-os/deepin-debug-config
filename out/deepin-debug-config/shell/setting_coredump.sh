#!/bin/bash
# A script to configure the module's debug log level.
#
# Note: Please do not modify this script directly,
# as modifying this script will invalidate this script.
set -e

readonly debug_package="systemd-coredump"
readonly coredump_config="/etc/systemd/coredump.conf"
readonly coredump_config_default="/etc/systemd/coredump.conf.bak-off-debug"

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
  if [ "$1" != "on" ] && [ "$1" != "off" ]; then
    echo "Invalid argument,use 'on' or 'off"
    return 1
  fi
}

# 判断systemd-coredump是否已经安装
check_package_installed() {
    local package_name="$1"
    dpkg-query -Wf='${db:Status-Abbrev}' "$package_name" 2>/dev/null | grep -q '^ii'
}

install_coredump_pkg() {
  local package_version
  if check_package_installed "$debug_package"; then
    echo "$debug_package already installed"
    return 0
  fi
  package_version=$(dpkg -s "systemd" 2>/dev/null | grep '^Version:' | awk '{print $2}')
  if [ -z "$package_version" ]; then
    echo "Cannot find package_version for systemd"
    return 1
  fi
  apt-get install -y "${debug_package}=${package_version}"
}

# 备份coredump_config
backup_default_coredump_config() {
    if [[ ! -f "$coredump_config_default" ]]; then
        if [[ ! -f "$coredump_config" ]]; then
          echo "$coredump_config does not exist"
          return 0
        fi
        cp "$coredump_config" "$coredump_config_default"
    fi
}

mod_config() {
  backup_default_coredump_config
  if [[ ! -f "$coredump_config" ]]; then
    echo "$coredump_config does not exist"
    return 1
  fi
  case "$1" in
  "on")
    if [[ $(grep -cE '^\s*Storage=' "$coredump_config") -eq 0 ]]; then
        echo "Coredump are already enabled."
    else
      # 删除含有Storage=none的行,此时coredump默认是打开的
        sed -i "/^\s*Storage=none/d" "$coredump_config"
        # 因为systemd-coredump服务是需要core时才会启动，所以只需要重新加载配置文件
        systemctl daemon-reload || true
        echo "Coredump are enabled."
    fi
    ;;
  "off")
    if [[ $(grep -cE '^\s*Storage=none' "$coredump_config") -gt 0 ]]; then
        echo "Coredump are already disabled."
    else
        # 删除含有Storage=的行
        sed -i "/^\s*Storage=/d" "$coredump_config"
        echo "Storage=none" >> "$coredump_config"
        # 因为systemd-coredump服务是需要core时才会启动，所以只需要重新加载配置文件
        systemctl daemon-reload || true
        echo "Coredump are disabled."
    fi
    ;;
  esac
}

main() {
  if ! can_exec "$@"; then
    exit 1
  fi
  # 不管开不开启coredump都去安装systemd-coredump
  install_coredump_pkg
  mod_config "$@"
}

main "$@"