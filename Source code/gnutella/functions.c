//
//  functions.c
//  gnutella
//
//  Created by Cyril Trosset and Uzair Ahmed on 23/10/2013.
//  Copyright (c) 2013. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "functions.h"
#include <dirent.h>

void splitHeader(char string[],char type[], char uid[],int *ttl, char filename[], char ip[],int *port, int* versionNumber)
{
    char cut[300];
    strcpy(cut, string);
    char* firstResult = strtok(cut,":");
    strcpy(type, firstResult);
    int i=1;
    while (i<7)
    {
        char* result = strtok(NULL,":");
        
        switch (i) {
            case 1:
                strcpy(uid, result);
                break;
            case 2:
                *ttl = strtoint(result);
                break;
            case 3:
                strcpy(filename, result);
                break;
            case 4:
                strcpy(ip, result);
                break;
            case 5:
                *port = strtoint(result);
                break;
            case 6:
                *versionNumber = strtoint(result);
                break;
                
            default:
                break;
        }
        i++;
    }
}
void constructHeader(char string[],char type[], char uid[],int *ttl, char filename[], char ip[],int *port, int* versionNumber)
{
    strcpy(string,type);
    strcat(string,":");
    
    strcat(string,uid);
    strcat(string,":");
    
    char str[3];
    sprintf(str, "%d", *ttl);
    strcat(string,str);
    strcat(string,":");
    
    strcat(string,filename);
    strcat(string,":");
    
    strcat(string,ip);
    strcat(string,":");
    
    char str2[6];
    sprintf(str2, "%d", *port);
    strcat(string,str2);
    strcat(string,":");
    
    char str3[6];
    sprintf(str3, "%d", *versionNumber);
    strcat(string,str3);
}
int strtoint_n(char* str, int n)
{
    int sign = 1;
    int place = 1;
    int ret = 0;
    
    int i;
    for (i = n-1; i >= 0; i--, place *= 10)
    {
        int c = str[i];
        switch (c)
        {
            case 45:
                if (i == 0) sign = -1;
                else return -1;
                break;
            default:
                if (c >= 48 && c <= 57) ret += (c - 48) * place;
                else return -1;
        }
    }
    
    return sign * ret;
}
int strtoint(char* str)
{
    char* temp = str;
    int n = 0;
    while (*temp != '\0')
    {
        n++;
        temp++;
    }
    return strtoint_n(str, n);
}
int stringLength(char string[])
{
    for(int i=0;i>=0;i++)
    {
        if(string[i]=='\0')
            return i;
    }
    
    return 0;
}
int compareStringStart(char string[],char start[])
{
    int match = 1;
    for(int i=0;i<stringLength(start);i++)
    {
        if(start[i]!=string[i])
            match=0;
    }
    
    return match;
}
void determineNeighbors(char *path,char ipNeighbors[][16],int portNeighbors[],int *numberOfNeighbors)
{
    FILE * fp;
    char * line = NULL;
    size_t len = 0;
    ssize_t read;
    
    *numberOfNeighbors = 0;
    
    fp = fopen(path, "r");
    if (fp == NULL)
        exit(EXIT_FAILURE);
    
    while ((read = getline(&line, &len, fp)) != -1) {
        strtok(line,":");
        strcpy(ipNeighbors[*numberOfNeighbors], line);
        char *result = strtok(NULL, ":");
        char port[7];
        strcpy(port, result);
        if(port[strlen(port)-1] == '\n')
            port[strlen(port)-1] = '\0';
        portNeighbors[*numberOfNeighbors] = strtoint(port);
        (*numberOfNeighbors)++;
    }
    
    if (line)
        free(line);
}
