[1mdiff --git a/src/zps.c b/src/zps.c[m
[1mindex afda877..b89bd2f 100644[m
[1m--- a/src/zps.c[m
[1m+++ b/src/zps.c[m
[36m@@ -31,18 +31,19 @@[m
 #include <time.h>[m
 #include <ftw.h>[m
 [m
[31m-static unsigned int fd,         /* File descriptor to be used in file operations */[m
[31m-    defunctCount = 0,           /* Number of found defunct processes */[m
[31m-    terminatedProcs = 0,        /* Number of terminated processes */[m
[31m-    procsChecked = 0;           /* Return value for the process check operation */[m
[31m-static bool terminate = false,  /* Boolean value for terminating defunct processes */[m
[31m-    showProcList = true;        /* Boolean value for listing the running processes */[m
[31m-static char *strPath,           /* String part of a path in '/proc' */[m
[31m-    fileContent[BLOCK_SIZE],    /* Text content of a file */[m
[31m-    match[BLOCK_SIZE/4],        /* Regex match */[m
[31m-    buff,                       /* Char variable that used as buffer in read */[m
[31m-    *statContent, *cmdContent;  /* Text content of the process' information file */[m
[31m-typedef struct {                /* Struct for storing process stats */[m
[32m+[m[32mstatic unsigned int fd;                  /* File descriptor to be used in file operations */[m
[32m+[m[32mstatic unsigned int defunctCount = 0;    /* Number of found defunct processes */[m
[32m+[m[32mstatic unsigned int terminatedProcs = 0; /* Number of terminated processes */[m
[32m+[m[32mstatic unsigned int procsChecked = 0;    /* Return value for the process check operation */[m
[32m+[m[32mstatic bool terminate = false;           /* Boolean value for terminating defunct processes */[m
[32m+[m[32mstatic bool showProcList = true;         /* Boolean value for listing the running processes */[m
[32m+[m[32mstatic char *strPath;                    /* String part of a path in '/proc' */[m
[32m+[m[32mstatic char fileContent[BLOCK_SIZE];     /* Text content of a file */[m
[32m+[m[32mstatic char match[BLOCK_SIZE/4];         /* Regex match */[m
[32m+[m[32mstatic char buff;                        /* Char variable that used as buffer in read */[m
[32m+[m[32mstatic char *statContent;                /* Text content of the process' stat file */[m
[32m+[m[32mstatic char *cmdContent;                 /* Text content of the process' command file */[m
[32m+[m[32mtypedef struct {                         /* Struct for storing process stats */[m
     unsigned int pid;[m
     unsigned int ppid;[m
     char name[BLOCK_SIZE/64];[m
[36m@@ -156,11 +157,11 @@[m [mstatic ProcStats getProcStats(const char *procPath) {[m
      */[m
     if ((regexec(&regex, statContent, REG_MAX_MATCH, regMatch, REG_NOTEOL))[m
         != REG_NOMATCH) {[m
[31m-        char *offsetBegin = statContent + regMatch[1].rm_so, /* Beginning offset of first match. */[m
[31m-            *offsetEnd    = statContent + regMatch[1].rm_eo, /* Ending offset of first match. */[m
[31m-            *contentDup   = strdup(statContent);             /* Duplicate of content for changing */[m
[31m-        int unsigned offsetSpace   = regMatch[1].rm_so - 1,  /* Offset of first space in content */[m
[31m-            matchLength   = (int) strcspn(offsetBegin, " "); /* Length of the match */[m
[32m+[m[32m        char *offsetBegin = statContent + regMatch[1].rm_so; /* Beginning offset of first match. */[m
[32m+[m[32m        char *offsetEnd   = statContent + regMatch[1].rm_eo; /* Ending offset of first match. */[m
[32m+[m[32m        char *contentDup  = strdup(statContent);             /* Duplicate of content for changing */[m
[32m+[m[32m        unsigned int offsetSpace = regMatch[1].rm_so - 1;    /* Offset of first space in content */[m
[32m+[m[32m        unsigned int matchLength = (int) strcspn(offsetBegin, " "); /* Length of the match */[m
         /* Check the match length for parsing or replacing spaces. */[m
         if (matchLength <= (regMatch[1].rm_eo-regMatch[1].rm_so)) {[m
             matchLength = 0;[m
