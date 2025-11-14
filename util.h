#ifndef UTIL_H_included
#define UTIL_H_included 1
#include <libintl.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include<stdlib.h>
#include <limits.h>
#include <assert.h>

typedef enum _config_op
{
    CONFIG_OP_NONE,
    CONFIG_OP_ADD,
    CONFIG_OP_DEL
} config_op;

typedef struct config_item
{
    const char *key_name;
    const char *value;
    config_op op; // 1 is add,2 is del,0 means nothing
} config_item;

typedef struct set
{
    char *data;
    struct set *next;
} set;

static inline bool isempty(const char *p) {
        return !p || !p[0];
}

/*汉化相关*/
#define N_(String) gettext(String)
#define _(String) gettext(String)


/*跟内存清理相关的操作*/
#define DEFINE_TRIVIAL_CLEANUP_FUNC(type, func)                 \
        static inline void func##p(type *p) {                   \
                if (*p)                                         \
                        func(*p);                               \
        }
void freep(void *p);
#define _cleanup_(x) __attribute__((__cleanup__(x)))
#define _cleanup_free_ _cleanup_(freep)
void strv_clear(char **l);
char **strv_free(char **l);
DEFINE_TRIVIAL_CLEANUP_FUNC(char**, strv_free);
#define _cleanup_strv_free_ _cleanup_(strv_freep)

/*跟集合相关的操作*/
bool IN_SET(struct set *S, const char* data);
int INSERT_SET(struct set *S, const char* data);
void set_unrefp(struct set *S);

/*跟文件、目录相关的操作*/
int recurive_create_dir(char* dir);
int get_dir_file_count_with_suffix(const char *dir_path, const char *suffix);
const char *Basename (const char *str);
int strupr_custom(const char *str, char *upr_str);

/*跟加密计算相关*/
int calculateFileMD5(const char* file_path, unsigned char* md5Digest);
int calculate_sha256(const char* file_path, unsigned char* sha256_hash);
/*字符串处理相关*/
int str_endsWith(const char *str, const char *suffix);
char *trim_string(const char *str);
char** parseString(const char* input, const char* delimiter, int* count);

int start_process(const char *cmd_path, const char *arg_string, char **output);
#endif