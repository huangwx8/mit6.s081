#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

int match(char *filepath, char* filename)
{
    //printf("match filepath = %s, filename = %s\n", filepath, filename);
    int filename_length;
    int filepath_length;
    filename_length = strlen(filename);
    filepath_length = strlen(filepath);

    // file path shorter than file name, impossible case
    if (filepath_length < filename_length) 
    {
        return 0;
    }

    // '*' means match all files
    if (strcmp(filename, "*") == 0) 
    {
        return 1;
    }

    // compare last n characters of filepath and filename
    if (strcmp(filepath + filepath_length - filename_length, filename) == 0)
    {
        return 1;
    }

    return 0;
}

void find(char *path, char* filename)
{
    //printf("match path = %s, filename = %s\n", path, filename);
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;

    if ((fd = open(path, 0)) < 0)
    {
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }

    if (fstat(fd, &st) < 0)
    {
        fprintf(2, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }

    switch (st.type)
    {
    case T_FILE:
        if (match(path, filename)) 
        {
            printf("%s\n", path);
        }
        break;
    case T_DIR:
        if (strlen(path) + 1 + DIRSIZ + 1 > sizeof buf)
        {
            printf("find: path too long\n");
            break;
        }
        strcpy(buf, path);
        p = buf + strlen(buf);
        *p++ = '/';
        // traveral all items in current directory
        while (read(fd, &de, sizeof(de)) == sizeof(de))
        {
            if (de.inum == 0)
                continue;
            if ((strcmp(de.name, ".") == 0) || (strcmp(de.name, "..") == 0)) 
            {
                continue;
            }
            // extend path
            memmove(p, de.name, DIRSIZ);
            p[DIRSIZ] = 0;
            if (stat(buf, &st) < 0)
            {
                fprintf(2, "find: cannot stat %s\n", buf);
                continue;
            }
            // recursion
            find(buf, filename);
        }
        break;
    }
    close(fd);
}

int main(int argc, char *argv[])
{
    const char* usage_msg = "Usage: find [path...] [expression]";

    if (argc > 3)
    {
        fprintf(2, usage_msg);
        exit(0);
    }

    if (argc == 1) 
    {
        find(".", "*");
    }
    else if (argc == 2) 
    {
        find(argv[1], "*");
    }
    else 
    {
        find(argv[1], argv[2]);
    }

    exit(0);
}
