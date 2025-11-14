#include "util.h"
#include "common.h"
#include <dirent.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <openssl/sha.h>
typedef struct {
    char filename[PATH_MAX];
    unsigned char sha256[SHA256_DIGEST_LENGTH];
} ConfigFile;

int main(int argc, char **argv)
{
    DIR  *pdir = NULL;
    struct dirent *pdirent;
    char dir_path[PATH_MAX]={0};

    snprintf(dir_path,PATH_MAX,"%s%s",argc>1?argv[1]:"",CONFIG_SHELL_IN_CODE_PATH);    
    int shell_count = get_dir_file_count_with_suffix(dir_path, ".sh");
    if(shell_count <= 0) {
        fprintf(stderr, N_("Error: Failed to get shell file count from %s\n"), dir_path);
        return shell_count;
    }

    pdir=opendir(dir_path);
    if(pdir==NULL)
    {
        fprintf(stderr, N_("Error: Failed to open dir %s, err: %m\n"), dir_path);
        return ERROR;
    }
    ConfigFile *files = (ConfigFile *)malloc(sizeof(ConfigFile)*shell_count);
    if (files == NULL) {
        fprintf(stderr, N_("Error: Failed to allocate memory for ConfigFile.\n"));
        return ERROR;
    }
    memset(files, 0, sizeof(ConfigFile)*shell_count);
    char buff[PATH_MAX] = {0};
    int count = 0;
    for(pdirent= readdir(pdir); pdirent!=NULL; pdirent=readdir(pdir))
    {
        if( strncmp(pdirent->d_name,".", 3)==0 || strncmp(pdirent->d_name,"..", 3)==0 )
            continue;
        snprintf( buff, PATH_MAX, "%s/%s", dir_path, pdirent->d_name);
        struct stat sbuf;
        if( lstat(buff, &sbuf) == -1 )
        {
            fprintf(stderr,N_("Error: lstat error %s\n"),buff);
            continue;
        }
        if( S_ISREG(sbuf.st_mode) && str_endsWith(buff, ".sh"))
        {
            unsigned char sha256_hash[SHA256_DIGEST_LENGTH];
            if(calculate_sha256(buff, sha256_hash) < 0)
            {
                fprintf(stdout, N_("Error: Failed to calculate the sha256 digest for the shell file: %s.\n"), buff);
                closedir(pdir);
                return -1;
            }
            strcpy(files[count].filename, buff);
            for(int i = 0; i < SHA256_DIGEST_LENGTH; i++)
                files[count].sha256[i] = sha256_hash[i];

            count++;
        }
    }

    FILE *output = fopen("config_sha256.c", "w");
    fprintf(output, "#include <openssl/sha.h>\n");
    fprintf(output, "unsigned char config_sha256[][SHA256_DIGEST_LENGTH] = {\n");
    for (int i = 0; i < count; i++) {
        fprintf(output, "    {");
        for (int j = 0; j < SHA256_DIGEST_LENGTH; j++) {
            fprintf(output, "0x%02X", files[i].sha256[j]);
            if (j < SHA256_DIGEST_LENGTH - 1)
                fprintf(output, ",");
        }
        fprintf(output, "},\n");
    }
    fprintf(output, "};\n");
    fprintf(output, "int config_sha256_count = %d;\n", count);
    closedir(pdir);
    fclose(output);
    return 0;
}