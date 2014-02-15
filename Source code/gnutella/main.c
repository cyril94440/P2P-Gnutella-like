//
//  main.c
//  gnutella
//
//  Created by Cyril Trosset and Uzair Ahmed on 23/10/2013.
//  Copyright (c) 2013. All rights reserved.
//

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket(s) close(s)
typedef int SOCKET;
typedef struct sockaddr_in SOCKADDR_IN;
typedef struct sockaddr SOCKADDR;

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <dirent.h>
#include "functions.h"
#include <sys/time.h>

#define PORT_CLIENT 5001
#define SHARED_FOLDER "/Users/Cyril/Desktop/p2p/uploadp2p/"
#define SHARED_FOLDER_OWNER "/Users/Cyril/Desktop/p2p/uploadp2pOWNED/"
#define MAX_FILES 10
#define MAX_QUERIES 10
#define TTL 3

#define PUSH 1
#define PULL 0
#define TTR 10


pthread_mutex_t lock;
pthread_mutex_t downloadMutex;

/* GLOBAL VARs */
char queries[MAX_QUERIES][300];
int iQueries = 0;
int numberOfNeighbors = 0;
char ipNeighbors[10][16];
int portNeighbors[10];
char waitingQuery[10] = "";
int waitingQueryType = 0;
char waitingQueryFileName[100] = "";
int stop=0;
int versionsForOwnedFiles[MAX_FILES];

char filenames[MAX_FILES][50];
int versionsOfDownloadedFiles[MAX_FILES];
int downloadedFiles = 0;
char ipFileOwner[MAX_FILES][16];
int portFileOwner[MAX_FILES];
int ttrForFile[MAX_FILES];
int globalTtrForFile[MAX_FILES];

SOCKET sockServer;
SOCKADDR_IN sinServer;
socklen_t recsizeServer = sizeof(sinServer);

/* THREAD FUNCTIONS */
void* launchServer();
void* newClient(void* sockClient);
void* ttrFunction();

/* SOCKET FUNCTIONS */
void sendHeader(char header[],char ipDest[],int portDest);
void downloadfile(char ip[],int port,char filename[],char uid[],int ttl,int versionNumber);

/* OTHER FUNCTIONS */
int hasQuery(char uid[]);
void addQuery(char header[],char ip[]);
int checkForFile(char filename[]);
int incrementVersionNumberForFile(char filename[]);
int getVersionNumberOfFile(char filename[]);
int indexOfFile(char filename[]);


/* STRUCT */
typedef struct infosSock
{
    int *sock;
    SOCKADDR_IN *sin;
} infosSock;

/*CHRONO VARS*/
struct timeval tim;
double t1;

int main(int argc, const char * argv[])
{
    srand((int)time(NULL));

    //DETERMINE NEIGHBORS CONFIGURATION
    char pathToConf[100];
    strcpy(pathToConf,SHARED_FOLDER_OWNER);
    strcat(pathToConf,"config.txt");
    determineNeighbors(pathToConf, ipNeighbors, portNeighbors, &numberOfNeighbors);

    //Index the owned files
    DIR *dir;
    int iFile = 0;
    struct dirent *ent;
    if ((dir = opendir (SHARED_FOLDER_OWNER)) != NULL) {
        while ((ent = readdir (dir)) != NULL)
        {
            if(ent->d_name[0]!='.')
            {
                versionsForOwnedFiles[iFile]=1;

                iFile++;
            }
        }
        closedir (dir);
    } else {
        /* could not open directory */
        perror ("");
    }


    // LAUNCH THE SERVER
    pthread_t serverThread;
    pthread_create(&serverThread, NULL, launchServer, NULL);

    // LAUNCH THE TTR THREAD
    if(PULL==1)
    {
        pthread_t ttrThread;
        pthread_create(&ttrThread, NULL, ttrFunction, NULL);
    }

    int choice = -1;

    printf("MENU\n1:Search file\n2:Download file\n");
    printf("3:Increment file version\n");
    printf("0:EXIT\n\n");

    fflush(stdout);
    while(choice!=0)
    {
        scanf("%d",&choice);
        if(choice==2||choice==1)
        {
            waitingQueryType = choice;

            char input[100];
            if(waitingQueryType==1)
                printf("\nEnter your search (* for all files) : ");
            else if(waitingQueryType==2)
                printf("\nEnter the exact filename you want : ");
            fflush(stdout);
            scanf("%s",input);

            char type[10],filename[100],ip[16],uid[10];
            int ttl,port;

            strcpy(waitingQueryFileName, input);
            strcpy(type, "query");
            strcpy(filename, input);
            strcpy(ip,"127.0.0.1");
            ttl=3;
            port=PORT_CLIENT;
            int ttlInt = rand()%100000;
            sprintf(uid, "%d",ttlInt);
            int versionNumber = 0;

            char header[300];
            constructHeader(header, type, uid, &ttl, filename, ip, &port,&versionNumber);

            addQuery(header, "127.0.0.1");
            strcpy(waitingQuery, uid);

            //START CHRONO
            gettimeofday(&tim, NULL);
            t1=tim.tv_sec+(tim.tv_usec/1000000.0);
            //START CHRONO END

            for(int i=0;i<numberOfNeighbors;i++)
            {
                sendHeader(header, ipNeighbors[i], portNeighbors[i]);
            }

        }
        else if(choice==3)
        {
            //Increase version and broadcast invalidate message.
            char fileToInvalidate[50];
            printf("Which file you want to invalidate ? \n");
            scanf("%s",fileToInvalidate);

            int versionNumber = incrementVersionNumberForFile(fileToInvalidate);

            if(versionNumber==0)
                printf("File not found");
            else if(PUSH==1)
            {

                char type[10],filename[100],ip[16],uid[10];
                int ttl,port;

                strcpy(type, "inval");
                strcpy(filename, fileToInvalidate);
                int randNumber = rand()%100000;
                sprintf(uid, "%d",randNumber);
                ttl=3;
                char header[300];
                strcpy(ip,"127.0.0.1");
                port = PORT_CLIENT;

                constructHeader(header, type, uid, &ttl, fileToInvalidate, ip, &port, &versionNumber);

                for(int i=0;i<numberOfNeighbors;i++)
                {
                    sendHeader(header, ipNeighbors[i], portNeighbors[i]);
                }

                printf("\n file successfully incremented \n");
            }

        }

    }

    stop = 1;
    closesocket(sockServer);
    pthread_join(serverThread, NULL);



    return 0;
}

void* launchServer()
{
    SOCKADDR_IN csin;
    socklen_t crecsize = sizeof(csin);

    int sock_err;

    sockServer = socket(AF_INET, SOCK_STREAM, 0);

    if(sockServer!=INVALID_SOCKET)
    {

        /* Configuration */
        sinServer.sin_addr.s_addr = htonl(INADDR_ANY);
        sinServer.sin_family = AF_INET;
        sinServer.sin_port = htons(PORT_CLIENT);
        bind(sockServer, (SOCKADDR*)&sinServer, recsizeServer);

        sock_err = listen(sockServer, 5);

        /* Main loop */
        while (stop==0) {
            int newsock = accept(sockServer, (SOCKADDR*)&csin, &crecsize);
            if (newsock == -1) {
                perror("accept");
            }
            else {
                pthread_mutex_lock(&lock);

                infosSock sockClient;
                sockClient.sock = &newsock;
                sockClient.sin = &csin;

                pthread_t thread;
                if (pthread_create(&thread, NULL, newClient, &sockClient) != 0) {
                    fprintf(stderr, "Failed to create thread\n");
                }
            }
        }

        closesocket(sockServer);
        return NULL;
    }

    closesocket(sockServer);

    return NULL;
}
void* newClient(void* sockClient)
{
    int csock = *((infosSock*)sockClient)->sock;
    SOCKADDR_IN addr = *((infosSock*)sockClient)->sin;

    pthread_mutex_unlock(&lock);

    char buffer[1024] = "";

    //Wait for header
    while(1)
    {
        long length = 0;
        length = recv(csock, buffer, 1024, 0);

        if(length>0)
        {
            char type[10],uid[10],filename[100],ip[16];
            int ttl,port;
            int versionNumber = 0;

            splitHeader(buffer, type, uid, &ttl, filename, ip, &port,&versionNumber);

            if(strcmp(type,"query")==0)  // HEADER QUERY
            {
                if(hasQuery(uid)==-1)
                {
                    addQuery(buffer,inet_ntoa(addr.sin_addr));

                    //Scan files.
                    DIR *dir;
                    struct dirent *ent;
                    if ((dir = opendir (SHARED_FOLDER_OWNER)) != NULL) {
                        while ((ent = readdir (dir)) != NULL) {
                            if(ent->d_name[0]!='.')
                            {
                                if((strcmp(filename, "*")==0 || compareStringStart(ent->d_name, filename)) && strcmp(ent->d_name,"config.txt")!=0)
                                {
                                    //SEND HITQUERY
                                    char type2[10],filename2[100],ip2[16];
                                    int ttl2,port2;
                                    int versionNumber2 = getVersionNumberOfFile(filename);

                                    strcpy(type2,"hitquery");

                                    ttl2=TTR;

                                    strcpy(filename2,ent->d_name);

                                    strcpy(ip2,"127.0.0.1");

                                    port2 = PORT_CLIENT;

                                    char hitQueryHeader[300];
                                    constructHeader(hitQueryHeader, type2, uid, &ttl2, filename2, ip2, &port2,&versionNumber2);

                                    sendHeader(hitQueryHeader, inet_ntoa(addr.sin_addr), port);
                                }
                            }
                        }
                        closedir (dir);
                    } else {
                        /* could not open directory */
                        perror ("");
                    }


                    if(--ttl>0)
                    {
                        //SEND QUERY
                        port = PORT_CLIENT;
                        constructHeader(buffer, type, uid, &ttl, filename, ip, &port,&versionNumber);

                        for(int i=0;i<numberOfNeighbors;i++)
                        {
                            sendHeader(buffer, ipNeighbors[i], portNeighbors[i]);
                        }
                    }

                    closesocket(csock);
                    return NULL;
                }
                else
                {
                    closesocket(csock);
                    return NULL;
                }
            }
            else if(strcmp(type,"hitquery")==0)  // HEADER HITQUERY
            {
                pthread_mutex_lock(&downloadMutex);
                if(strcmp(waitingQuery,uid)==0)
                {
                    if(waitingQueryType==2 && (strcmp(waitingQueryFileName, filename)==0))
                    {
                        downloadfile(ip, port, filename, uid, ttl, versionNumber);
                    }
                    else if(waitingQueryType==1)
                    {
                        printf ("%s  :  ",filename);
                        //END CHRONO
                        gettimeofday(&tim, NULL);
                        double t2=tim.tv_sec+(tim.tv_usec/1000000.0);
                        printf("%.3lf seconds elapsed \n", t2-t1);
                        //END CHRONO END
                    }

                    closesocket(csock);
                    pthread_mutex_unlock(&downloadMutex);
                    return NULL;

                }
                else
                {
                    int idQuery = hasQuery(uid);
                    if(idQuery!=-1)
                    {
                        char type2[10],filename2[100],ip2[16],uid2[10];
                        int ttl2,port2;
                        int versionNumber2 = 0;

                        splitHeader(queries[idQuery], type2, uid2, &ttl2, filename2, ip2, &port2,&versionNumber2);

                        sendHeader(buffer, ip2, port2);
                    }
                    else
                    {
                        printf("Can't find previous queries about this... or download is already achieved");
                        fflush(stdout);
                    }

                    closesocket(csock);
                    pthread_mutex_unlock(&downloadMutex);
                    return NULL;
                }
            }
            else if(strcmp(type,"download")==0)   // HEADER DOWNLOAD
            {
                /* Send File to client */

                char fs_name[100] = "";
                strcpy(fs_name, SHARED_FOLDER_OWNER);
                strcat(fs_name, filename);

                char sdbuf[1024];
                printf("[Client] Sending %s to the peer...\n", fs_name);
                FILE *fs = fopen(fs_name, "r");
                if(fs == NULL)
                {
                    printf("ERROR: File %s not found.\n", fs_name);
                    exit(1);
                }

                bzero(sdbuf, 1024);
                long fs_block_sz;
                while((fs_block_sz = fread(sdbuf, sizeof(char), 1024, fs)) > 0)
                {
                    if(send(csock, sdbuf, 1024, 0) < 0)
                    {
                        fprintf(stderr, "ERROR: Failed to send file %s. (errno = %d)\n", fs_name, errno);
                        break;
                    }
                    bzero(sdbuf, 1024);
                }
                printf("Ok File %s is sent !\n", fs_name);
                sleep(1);
                send(csock,"END",1024,0);

                closesocket(csock);
                return NULL;

            }
            else if(strcmp(type,"inval")==0)   // HEADER inval
            {
                if(hasQuery(uid)==-1)
                {
                    addQuery(buffer,inet_ntoa(addr.sin_addr));

                    if(checkForFile(filename)==1)
                    {
                        int myVersion = versionsOfDownloadedFiles[indexOfFile(filename)];
                        if(myVersion<versionNumber)
                        {
                            char fs_name[100] = "";
                            strcpy(fs_name, SHARED_FOLDER);
                            strcat(fs_name, filename);

                            printf("\n\n %s is invalid. New download.\n\n",filename);
                            fflush(stdout);

                            remove(fs_name);
                            downloadfile(ip, port, filename, uid, ttl, versionNumber);
                            closesocket(csock);
                            return NULL;
                        }
                    }
                }
            }
            else if(strcmp(type,"ttr")==0)
            {
                if(hasQuery(uid)==-1)
                {
                    addQuery(buffer, inet_ntoa(addr.sin_addr));

                    int myVersion = getVersionNumberOfFile(filename);
                    if(myVersion>versionNumber)
                    {
                        //The version of the client has expired

                        char typeQ[10],filenameQ[100],ipQ[16],uidQ[10];
                        int ttlQ,portQ;

                        strcpy(typeQ, "inval");
                        strcpy(filenameQ, filename);
                        int randNumber = rand()%100000;
                        sprintf(uidQ, "%d",randNumber);
                        ttlQ=TTR;
                        char headerQ[300];
                        strcpy(ipQ,"127.0.0.1");
                        portQ = PORT_CLIENT;

                        constructHeader(headerQ, typeQ, uidQ, &ttlQ, filenameQ, ipQ, &portQ, &myVersion);

                        sendHeader(headerQ, ip, port);

                    }
                    else if(myVersion==-1)
                    {
                        printf("I don't own the file : %s",filename);
                    }
                }

                closesocket(csock);
                return NULL;
            }
            else
            {
                printf("\n\nUnknown heaader\n\n");
                fflush(stdout);
                closesocket(csock);
                return NULL;
            }
        }
    }


    closesocket(csock);

    return NULL;
}
void* ttrFunction()
{
    while(1)
    {
        sleep(1);

        for(int i=0;i<downloadedFiles;i++)
        {
            if(--ttrForFile[i]<=0)
            {

                char typeQ[10],filenameQ[100],ipQ[16],uidQ[10];
                int ttlQ,portQ;

                strcpy(typeQ, "ttr");
                strcpy(filenameQ, filenames[i]);
                int randNumber = rand()%100000;
                sprintf(uidQ, "%d",randNumber);
                ttlQ=3;
                char headerQ[300];
                strcpy(ipQ,"127.0.0.1");
                portQ = PORT_CLIENT;
                int myVersion = versionsOfDownloadedFiles[i];

                constructHeader(headerQ, typeQ, uidQ, &ttlQ, filenameQ, ipQ, &portQ, &myVersion);

                sendHeader(headerQ, ipFileOwner[i], portFileOwner[i]);

                ttrForFile[i]=globalTtrForFile[i];
            }
        }
    }

    return NULL;
}
void sendHeader(char header[],char ipDest[],int portDest)
{
    SOCKET sock;
    SOCKADDR_IN sin;

    sock = socket(AF_INET, SOCK_STREAM, 0);

    sin.sin_addr.s_addr = inet_addr(ipDest);
    sin.sin_family = AF_INET;
    sin.sin_port = htons(portDest);


    if(connect(sock, (SOCKADDR*)&sin, sizeof(sin)) != SOCKET_ERROR)
    {
        send(sock,header,1024,0);
        closesocket(sock);
    }
    else
    {
        printf("Impossible de se connecter");
        fflush(stdout);
    }
}

int hasQuery(char uidQuery[])
{
    for(int i=0;i<MAX_QUERIES;i++)
    {
        if(strcmp(queries[i],"")!=0)
        {
            char type[10],uid[10],filename[100],ip[16];
            int ttl,port;
            int versionNumber = 0;

            splitHeader(queries[i], type, uid, &ttl, filename, ip, &port,&versionNumber);

            if(strcmp(uid,uidQuery)==0)
                return i;
        }
    }

    return -1;
}
void addQuery(char header[],char ipSender[])
{
    char type[10],uid[10],filename[100],ip[16];
    int ttl,port;
    int versionNumber = 0;

    splitHeader(header, type, uid, &ttl, filename, ip, &port,&versionNumber);
    strcpy(ip, ipSender);
    constructHeader(header, type, uid, &ttl, filename, ip, &port,&versionNumber);

    strcpy(queries[iQueries], header);
    if(++iQueries==MAX_QUERIES)
        iQueries=0;
}
int checkForFile(char filename[])
{
    char path[300];
    strcpy(path,SHARED_FOLDER);
    strcat(path,filename);
    if( access( path, F_OK ) != -1 ) {
        return 1;
    } else {
        return -1;
    }
}
int incrementVersionNumberForFile(char filename[])
{
    //Index the owned files
    DIR *dir;
    int iFile = 0;
    struct dirent *ent;
    if ((dir = opendir (SHARED_FOLDER_OWNER)) != NULL) {
        while ((ent = readdir (dir)) != NULL)
        {
            if(ent->d_name[0]!='.')
            {
                if(strcmp(ent->d_name, filename))
                {
                    return ++versionsForOwnedFiles[iFile];
                    break;
                }

                iFile++;
            }
        }
        closedir (dir);
    } else {
        /* could not open directory */
        perror ("");
    }

    return 0;

}
int getVersionNumberOfFile(char filename[])
{
    //Index the owned files
    DIR *dir;
    int iFile = 0;
    struct dirent *ent;
    if ((dir = opendir (SHARED_FOLDER_OWNER)) != NULL) {
        while ((ent = readdir (dir)) != NULL)
        {
            if(ent->d_name[0]!='.')
            {
                if(strcmp(ent->d_name, filename))
                {
                    return versionsForOwnedFiles[iFile];
                    break;
                }

                iFile++;
            }
        }
        closedir (dir);
    } else {
        /* could not open directory */
        perror ("");
    }

    return -1;
}
int indexOfFile(char filename[])
{
    for(int i=0;i<downloadedFiles;i++)
    {
        if(strcmp(filename,filenames[i])==0)
            return i;
    }

    return -1;
}
void downloadfile(char ip[],int port,char filename[],char uid[],int ttl,int versionNumber)
{
    //I AM THE ONE WHO SENDS THE REQUEST
    SOCKET sockFile;
    SOCKADDR_IN sinFile;

    sockFile = socket(AF_INET, SOCK_STREAM, 0);

    sinFile.sin_addr.s_addr = inet_addr(ip);
    sinFile.sin_family = AF_INET;
    sinFile.sin_port = htons(port);


    if(connect(sockFile, (SOCKADDR*)&sinFile, sizeof(sinFile)) != SOCKET_ERROR)
    {
        char downloadHeader[300];
        constructHeader(downloadHeader, "download", uid, &ttl, filename, ip, &port,&versionNumber);
        send(sockFile,downloadHeader,1024,0);

        /* Receive File from Server */
        printf("[Client] Receiveing file from the peer and saving it ...\n");
        char fr_name[100] = "";
        strcpy(fr_name, SHARED_FOLDER);
        strcat(fr_name, filename);

        FILE *fr = fopen(fr_name, "a");
        if(fr == NULL)
            printf("File %s Cannot be opened.\n", fr_name);
        else
        {
            char revbuf[1024];
            bzero(revbuf, 1024);
            long fr_block_sz = 0;
            while(1)
            {
                fr_block_sz = recv(sockFile, revbuf, 1024, 0);
                if(fr_block_sz>0)
                {
                    if(revbuf[0]=='E' && revbuf[1]=='N' && revbuf[2]=='D' && revbuf[3]=='\0')
                    {
                        break;
                    }
                    else
                    {
                        long write_sz = fwrite(revbuf, sizeof(char), fr_block_sz, fr);
                        if(write_sz < fr_block_sz)
                        {
                            printf("File write failed.\n");
                        }
                        bzero(revbuf, 1024);
                    }
                }
            }
            if(fr_block_sz < 0)
            {
                if (errno == EAGAIN)
                {
                    printf("recv() timed out.\n");
                }
                else
                {
                    fprintf(stderr, "recv() failed due to errno = %d\n", errno);
                }
            }
            printf("Ok received from the peer!\n");
            fclose(fr);
            strcpy(waitingQuery, "");
            closesocket(sockFile);

            int indexOfFilename = indexOfFile(filename);
            if(indexOfFilename == -1)
            {
                strcpy(filenames[downloadedFiles],filename);
                versionsOfDownloadedFiles[downloadedFiles] = versionNumber;
                strcpy(ipFileOwner[downloadedFiles], ip);
                portFileOwner[downloadedFiles] = port;
                ttrForFile[downloadedFiles] = ttl;
                globalTtrForFile[downloadedFiles] = ttl;
                downloadedFiles++;
            }
            else
            {
                versionsOfDownloadedFiles[indexOfFilename] = versionNumber;
            }
        }
    }
}
