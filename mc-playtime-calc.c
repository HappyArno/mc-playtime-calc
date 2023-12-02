// SPDX-License-Identifier: MIT
// Copyright (c) 2023 Happy_Arno
static const char *help =
    "A tool to calculate your playtime in minecraft by parsing logs\n"
    "Usage:\n"
    "    mc-playtime-calc [<log file>] [<logs dir>] [<.minecraft dir>] ...\n"
    "Example:\n"
    "    mc-playtime-calc .\n"
    "    mc-playtime-calc ./.minecraft\n"
    "    mc-playtime-calc ./.minecraft/logs\n"
    "    mc-playtime-calc ./.minecraft/logs/latest.log\n"
    "    mc-playtime-calc ./version1/logs ./version2/logs\n";
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <zlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <stdlib.h>
#include <libgen.h>
#include <errno.h>
/**
 * @brief Extract a time by parsing the timestamp from a line in a log file.
 * @param[in] gf An opened `gzFile` object.
 * @param[out] time A `time_t` pointer for outputting time.
 * @return Return 0 on success, or 1 on failure.
 */
int parseLine(gzFile gf, time_t *time)
{
    const char *tag = "[hh:mm:ss]";
    time_t hour = 0, minute = 0, second = 0;
    int ch;
    for (int i = 0; tag[i] != '\0'; i++)
    {
        ch = gzgetc(gf);
        if (ch == EOF)
            return 1;
        if (tag[i] == 'h' || tag[i] == 'm' || tag[i] == 's')
        {
            if (!isdigit(ch))
                return 1;
            if (tag[i] == 'h')
                hour = hour * 10 + (ch - '0');
            else if (tag[i] == 'm')
                minute = minute * 10 + (ch - '0');
            else
                second = second * 10 + (ch - '0');
        }
        else if (ch != tag[i])
            return 1;
    }
    *time = (hour * 60 + minute) * 60 + second;
    while ((ch = gzgetc(gf)) != EOF && ch != '\r' && ch != '\n');
    while ((ch = gzgetc(gf)) != EOF && (ch == '\r' || ch == '\n'));
    if (ch != EOF)
        gzungetc(ch, gf);
    return 0;
}
/**
 * @brief Parse a minecraft log file and calculate the playtime recorded by the log.
 * @param[in] path The path to the minecraft log file.
 * @param[out] time A `time_t` pointer for outputting time.
 * @return Return 0 on success, 1 on system failure, or 2 on parsing failure.
 */
int parseFile(const char *path, time_t *time)
{
    gzFile gf = gzopen(path, "r");
    if (gf == NULL)
        return 1;
    time_t start, end, tmp;
    while (parseLine(gf, &start) == 1)
        if (gzeof(gf) == 1)
            return 2;
    end = start;
    while (gzeof(gf) == 0)
        if (parseLine(gf, &tmp) == 0)
            end = tmp;
    gzclose(gf);
    *time = end - start;
    printf("%s: %d\n", path, *time);
    return 0;
}
/**
 * @brief Determine whether it is a minecraft `.log.gz` file by name.
 * @param[in] name The name of the file.
 * @return Return 1 on success, or 0 on failure.
 */
int isLogGzFile(const char *name)
{
    const char *format = "nnnn-nn-nn-n.log.gz";
    int i = 0;
    for (;;)
    {
        if (format[i] == 'n')
        {
            if (!isdigit(name[i]))
                return 0;
        }
        else
        {
            if (name[i] != format[i])
                return 0;
            if (format[i] == '\0')
                return 1;
        }
        i++;
    }
}
/**
 * @brief Change the working directory and terminate the program on failure.
 * @param[in] path The path to the new working directory.
 */
void chdir_s(const char *path)
{
    if (chdir(path) != 0)
    {
        fprintf(stderr, "FATAL ERROR: Fail to chdir to %s: %s", path, strerror(errno));
        exit(1);
    }
}
/**
 * @brief Parse a directory and calculate the playtime recorded by the log files within the directory.
 * @param[in] path The path to the log directory. If it is NULL, it will be treated as current working directory.
 * @param[out] time A `time_t` pointer for outputting time.
 * @param[in] origin The origin path. The function will chdir to it in the end if it isn't NULL.
 * @return Return the number of parsed files on success, or -1 on failure.
 * @note There is a more graceful way to realize by using openat() instead of chdir(), but MinGW doesn't support it.
 */
int parseDirectory(const char *path, time_t *time, const char *origin)
{
    if (path != NULL && chdir(path) != 0)
        return -1;
    DIR *dir = opendir(".");
    if (dir == NULL)
        return -1;
    struct dirent *entry;
    time_t sum = 0, tmp;
    int file = 0;
    while ((entry = readdir(dir)) != NULL)
    {
        if (isLogGzFile(entry->d_name))
            if (parseFile(entry->d_name, &tmp) == 0)
                sum += tmp, file++;
    }
    if (parseFile("latest.log", &tmp) == 0)
        sum += tmp, file++;
    closedir(dir);
    *time = sum;
    if (origin != NULL)
        chdir_s(origin);
    return file;
}
/**
 * @brief Parse a `.minecraft` directory and calculate the time recorded by the log files within each `logs` directory.
 * @param[in] path The path to the `.minecraft` directory. If it is NULL, it will be treated as current working directory.
 * @param[out] time A `time_t` pointer for outputting time.
 * @param[in] origin The origin path. The function will chdir to it in the end if it isn't NULL.
 * @return Return the number of parsed files on success, or -1 on failure.
 */
int parseDotMinecraftDirectory(const char *path, time_t *time, const char *origin)
{
    if (path != NULL && chdir(path) != 0)
        return -1;
    time_t sum = 0, tmp;
    int file = 0, ret;
    if ((ret = parseDirectory("logs", &tmp, "..")) != -1)
        sum += tmp, file += ret;
    if (chdir("versions") != 0)
        goto VERSIONS_END;
    DIR *dir = opendir(".");
    if (dir == NULL)
        goto DIR_END;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        if (chdir(entry->d_name) != 0)
            continue;
        if ((ret = parseDirectory("logs", &tmp, "..")) != -1)
            sum += tmp, file += ret;
        chdir_s("..");
    }
    closedir(dir);
    DIR_END:
    chdir_s("..");
    VERSIONS_END:
    *time = sum;
    if (origin != NULL)
        chdir_s(origin);
    return file;
}
/**
 * @brief Get the current working directory and terminate the program on failure.
 * @return Return the path to the current working directory.
 */
char *getcwd_s()
{
    char *ret = getcwd(NULL, 0);
    if (ret == NULL)
    {
        fprintf(stderr, "FATAL ERROR: Fail to get current working directory: %s\n", strerror(errno));
        exit(1);
    }
    return ret;
}
/**
 * @brief Automatically recognize whether the path points to a file, a log directory or a .minecraft directory and parse it.
 * @param[in] path The path to the file or directory. If it is NULL, it will be treated as current working directory.
 * @param[out] time A `time_t` pointer for outputting time.
 * @param[in] origin The origin path. The function will chdir to it in the end if it isn't NULL.
 * @return Return the number of parsed files on success, or -1 on failure.
 */
int autoParse(const char *path, time_t *time, const char *origin)
{
    time_t tmp;
    int file = 0, ret;
    struct stat status;
    if (path == NULL)
        path = ".";
    if (stat(path, &status) == -1)
    {
        fprintf(stderr, "ERROR: %s: %s\n", path, strerror(errno));
        return -1;
    }
    if (S_ISDIR(status.st_mode))
    {
        if (chdir(path) != 0)
        {
            fprintf(stderr, "ERROR: %s: %s\n", path, strerror(errno));
            return -1;
        }
        char *cwd = getcwd_s();
        char *name = basename(cwd);
        if (strcmp(name, ".minecraft") == 0)
        {
            switch(ret = parseDotMinecraftDirectory(NULL, &tmp, NULL))
            {
            case -1:
                fprintf(stderr, "ERROR: %s: %s\n", path, strerror(errno));
                return -1;
            case 0:
                fprintf(stderr, "WARNING: %s: No file parsed\n", path);
                return -1;
            default:
                file += ret;
            }
        }
        else
        {
            switch (ret = parseDirectory(NULL, &tmp, NULL))
            {
            case -1:
                fprintf(stderr, "ERROR: %s: %s\n", path, strerror(errno));
                return -1;
            case 0:
                fprintf(stderr, "WARNING: %s: No file parsed\n", path);
                return -1;
            default:
                file += ret;
            }
        }
        free(cwd);
        if (origin != NULL)
            chdir_s(origin);
    }
    else if (S_ISREG(status.st_mode))
    {
        switch (parseFile(path, &tmp))
        {
        case 1:
            fprintf(stderr, "ERROR: %s: %s\n", path, strerror(errno));
            return -1;
        case 2:
            fprintf(stderr, "ERROR: %s: Not a minecraft log file\n", path);
            return -1;
        default:
            file++;
        }
    }
    else
    {
        fprintf(stderr, "ERROR: %s: Not a directory or a regular file\n", path);
        return -1;
    }
    *time = tmp;
    return file;
}
int main(int argc, char* argv[])
{
    time_t sum = 0, tmp;
    int file = 0, ret;
    if (argc == 1)
    {
        puts(help);
        return 0;
    }
    else
    {
        char *origin = getcwd_s();
        for (int i = 1; i < argc; i++)
            if ((ret = autoParse(argv[i], &tmp, origin)) != -1)
                sum += tmp, file += ret;
        free(origin);
    }
    printf("%d files parsed\n", file);
    printf("total time: %d = %dh %dmin %ds\n", sum, sum / 60 / 60, sum / 60 % 60, sum % 60);
    return 0;
}
