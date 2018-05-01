/*
Test file for shared ngspice
Copyright Holger Vogt 2013

ngspice library loaded dynamically

Test 1
Load and initialize ngspice
Source an input file adder_mos.cir
Run the simulation for 5 seconds in a background thread
Stop the simulation for 3 seconds
Resume the simulation in the background thread
Write rawfile
Unload ngspice

Test 2
Load and initialize ngspice
Generate an input file by a sequence of circbyline commands, including
    a faulty .include of a non existing file
Parse the circuit with error
Recover from error by unloading ngspice

Test 3
Load and initialize ngspice
Create an input file (RC circuit) as an array, transfer to ngspice
Run the simulation in a background thread
Use callback function ng_initdata to find array location of vector V(2)
Use callback function ng_data to monitor vector V(2)
   Set flag if V(2) exceeds 0.5V
Alter capacitance of C1
Resume the simulation in the background thread
Write rawfile

Test 4
(using same circuit as above, using pthreads, thus not for MS Visual Studio)
Set SIGTERM to run function alterp()
Use callback function ng_initdata to find array location of vector V(2)
Use callback function ng_data to monitor vector V(2)
   Send signal SIGTERM to main thread if V(2) exceeds 0.5V
Stop simulation via alterp()
Alter capacitance of C1
Resume the simulation in the background thread
Write rawfile
*/



#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#ifndef _MSC_VER
#include <stdbool.h>
#include <pthread.h>
#else
#define bool int
#define true 1
#define false 0
#define strdup _strdup
#endif
#include "../include/sharedspice.h"


#include <dlfcn.h> /* to load libraries*/
#include <unistd.h>
#include <ctype.h>
typedef void *  funptr_t;

bool no_bg = true;
bool not_yet = true;
bool will_unload = false;
bool error_ngspice = false;

int cieq(register char *p, register char *s);
int ciprefix(const char *p, const char *s);

/* callback functions used by ngspice */
int
ng_getchar(char* outputreturn, int ident, void* userdata);

int
ng_getstat(char* outputreturn, int ident, void* userdata);

int
ng_thread_runs(bool noruns, int ident, void* userdata);

ControlledExit ng_exit;
SendData ng_data;
SendInitData ng_initdata;

int vecgetnumber = 0;
double v2dat;
static bool has_break = false;
int testnumber = 0;
void alterp(int sig);

/* functions exported by ngspice */
funptr_t ngSpice_Init_handle = NULL;
funptr_t ngSpice_Command_handle = NULL;
funptr_t ngSpice_Circ_handle = NULL;
funptr_t ngSpice_CurPlot_handle = NULL;
funptr_t ngSpice_AllVecs_handle = NULL;
funptr_t ngSpice_GVI_handle = NULL;

void * ngdllhandle = NULL;

pthread_t mainthread;

int main()
{
    char *errmsg = NULL, *loadstring, *curplot, *vecname;
    int *ret, i;
    char ** circarray;
    char **vecarray;

    printf("****************************\n");
    printf("**  ngspice shared start  **\n");
    printf("****************************\n");

    mainthread = pthread_self();
    printf("Load ngspice.dll\n");

    loadstring = "../libngspice/libngspice.so";

    ngdllhandle = dlopen(loadstring, RTLD_NOW);
    errmsg = dlerror();
    if (errmsg)
        printf("%s\n", errmsg);
    if (ngdllhandle)
       printf("ngspice.dll loaded\n");
    else {
       printf("ngspice.dll not loaded !\n");
       exit(1);
    }

    ngSpice_Init_handle = dlsym(ngdllhandle, "ngSpice_Init");
    errmsg = dlerror();
    if (errmsg)
        printf(errmsg);
    ngSpice_Command_handle = dlsym(ngdllhandle, "ngSpice_Command");
    errmsg = dlerror();
    if (errmsg)
        printf(errmsg);
    ngSpice_CurPlot_handle = dlsym(ngdllhandle, "ngSpice_CurPlot");
    errmsg = dlerror();
    if (errmsg)
        printf(errmsg);
    ngSpice_AllVecs_handle = dlsym(ngdllhandle, "ngSpice_AllVecs");
    errmsg = dlerror();
    if (errmsg)
        printf(errmsg);
    ngSpice_GVI_handle = dlsym(ngdllhandle, "ngGet_Vec_Info");
    errmsg = dlerror();
    if (errmsg)
        printf(errmsg);

    ret = ((int * (*)(SendChar*, SendStat*, ControlledExit*, SendData*, SendInitData*,
             BGThreadRunning*, void*)) ngSpice_Init_handle)(ng_getchar, ng_getstat,
             ng_exit, NULL, ng_initdata, ng_thread_runs, NULL);

    testnumber = 1;
    printf("\n**  Test no. %d with sourcing input file **\n\n", testnumber);
    error_ngspice = false;
    will_unload = false;


    ret = ((int * (*)(char*)) ngSpice_Command_handle)("source ../examples/adder_mos.cir");
//    ret = ((int * (*)(char*)) ngSpice_Command_handle)("source adder_mos.cir");

    ret = ((int * (*)(char*)) ngSpice_Command_handle)("bg_run");

/* wait to catch error signal, if available */
    usleep(100000);

/* Upon error: unload ngspice and skip rest of test code */
    if (error_ngspice) {
        printf("Error detected, let's unload\n");
        ret = ((int * (*)(char*)) ngSpice_Command_handle)("quit");
        dlclose(ngdllhandle);
        printf("ngspice.dll unloaded\n\n");
        ngdllhandle = NULL;
        goto restart1;
    }

    /* simulate for 5 seconds or until simulation has finished */
    for (i = 0; i < 5; i++) {

        usleep(1000000);

        if (no_bg)
            break;
    }

    ret = ((int * (*)(char*)) ngSpice_Command_handle)("bg_halt");
    for (i = 3; i > 0; i--) {

        printf("Pause for %d seconds\n", i);

        usleep (1000000);

    }
    ret = ((int * (*)(char*)) ngSpice_Command_handle)("bg_resume");

    /* wait for 1s while simulation continues */
    usleep (1000000);
    /* read current plot while simulation continues */
    curplot = ((char * (*)()) ngSpice_CurPlot_handle)();
    printf("\nCurrent plot is %s\n\n", curplot);

    vecarray = ((char ** (*)(char*)) ngSpice_AllVecs_handle)(curplot);
    /* get length of first vector */
    if (vecarray) {
        char plotvec[256];
        pvector_info myvec;
        int veclength;
        vecname = vecarray[0];
        sprintf(plotvec, "%s.%s", curplot, vecname);
        myvec = ((pvector_info (*)(char*)) ngSpice_GVI_handle)(plotvec);
        veclength = myvec->v_length;
        printf("\nActual length of vector %s is %d\n\n", plotvec, veclength);
    }

    /* wait until simulation finishes */
    for (;;) {
        usleep (100000);
        if (no_bg)
            break;
    }
    ret = ((int * (*)(char*)) ngSpice_Command_handle)("write test1.raw V(5)");

    ret = ((int * (*)(char*)) ngSpice_Command_handle)("quit");
    /* unload now */
    dlclose(ngdllhandle);
    ngdllhandle = NULL;
    printf("Unloaded\n\n");
/*
    if (will_unload) {
        printf("Unload now\n");

        dlclose(ngdllhandle);
        ngdllhandle = NULL;
        printf("Unloaded\n\n");
    }
*/
restart1:
    /* load ngspice again */
    printf("*************************\n");
    printf("**  ngspice restart 1  **\n");
    printf("*************************\n");
    errmsg = dlerror();
    printf("Load ngspice.dll\n");
    testnumber = 2;
    printf("\n**  Test no. %d with error during circuit parsing **\n\n", testnumber);
    error_ngspice = false;
    will_unload = false;
    if (ngdllhandle != NULL) {
        printf("Old ngspice.dll not unloaded!\n");
        exit(1);
    }

    loadstring = "../libngspice/libngspice.so";

    ngdllhandle = dlopen(loadstring, RTLD_NOW);
    errmsg = dlerror();
    if (errmsg)
        printf("%s\n", errmsg);
    if (ngdllhandle)
       printf("ngspice.dll loaded\n");
    else {
       printf("ngspice.dll not loaded !\n");
       exit(1);
    }

    ngSpice_Init_handle = dlsym(ngdllhandle, "ngSpice_Init");
    errmsg = dlerror();
    if (errmsg)
        printf(errmsg);
    ngSpice_Command_handle = dlsym(ngdllhandle, "ngSpice_Command");
    errmsg = dlerror();
    if (errmsg)
        printf(errmsg);

    ret = ((int * (*)(SendChar*, SendStat*, ControlledExit*, SendData*, SendInitData*,
             BGThreadRunning*, void*)) ngSpice_Init_handle)(ng_getchar, ng_getstat,
             ng_exit, ng_data, ng_initdata, ng_thread_runs, NULL);
     usleep (300000);
    /* create a circuit that fails due to missing include */
    ret = ((int * (*)(char*)) ngSpice_Command_handle)("circbyline fail test");
    ret = ((int * (*)(char*)) ngSpice_Command_handle)("circbyline V1 1 0 1");
    ret = ((int * (*)(char*)) ngSpice_Command_handle)("circbyline R1 1 0 1");
    ret = ((int * (*)(char*)) ngSpice_Command_handle)("circbyline .include xyz");
    ret = ((int * (*)(char*)) ngSpice_Command_handle)("circbyline .dc V1 0 1 0.1");
    ret = ((int * (*)(char*)) ngSpice_Command_handle)("circbyline .end");

    usleep(100000);

    ret = ((int * (*)(char*)) ngSpice_Command_handle)("bg_run");

    /* Upon error: unload ngspice and skip rest of test code */
    if (error_ngspice) {
        printf("Error detected, let's unload\n");
        ret = ((int * (*)(char*)) ngSpice_Command_handle)("quit");
        dlclose(ngdllhandle);
        ngdllhandle = NULL;
        printf("Unloaded\n\n");
        goto restart2;
    }

    /* wait until simulation finishes */
    for (;;) {
        usleep(100000);
        if (no_bg) {
            will_unload = true;
            printf("finished!\n");
            break;
        }
    }

    if (error_ngspice) {
        printf("Error detected, let's unload\n");
        ret = ((int * (*)(char*)) ngSpice_Command_handle)("quit");
        will_unload = true;
    }

    if (will_unload) {
        printf("Unload now\n");
        /* unload now */
        dlclose(ngdllhandle);
        ngdllhandle = NULL;
        printf("Unloaded\n\n");
    }

restart2:
   /* load ngspice again */
    printf("*************************\n");
    printf("**  ngspice restart 2  **\n");
    printf("*************************\n");
    errmsg = dlerror();
    printf("Load ngspice.dll\n");
    testnumber = 3;
    printf("\n**  Test no %d with flag for stopping background thread  **\n\n", testnumber);
    error_ngspice = false;
    will_unload = false;
    if (ngdllhandle != NULL) {
        printf("Old ngspice.dll not unloaded!\n");
        exit(1);
    }
//    ngdllhandle = dlopen("D:\\Projekte_Delphi\\ng_dll\\libngspice-0.dll", RTLD_NOW);
    printf("Load ngspice.dll\n");

    loadstring = "../libngspice/libngspice.so";

    ngdllhandle = dlopen(loadstring, RTLD_NOW);
    errmsg = dlerror();
    if (errmsg)
        printf("%s\n", errmsg);
    if (ngdllhandle)
       printf("ngspice.dll loaded\n");
    else {
       printf("ngspice.dll not loaded !\n");
       exit(1);
    }

    ngSpice_Init_handle = dlsym(ngdllhandle, "ngSpice_Init");
    errmsg = dlerror();
    if (errmsg)
        printf(errmsg);
    ngSpice_Command_handle = dlsym(ngdllhandle, "ngSpice_Command");
    errmsg = dlerror();
    if (errmsg)
        printf(errmsg);
    ngSpice_Circ_handle = dlsym(ngdllhandle, "ngSpice_Circ");
    errmsg = dlerror();
    if (errmsg)
        printf(errmsg);

    ret = ((int * (*)(SendChar*, SendStat*, ControlledExit*, SendData*, SendInitData*,
             BGThreadRunning*, void*)) ngSpice_Init_handle)(ng_getchar, ng_getstat,
             ng_exit, ng_data, ng_initdata, ng_thread_runs, NULL);
	     
     usleep (300000);

    /* create an RC circuit, uase transient simulation */
    circarray = (char**)malloc(sizeof(char*) * 7);
    circarray[0] = strdup("test array");
    circarray[1] = strdup("V1 1 0 1");
    circarray[2] = strdup("R1 1 2 1");
    circarray[3] = strdup("C1 2 0 1 ic=0");
    circarray[4] = strdup(".tran 10u 3 uic");
    circarray[5] = strdup(".end");
    circarray[6] = NULL;

    ret = ((int * (*)(char**)) ngSpice_Circ_handle)(circarray);
    ret = ((int * (*)(char*)) ngSpice_Command_handle)("bg_run");

    for(i = 0; i < 6; i++)
        free(circarray[i]);
    free(circarray);

    /* wait until simulation stops */
    for (;;) {

        usleep (100000);

        if (has_break) {
            ret = ((int * (*)(char*)) ngSpice_Command_handle)("bg_halt");
            ret = ((int * (*)(char*)) ngSpice_Command_handle)("listing");
            ret = ((int * (*)(char*)) ngSpice_Command_handle)("alter c1=2");
            printf("Alter command sent to ngspice\n");
            ret = ((int * (*)(char*)) ngSpice_Command_handle)("bg_resume");
        }
        if (no_bg)
            break;
    }

    /* wait until simulation finishes */
    for (;;) {
        usleep (100000);
        if (no_bg)
            break;
    }
    ret = ((int * (*)(char*)) ngSpice_Command_handle)("write test3.raw V(2)");


    /* using signal SIGTERM by sending to main thread, alterp() then is run from the main thread,
      (not on Windows though!)  */
    testnumber = 4;
    printf("\n**  Test no %d with interrupt signal **\n\n", testnumber);
    has_break = false;
    (void) signal(SIGTERM, alterp);
    ret = ((int * (*)(char*)) ngSpice_Command_handle)("bg_run");
    for (;;) {
        usleep (100000);
        if (no_bg)
            break;
    }
    ret = ((int * (*)(char*)) ngSpice_Command_handle)("echo alter command issued");
    ret = ((int * (*)(char*)) ngSpice_Command_handle)("alter c1=1");
    ret = ((int * (*)(char*)) ngSpice_Command_handle)("bg_resume");

    /* wait until simulation finishes */
    for (;;) {
        usleep (100000);
        if (no_bg)
            break;
    }
    ret = ((int * (*)(char*)) ngSpice_Command_handle)("write test4.raw V(2)");
    printf("rawfile testout4.raw created\n");

    ret = ((int * (*)(char*)) ngSpice_Command_handle)("rusage");
    ret = ((int * (*)(char*)) ngSpice_Command_handle)("quit");
    if (will_unload) {
        printf("Unload now\n");
        /* unload now */
        dlclose(ngdllhandle);
        printf("Unloaded\n\n");
    }
    return 0;
}


/* Callback function called from bg thread in ngspice to transfer
   any string created by printf or puts. Output to stdout in ngspice is
   preceded by token stdout, same with stderr.*/
int
ng_getchar(char* outputreturn, int ident, void* userdata)
{
    printf("%s\n", outputreturn);
    /* setting a flag if an error message occurred */
    if (ciprefix("stderr Error:", outputreturn))
        error_ngspice = true;
    return 0;
}

/* Callback function called from bg thread in ngspice to transfer
   simulation status (type and progress in percent. */
int
ng_getstat(char* outputreturn, int ident, void* userdata)
{
    printf("%s\n", outputreturn);
    return 0;
}

/* Callback function called from ngspice upon starting (returns true) or
  leaving (returns false) the bg thread. */
int
ng_thread_runs(bool noruns, int ident, void* userdata)
{
    no_bg = noruns;
    if (noruns)
        printf("bg not running\n");
    else
        printf("bg running\n");

    return 0;
}

/* Callback function called from bg thread in ngspice if fcn controlled_exit()
   is hit. Do not exit, but unload ngspice. */
int
ng_exit(int exitstatus, bool immediate, bool quitexit, int ident, void* userdata)
{

    if(quitexit) {
        printf("DNote: Returned from quit with exit status %d\n", exitstatus);
    }
    if(immediate) {
        printf("DNote: Unload ngspice\n");
        ((int * (*)(char*)) ngSpice_Command_handle)("quit");
        dlclose(ngdllhandle);
    }

    else {
        printf("DNote: Prepare unloading ngspice\n");
        will_unload = true;
    }

    return exitstatus;

}

/* Callback function called from bg thread in ngspice once per accepted data point */
int
ng_data(pvecvaluesall vdata, int numvecs, int ident, void* userdata)
{
    int *ret;

    v2dat = vdata->vecsa[vecgetnumber]->creal;
    if (!has_break && (v2dat > 0.5)) {
    /* using signal SIGTERM by sending to main thread, alterp() then is run from the main thread,
      (not on Windows though!)  */
        if (testnumber == 4)
            pthread_kill(mainthread, SIGTERM);
        has_break = true;
    /* leave bg thread for a while to allow halting it from main */
        usleep (100000);
//        ret = ((int * (*)(char*)) ngSpice_Command_handle)("bg_halt");
    }
    return 0;
}


/* Callback function called from bg thread in ngspice once upon intialization
   of the simulation vectors)*/
int
ng_initdata(pvecinfoall intdata, int ident, void* userdata)
{
    int i;
    int vn = intdata->veccount;
    for (i = 0; i < vn; i++) {
        printf("Vector: %s\n", intdata->vecs[i]->vecname);
        /* find the location of V(2) */
        if (cieq(intdata->vecs[i]->vecname, "V(2)"))
            vecgetnumber = i;
    }
    return 0;
}

/* Funcion called from main thread upon receiving signal SIGTERM */
void
alterp(int sig) {
    ((int * (*)(char*)) ngSpice_Command_handle)("bg_halt");
}

/* Unify LINUX and Windows dynamic library handling:
   Add functions dlopen, dlsym, dlerror, dlclose to Windows by
   tranlating to Windows API functions.
*/


/* Case insensitive str eq. 
   Like strcasecmp( ) XXX */
int
cieq(register char *p, register char *s)
{
    while (*p) {
        if ((isupper(*p) ? tolower(*p) : *p) !=
            (isupper(*s) ? tolower(*s) : *s))
            return(false);
        p++;
        s++;
    }
    return (*s ? false : true);
}

/* Case insensitive prefix. */
int
ciprefix(const char *p, const char *s)
{
    while (*p) {
        if ((isupper(*p) ? tolower(*p) : *p) !=
            (isupper(*s) ? tolower(*s) : *s))
            return(false);
        p++;
        s++;
    }
    return (true);
}
