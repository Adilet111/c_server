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
#include <semaphore.h>
#include "prodcon.h"
#include <stdbool.h>


#define	QLEN			5

int passivesock( char *service, char *protocol, int qlen, int *rport );




typedef struct timer_fd
{
	time_t start, end;
	double time_elapsed;
	bool prod_con;
} TIMER;


void stop_timer(TIMER *t, int f){
	time(&t->end);
	if(f==0){
		t->prod_con =true;
	}
	t->time_elapsed =  difftime(t->end, t->start);
	
}

TIMER *createtimer(){
	TIMER *t = malloc( sizeof(TIMER) );
	time(&t->start);
	t->prod_con = false;
	return t;
}


ITEM *makeItem(int s,int d)
{
	ITEM *p = malloc( sizeof(ITEM) );
	p->size = s;
	p->psd = d;
	return p;
}




void useItem( ITEM *p )
{
	free( p );
}


void *handleProducer( );
void *handleConsumer( );


int count=0;
int num_client=0;
int num_producers=0;
int num_consumers=0;
int producers_served = 0;
int consumers_served = 0;
int rejected_max = 0;
int rejected_idle =0;
int cons_rejected = 0;
int prod_rejected = 0;



ITEM** buffer;
int bufsize;
TIMER** ts;


pthread_mutex_t mutex, lock;
sem_t full, empty;
pthread_mutex_t producer, consumer;


int
main( int argc, char *argv[] ){
	char			*service;
	struct sockaddr_in	fsin;
	int			alen;
	int			msock;
	int			ssock;
	int			rport = 0;
	pthread_mutex_init( &mutex, NULL );
	pthread_mutex_init( &lock, NULL );
	pthread_mutex_init( &producer, NULL );
	pthread_mutex_init( &consumer, NULL );
	fd_set			rfds;
	fd_set			afds;
	int			fd;
	int 			ch;
	int			nfds;
	char buf_p[10];


	switch (argc)
	{
		case	1:
			// No args? let the OS choose a port and tell the user
			rport = 1;
			break;
		case	2:
			// User provides a port? then use it
			service = argv[1];
			break;
		case	3:
			service = argv[1];
			bufsize = atoi(argv[2]);
			break;
		default:
			fprintf( stderr, "usage: server [port]\n" );
			exit(-1);
	}

	sem_init( &full, 0, 0 );
	sem_init( &empty, 0, bufsize );
	buffer = malloc( bufsize*sizeof(ITEM*));
	ts = malloc(1000*sizeof(TIMER*));
	struct timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 500000;

	msock = passivesock( service, "tcp", QLEN, &rport );
	if (rport)
	{
		//	Tell the user the selected port
		printf( "server: port %d\n", rport );
		fflush( stdout );
	}

	nfds = msock+1;

	FD_ZERO(&afds);
	FD_SET( msock, &afds );		
	TIMER *t;
	printf("server started\n");
	for (;;)
	{

		memcpy((char *)&rfds, (char *)&afds, sizeof(rfds));

		if (select(nfds, &rfds, (fd_set *)0, (fd_set *)0,&timeout) < 0)
		{
			fprintf( stderr, "server select: %s\n", strerror(errno) );
			exit(-1);
		}



		if(timeout.tv_usec==0){
			timeout.tv_sec = 0;
			timeout.tv_usec = 500000;
			for (fd = 0; fd < nfds; fd++){
				if(fd != msock && FD_ISSET(fd, &afds)){
					stop_timer(ts[fd], 1);
					if(ts[fd]->time_elapsed>=REJECT_TIME && !(t->prod_con)){
						printf("TIME:%lf\n",ts[fd]->time_elapsed);
						fflush(stdout);
						(void) close(fd);
						ts[fd]=NULL;
						rejected_idle++;
						pthread_mutex_lock( &lock );
							num_client--;
						pthread_mutex_unlock( &lock );
						FD_CLR( fd, &afds );
						// lower the max socket number if needed
						if ( nfds == fd+1 )
							nfds--;
					}
				}

			}
		}
		else{
			if (FD_ISSET( msock, &rfds))
			{
				int	ssock;

				// we can call accept with no fear of blocking
				alen = sizeof(fsin);
				ssock = accept( msock, (struct sockaddr *)&fsin, &alen );
				if (ssock < 0)
				{
					fprintf( stderr, "accept: %s\n", strerror(errno) );
					exit(-1);
				}

				t = createtimer();
				ts[ssock] = t;


				pthread_mutex_lock( &lock );
					if(num_client<MAX_CLIENTS){
						num_client++;
						printf("CLIENT NUMBER: %d \n", num_client);
						fflush(stdout);
						FD_SET( ssock, &afds );

						if ( ssock+1 > nfds )
							nfds = ssock+1;

					}else
					{
						close(ssock);
						rejected_max++;
						ts[ssock]=NULL;
						printf("CLOSING SOCKET %d \n", ssock);
						fflush(stdout);

					}
				pthread_mutex_unlock( &lock );

			}

			pthread_t	thr;
			int status;

			for ( fd = 0; fd < nfds; fd++ )
			{

				if (fd != msock && FD_ISSET(fd, &rfds))
				{
					if ( (ch= read( fd, buf_p, 20 )) <= 0  )
					{
						printf( "The client has gone.\n" );
						(void) close(fd);
						pthread_mutex_lock( &lock );
							num_client--;
						pthread_mutex_unlock( &lock );
						// If the client has closed the connection, we need to
						// stop monitoring the socket (remove from afds)

						// lower the max socket number if needed

					}
					else
					{
						buf_p[ch]='\0';
						
						if (strcmp(buf_p, "PRODUCE\r\n")==0){
							stop_timer(ts[fd],0);
							status = pthread_create(&thr, NULL, handleProducer, (void*)fd);

							if (status !=0){
								printf("pthread_create error %d.\n", status );
								pthread_mutex_lock( &lock );
									num_client--;
								pthread_mutex_unlock( &lock );
								fflush(stdout);
							}
						}
						else if (strcmp(buf_p, "CONSUME\r\n")==0){
							stop_timer(ts[fd],0);
							status = pthread_create(&thr, NULL, handleConsumer, (void*)fd);
							if (status !=0){
								printf("pthread_create error %d.\n", status );
								pthread_mutex_lock( &lock );
									num_client--;
								pthread_mutex_unlock( &lock );
								fflush(stdout);
							}
						}else if(strncmp("STATUS", buf_p, 6)==0) {
							stop_timer(ts[fd],0);
							char num[20];
							buf_p[ch]='\0';
							if(strncmp(buf_p, "STATUS/CURRCLI",14)==0){
								sprintf(num, "%d", num_client);
								printf("sending number");
								fflush(stdout);
								write(fd, num, strlen(num));
							}else if(strncmp(buf_p, "STATUS/CURRPROD",15)==0){
								sprintf(num, "%d", num_producers);
								printf("sending number");
								fflush(stdout);
								write(fd, num, strlen(num));
							}else if(strncmp(buf_p, "STATUS/CURRCONS",15)==0){
								sprintf(num, "%d", num_consumers);
								printf("sending number");
								fflush(stdout);
								write(fd, num, strlen(num));
							}else if(strncmp(buf_p, "STATUS/TOTPROD",14)==0){
								sprintf(num, "%d", producers_served);
								printf("sending number");
								fflush(stdout);
								write(fd, num, strlen(num));	
							}else if(strncmp(buf_p, "STATUS/TOTCONS",14)==0){
								sprintf(num, "%d", consumers_served);
								printf("sending number");
								fflush(stdout);
								write(fd, num, strlen(num));
							}else if(strncmp(buf_p, "STATUS/REJMAX",14)==0){
								sprintf(num, "%d", rejected_max);
								printf("sending number");
								fflush(stdout);
								write(fd, num, strlen(num));
							}else if(strncmp(buf_p, "STATUS/REJSLOW",14)==0){
								sprintf(num, "%d", rejected_idle);
								printf("sending number");
								fflush(stdout);
								write(fd, num, strlen(num));
							}else if(strncmp(buf_p, "STATUS/REJPROD",14)==0){
								sprintf(num, "%d", prod_rejected );
								printf("sending number");
								fflush(stdout);
								write(fd, num, strlen(num));
							}else{
								sprintf(num, "%d", cons_rejected );
								printf("sending number");
								fflush(stdout);
								write(fd, num, strlen(num));
							}
							close(fd);
							pthread_mutex_lock( &lock );
								num_client--;
							pthread_mutex_unlock( &lock );
						}
						else{
							printf("status: %s\n", buf_p);
							printf("bad request\n");
							close(fd);
							pthread_mutex_lock( &lock );
								num_client--;
							pthread_mutex_unlock( &lock );
						}
					}
					if ( nfds == fd+1 )
						nfds--;
					FD_CLR( fd, &afds );
					ts[fd] = NULL;

				}
			}
				for (fd = 0; fd < nfds; fd++){
					if(fd != msock && FD_ISSET(fd, &afds)){
						stop_timer(ts[fd], 1);
						if(ts[fd]->time_elapsed>=REJECT_TIME&& !(t->prod_con)){
							(void) close(fd);
							ts[fd]=NULL;
							rejected_idle++;
							pthread_mutex_lock( &lock );
								num_client--;
							pthread_mutex_unlock( &lock );
							FD_CLR( fd, &afds );
							// lower the max socket number if needed
							if ( nfds == fd+1 )
								nfds--;
						}
					}
				}
			}
		}
	free(t);
	printf("reached end\n");
	fflush(stdout);
	pthread_exit(0);
}

void *handleConsumer (void *s){
	int clsock = (int) s;
	pthread_mutex_lock( &consumer);
		if(num_consumers<MAX_CON){
			num_consumers++;
		}else{
			printf("max number of consumers reached");
			fflush(stdout);
			printf("CLIENT NUMBER: %d \n", num_client);
			fflush(stdout);
			close(clsock);
			cons_rejected++;
			pthread_mutex_unlock( &consumer );
			pthread_mutex_lock( &lock );
				num_client--;
			pthread_mutex_unlock( &lock );
			pthread_exit( NULL );
		}

	pthread_mutex_unlock( &consumer );


	sem_wait( &full );
	pthread_mutex_lock( &mutex );
	ITEM *p = buffer[count-1];
	buffer[count-1] = NULL;
	count--;
	consumers_served++;
	printf( "C Cons Count %d.\n", count );
	fflush(stdout);
	pthread_mutex_unlock( &mutex );
	sem_post( &empty );


	int cc;
	int num_of_chars;
	int ch;


	int cn = htonl(p->size);

	if ( (cc=write( clsock,&cn, sizeof(cn) )) < 0 )
		{
			fprintf( stderr, "client write: %s\n", strerror(errno) );
			close( clsock );
			useItem(p);
			pthread_mutex_lock( &consumer );
				num_consumers--;
			pthread_mutex_unlock( &consumer );
			pthread_mutex_lock( &lock );
				num_client--;
			pthread_mutex_unlock( &lock );
			pthread_exit( NULL );
		}


	//writing GO to producers client
	int producer_d = p->psd;
	char *go = "GO\r\n";

	if ( write( producer_d, go, strlen(go) ) < 0 )
	{
		(void)close( producer_d );
		pthread_mutex_lock( &consumer );
			num_consumers--;
		pthread_mutex_unlock( &consumer );
		pthread_mutex_lock( &lock );
			num_client--;
		pthread_mutex_unlock( &lock );
		pthread_exit( NULL );
	}


//reading from producer client and writing to consumer 

	int received_characters = 0;
	int chunk_size =BUFSIZE;
	char *letters = malloc(chunk_size);
	while (p->size!=received_characters){

		if ( (cc = read( producer_d, letters, chunk_size )) <= 0 )
		{
			(void)close( producer_d );
			useItem(p);
			free(letters);
			pthread_mutex_lock( &consumer );
				num_consumers--;
			pthread_mutex_unlock( &consumer );
			pthread_mutex_lock( &lock );
				num_client--;
			pthread_mutex_unlock( &lock );
			pthread_exit( NULL );
		}else{
			received_characters += cc;
			if ( (cc=write( clsock, letters, cc )) < 0 )
				{
					fprintf( stderr, "client write: %s\n", strerror(errno) );
					fflush(stdout);
					close( clsock );
					useItem(p);
					free(letters);
					pthread_mutex_lock( &consumer );
						num_consumers--;
					pthread_mutex_unlock( &consumer );
					pthread_mutex_lock( &lock );
						num_client--;
					pthread_mutex_unlock( &lock );
					pthread_exit( NULL );
				}
		}

	}
	free(letters);
	char endMsg[] = "DONE\r\n";
	//writing to producer client DONE message
	if ( write( producer_d, endMsg, strlen(endMsg) ) < 0 )
	{

		close( producer_d );
		useItem(p);
		pthread_mutex_lock( &producer );
			num_producers--;
		pthread_mutex_unlock( &producer );
		pthread_mutex_lock( &lock );
			num_client--;
		pthread_mutex_unlock( &lock );
		pthread_exit( NULL );
	}


	close(producer_d);
	pthread_mutex_lock( &producer );
		num_producers--;
	pthread_mutex_unlock( &producer );
	pthread_mutex_lock( &lock );
		num_client--;
	pthread_mutex_unlock( &lock );


	close(clsock);
	useItem( p);
	pthread_mutex_lock( &consumer );
		num_consumers--;
	pthread_mutex_unlock( &consumer );
	pthread_mutex_lock( &lock );
		num_client--;
	pthread_mutex_unlock( &lock );
	pthread_exit( NULL );
}

void *handleProducer(void *s ){
	int clsock = (int) s;
	pthread_mutex_lock( &producer );
		if(num_producers<MAX_PROD){
			num_producers++;
		}else{
			printf("max number of producers reached\n");
			fflush(stdout);
			printf("CLIENT NUMBER: %d \n", num_client);
			fflush(stdout);
			close(clsock);
			prod_rejected++;
			pthread_mutex_unlock( &producer );
			pthread_mutex_lock( &lock );
				num_client--;
			pthread_mutex_unlock( &lock );
			pthread_exit( NULL );
		}

	pthread_mutex_unlock( &producer );

	int cc;
	int num_of_chars;

	char *buf = "GO\r\n";
	char endMsg[] = "DONE\r\n";
	int received_size = 0;
	ITEM *p;


	if ( write( clsock, buf, strlen(buf) ) < 0 )
	{
		close( clsock );
		pthread_mutex_lock( &producer );
			num_producers--;
		pthread_mutex_unlock( &producer );
		pthread_mutex_lock( &lock );
			num_client--;
		pthread_mutex_unlock( &lock );
		pthread_exit( NULL );
	}


	if ( (cc = read( clsock, &received_size, sizeof(received_size) )) <= 0 )
	{
		printf( "The client has gone123123123.\n" );
		fflush(stdout);
		close(clsock);
		pthread_mutex_lock( &producer );
			num_producers--;
		pthread_mutex_unlock( &producer );
		pthread_mutex_lock( &lock );
			num_client--;
		pthread_mutex_unlock( &lock );
		pthread_exit( NULL );
	}else{
		num_of_chars = ntohl(received_size);
		p = makeItem(num_of_chars, clsock);
		cc=0;
	}



	sem_wait( &empty );
	pthread_mutex_lock( &mutex );


	buffer[count] = p;
	count++;
	producers_served++;
	printf( "C Prod Count %d.\n", count );
	fflush(stdout);
	pthread_mutex_unlock( &mutex );

	sem_post( &full );



	pthread_exit( NULL );

}




