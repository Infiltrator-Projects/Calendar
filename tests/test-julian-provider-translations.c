// SPDX-License-Identifier: GPL-3.0-or-later
// Exercise the independently installed Julian calendar provider without Python.
// The ABI is owned by Common; do not duplicate its struct layout in the test.
#define _POSIX_C_SOURCE 200809L
#include <infiltratr/temporal_calendar.h>
#include <dlfcn.h>
#include <locale.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failed(const char *what) {
    fprintf(stderr,"Julian provider translations: %s\n",what);
    return 1;
}
int main(int argc,char **argv) {
    if(argc!=2)return failed("Usage: test-julian-provider-translations MODULE.so");
    if(setlocale(LC_ALL,"")==NULL)
        return failed("Cannot configure locale");
    void *handle=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);
    if(handle==NULL)return failed(dlerror());
    const void *address=dlsym(handle,INFILTRATR_TEMPORAL_CALENDAR_PROVIDER_SYMBOL);
    if(address==NULL){dlclose(handle);return failed("Provider symbol is missing");}
    InfiltratrTemporalCalendarProviderEntry entry=NULL;
    // POSIX dlsym is a void*; memcpy avoids an ISO C object/function pointer cast.
    if(sizeof(entry)!=sizeof(address)){dlclose(handle);return failed("Unsupported function pointer representation");}
    memcpy(&entry,&address,sizeof(entry));
    if(entry==NULL){dlclose(handle);return failed("Provider entry point is NULL");}
    const InfiltratrTemporalCalendarProvider *provider=entry();
    if(provider==NULL||provider->struct_size<sizeof(*provider)||
       provider->abi_version!=INFILTRATR_TEMPORAL_CALENDAR_PROVIDER_ABI||
       provider->calendar_id==NULL||strcmp(provider->calendar_id,"julian")!=0||
       provider->format==NULL){
        dlclose(handle);return failed("Julian provider identity or ABI is invalid");
    }
    const struct {
        const char *language;
        const char *expected;
    } cases[]={
        {"en_US","January"},
        {"fr","Janvier"}
    };
    for(size_t i=0;i<sizeof(cases)/sizeof(cases[0]);i++){
        if(setenv("LANGUAGE",cases[i].language,1)!=0){
            dlclose(handle);return failed("Cannot set translation language");
        }
        char output[INFILTRATR_TEMPORAL_CALENDAR_TEXT_CAPACITY]={0};
        size_t length=0;
        // JDN 2460324 is 1 January 2024 in the Julian chronology.
        if(!provider->format(provider,INT64_C(2460324),
                             INFILTRATR_TEMPORAL_CALENDAR_DATE_PART_MONTH,
                             output,sizeof(output),&length)||
           length>=sizeof(output)||output[length]!='\0'||
           length!=strlen(output)||strcmp(output,cases[i].expected)!=0){
            fprintf(stderr,"Julian %s: expected '%s', received '%s'\n",
                    cases[i].language,cases[i].expected,output);
            dlclose(handle);return 1;
        }
    }
    dlclose(handle);
    puts("Julian provider embedded translations remain available");
    return 0;
}
