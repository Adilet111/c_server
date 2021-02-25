#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <pthread.h>
#include<fcntl.h>
#include <stdbool.h>
#include "prodcon.h"
#include <math.h>

int connectsock( char *host, char *service, char *protocol );

char		*service;
char		*host = "localhost";

double poissonRandomInterarrivalDelay( double r )
{
    return (log((double) 1.0 -((double) rand())/((double) RAND_MAX)   ))/-r;
}

ITEM *makeItem(int s)
{
	ITEM *p = malloc( sizeof(ITEM) );
	p->size = s;
//	p->letters = malloc((p->size));

	return p;
}

int createFile(  int er, int bytes )
{
	int cc;
	char str[40];
	pthread_t tid = pthread_self();
	printf("tid: %ld\n", tid);
	sprintf(str, "%ld.txt", tid);
	int fd ;
	char message[100];
	if(er ==0){
		sprintf(message, "%s", SUCCESS);
		sprintf(message+strlen(SUCCESS), " %d", bytes);
	}
	else if(er==1){
		sprintf(message, "%s", BYTE_ERROR);
		sprintf(message+strlen(BYTE_ERROR), " %d", bytes);
	}else{
		sprintf(message, "%s", REJECT);
	}


	if((fd = open(str, O_RDWR | O_CREAT, S_IRWXU )) == -1){
		return -1;
		}

	if(write(fd, message, strlen(message)) < 0){
		return -1;
		}

//	free(p->letters);

	return 0;

}




void *consume(void *args);
/*
**	Client
*/

int counter=0;
int bad;
int
main( int argc, char *argv[] )
{

	int len =0;
	float rate;
	switch( argc ) 
	{
		case    2:
			service = argv[1];
			break;
		case    3:
			host = argv[1];
			service = argv[2];
			break;
		case    4:
				host = argv[1];
				service = argv[2];
				len  = atoi(argv[3]);
				break;
		case	6:
			host = argv[1];
			service = argv[2];
			len =atoi(argv[3]);
			rate = strtof(argv[4], NULL);
			bad = atoi(argv[5]);
			break;
		default:
			fprintf( stderr, "usage: chat [host] port\n" );
			exit(-1);
	}




	useconds_t interval;
	double t;
	pthread_t threads[len];
	int status, i, j;

	for ( j = 0, i = 0; i < len; i++ )
		{
			t = poissonRandomInterarrivalDelay(rate);
			t = t*1000000.0;
			interval = (int)t;
			usleep(interval);
			status = pthread_create( &threads[j++], NULL, consume, NULL );
			if ( status != 0 )
			{
				printf( "pthread_create error %d.\n", status );
				exit( -1 );
			}
		}
	for ( j = 0; j < len; j++ ){
		pthread_join( threads[j], NULL );
	}


	fflush(stdout);
	pthread_exit( 0 );

}

void *consume(void *args){




	int		csock;
	if ( ( csock = connectsock( host, service, "tcp" )) == 0 )
		{
			fprintf( stderr, "Cannot connect to server.\n" );
			exit( -1 );
		}
	if((rand()%100+1) <= bad){
		printf("bad client\n");
		fflush(stdout);
		sleep(SLOW_CLIENT);
	}



	printf("consuming item\n socket num: %d\n", csock);
	fflush(stdout);
	char *msg = "CONSUME\r\n";
	int letter_count;
	int cc, cn;
	int error=2;
	if ( write( csock, msg, strlen(msg) ) < 0 )
			{
				fprintf( stderr, "client write: %s\n", strerror(errno) );
				createFile(error, 0);
				pthread_exit( NULL );

			}

		printf("waiting...\n");
		fflush(stdout);
		if ( (cc = read( csock, &letter_count, sizeof(letter_count) )) <= 0 )
			{
				printf( "The server has gone.\n" );
				close(csock);
				createFile(error, 0);
				pthread_exit( NULL );
			}
			else
			{
				cn = ntohl(letter_count);

				}

		ITEM *p = makeItem(cn);

		int received_characters=0;
		int devNull = open("/dev/null", O_WRONLY);

		int chunk_size =BUFSIZE;
		char *letters = malloc(p->size);
		while (received_characters<p->size){
			if ( (cc = read( csock, letters, chunk_size )) <= 0 )
			{
				(void)close( csock );
				createFile(error, 0);
				free(p);
				free(letters);
				pthread_exit( NULL );
			}else{
				received_characters += cc;

				if ( (cc=write( devNull, letters, cc )) < 0 )
					{
						fprintf( stderr, "client write: %s\n", strerror(errno) );
						fflush(stdout);
						close( csock );
						createFile(error, 0);
						free(p);
						free(letters);
						pthread_exit( NULL );
					}

			}

		}
		free(letters);
		close(devNull);
		
	if(received_characters==p->size){
		error=0;
	}else{
		error=1;
	}
	free(p);

	if(createFile(error, received_characters)==-1){
		printf("error in writing to file");
	}
	close(csock);
	pthread_exit( NULL );
}



