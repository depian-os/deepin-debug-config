#include "util.h"
#include "module_configure.h"
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <locale.h>
#include <unistd.h>

typedef struct arg_cfg
{
    /* data */
    char *module_types;
    char *module_names;
    char *level;
    char *coredump_arg;
    char *dbg_pkg_name;
    bool set;
    bool get;
    bool get_coredump_state;
    bool install_dbg;
    bool show_debug_level_of_type;
} arg_cfg;

bool check_g_cfg_is_valid(arg_cfg *g_cfg);
void arg_cfg_unrefp(arg_cfg **g_cfg);

void showUsage(const char *cmd) {
    printf(N_("Usage: %s [options]\n"),cmd);
    printf(N_("options:\n"));
    printf(N_("\t-h --help:\tno arg,show help\n"));
    printf(N_("\t-s --set:\tno arg, means to configure debug mode, used together with --coredump or --level\n"));
    printf(N_("\t-g --get:\tno arg, means to obtain debug mode information, used together with --coredump or --level\n"));
    printf(N_("\t-c --coredump:\toptionally receive a parameter, depending on the --set and --get options, which indicates switch coredump or get coredump status\n"));
    printf(N_("\t-i --install-dbg:\trequire one arg, which means to install the debug package of the specified pkg, example: -i systemd\n"));
    printf(N_("\t-l --level:\toptionally receive a parameter, depending on the --set and --get options, which indicates setting and getting the module log level and coredump status\n"));
    printf(N_("\t-m --module:\trequire one arg, input the name of the module to be configured, example: -m systemd\n"));
    printf("\n\n");
}

/*用于自动清理g_cfg这个结构体，配合_cleanup_一起使用：
*
* g_cfg：保存了用户输入参数的结构体；

* 函数返回值：
*
* 无*/
void arg_cfg_unrefp(arg_cfg **g_cfg) {
    arg_cfg *cfg = *g_cfg;
    if(!cfg) return;
    if(cfg->module_names)
        free(cfg->module_names);

    if(cfg->module_types)
        free(cfg->module_types);

    if(cfg->level)
        free(cfg->level);

    if(cfg->coredump_arg)
        free(cfg->coredump_arg);

    if(cfg->dbg_pkg_name)
        free(cfg->dbg_pkg_name);

    free(cfg);
}

/*检查用户输入参数是否符合有效，主要检查参数是否存在冲突：
*
* g_cfg：保存了用户输入参数的结构体；

* 函数返回值：
*
* 有效：返回 true；
* 无效：返回 false。*/
bool check_g_cfg_is_valid(arg_cfg *g_cfg)
{
    if(!g_cfg) return false;

    //指定了--set或--get后，必须使用--coredump或者--level
    if (g_cfg->set) {
        if (g_cfg->get || g_cfg->install_dbg) {
            return false;
        }

        if (g_cfg->coredump_arg) {
            if (g_cfg->module_names || g_cfg->module_types) {
                return false;
            }
        } else {
            if (!g_cfg->module_names && !g_cfg->module_types) {
                return false;
            }
            if (!g_cfg->level) {
                return false;
            }
        }
    } else if (g_cfg->get) {
        if (g_cfg->install_dbg) {
            return false;
        }

        if (g_cfg->get_coredump_state) {
            if (g_cfg->module_names || g_cfg->module_types) {
                return false;
            }
        } else {
            if (g_cfg->module_names || g_cfg->module_types) {
                if (!g_cfg->show_debug_level_of_type) {
                    return false;
                }
            }
        }
    } else {
        if (!g_cfg->install_dbg || !g_cfg->dbg_pkg_name) {
            return false;
        }
    }

    return true;
}

int main(int argc,char *argv[]) {
    _cleanup_(arg_cfg_unrefp) arg_cfg *g_cfg = NULL;
    int argidx = 1;

    //configure locale
    setlocale (LC_ALL, "");
    bindtextdomain (PACKAGE, LOCALEDIR);
    textdomain (PACKAGE);

    if(argc == 1){
        showUsage(PACKAGE);
        return 0;
    }

    g_cfg = (arg_cfg*) malloc(sizeof(arg_cfg));

    if(!g_cfg){
        fprintf(stderr,N_("Error: NOMEMORY:%m\n"));
        goto fail;
    }
    memset(g_cfg, 0, sizeof(arg_cfg));

    static const struct option longopts[] = {
        { "module",	      required_argument, NULL, 'm' },
        { "level",	      no_argument, NULL, 'l' },
        { "coredump",       no_argument, NULL, 'c' },
        { "install-dbg",    required_argument, NULL, 'i' },
        { "help",	      no_argument,       NULL, 'h' },
        { "set",	      no_argument,       NULL, 's' },
        { "get",	      no_argument,       NULL, 'g' },
        { NULL, 0, NULL, 0 }
	};

#if 0
    //这里打算加一条打印输入参数和执行时间到/var/log/下日志文件
    //因为本工具需要修改系统配置文件，可能有一定的风险，需要记录
    //执行本工具的记录，看用户执行过什么操作，方便以后分析问题
#endif

    int c;
    while ((c = getopt_long (argc, argv,
			    "t:m:lci:hgs", longopts, NULL)) != -1) {
        ++argidx;
        switch (c) {
            case 's':
                if(g_cfg->get) {
                    fprintf(stderr, N_("Error: Invalid argument, --set and --get cannot be used together\n"));
                    goto fail;
                }
                g_cfg->set = true;
                break;
            case 'g':
                if(g_cfg->set) {
                    fprintf(stderr, N_("Error: Invalid argument, --set and --get cannot be used together\n"));
                    goto fail;
                }
                g_cfg->get = true;
                break;
            case 'm':
                g_cfg->module_names = strdup(optarg);
                ++argidx;
                break;
            case 'l':
                if (g_cfg->set) {
                    if (argidx < argc) {
                        g_cfg->level = strdup(argv[argidx]);
                        ++argidx;
                    }
                    else {
                        fprintf(stderr, N_("Error: %s missing parameter\n"), "-l --level");
                        fprintf(stderr, N_("Try '%s --help' for more information.\n"), \
                        PACKAGE);
                        goto fail;
                    }
                } else if(g_cfg->get) {
                    g_cfg->show_debug_level_of_type = true;
                } else {
                    fprintf(stderr, N_("Error: -l --level should be used with --set or --get\n"));
                    fprintf(stderr, N_("Try '%s --help' for more information.\n"), \
                        PACKAGE);
                    goto fail;
                }
                break;
            case 'c':
                if(g_cfg->set){
                    if (argidx < argc) {
                        g_cfg->coredump_arg = strdup(argv[argidx]);
                        ++argidx;
                    }
                    else {
                        fprintf(stderr, N_("Error: %s missing parameter\n"), "-c --coredump");
                        fprintf(stderr, N_("Try '%s --help' for more information.\n"), \
                        PACKAGE);
                        goto fail;
                    }
                    if(!(strcmp(g_cfg->coredump_arg, "on")==0 || strcmp(g_cfg->coredump_arg, "off")==0)){
                        fprintf(stdout, N_("Error: Invalid argument for -c or --coredump which you should use with \"on\" or \"off\"\n"));
                        fprintf(stderr, N_("Try '%s --help' for more information.\n"), \
                            PACKAGE);
                        goto fail;
                    }
                } else if(g_cfg->get) {
                    g_cfg->get_coredump_state = true;
                } else {
                    fprintf(stderr, N_("Error: -c --coredump should be used with --set or --get\n"));
                    fprintf(stderr, N_("Try '%s --help' for more information.\n"), \
                        PACKAGE);
                    goto fail;
                }
                break;
            case 'i':
                g_cfg->dbg_pkg_name = strdup(optarg);
                ++argidx;
                g_cfg->install_dbg = true;
                break;
            case 'h':
                showUsage(Basename (argv[0]));
                return OK;
		    default:
                fprintf(stderr, N_("Try '%s --help' for more information.\n"), \
                        PACKAGE);
                goto fail;
        }
        if(argidx < argc && argidx != optind){
            fprintf(stderr,N_("Error: Invalid parameter, please check parameters\n"));
            fprintf(stderr, N_("Try '%s --help' for more information.\n"), \
                        PACKAGE);
            goto fail;
        }
    }

    //加载json配置文件
    int r = init_module_cfgs(MODULES_DEBUG_CONFIG_PATH);
    if (r < 0) {
        goto fail;
    }

    //检查参数是否符合有效
    if(!check_g_cfg_is_valid(g_cfg)) {
        fprintf(stderr, N_("Try '%s --help' for more information.\n"), \
            PACKAGE);
        goto fail;
    }

    //打印某个类型的模块的开关状态
    if(g_cfg->show_debug_level_of_type) {
        char *debug_level = NULL;
        if (!g_cfg->module_names) {
            fprintf(stderr, N_("Error: -l --level should be used with -m or --module\n"));
            goto fail;
        }
        r = config_module_get_debug_level_by_type(g_cfg->module_names, &debug_level);
        if(r < 0) {
            goto fail;
        }
        fprintf(stdout,"%s\n", debug_level);
        free(debug_level);
        goto success;
    }

    //打印coredump的打开状态
    if(g_cfg->get_coredump_state) {
        char *coredump_state = NULL;
        r = config_module_get_debug_level_by_type("coredump", &coredump_state);
        if(r < 0){
            fprintf(stderr, N_("Error: Failed to get coredump state.\n"));
            goto fail;
        }
        fprintf(stdout,"%s\n", coredump_state);
        free(coredump_state);
        goto success;
    }

    if (g_cfg->get) {
        char **names = get_module_names();

        printf("Support module names:\n");
        for (int i = 0; names[i] != NULL; i++) {
            printf("\t%s\n", names[i]);
        }
        free(names);
        goto success;
    }

    /*下面执行的操作都需要root权限才能运行*/
    if(getuid() != 0) {
        fprintf (stderr,
                    N_("Error: %s: Permission denied.\n"),
                    Basename (argv[0]));
        goto fail;
    }


    //当-m,--module参数被传入，进行相应的配置
    if (g_cfg->set) {
        if (g_cfg->module_types) {
            r = config_modules_set_debug_level_by_types(g_cfg->module_types, g_cfg->level);
            config_module_check_log();
            if(r < 0)
                goto fail;
        } else if (g_cfg->module_names) {
            r = config_module_set_debug_level_by_module_names(g_cfg->module_names, g_cfg->level);
            config_module_check_log();
            if(r < 0)
                goto fail;
        } else if (g_cfg->coredump_arg) {
            bool open_coredump = strcmp(g_cfg->coredump_arg,"on")==0?true:false;
            r = config_system_coredump(open_coredump);
            if(r < 0)
                goto fail;
        }
    } else if (g_cfg->install_dbg) {
        //安装调试包
        r = config_modules_install_dbgpkgs(g_cfg->dbg_pkg_name);
        if(r < 0)
            goto fail;
    }

success:
    fprintf(stdout, N_("Done.\n"));
    return 0;

fail:
    fprintf(stderr, N_("Failed.\n"));
    deinit_module_cfgs();
    return ERROR;
}