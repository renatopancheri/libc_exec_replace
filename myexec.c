#define _GNU_SOURCE
#define PCRE2_CODE_UNIT_WIDTH 8
#include <dlfcn.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include "tlog.h"
#include <pcre2.h>

int __thread (*_execve)(const char * file, char *const argv[], char *const envp[]) = NULL;
int __thread (*_execv)(const char * file, char *const argv[]) = NULL;
int __thread (*_execvpe)(const char * file, char *const argv[], char *const envp[]) = NULL;
int __thread (*_execvp)(const char * file, char *const argv[]) = NULL;

static int my_tlog_format(char *buff, int maxlen, struct tlog_loginfo *info, void *userptr, const char *format, va_list ap);

#define MYEXEC_VECTOR_SIZE 8192
#define MYEXEC_MAX_WORDS 64
char log_path[100];
char myexec_match_content[MYEXEC_VECTOR_SIZE];
char myexec_replace_content[MYEXEC_VECTOR_SIZE];
char *myexec_return[MYEXEC_MAX_WORDS];

/**
 * This function is a wrapper of fprintf 
 * that prints to the file ~/myexec.log
 * Params:
 *     format: usual format for all stdio prints: "test %s"
 *     ...: usual params for all stdio prints
 */
void myexec_log(const char* format, ...)
{
#ifdef VERBOSE
    va_list arg;
    va_start (arg, format);
    char temp[MYEXEC_VECTOR_SIZE];
    vsprintf(temp, format, arg);
    tlog_reg_format_func(NULL);
    tlog_init(log_path, 64 * 1024 * 1024, 0, 0, TLOG_MULTI_WRITE);
    tlog_reg_format_func(my_tlog_format);
    tlog(TLOG_INFO, "%s", temp);
    tlog_exit();
    va_end(arg);
#endif
}

int split_in_words(char** words, char* string)
{
    int i;
    char* tok;
    tok=strtok(string, " ");
    //tok=string;
    for(i=0;tok!=NULL;i++)
    {
        words[i]=tok;
        //tok=strchr(tok+1, ' ');//split the string when you find space char
        tok=strtok(NULL, " ");//split the string when you find space char
    }
    words[i]=NULL;
    i++;
    return i;
}

/**
 * This function compares regex_match from environment variable
 * with the stuff passed as argument
 * Params:
 *     function_name: used only for logging
 *     argv: array of pointers(strings) in which every string
 *         is a word in the command line
 */
char** myexec_compare(const char* function_name, char *const argv[])
{
    size_t argc,pos=0;
    bool match=true;
    char temp[MYEXEC_VECTOR_SIZE];
    pcre2_code *re=NULL;
    int errornumber;
    PCRE2_SIZE erroroffset;
    pcre2_match_data *match_data=NULL;
    PCRE2_SIZE buffer_size=MYEXEC_VECTOR_SIZE;
    PCRE2_UCHAR buffer[MYEXEC_VECTOR_SIZE];
    for(argc=0;match && argv[argc]!=NULL ;argc++)
    {
        strcpy(&temp[pos], argv[argc]);
        pos=pos+strlen(argv[argc]);
        temp[pos]=' ';
        pos++;
    }
    temp[pos-1]='\0';
    myexec_log("%s, args:%s", function_name, temp);
    re = pcre2_compile(
        myexec_match_content,               /* the pattern */
        PCRE2_ZERO_TERMINATED, /* indicates pattern is zero-terminated */
        0,                     /* default options */
        &errornumber,          /* for error number */
        &erroroffset,          /* for error offset */
        NULL);
    if (re == NULL)
    {
        PCRE2_UCHAR buffer[256];
        pcre2_get_error_message(errornumber, buffer, sizeof(buffer));
        myexec_log("PCRE2 compilation failed at offset %d: %s", (int)erroroffset,buffer);
        return NULL;
    }
    match_data = pcre2_match_data_create_from_pattern(re, NULL);
    if(match_data == NULL)
    {
        pcre2_code_free(re);
        myexec_log("PCRE2 match data create failed!");
        return NULL;
    }
    errornumber=pcre2_substitute(
        re,
        temp,
        strlen(temp),
        0,
        PCRE2_SUBSTITUTE_GLOBAL,
        match_data,
        NULL,
        myexec_replace_content,
        strlen(myexec_replace_content),
        buffer,
        &buffer_size
    );
    pcre2_match_data_free(match_data);
    pcre2_code_free(re);
    if(errornumber<0)
    {
        pcre2_get_error_message(errornumber, buffer, sizeof(buffer));
        myexec_log("PCRE2 substitute failed: %s", buffer);
        return NULL;
    }
    if(errornumber==0)
        return NULL;
    else
    {
        size_t i,words;
        myexec_log("match found, executing: %s", buffer);
        words=split_in_words(myexec_return, buffer);
        char **ret=malloc(words * sizeof(char*));
        for(i=0;i<words;i++)
        {
            ret[i]=myexec_return[i];
        }
        return ret;
    }
}

/**
 * Redefinition of execve, _execve is the original function
 */
int execve(const char *file, char *const argv[], char *const envp[])
{
    int ret;
    char** match=myexec_compare("execve", argv);
    if (_execve==NULL)
        _execve = (int (*)(const char * file, char *const argv[], char *const envp[])) dlsym(RTLD_NEXT, "execve");
    if(match==NULL)
    {
        ret=_execve(file, argv, envp);
    }
    else
    {
        ret=_execvpe(match[0], match, envp);
        free(match);
    }
    return ret;
}

/**
 * Redefinition of execv, _execv is the original function
 */
int execv(const char *file, char *const argv[])
{
    int ret;
    char** match=myexec_compare("execv", argv);
    if(_execv==NULL)
        _execv = (int (*)(const char * file, char *const argv[])) dlsym(RTLD_NEXT, "execv");
    if(match==NULL)
        ret=_execv(file, argv);
    else
    {
        ret=_execvp(match[0], match);
        free(match);
    }
    return ret;
}
/**
 * Still to implement this functions, looks like most apllications do not use this glibc functions
 *
 */
/*int execvp(const char *file, char *const argv[])
{
    myexec_log("qweqeqwewq\n");
    return 0;
}
int execl (const char *path, const char *arg, ...)
{
    myexec_log("qweqeqwewq\n");
    return 0;
}
int execle (const char *path, const char *arg, ...)
{
    myexec_log("qweqeqwewq\n");
    return 0;
}
int execlp (const char *file, const char *arg, ...)
{
    myexec_log("qweqeqwewq\n");
    return 0;
}*/
/* Trampoline for the real main() */
static int (*main_orig)(int, char **, char **);

/* Our fake main() that gets called by __libc_start_main() */
int main_hook(int argc, char **argv, char **envp)
{
    const char home[]="HOME=";
    const char myexec_match_string[]="MYEXEC_MATCH=";
    const char myexec_replace_string[]="MYEXEC_REPLACE=";
    bool found_myexec_match=false, found_myexec_replace=false, found_home=false;
    int i=0;
    for (i=0;envp[i]!=NULL;i++)
    {
        if(memcmp(envp[i], home, 5)==0) //HOME env var, used to set path of log file
        {
            sprintf(log_path, "%s/myexec.log", envp[i]+5);
            found_home=true;
        }
        //save the content of env var MYEXEC_MATCH into an array of strings,
        //every word is an element of the array
        if(memcmp(envp[i], myexec_match_string, 13)==0)
        {
            sprintf(myexec_match_content, "%s", envp[i]+13);
            found_myexec_match=true;
        }
        //same thins for env var MYEXEC_REPLACE
        if(memcmp(envp[i], myexec_replace_string, 15)==0)
        {
            sprintf(myexec_replace_content, "%s", envp[i]+15);
            found_myexec_replace=true;
        }
    }
    if (!found_myexec_match || !found_myexec_replace || !found_home)
    {
        printf("Missing environment variables: %s %s %s\n", found_myexec_match ? "true" : "false", found_myexec_replace ? "true" : "false", found_home ? "true" : "false");
        return 1;
    }
    myexec_log("process started, MATCH=%s, REPLACE=%s\n", myexec_match_content, myexec_replace_content);
    //save the original functions that we are redefining
    _execve = (int (*)(const char * file, char *const argv[], char *const envp[])) dlsym(RTLD_NEXT, "execve");
    _execv = (int (*)(const char * file, char *const argv[])) dlsym(RTLD_NEXT, "execv");
    _execvpe = (int (*)(const char * file, char *const argv[], char *const envp[])) dlsym(RTLD_NEXT, "execvpe");
    _execvp = (int (*)(const char * file, char *const argv[])) dlsym(RTLD_NEXT, "execvp");
    int ret = main_orig(argc, argv, envp);
    return ret;
}

/*
 * Wrapper for __libc_start_main() that replaces the real main
 * function with our hooked version.
 */
int __libc_start_main(
    int (*main)(int, char **, char **),
    int argc,
    char **argv,
    int (*init)(int, char **, char **),
    void (*fini)(void),
    void (*rtld_fini)(void),
    void *stack_end)
{
    /* Save the real main function address */
    main_orig = main;

    /* Find the real __libc_start_main()... */
    typeof(&__libc_start_main) orig = dlsym(RTLD_NEXT, "__libc_start_main");

    /* ... and call it with our custom main function */
    return orig(main_hook, argc, argv, init, fini, rtld_fini, stack_end);
}


static int my_tlog_format(char *buff, int maxlen, struct tlog_loginfo *info, void *userptr, const char *format, va_list ap)
{
    int len = 0;
    int total_len = 0;
    struct tlog_time *tm = &info->time;
    void* unused __attribute__ ((unused));

    unused = userptr;

    //if (tlog.root->multi_log) {
        /* format prefix */
        len = snprintf(buff, maxlen, "[%.4d-%.2d-%.2d %.2d:%.2d:%.2d,%.3d][%5d][%4s] ",
            tm->year, tm->mon, tm->mday, tm->hour, tm->min, tm->sec, tm->usec / 1000, getpid(),
            tlog_get_level_string(info->level));
    /*} else {
        len = snprintf(buff, maxlen, "[%.4d-%.2d-%.2d %.2d:%.2d:%.2d,%.3d][%5s] ",
            tm->year, tm->mon, tm->mday, tm->hour, tm->min, tm->sec, tm->usec / 1000,
            tlog_get_level_string(info->level));
    }*/

    if (len < 0 || len >= maxlen) {
        return -1;
    }
    buff += len;
    total_len += len;
    maxlen -= len;

    /* format log message */
    len = vsnprintf(buff, maxlen, format, ap);
    if (len < 0 || len == maxlen) {
        return -1;
    }
    buff += len;
    total_len += len;

    /* return total length */
    return total_len;
}
