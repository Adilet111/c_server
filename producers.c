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
#include <math.h>
#include <stdbool.h>
#include "prodcon.h"




int connectsock( char *host, char *service, char *protocol );

char		*service;
char		*host = "localhost";

ITEM *makeItem()
{
	ITEM *p = malloc( sizeof(ITEM) );
	p->size = rand()%MAX_LETTERS;

	return p;
}

char *randomLetters(int chunk_size){
	char *letters = malloc(chunk_size);
	char c;
	for ( int i = 0; i < chunk_size; i++ ){
		c = rand()%26+97;
		letters[i]=c;
	}
	return letters;
}

void useItem( ITEM *p )
{
	free( p );
}
double poissonRandomInterarrivalDelay( double r )
{
    return (log((double) 1.0 -((double) rand())/((double) RAND_MAX)   ))/-r;
}



void *produce(void *args);


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
		case	4:
			host = argv[1];
			service = argv[2];
			len =atoi(argv[3]);
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

	/*	Create the socket to the controller  */


	pthread_t threads[len];
	int status, i, j;
	useconds_t interval;
	double t;

	for ( j = 0, i = 0; i < len; i++ )
		{
			t = poissonRandomInterarrivalDelay(rate);
			t = t*1000000.0;
			interval = (int)t;
			usleep(interval);
			status = pthread_create( &threads[j++], NULL, produce, NULL );
			if ( status != 0 )
			{
				printf( "pthread_create error %d.\n", status );
				exit( -1 );
			}
		}
	for ( j = 0; j < len; j++ ){
		pthread_join( threads[j], NULL );
	}

//	produce(csock);
//	// 	Start the loop

	fflush(stdout);
	pthread_exit( 0 );

}
void *produce(void *args){


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



	printf("producing item\n socket num: %d\n", csock);
	fflush(stdout);
	char *msg = "PRODUCE\r\n";
	char buf[5]= "";
	int cc;

	if ( write( csock, msg, strlen(msg) ) < 0 )
	{
		fprintf( stderr, "client write: %s\n", strerror(errno) );
		exit( -1 );
	}

	printf("waiting...\n");
	fflush(stdout);
	ITEM *p =makeItem();
	int size = p->size;
	if ( (cc = read( csock, buf, 4 )) <= 0 )
	{
		printf( "The server has gone1111 for socket %d.\n", csock );
		close(csock);
		useItem(p);
		pthread_exit( NULL );
	}
	else
	{
		buf[cc]='\0';
		if(strcmp(buf, "GO\r\n")==0){

		printf("GOT GO for sock num %d\n", csock);
		fflush(stdout);}
		int cn = htonl(size);

		if ( write( csock,&cn, sizeof(htonl(size)) ) < 0 )
			{
				fprintf( stderr, "client write: %s\n", strerror(errno) );
				close(csock);
				useItem(p);
				pthread_exit( NULL );
			}


		if ( (cc = read( csock, buf, 4 )) <= 0 )
		{
			printf( "The server has gone1111 for socket %d.\n", csock );
			close(csock);
			useItem(p);
			pthread_exit( NULL );
		}
		else
		{
			buf[cc]='\0';
			if(strcmp(buf, "GO\r\n")==0){
				printf("GOT GO for sock num %d\n", csock);
				fflush(stdout);
			}
		}

		printf("sending letters to sock %d \n", csock);
		fflush(stdout);

		int letters_to_send = p->size;
		int chunk_size = BUFSIZE;
		char *letters = randomLetters(chunk_size);
		while(letters_to_send>0){
			if(letters_to_send<chunk_size){
				if ( write( csock, letters, letters_to_send  ) < 0 )
				{
					fprintf( stderr, "client write: %s\n", strerror(errno) );
					close(csock);
					useItem(p);
					free(letters);
					pthread_exit( NULL );
				}
			}
			else{
			if ( write( csock, letters, chunk_size  ) < 0 )
				{
					fprintf( stderr, "client write: %s\n", strerror(errno) );
					close(csock);
					useItem(p);
					free(letters);
					pthread_exit( NULL );
				}
			}
			letters_to_send -= chunk_size;
		}
		free(letters);
		printf("done sending to sock %d \n", csock);
		fflush(stdout);
		free( p );
		char endMsg[7];
		if ( (cc = read( csock, endMsg , 6)) <= 0 )
			{
				printf( "The server has gone.\n" );
				close(csock);
				pthread_exit( NULL );
			}else{
				endMsg[6] = '\0';
				if(strcmp(endMsg, "DONE\r\n")==0){
					close(csock);
					pthread_exit( NULL );
				}
			}
	}


}



