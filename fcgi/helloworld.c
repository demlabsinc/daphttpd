
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <fcgiapp.h>

//#include "fcgi_stdio.h" /* fcgi library; put it first*/
//#include <stdlib.h>

#if 0
typedef struct FCGX_Request { 
    int requestId;
    int role; 
    FCGX_Stream *in; 
    FCGX_Stream *out; 
    FCGX_Stream *err; 
    char **envp; // SERVER_PROTOCOL, REQUEST_METHOD, REQUEST_URI, QUERY_STRING, CONTENT_LENGTH, HTTP_USER_AGENT, HTTP_COOKIE, HTTP_REFERER
								 // NULL)
								// CONTENT_LENGTH=0
   struct Params *paramsPtr; 
    int ipcFd;
    int isBeginProcessed;
    int keepConnection;
    int appStatus; 
    int nWriters;
    int flags; 
    int listen_sock; 
    int detached; 
} FCGX_Request;

int FCGX_PutStr(const char *str, int n, FCGX_Stream *stream);
int FCGX_PutS(const char *str, FCGX_Stream *stream);

#endif

void	fcgi_printf( FCGX_Stream *stream, const char * __restrict fmt, ... )
{
	va_list va;
	va_start( va, fmt );
	char buff[8192];

	int len = vsprintf( buff, fmt, va );

	FCGX_PutStr( buff, len, stream );

	va_end( va );
	return;
}

//#ifdef WIN32
	#define	USE_LOCAL_SOCKET 1
//#endif

int main( void )
{
	FCGX_Request r;

	int ret = FCGX_Init( );

	#ifdef USE_LOCAL_SOCKET
		int sock = FCGX_OpenSocket( "localhost:9000", 0 );
	#else
		int sock = FCGX_OpenSocket( "/tmp/fcgi.socket", 0 );
	#endif

	int ret2 = FCGX_InitRequest( &r, sock, 0 ); //FCGI_FAIL_ACCEPT_ON_INTR 

	printf("Waiting for requests..\n" );

  int count = 0;

  while ( FCGX_Accept_r(&r) >= 0 ) {

		fcgi_printf( r.out, "Content-type: text/html\r\n"
           "\r\n"
           "<title>FastCGI Hello! (C, fcgi_stdio library)</title>"
           "<h1>FastCGI Hello! (C, fcgi_stdio library)</h1>"
           "Request number %d running on host <i>%s</i>\n",
            ++count, getenv("SERVER_HOSTNAME"));

		printf("Page sent, %u\n", count );
  }

	return 0;
}
