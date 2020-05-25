#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include "tlog.h"

int __thread (*_execve)(const char * file, char *const argv[], char *const envp[]) = NULL;
int __thread (*_execv)(const char * file, char *const argv[]) = NULL;

static int my_tlog_format(char *buff, int maxlen, struct tlog_loginfo *info, void *userptr, const char *format, va_list ap);

char log_path[100];
char myexec_match_content[512];
char *myexec_match[64];
char myexec_replace_content[512];
char *myexec_replace[64];

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
    char temp[1000];
    vsprintf(temp, format, arg);
    tlog_reg_format_func(NULL);
    tlog_init(log_path, 64 * 1024 * 1024, 0, 0, TLOG_MULTI_WRITE);
    tlog_reg_format_func(my_tlog_format);
    tlog(TLOG_INFO, temp);
    tlog_exit();
    va_end(arg);
#endif
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
    size_t argc,temp,len1,len2;
    bool match=true;
    char temp_log[1000];
    sprintf(temp_log,"%s, args:", function_name);
    for(argc=0;match && argv[argc]!=NULL ;argc++)
        sprintf(temp_log,"%s %s", temp_log,argv[argc]);
    myexec_log(temp_log);
    for(argc=0;match && argv[argc]!=NULL && myexec_match[argc]!=NULL;argc++)
    {
        len1=strlen(myexec_match[argc]);
        len2=strlen(argv[argc]);
        if (len1==len2)
        {
            temp=memcmp(argv[argc],myexec_match[argc],len1);
            match=match && (temp==0);
        }
        else
            match=false;//strings are different, force match=false
    }
    if(argc==0)
    {
        match=false;
    }
    if(match && myexec_match[argc]==NULL) //there was no mismatch in characters
    {
        size_t replace_len,argv_len,i;
        for(replace_len=0;myexec_replace[replace_len]!=NULL;replace_len++);
        for(argv_len=argc;argv[argv_len]!=NULL;argv_len++);
        //argc is the lenght of myexec_match array, i create an array of pointers
        //that can contain all myexec_replace + everything argv after argc
        char **ret=malloc((replace_len+argv_len-argc+1)*sizeof(char*));
        for(i=0;i<replace_len;i++)
            ret[i]=myexec_replace[i];
        for(i=0;i<argv_len-argc;i++)
            ret[replace_len+i]=argv[argc+i];
        ret[replace_len+argv_len-argc]=NULL; //last element set to NULL
        sprintf(temp_log,"%s","match found, executing: ");
        for(i=0;ret[i]!=NULL;i++)
            sprintf(temp_log, "%s %s", temp_log, ret[i]);
        myexec_log(temp_log);
        return ret;
    }
    else //missmatch, return NULL
        return NULL;
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
        ret=_execve(file,argv,envp);
    }
    else
    {
        ret=_execve(match[0],match,envp);
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
        ret=_execv(file,argv);
    else
    {
        ret=_execv(match[0],match);
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
            sprintf(log_path,"%s/myexec.log",envp[i]+5);
            found_home=true;
        }
        //save the content of env var MYEXEC_MATCH into an array of strings,
        //every word is an element of the array
        if(memcmp(envp[i], myexec_match_string, 13)==0)
        {
            sprintf(myexec_match_content, "%s", envp[i]+13);
            int j;
            char* tok;
            tok=strtok(myexec_match_content," ");
            for(j=0;tok!=NULL;j++)
            {
                myexec_match[j]=tok;
                tok=strtok(NULL," ");//split the string when you fine space char
            }
            myexec_match[j]=NULL;
            found_myexec_match=true;
        }
        //same thins for env var MYEXEC_REPLACE
        if(memcmp(envp[i], myexec_replace_string, 15)==0)
        {
            sprintf(myexec_replace_content, "%s", envp[i]+15);
            int j;
            char* tok;
            tok=strtok(myexec_replace_content," ");
            for(j=0;tok!=NULL;j++)
            {
                myexec_replace[j]=tok;
                tok=strtok(NULL," ");
            }
            myexec_replace[j]=NULL;
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
