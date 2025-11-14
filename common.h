#ifndef COMMON_H_included
#define COMMON_H_included 1

#define PACKAGE "deepin-debug-config"
#define LOCALEDIR "/usr/share/locale/"

#define INSTALL_DBGPKG_SHELL_PATH "/usr/share/deepin-debug-config/shell/installdbg.sh"
#define CONFIG_COREDUMP_SHELL_PATH "/usr/share/deepin-debug-config/shell/setting_coredump.sh"
#define CONFIG_SHELL_PATH "/usr/share/deepin-debug-config/shell"
#define MODULES_DEBUG_CONFIG_PATH "/usr/share/deepin-debug-config/deepin-debug-config.d"
#define MODULES_DEBUG_LEVELS_PATH "/var/lib/deepin-debug-config/deepin-debug-levels.cfg"
#define DEFAULT_CORE_PATH "/var/lib/systemd/coredump/"

#define CONFIG_SHELL_IN_CODE_PATH "out/deepin-debug-config/shell"
#define MD5_DIGEST_LENGTH 16
#define MAX_PARAMETER_RANGES_SIZE 128
#define MAX_MODULE_CFG_STRING_SIZE 128

#define JOURNAL_LOG_ON_CONF "/usr/share/deepin-debug-config/00-on-journald.conf"
#define JOURNAL_LOG_OFF_CONF "/usr/share/deepin-debug-config/00-off-journald.conf"
#define JOURNAL_LOG_ON_INSTALLED "/etc/systemd/journald.conf.d/00-on-deepin-config-debug.conf"
#define JOURNAL_LOG_OFF_INSTALLED "/etc/systemd/journald.conf.d/00-off-deepin-config-debug.conf"
#define JOURNAL_LOG_PATH "/etc/systemd/journald.conf.d/"
#define OK      0
#define ERROR   errno?-errno:-1
#endif
