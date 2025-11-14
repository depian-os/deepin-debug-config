#include "module_configure.h"
#include "util.h"
#include "cJSON.h"
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <openssl/sha.h>
#include <stdlib.h>

#include <glib.h>

#define MAX_KEY_LEN 1024
#define MAX_LINE_LEN 512
#define LINE_BUF_SIZE 512
extern struct set *S_modules, *S_types;
extern unsigned char config_sha256[][SHA256_DIGEST_LENGTH];
extern int config_sha256_count;

GHashTable *g_module_cfgs = NULL;

static void free_submodule_cfg(sub_module_cfg *p_cfg) {
    if (!p_cfg) return;
    if (p_cfg->name) free(p_cfg->name);
    if (p_cfg->shell_cmd) free(p_cfg->shell_cmd);
}

static void free_module_cfg(void *t_pointer) {
    module_cfg *p_cfg = t_pointer;
    if (!p_cfg) return;

    if (p_cfg->name) free(p_cfg->name);
    if (p_cfg->type) free(p_cfg->type);
    if (p_cfg->sub_modules) {
        int i=0;
        for (i=0;p_cfg->sub_modules[i];i++) {
            free_submodule_cfg(p_cfg->sub_modules[i]);
        }
        free(p_cfg->sub_modules);
    }
}

/*加载json配置文件目录下的json配置文件：
*
* dir_path：json配置文件目录；
* module_cfgs：所有的模块的配置数组，用于保存解析好的配置文件；
* valid_module_cfgs_count：用于保存解析好的配置文件的数目。
* 函数返回值：
*
* 成功：返回 0；
* 失败：返回 ERR_RET。*/
int init_module_cfgs(const char *dir_path)
{
    DIR  *pdir = NULL;
    module_cfg *mdle_cfg = NULL;
    struct dirent *pdirent;
    int ret = 0;
    char buff[PATH_MAX] = {0};

    if (g_module_cfgs)
        return OK;

    pdir=opendir(dir_path);
    if(pdir==NULL)
    {
        fprintf(stderr, N_("Error: Failed to open dir %s, err: %m\n"), dir_path);
        return ERROR;
    }
    g_module_cfgs = g_hash_table_new_full (g_str_hash,
                                            g_str_equal,
                                            g_free,
                                            free_module_cfg);

    for(pdirent= readdir(pdir); pdirent!=NULL; pdirent=readdir(pdir))
    {
        if( strncmp(pdirent->d_name,".", 3)==0 || strncmp(pdirent->d_name,"..", 3)==0 )
            continue;
        snprintf( buff, PATH_MAX, "%s/%s", dir_path, pdirent->d_name);
        struct stat sbuf;
        if(lstat(buff, &sbuf) == -1)
        {
            ret = ERROR;
            fprintf(stderr,N_("Error: lstat error %s\n"),buff);
            goto ERRRET;
        }

        if( S_ISREG(sbuf.st_mode) && str_endsWith(buff, ".json"))
        {
            mdle_cfg = (module_cfg*)malloc(sizeof(module_cfg));
            assert(mdle_cfg);
            memset(mdle_cfg, 0, sizeof(module_cfg));

            ret = parse_hook_json_file(buff, mdle_cfg);
            if(ret < 0) {
                free_module_cfg(mdle_cfg);
                fprintf(stderr,N_("Error: cann't paste %s\n"),pdirent->d_name);
                continue;
            }
            g_hash_table_insert (g_module_cfgs, g_strdup (mdle_cfg->name), mdle_cfg);
        }
    }
    closedir(pdir);
    return OK;
ERRRET:
    g_hash_table_destroy(g_module_cfgs);
    g_module_cfgs = NULL;
    closedir(pdir);
    return ret;
}

void deinit_module_cfgs() {
    if (g_module_cfgs) {
        g_hash_table_destroy(g_module_cfgs);
        g_module_cfgs = NULL;
    }
}

static void collect_module_names(gpointer key, gpointer value, gpointer user_data) {
    GPtrArray *names = (GPtrArray *)user_data;
    module_cfg *cfg = (module_cfg *)value;
    if (cfg && cfg->name) {
        g_ptr_array_add(names, g_strdup(cfg->name));
    }
}

char **get_module_names() {
    if (!g_module_cfgs) {
        return NULL;
    }

    GPtrArray *names = g_ptr_array_new();

    g_hash_table_foreach(g_module_cfgs, collect_module_names, names);
    g_ptr_array_add(names, NULL);
    char **result = (char **)g_ptr_array_free(names, FALSE);
    return result;
}

static int create_dir(const char *path) {
    assert(path);
    char* dir = NULL, *p = NULL;
    struct stat st = {0};
    int ret = OK;

    if (stat(path, &st) == 0)
        return OK;

    dir = strdup(path);
    p = dir;
    for (;*p && (*p=='/'||*p==' '||*p=='\t');p++);
    while (*p) {
        if (*p == '/') {
            *p = '\0';  // 临时截断字符串，便于逐级检查目录是否存在
            if (stat(dir, &st) != 0) {
                fprintf(stderr, "create directory: %s\n", dir);
                if (mkdir(dir, 0775) != 0) {
                    ret = ERROR;
                    fprintf(stderr, "Failed to create directory: %s\n", dir);
                    free(dir);
                    return ret;
                }
            }
            *p = '/';  // 恢复截断的字符串
        }
        p++;
    }

    free(dir);
    return OK;
}

static int copy_file(const char *src,const char *dest) {
        const int BUFFER_SIZE = 4096;
        FILE *fpsrc = NULL, *fpdest = NULL;
        char buffer[BUFFER_SIZE];
        size_t bytesRead,bytesWrite;
        int ret = OK;


        assert(src&&dest);

        ret = create_dir(JOURNAL_LOG_PATH);
        if (ret) return ret;

        fpsrc = fopen(src,"r");
        fpdest = fopen(dest,"w+");

        if (fpsrc == NULL || fpdest == NULL) {
                return ERROR;
        }

        while ((bytesRead = fread(buffer, 1, sizeof(buffer), fpsrc)) > 0) {
                bytesWrite = fwrite(buffer, 1, bytesRead, fpdest);
                if (bytesWrite != bytesRead) {
                        ret = ERROR;
                        break;
                }
        }

        // 关闭文件
        fclose(fpsrc);
        fclose(fpdest);
        return ret;
}

static int set_journal_log (bool flag) {
        const char *src = NULL, *dest = NULL, *need_del = NULL;
        int ret = OK;

        if (flag) {
                src = JOURNAL_LOG_ON_CONF;
                dest = JOURNAL_LOG_ON_INSTALLED;
                need_del = JOURNAL_LOG_OFF_INSTALLED;
        } else {
                src = JOURNAL_LOG_OFF_CONF;
                dest = JOURNAL_LOG_OFF_INSTALLED;
                need_del = JOURNAL_LOG_ON_INSTALLED;
        }

        if (access(need_del,F_OK)==0) {
                ret = remove(need_del);
                fprintf(stdout, N_("remove %s %s.\n"),need_del,(ret==OK)?"ok":"fail");
                if (ret) return ret;
        }
        if (access(dest,F_OK)==0)
                return OK;
        ret = copy_file(src,dest);
        fprintf(stdout, N_("copyfile %s to %s %s.\n"),src,dest,(ret==OK)?"ok":"fail");
        return ret;
}

/*用于打印指定模块类型的日志开关状态：
*
* module_type：模块类型；
* debug_on：bool指针，作为返回值；
*
* 函数返回值：
* 成功：返回 0；
* 失败：返回 ERR_RET。*/
int config_module_get_debug_level_by_type(const char *module_type, char **level) {
    FILE *fp;
    char *filename = MODULES_DEBUG_LEVELS_PATH;
    char *line = NULL, *pos = NULL, *comment_pos = NULL, *trimmed_line = NULL;
    char key[LINE_BUF_SIZE],value[LINE_BUF_SIZE];//key和value都默认为字符串
    int ret = 0;
    size_t len;
    size_t read;

    assert(level);
    *level = NULL;

    if (access(filename, F_OK) == -1) {
        //MODULES_DEBUG_LEVELS_PATH不存在
        // 认为所有模块的debug状态是关闭的
        *level = strdup("off");
        return OK;
    }

    fp = fopen(filename, "r");
    if (fp == NULL) {
        ret = ERROR;
        fprintf(stderr, N_("Error: %s,failed :%m\n"), filename);
        return ret;
    }

    while ((read = getline(&line, &len, fp)) != -1) {
        //忽略注释，包括键值对后的注释
        comment_pos = strchr(line, '#');
        if (comment_pos != NULL)
            *comment_pos = '\0'; // 截断注释部分

        // 去除行首和行尾的空格
        trimmed_line = strtok(line, " \t\r\n");
        if (trimmed_line == NULL)
            continue; // 跳过空行

        //通过判断是否有"="来判断是不是一个有效的配置，如果之后配置文件
        //有其他格式请修改这里的代码
        pos = strchr(trimmed_line, '=');
        if (pos == NULL)//该行不是一个有效的配置
            continue;
        //获取并处理key和value，如果要处理其他的配置请在这里修改
        if (sscanf(trimmed_line, "%255[^=]=%255[^\n]", key, value) == 2)
        {
            if(strncmp(module_type, key, 255) == 0){
                *level = strdup(value);
                break;
            }
            comment_pos = NULL;
            trimmed_line = NULL;
            pos = NULL;
        }
    }
    fclose(fp);
    if (line)//getline获取的line要free,因为filename文件可能是空的，所以这里要加个判断
        free(line);
    if (*level == NULL) return ERROR;
    return OK;
}

int config_module_check_debug_level_has_on(bool *level) {
    FILE *fp;
    char *filename = MODULES_DEBUG_LEVELS_PATH;
    char *line = NULL, *pos = NULL, *comment_pos = NULL, *trimmed_line = NULL;
    char key[LINE_BUF_SIZE],value[LINE_BUF_SIZE];//key和value都默认为字符串
    int ret = 0;
    size_t len;
    size_t read;

    assert(level);
    *level = false;

    if (access(filename, F_OK) == -1) {
        //MODULES_DEBUG_LEVELS_PATH不存在
        // 认为所有模块的debug状态是关闭的
        return OK;
    }

    fp = fopen(filename, "r");
    if (fp == NULL) {
        ret = ERROR;
        fprintf(stderr, N_("Error: %s,failed :%m\n"), filename);
        return ret;
    }

    while ((read = getline(&line, &len, fp)) != -1) {
        //忽略注释，包括键值对后的注释
        comment_pos = strchr(line, '#');
        if (comment_pos != NULL)
            *comment_pos = '\0'; // 截断注释部分

        // 去除行首和行尾的空格
        trimmed_line = strtok(line, " \t\r\n");
        if (trimmed_line == NULL)
            continue; // 跳过空行

        //通过判断是否有"="来判断是不是一个有效的配置，如果之后配置文件
        //有其他格式请修改这里的代码
        pos = strchr(trimmed_line, '=');
        if (pos == NULL)//该行不是一个有效的配置
            continue;
        //获取并处理key和value，如果要处理其他的配置请在这里修改
        if (sscanf(trimmed_line, "%255[^=]=%255[^\n]", key, value) == 2)
        {
            if (strcmp(value,"debug") == 0 || strcmp(value,"on") == 0) {
                *level = true;
                break;
            }
            comment_pos = NULL;
            trimmed_line = NULL;
            pos = NULL;
        }
    }
    fclose(fp);
    if (line)//getline获取的line要free,因为filename文件可能是空的，所以这里要加个判断
        free(line);

    return OK;
}

// 判断一行是否读取完整
static int newline_terminated(char *buf, size_t buflen)
{
    size_t len = strlen(buf);
    if (len == buflen - 1 && buf[len - 1] != '\r' &&
        buf[len - 1] != '\n')
        return 0;
    return 1;
}

// 去除字符数组（字符串）两端的空格和制表符，并返回新字符串的起始位置
static char *trim_space(char *buf)
{
    char *pos = buf;
    int len = strlen(buf);
    if (len == 0)
        return pos;

    --len;
    while (len && (buf[len] == ' ' || buf[len] == '\t'))
    {
        buf[len] = '\0';
        --len;
    }
    while (*pos == ' ' || *pos == '\t')
        ++pos;
    return pos;
}

static int compare_config_item(const char *pos, config_item *items, int items_num)
{
    char *pos_end, *pos2;
    char key_buf[LINE_BUF_SIZE];
    int i;

    pos_end = strchr(pos, '=');
    if (pos_end == NULL)//该行不是一个有效的配置
        return items_num;

    memcpy(key_buf, pos, pos_end - pos);//将配置文件某一行的key复制到key_buf
    key_buf[pos_end - pos] = '\0';
    pos2 = trim_space(key_buf);// 去除key_buf两端的空格和制表符，并返回新字符串的起始位置
    for (i = 0; i < items_num; i++)
    {
        if (items[i].op == CONFIG_OP_NONE)
            continue;
        if (strcmp(items[i].key_name, pos2) == 0)//若items中有一项与key相同
            break;
    }
    return i;//返回key对应items中的位置
}

/*用于从文件中读取配置项并进行修改
* 函数参数：
*
* filename：要修改的配置文件名；
* items：配置项数组，用于存储要修改的配置项和相应的值；
* items_num：配置项数量。
* 函数返回值：
*
* 成功：返回 0；
* 失败：返回 ERR_RET。*/
static int mod_config(const char *filename, config_item *items, int items_num)
{
    char *line_buf = (char *)malloc(LINE_BUF_SIZE);
    if (line_buf == NULL)
    {
        fprintf(stderr, N_("Error: NOMEMORY:%m\n"));
        return -ENOMEM;
    }
    memset(line_buf, 0, LINE_BUF_SIZE);
    char *buf_long = NULL;
    int readsize = LINE_BUF_SIZE;
    int len = 0;
    int attr = 0;
    char bak_file[64];
    int line_len, i, ret = 0;
    FILE *fp_conf = NULL, *fp_conf_bak = NULL;
    char *pos;

    //打开要修改的配置文件
    fp_conf = fopen(filename, "r");
    if (fp_conf == NULL)
    {
        ret = ERROR;
        free(line_buf);
        line_buf = NULL;
        fprintf(stderr, N_("Error: file %s,failed:%m\n"), filename);
        return ret;
    }

    //创建一个备份文件
    snprintf(bak_file, 64, "%s.bak", filename);
    fp_conf_bak = fopen(bak_file, "w+");
    if (fp_conf_bak == NULL)
    {
        ret = ERROR;
        free(line_buf);
        line_buf = NULL;
        fclose(fp_conf);
        fprintf(stderr, N_("Error: file %s,failed:%m\n"), bak_file);
        return ret;
    }

    while (fgets(&line_buf[attr], readsize - len, fp_conf) != NULL)
    {
        //检查传入的字符串 buf 是否以新行符（\n 或 \r\n）为结尾，若不是，则再次读取直到找到新行符为止
        if (!newline_terminated(&line_buf[attr], readsize - len))
        {
            len = readsize;
            attr = strlen(line_buf);
            readsize = readsize * 2;
            buf_long = (char *)malloc(readsize);
            if (buf_long == NULL)
            {
                goto fail_rw;
            }
            memset(buf_long, 0, readsize);
            strncpy(buf_long, line_buf, len);
            free(line_buf);
            line_buf = NULL;
            line_buf = buf_long;
        }
        else
        {
            pos = line_buf;
            /* Skip white space from the beginning of line. */
            while (*pos == ' ' || *pos == '\t' || *pos == '\r')
                pos++;

            /* Skip comment lines and empty lines */
            if (*pos == '#' || *pos == '\n' || *pos == '\0')
            {
                if (fwrite(line_buf, strlen(line_buf), 1, fp_conf_bak) != 1){//直接将该行写入备份文件
                    ret = ERROR;
                    goto fail_rw;
                }
                continue;
            }
            //检查该行是否包含要修改的配置项
            ret = compare_config_item(pos, items, items_num);
            if (ret < items_num && items[ret].op == CONFIG_OP_ADD)
            {
                //写入修改后的值到备份文件
                line_len = fprintf(fp_conf_bak, "%s=%s\n", items[ret].key_name, items[ret].value);
                if (line_len < 0 || line_len >= LINE_BUF_SIZE){
                    ret = ERROR;
                    goto fail_rw;
                }
                // 将对应的配置项设为已操作
                items[ret].op = CONFIG_OP_NONE;
            }
            else if (ret >= items_num)
            {
                //将该行写入备份文件
                if (fwrite(line_buf, strlen(line_buf), 1, fp_conf_bak) != 1){
                    ret = ERROR;
                    goto fail_rw;
                }
            }
            // 对于CONFIG_OP_DEL这种操作，不复制到备份文件就等同于删除
            //将len、attr、readsize等恢复默认值
            len = 0;
            attr = 0;
            memset(line_buf, 0, readsize);
        }
    }
    if (ferror(fp_conf))
    {
        ret = ERROR;
        fprintf(stderr, N_("Error: read file %s error,failed:%m\n"), filename);
        goto fail_rw;
    }
    for (i = 0; i < items_num; i++)
    {
        if (items[i].op == CONFIG_OP_ADD)
        {
            line_len = fprintf(fp_conf_bak, "%s=%s\n", items[i].key_name, items[i].value);
            if (line_len < 0)
                goto fail_rw;
        }
    }
    free(line_buf);
    line_buf = NULL;
    fclose(fp_conf_bak);
    fclose(fp_conf);
    // 将备份文件重命名为原文件
    ret = rename(bak_file, filename);
    if (ret < 0)
    {
        unlink(bak_file);
        fprintf(stderr, N_("Error: rename file %s error,failed:%m\n"), bak_file);
        return ret;
    }
    return 0;

fail_rw:
    free(line_buf);
    line_buf = NULL;
    fclose(fp_conf_bak);
    fclose(fp_conf);
    unlink(bak_file);
    return ret;
}

/*存储 模块类型为type的日志打开状态信息*/
static int modify_debug_levels(const char *type, const char *level)
{
    const char *conf_file = MODULES_DEBUG_LEVELS_PATH;
    if (access(conf_file, F_OK) == -1)
    {
        //文件不存在的话创建该文件
        if (create_dir(conf_file)!=OK) {
            return ERROR;
        }

        FILE *fp = fopen(conf_file, "a+");
        if(fp == NULL){
            int r = ERROR;
            fprintf(stderr,N_("Error: %s,failed :%m\n"), MODULES_DEBUG_LEVELS_PATH);
            return r;
        }
        fclose(fp);
    }
    config_item items[1] = {
        {type, level, CONFIG_OP_ADD},
    };
    int r = mod_config(conf_file, items, 1);
    if(r < 0)
        return r;
    return 0;
}

//通过检查哈希算法值来判断要执行的脚本文件是否被允许执行
static bool is_shell_cmd_allowed(const char* file_path)
{
    unsigned char sha256Digest[SHA256_DIGEST_LENGTH];
    struct stat fileInfo;

    assert(file_path);

    if(!(stat(file_path, &fileInfo) == 0 && S_ISREG(fileInfo.st_mode)))
    {
        return false;
    }

    if(calculate_sha256(file_path, sha256Digest) < 0)
    {
        fprintf(stdout, "Warning: failed to calculate the sha256 digest for %s.\n",file_path);
        return false;
    }
    for(int i = 0; i < config_sha256_count; i++)
    {
        for(int j = 0; j < SHA256_DIGEST_LENGTH; j++){
            if(config_sha256[i][j] != sha256Digest[j])
            {
                break;
            }
            if(j == SHA256_DIGEST_LENGTH - 1)
                return true;
        }
    }
    return false;
}

int exec_debug_shell_cmd_internal(const char *filename,const char *level) {
    int r = 0;
    char real_level[PATH_MAX] = {0};
    char real_path[PATH_MAX] = {0};

    assert(filename&&level);
    snprintf(real_path,PATH_MAX,"%s/%s",CONFIG_SHELL_PATH,filename);
    snprintf(real_level, PATH_MAX, "debug=%s", level);

    if(!is_shell_cmd_allowed(real_path))
    {
        r = ERROR;
        fprintf(stderr, N_("Error: The sha256 digest of the shell file does not match, the shell file may be rewritten.\n"));
        return r;
    }

    r = start_process(real_path, real_level,NULL);
    if(r != 0)
    {
        fprintf(stderr, N_("Error: Failed to exec %s %s ret=%d errno=%d\n"), real_path,real_level,r,errno);
        r = ERROR;
    }
    return r;
}

// Define check_package_installed function to check if a package is installed
// 0 is not installed, 1 is installed
int check_package_installed(const char *package_name) {
    if (!package_name || package_name[0] == '\0') {
        fprintf(stderr, "Error: Package name cannot be empty.\n");
        return ERROR;
    }

    char cmd_arg[512];
    snprintf(cmd_arg, sizeof(cmd_arg), "-Wf=${db:Status-Abbrev} %s", package_name);

    char *output = NULL;
    if (start_process("/usr/bin/dpkg-query", cmd_arg, &output) != OK) {
        if (output) {
            free(output);
        }
        return 0; // If dpkg-query returns non-zero or fails, the package does not exist or query failed
    }

    int result = strncmp(output, "ii", 2) == 0;
    free(output);
    return result;
}
static int config_modules_set_debug_level_internal(const module_cfg *mdle_cfg,const char *level) {
    assert(mdle_cfg&&level);

    int ret = OK,r = OK,i = 0;
    for (i=0;mdle_cfg->sub_modules[i];i++) {
        r = exec_debug_shell_cmd_internal(mdle_cfg->sub_modules[i]->shell_cmd,level);
        if (r != OK) {
            fprintf(stderr,"exec file %s level %s failed\n",mdle_cfg->sub_modules[i]->shell_cmd,level);
            fprintf(stderr, N_("Error: Failed to configure %s.\n"), mdle_cfg->name);
            if (check_package_installed(mdle_cfg->sub_modules[i]->name) == 0) {
                fprintf(stderr, "The package %s is not installed,skip.\n", mdle_cfg->sub_modules[i]->name);
                continue;
            }
        }
        if (ret == OK) ret = r;
    }
    if (ret == OK)
        modify_debug_levels(mdle_cfg->name,level);
    fprintf(stdout,"set %s debug level to %s %s\n",mdle_cfg->name,level,(ret==OK)?"ok":"fail");
    return ret;
}

static int config_modules_set_debug_level_all(const char *level)
{
    int ret = OK,r = OK;
    GHashTableIter iter;
    module_cfg *mdle_cfg = NULL;

    assert(g_module_cfgs);

    g_hash_table_iter_init (&iter, g_module_cfgs);
    while (g_hash_table_iter_next (&iter, NULL, (void**)&mdle_cfg)) {
        r = config_modules_set_debug_level_internal(mdle_cfg,level);
        if (ret == OK)
            ret = r;
    }
    if (ret == OK)
        modify_debug_levels("all", level);

    return ret;
}

int config_modules_set_debug_level_by_type(const char* module_type, const char *level)
{
    int ret = OK,r = OK,find=0;
    GHashTableIter iter;
    module_cfg *mdle_cfg = NULL;

    assert(module_type);
    assert(g_module_cfgs);

    if (g_strcmp0(module_type,"all")==0) {
        ret = config_modules_set_debug_level_all(level);
    } else {
        g_hash_table_iter_init (&iter, g_module_cfgs);
        while (g_hash_table_iter_next (&iter, NULL, (void**)&mdle_cfg)) {
            if (g_strcmp0(mdle_cfg->type,module_type))
                continue;

            find = 1;
            r = config_modules_set_debug_level_internal(mdle_cfg,level);
            if (ret == OK)
                ret = r;
        }
    }

    if (find == 0) {
        fprintf(stderr,N_("Error: No module type %s found.\n"), module_type);
        ret = ERROR;
    }
    return ret;
}

int config_modules_set_debug_level_by_types(const char* module_types, const char *level)
{
    int count,r = OK,ret = OK;

    if (!module_types || !level)
        return ERROR;

    _cleanup_strv_free_ char** result = parseString(module_types, ",", &count);

    if (count <= 0 || !result) {
        r = ERROR;
        fprintf(stderr,N_("Error: Invalid module_types: %s\n"), module_types);
        return r;
    }
    for (int i = 0; i < count; i++) {
        r = config_modules_set_debug_level_by_type(result[i], level);
        if(ret == OK) ret = r;
    }

    return ret;
}

/*对多个模块直接设置调试等级：
*
* module_names：保存所有要配置的模块名的字符串，各个模块名用“，”分隔
* level：要配置的调试日志等级
* 函数返回值：
*
* 成功：返回 0；
* 失败：返回 ERR_RET。*/
int config_module_set_debug_level_by_module_names(const char *module_names, const char *level)
{
    int count,r = OK,ret = OK;

    if (!module_names || !level)
        return ERROR;

    _cleanup_strv_free_ char** result = parseString(module_names, ",", &count);

    if (count <= 0 || !result) {
        r = ERROR;
        fprintf(stderr,N_("Error: Invalid module_name: %s\n"), module_names);
        return r;
    }
    for (int i = 0; i < count; i++) {
        r = config_module_set_debug_level_by_module_name(result[i], level);
        if(ret == OK) ret = r;
    }

    return ret;
}

/*对单个模块直接设置调试等级：
*
* module_cfgs：所有的模块的配置数组；
* valid_module_cfgs_count：模块配置的数目；
* module_type：保存单个要配置的模块的模块名
* level：要配置的调试日志等级
* 函数返回值：
*
* 成功：返回 0；
* 失败：返回 ERR_RET。*/
int config_module_set_debug_level_by_module_name(const char *module_name, const char *level)
{
    int ret = OK;
    module_cfg *mdle_cfg = NULL;

    assert(module_name && level);
    assert(g_module_cfgs);

    if (g_strcmp0(module_name,"all")==0) {
        ret = config_modules_set_debug_level_all(level);
    } else {
        mdle_cfg = g_hash_table_lookup (g_module_cfgs, module_name);
        if (mdle_cfg == NULL) {
            fprintf(stderr,N_("Error: cann't find module %s.\n"),module_name);
            return ERROR;
        }
        ret = config_modules_set_debug_level_internal(mdle_cfg,level);
    }

    return ret;
}

int config_module_check_log() {
    int ret = OK;
    bool level = false;

    ret = config_module_check_debug_level_has_on(&level);
    if (ret != OK)
        return ret;
    return set_journal_log(level);
}

int config_module_get_property_reboot(const char *module_name,int *reboot) {
    module_cfg *mdle_cfg = NULL;
    GHashTableIter iter;

    assert(module_name && reboot);
    assert(g_module_cfgs);

    *reboot = 0;
    if (g_strcmp0(module_name,"all")==0) {
        g_hash_table_iter_init (&iter, g_module_cfgs);
        while (g_hash_table_iter_next (&iter, NULL, (void**)&mdle_cfg)) {
            *reboot = mdle_cfg->reboot;
            if (*reboot)
                break;
        }
    } else {
        mdle_cfg = g_hash_table_lookup (g_module_cfgs, module_name);
        if (mdle_cfg == NULL) {
            fprintf(stderr,N_("Error: cann't find module %s.\n"),module_name);
            return ERROR;
        }
        *reboot = mdle_cfg->reboot;
    }
    return OK;
}

bool check_can_install_dbg() {
    return is_shell_cmd_allowed(INSTALL_DBGPKG_SHELL_PATH);
}

/*针对多个模块安装调试包：
*
* module_names：要安装调试包的模块名，多个模块名直接
用","分隔
* 函数返回值：
*
* 成功：返回 0；
* 失败：返回 ERR_RET。*/
int config_modules_install_dbgpkgs(const char *module_names)
{
    int count;
    _cleanup_strv_free_ char** result = NULL;
    int r = 0;

    assert(module_names);

    result = parseString(module_names, ",", &count);
    if (count <= 0 || !result) {
        r = ERROR;
        fprintf(stderr,N_("Error: Invalid module_name: %s\n"), module_names);
        return r;
    }

    if(!is_shell_cmd_allowed(INSTALL_DBGPKG_SHELL_PATH))
    {
        r = ERROR;
        fprintf(stdout, N_("Error: The sha256 digest of the shell file does not match, the shell file may be rewritten.\n"));
        return r;
    }

    for (int i = 0; i < count; i++) {
        r = config_module_install_dbgpkgs_internal(result[i]);
        if(r < 0) return r;
    }

    return 0;
}

/*针对一个模块安装调试包：
*
* module_name：要安装调试包的模块名
* 函数返回值：
*
* 成功：返回 0；
* 失败：返回 ERR_RET。*/
int config_module_install_dbgpkgs_internal(const char *module_name)
{
    int r = 0;

    r = start_process(INSTALL_DBGPKG_SHELL_PATH, module_name,NULL);
    if(r != 0)
    {
        r = ERROR;
        fprintf(stderr, N_("Error: Failed to install dbg packages for %s\n"), module_name);
    }
    return r;
}

/*打开或关闭coredump：
*
* open_coredump： true表示打开coredump,
false表示关闭coredump
* 函数返回值：
*
* 成功：返回 0；
* 失败：返回 ERR_RET。*/
int config_system_coredump(bool open_coredump)
{
    int r = 0;
    char cmd_args[PATH_MAX];

    if(!is_shell_cmd_allowed(CONFIG_COREDUMP_SHELL_PATH))
    {
        r = ERROR;
        fprintf(stdout, N_("Error: The sha256 digest of the shell file does not match, the shell file may be rewritten.\n"));
        return r;
    }
    snprintf(cmd_args, PATH_MAX, open_coredump ? "on" : "off");
    r = start_process(CONFIG_COREDUMP_SHELL_PATH,cmd_args,NULL);
    if(r != 0)
    {
        r = ERROR;
        fprintf(stderr, N_("Error: Failed to configure coredump\n"));
        return r;
    }
    r = modify_debug_levels("coredump", open_coredump ? "on" : "off");
    return r;
}

/*解析一个json文件：
*
* filename： json文件的路径
* mdle_cfg： 保存解析好的json文件信息（即模块信息）；
* 函数返回值：
*
* 成功：返回 0；
* 失败：返回 ERR_RET。*/
int parse_hook_json_file(char *filename, module_cfg* mdle_cfg)
{
    int i = 0, numSubmodules = 0;
    assert(mdle_cfg && filename);
    // 读取 json 文件的内容
    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr,"Error before: [%s]\n",cJSON_GetErrorPtr());
        fprintf(stderr, N_("Error: Failed to open file %s.\n"), filename);
        return ERROR;
    }

    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *jsonData = (char *)malloc(fileSize + 1);

    size_t totalBytesRead = 0;
    size_t bytesRead;
    while (totalBytesRead < (size_t)fileSize &&
           (bytesRead = fread(jsonData + totalBytesRead, 1, fileSize - totalBytesRead, file)) > 0) {
        totalBytesRead += bytesRead;
    }

    if (totalBytesRead != (size_t)fileSize) {
        fprintf(stderr, "Failed to read the entire file: expected %ld bytes, but got %zu bytes\n", fileSize, totalBytesRead);
        free(jsonData);
        fclose(file);
        return ERROR;
    }

    jsonData[fileSize] = '\0';
    fclose(file);

    // 解析 JSON 字符串为 cJSON 对象
    cJSON *root = cJSON_Parse(jsonData);
    free(jsonData);

    if (!root) {
        fprintf(stderr, N_("Error: Failed to parse JSON.\n"));
        return -1;
    }

    cJSON *module_name = cJSON_GetObjectItem(root, "name");
    if (module_name == NULL || module_name->type != cJSON_String) {
        fprintf(stderr, N_("Error: Error parse a name in file %s\n"),filename);
        goto ERRRET;
    }
    mdle_cfg->name = strdup(module_name->valuestring);

    cJSON *module_type = cJSON_GetObjectItem(root, "group");
    if (module_type) {
        if (module_type->type != cJSON_String) {
            fprintf(stderr, N_("Error: Error parse a type\n"));
            goto ERRRET;
        }
        mdle_cfg->type = strdup(module_type->valuestring);
    }

    cJSON* jsonReboot = cJSON_GetObjectItem(root, "reboot");
    if (jsonReboot != NULL) {
        mdle_cfg->reboot = jsonReboot->valueint;
    }

    cJSON* jsonSubmodules = cJSON_GetObjectItem(root, "submodules");
    if (jsonSubmodules == NULL || !cJSON_IsArray(jsonSubmodules)) {
        fprintf(stderr, N_("Error: Error parse a submodules in file %s\n"),filename);
        goto ERRRET;
    }

    numSubmodules = cJSON_GetArraySize(jsonSubmodules);
    mdle_cfg->sub_modules = malloc(sizeof(sub_module_cfg*)*(numSubmodules+1));
    assert(mdle_cfg->sub_modules);
    memset(mdle_cfg->sub_modules,0,sizeof(sub_module_cfg*)*(numSubmodules+1));
    mdle_cfg->sub_modules_num = numSubmodules;

    for (i = 0; i < numSubmodules; i++) {
        cJSON* jsonSubmodule = cJSON_GetArrayItem(jsonSubmodules, i);
        if (jsonSubmodule == NULL || !cJSON_IsObject(jsonSubmodule)) {
            fprintf(stderr, N_("Error: Error parse a submodule in file %s,i=%d,numSubmodules=%d\n"),filename,i,numSubmodules);
            goto ERRRET;
        }
        mdle_cfg->sub_modules[i] = malloc(sizeof(sub_module_cfg));
        assert(mdle_cfg->sub_modules[i]);
        memset(mdle_cfg->sub_modules[i],0,sizeof(sub_module_cfg));

        cJSON* jsonSubmoduleName = cJSON_GetObjectItem(jsonSubmodule, "name");
        if (jsonSubmoduleName == NULL || jsonSubmoduleName->type != cJSON_String) {
            fprintf(stderr, N_("Error: Error parse a subname in file %s\n"),filename);
            goto ERRRET;
        }
        mdle_cfg->sub_modules[i]->name = strdup(jsonSubmoduleName->valuestring);

        cJSON* jsonSubmoduleExec = cJSON_GetObjectItem(jsonSubmodule, "exec");
        if (jsonSubmoduleExec == NULL || jsonSubmoduleExec->type != cJSON_String) {
            fprintf(stderr, N_("Error: Error parse a exec\n"));
            goto ERRRET;
        }
        mdle_cfg->sub_modules[i]->shell_cmd = strdup(jsonSubmoduleExec->valuestring);
    }

    cJSON_Delete(root);
    return 0;
ERRRET:
    cJSON_Delete(root);
    return -1;
}

// static void md5DigestToString(const unsigned char *md5Digest, char *md5String) {
//     for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
//         sprintf(&md5String[i*2], "%02x", md5Digest[i]);
//     }
// }