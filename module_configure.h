#ifndef MODULE_CONFIGURE_H_included
#define MODULE_CONFIGURE_H_included 1
#include <libintl.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include<stdlib.h>
#include <limits.h>
#include "common.h"


//读取一个配置文件所能获取到的一个module的信息
typedef struct sub_module_cfg
{
  char *name;
  char *shell_cmd;
} sub_module_cfg;

typedef struct module_cfg
{
  char *name;
  char *type;
  int reboot;
  int sub_modules_num;
  sub_module_cfg **sub_modules;
} module_cfg;

int config_modules_set_debug_level_by_type(const char* module_type, const char *level);
int config_modules_set_debug_level_by_types(const char* module_types, const char *level);
int config_module_set_debug_level_by_module_names(const char *module_names, const char *level);
int config_modules_install_dbgpkgs(const char *module_names);
int config_module_set_debug_level_by_module_name(const char *module_name, const char *level);
int config_module_install_dbgpkgs_internal(const char *module_name);

int config_module_get_debug_level_by_type(const char *module_type, char **level);
int config_system_coredump(bool open_coredump);

int config_module_get_property_reboot(const char *name,int *reboot);
int config_module_check_log();
int init_module_cfgs(const char *dir_path);
void deinit_module_cfgs();

char **get_module_names();

bool check_can_install_dbg();

int parse_hook_json_file(char *filename, module_cfg* mdle_cfg);

#endif