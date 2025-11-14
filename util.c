/* SPDX-License-Identifier: LGPL-2.1+ */
#include"util.h"
#include "module_configure.h"
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ctype.h>
#include <sys/wait.h>
#include <openssl/opensslv.h>

#if OPENSSL_VERSION_NUMBER < 0x30000000L
#define USE_OPENSSL_1_1
#include <openssl/md5.h>
#include <openssl/sha.h>
#else
#define USE_OPENSSL_3_0
#include <openssl/evp.h>
#endif


#define MAX_ALLOWED_SHELL_MD5S_NUM 128

void freep(void *p) {
    if (*(void**)p)
        free(*(void**) p);
}

static inline void *mfree(void *memory) {
        free(memory);
        return NULL;
}

void strv_clear(char **l) {
        char **k;

        if (!l)
                return;

        for (k = l; *k; k++)
                free(*k);

        *l = NULL;
}

char **strv_free(char **l) {
        strv_clear(l);
        return mfree(l);
}

const char *Basename (const char *str)
{
	char *cp = strrchr (str, '/');

	return (NULL != cp) ? cp + 1 : str;
}

int recurive_create_dir(char* dir)
{
    for(char* cur = dir + 1; *cur != '\0'; ++cur){
        if( *cur == '/'){
            char path[PATH_MAX] = {0};
            memcpy(path, dir, cur-dir);
            if( 0 != access( path, F_OK) ){
                int res = mkdir( path, 0755 );
                if( 0 != res) {
                    return res;
                }
            }
        }
    }

    return 0;
}

//去除字符串两端的空格和制表符，并返回新的修剪后的字符串
char *trim_string(const char *str)
{
    if(str == NULL)
        return NULL;

    unsigned int uLen = strlen(str);
    if(0 == uLen)
    {
        return NULL;
    }

    // 寻找第一个非空格和制表符的字符
    unsigned int start = 0;
    while (str[start] == ' ' || str[start] == '\t')
    {
        start++;
    }

    // 寻找最后一个非空格和制表符的字符
    unsigned int end = uLen - 1;
    while (end >= start && (str[end] == ' ' || str[end] == '\t'))
    {
        end--;
    }
    unsigned int trimmed_len = end - start + 1;

    char *strRet = (char *)malloc(trimmed_len + 1);
    strncpy(strRet, str + start, trimmed_len);
    strRet[trimmed_len] = '\0';

    return strRet;
}

/*判断一个字符串是否以suffix为后缀结尾*/
int str_endsWith(const char *str, const char *suffix) {
    size_t strLen = strlen(str);
    size_t suffixLen = strlen(suffix);

    if (suffixLen > strLen) {
        return 0;  // 后缀比字符串长，不匹配
    }

    const char *start = str + (strLen - suffixLen);
    return strncmp(start, suffix, 1024) == 0;
}

/*将字符串input根据字符delimiter分割为字符串数组*/
char** parseString(const char* input, const char* delimiter, int* count) {
    char** result = NULL;
    void *tmp_result = NULL;
    char* str = strdup(input); // 复制输入字符串，以便修改
    char* token;
    int i = 0;

    // 使用 strtok 函数将字符串分割为子串
    token = strtok(str, delimiter);
    while (token != NULL) {
        // 分配内存来存储每个子串，并将它们存储在字符串数组中
        tmp_result = realloc(result, (i + 2) * sizeof(char*));
        if (tmp_result == NULL) {
            fprintf(stderr, "Error: Failed to allocate memory for result array\n");
            free(result);
            return NULL;
        }
        result = (char**)tmp_result;
        result[i] = strdup(token);
        i++;
        result[i] = NULL;
        token = strtok(NULL, delimiter);
    }

    *count = i; // 子串的数量

    free(str); // 释放临时字符串的内存

    return result;
}

/*获取dir_path目录下有多少个以suffix为后缀的文件*/
int get_dir_file_count_with_suffix(const char *dir_path, const char *suffix)
{
    DIR  * pdir;
    struct dirent *pdirent;
    int fcnt;

    pdir=opendir(dir_path);
    if(pdir==NULL)
    {
        fprintf(stderr, "Error: Failed to open dir %s, err: %m\n", dir_path);
        return ERROR;
    }

    fcnt=0;
    char buff[PATH_MAX] = {0};
    for(pdirent= readdir(pdir); pdirent!=NULL; pdirent=readdir(pdir))
    {
        if( strncmp(pdirent->d_name, ".", 3)==0 || strncmp(pdirent->d_name, "..", 3)==0 )
            continue;

        sprintf( buff,"%s/%s", dir_path, pdirent->d_name);
        struct stat sbuf;
        if( lstat(buff, &sbuf) == -1 )
        {
            fprintf(stderr,"Error: lstat error %s\n",buff);
        }

        if( S_ISREG(sbuf.st_mode) && str_endsWith(buff, suffix))
            fcnt++;
    }

    closedir(pdir);
    return fcnt;
}

/*自定义的strupr函数*/
int strupr_custom(const char *str, char *upr_str)
{
    if( str == NULL){
        return -1;
    }
    size_t length = strlen(str);
    size_t i;

    for (i = 0; i < length && i < MAX_MODULE_CFG_STRING_SIZE; i++) {
        upr_str[i] = toupper((unsigned char)str[i]);
    }
    return 0;
}

#ifdef USE_OPENSSL_1_1
int calculateFileMD5_openssl1_1(const char* file_path, unsigned char* md5Digest) {
    FILE* file = fopen(file_path, "rb");
    if (!file) {
        perror("Error opening file");
        return -1;
    }

    MD5_CTX md5Context;
    MD5_Init(&md5Context);

    unsigned char buffer[1024];
    int bytesRead = 0;

    while ((bytesRead = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        MD5_Update(&md5Context, buffer, bytesRead);
    }

    if (ferror(file)) {
        perror("Error reading file");
        fclose(file);
        return -1;
    }

    fclose(file);
    MD5_Final(md5Digest, &md5Context);

    return 0;
}

int calculate_sha256_openssl1_1(const char* file_path, unsigned char* sha256_hash) {
    FILE* file = fopen(file_path, "rb");
    if (file == NULL) {
        fprintf(stderr, "Failed to open file: %s\n", file_path);
        return -1;
    }

    SHA256_CTX ctx;
    SHA256_Init(&ctx);

    unsigned char buffer[4096];
    size_t bytes_read;

    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        SHA256_Update(&ctx, buffer, bytes_read);
    }

    SHA256_Final(sha256_hash, &ctx);
    if (ferror(file)) {
        perror("Error reading file");
        fclose(file);
        return -1;
    }

    fclose(file);
    return 0;
}
#endif

#ifdef USE_OPENSSL_3_0

int calculateFileMD5_openssl3_0(const char* file_path, unsigned char* md5Digest) {
    FILE* file = fopen(file_path, "rb");
    if (!file) {
        perror("Error opening file");
        return -1;
    }

    EVP_MD_CTX *md_ctx = EVP_MD_CTX_new();
    const EVP_MD *md = EVP_md5();

    if (md_ctx == NULL || md == NULL) {
        fprintf(stderr, "Failed to create MD5 context\n");
        fclose(file);
        return -1;
    }

    if (EVP_DigestInit_ex(md_ctx, md, NULL) <= 0) {
        fprintf(stderr, "Failed to initialize MD5 context\n");
        EVP_MD_CTX_free(md_ctx);
        fclose(file);
        return -1;
    }

    unsigned char buffer[1024];
    size_t bytesRead;

    while ((bytesRead = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        if (EVP_DigestUpdate(md_ctx, buffer, bytesRead) <= 0) {
            fprintf(stderr, "Failed to update MD5 digest\n");
            EVP_MD_CTX_free(md_ctx);
            fclose(file);
            return -1;
        }
    }

    if (ferror(file)) {
        perror("Error reading file");
        EVP_MD_CTX_free(md_ctx);
        fclose(file);
        return -1;
    }

    if (EVP_DigestFinal_ex(md_ctx, md5Digest, NULL) <= 0) {
        fprintf(stderr, "Failed to finalize MD5 digest\n");
        EVP_MD_CTX_free(md_ctx);
        fclose(file);
        return -1;
    }

    EVP_MD_CTX_free(md_ctx);
    fclose(file);

    return 0;
}

int calculate_sha256_openssl3_0(const char* file_path, unsigned char* sha256_hash) {
    FILE* file = fopen(file_path, "rb");
    if (file == NULL) {
        fprintf(stderr, "Failed to open file: %s\n", file_path);
        return -1;
    }

    EVP_MD_CTX *md_ctx = EVP_MD_CTX_new();
    const EVP_MD *md = EVP_sha256();

    if (md_ctx == NULL || md == NULL) {
        fprintf(stderr, "Failed to create SHA-256 context\n");
        fclose(file);
        return -1;
    }

    if (EVP_DigestInit_ex(md_ctx, md, NULL) <= 0) {
        fprintf(stderr, "Failed to initialize SHA-256 context\n");
        EVP_MD_CTX_free(md_ctx);
        fclose(file);
        return -1;
    }

    unsigned char buffer[4096];
    size_t bytes_read;

    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        if (EVP_DigestUpdate(md_ctx, buffer, bytes_read) <= 0) {
            fprintf(stderr, "Failed to update SHA-256 digest\n");
            EVP_MD_CTX_free(md_ctx);
            fclose(file);
            return -1;
        }
    }

    if (ferror(file)) {
        perror("Error reading file");
        EVP_MD_CTX_free(md_ctx);
        fclose(file);
        return -1;
    }

    if (EVP_DigestFinal_ex(md_ctx, sha256_hash, NULL) <= 0) {
        fprintf(stderr, "Failed to finalize SHA-256 digest\n");
        EVP_MD_CTX_free(md_ctx);
        fclose(file);
        return -1;
    }

    EVP_MD_CTX_free(md_ctx);
    fclose(file);

    return 0;
}
#endif // USE_OPENSSL_3_0

int calculateFileMD5(const char* file_path, unsigned char* md5Digest) {
#ifdef USE_OPENSSL_1_1
    return calculateFileMD5_openssl1_1(file_path, md5Digest);
#elif defined(USE_OPENSSL_3_0)
    return calculateFileMD5_openssl3_0(file_path, md5Digest);
#else
    #error "Unsupported OpenSSL version"
#endif
}

int calculate_sha256(const char* file_path, unsigned char* sha256_hash) {
#ifdef USE_OPENSSL_1_1
    return calculate_sha256_openssl1_1(file_path, sha256_hash);
#elif defined(USE_OPENSSL_3_0)
    return calculate_sha256_openssl3_0(file_path, sha256_hash);
#else
    #error "Unsupported OpenSSL version"
#endif
}

bool IN_SET(struct set *S, const char* data) {
    for (struct set *p = S->next; p; p = p->next)
        if (strncmp(p->data, data, 255) == 0)
            return true;
    return false;
}

int INSERT_SET(struct set *S, const char* data) {
    struct set *p, *q = (struct set *) malloc(sizeof(struct set));
    q->data = strdup(data);
    if (!S->next) {
        S->next = q;
        q->next = NULL;
    } else {
        for (p = S->next; p->next; p = p->next) {
            if (strncmp(p->data, data, 255) == 0){
                free(q->data);
                free(q);
                return 0;
            }
        }
        if(strncmp(p->data, data, 255) != 0){
            q->next = p->next;
            p->next = q;
        }
        else {
            free(q->data);
            free(q);
        }
    }

    return 1;
}

void set_unrefp(struct set *S)
{
    struct set *set = S, *p;
    while(set != NULL){
        p = set;
        set = set->next;
        //head没有p->data,所以这里要判断下
        if(p->data)
            free(p->data);
        free(p);
    }
}

// Read data from file descriptor and store it in output
int read_from_fd(int fd, char **output) {
    if (fd < 0 || !output) return ERROR;
    // Initialize output buffer
    const int BUFFER_SIZE = 1024;
    size_t output_size = BUFFER_SIZE;
    *output = malloc(output_size);
    if (*output == NULL) {
        perror("malloc failed");
        return ERROR;
    }
    (*output)[0] = '\0';

    char buffer[BUFFER_SIZE];
    FILE *pipe_stream = fdopen(fd, "r");
    if (pipe_stream == NULL) {
        perror("fdopen failed");
        free(*output);
        *output = NULL;
        return ERROR;
    }

    while (fgets(buffer, sizeof(buffer), pipe_stream) != NULL) {
        size_t needed = strlen(*output) + strlen(buffer) + 1;
        while (needed > output_size) {
            output_size *= 2;
            char *new_output = realloc(*output, output_size);
            if (!new_output) {
                perror("realloc failed");
                fclose(pipe_stream);
                free(*output);
                *output = NULL;
                return ERROR;
            }
            *output = new_output;
        }
        strncat(*output, buffer, output_size - strlen(*output) - 1);
    }
    fclose(pipe_stream);

    return OK;
}

int start_process(const char *cmd_path, const char *arg_string, char **output) {
    if (!cmd_path || !arg_string)
        return ERROR;
    // Check if arg_string contains dangerous characters like semicolon, etc.
    if (strchr(arg_string, ';') != NULL || strchr(arg_string, '|') != NULL ||
        strchr(arg_string, '&') != NULL || strchr(arg_string, '>') != NULL ||
        strchr(arg_string, '<') != NULL) {
        fprintf(stderr, "Error: The argument string cannot contain special characters (;|&><).\n");
        return ERROR;
    }

    // Assuming the parameters are space-separated
    // Count how many spaces are in the string to allocate memory for the argument array
    int num_args = 1;  // At least one argument, which is the name itself
    for (int i = 0; arg_string[i] != '\0'; i++) {
        if (arg_string[i] == ' ') {
            num_args++;
        }
    }

    // Create an argument array of size num_args + 1 (the last NULL is for execvp)
    char **args = malloc((num_args + 2) * sizeof(char *)); // +2 for the path and NULL

    // The first argument is the path (e.g., "/xxx/xxx")
    args[0] = (char *)cmd_path;

    // Use strtok to split the name string into tokens based on space
    char *name_copy = strdup(arg_string);  // Duplicate the arg_string string for safe splitting
    char *token = strtok(name_copy, " ");
    int i = 1;
    while (token != NULL) {
        args[i] = token;
        token = strtok(NULL, " ");
        i++;
    }

    args[i] = NULL;  // execvp needs the last argument to be NULL

    // Create a pipe
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe failed");
        free(args);
        free(name_copy);
        return ERROR;
    }

    // Fork a child process
    pid_t pid = fork();
    if (pid == -1) {
        // Error handling: failed to create a child process
        perror("fork failed");
        close(pipefd[0]);
        close(pipefd[1]);
        free(args);
        free(name_copy);
        return ERROR;
    }

    if (pid == 0) {
        // Child process: close read end, redirect stdout to write end, execute command
        close(pipefd[0]);
        if (output && *output == NULL) {
            dup2(pipefd[1], STDOUT_FILENO);
        }
        close(pipefd[1]);
        if (execvp(args[0], args) == -1) {
            perror("execvp failed");
            exit(ERROR);
        }
        return ERROR;
    } else {
        // Parent process: close write end
        int exit_status = OK;
        close(pipefd[1]);
        if (output && *output == NULL) {
            // Read child process output
            if (read_from_fd(pipefd[0], output) != OK) {
                exit_status = ERROR;
            }
        }

        // In the parent process, wait for the child to finish
        int status;
        if (waitpid(pid, &status, 0) == -1) {
            perror("waitpid failed");
            exit_status = ERROR;
        }
         // Free allocated memory
        free(args);
        free(name_copy);
        close(pipefd[0]);
        if (exit_status != OK) return exit_status;

        // Check the exit status of the child process
        if (WIFEXITED(status)) {
            exit_status = WEXITSTATUS(status);
            if (exit_status != OK) {
                fprintf(stderr,"exec %s %s failed with exit status %d.\n", cmd_path,arg_string,exit_status);
            }
        } else if (WIFSIGNALED(status)) {
            exit_status = WTERMSIG(status);
            fprintf(stderr,"exec %s %s terminated by signal %d.\n", cmd_path,arg_string,exit_status);
        } else {
           fprintf(stderr,"exec %s %s terminated with unknown status.\n",cmd_path,arg_string);
        }

        return exit_status;
    }
}