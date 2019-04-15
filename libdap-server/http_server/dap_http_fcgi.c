/*
 Copyright (c) 2017-2018 (c) Project "DeM Labs Inc" https://github.com/demlabsinc
  All rights reserved.

 This file is part of DAP (Deus Applications Prototypes) the open source project

    DAP (Deus Applicaions Prototypes) is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    DAP is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with any DAP based project.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#ifndef _WIN32
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/un.h>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <poll.h>
#else
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#include <winsock2.h>
#include <windows.h>
#include <mswsock.h>
#include <ws2tcpip.h>
#include <io.h>
#include "wrappers.h"
#include <wepoll.h>
#include "../pthread-win32/pthread.h"
#endif

#include "dap_common.h"
#include "dap_config.h"
#include "dap_server.h"

#include "dap_client_remote.h"
#include "dap_http.h"
#include "dap_http_client.h"
#include "dap_http_folder.h"
#include "dap_config.h"
#include "dap_http_fcgi.h"

//#include "http_status_code.h"

#define LOG_TAG "dap_http_fcgi"

typedef struct {
  int  family;
  char value[16];
  int  size;
} ip_addr_t;

typedef struct charlist_s {
  int  size;
  char **item;
} charlist_t;

//typedef struct connect_to_s {
//  char          *unix_socket;
//  int           port;
//  ip_addr_t     ip_addr;
//  bool          available;
//  bool          localhost;
//  struct connect_to_s *next;
//} connect_to_t;

//typedef struct cgi_session_s {
//  ip_addr_t     client_ip;
//  connect_to_t  *connect_to;
//  time_t        session_timeout;
//  struct cgi_session_s *next;
//} cgi_session_t;

typedef struct fcgi_server_s {

  char          *fcgi_path;
  char          *fcgi_url;
  char          *unix_socket;
  int           port;

  charlist_t    extension;
  int           session_timeout;
  char          *chroot;
  size_t        chroot_len;
  bool          localhost;

  //cgi_session_t *cgi_session_list[256];
  //pthread_mutex_t cgi_session_mutex[256];

  struct fcgi_server_s *next;

} fcgi_server_t;


//#define FCGI_BUFFER_SIZE    65535

//#define KILOBYTE       1024
//#define MEGABYTE    1048576
//#define GIGABYTE 1073741824

//#define MINUTE    60
//#define HOUR    3600
//#define DAY    86400

//#define TASK_RUNNER_INTERVAL 10

//#define TIMER_OFF   0

//#define POLL_EVENT_BITS (POLLIN | POLLPRI | POLLHUP)

//#define PTHREAD_STACK_SIZE 512 * KILOBYTE

#define FCGI_VERSION_1           1

#define FCGI_BEGIN_REQUEST       1
#define FCGI_ABORT_REQUEST       2
#define FCGI_END_REQUEST         3
#define FCGI_PARAMS              4
#define FCGI_STDIN               5
#define FCGI_STDOUT              6
#define FCGI_STDERR              7
#define FCGI_DATA                8
#define FCGI_GET_VALUES          9
#define FCGI_GET_VALUES_RESULT  10
#define FCGI_UNKNOWN_TYPE       11
#define FCGI_MAXTYPE (FCGI_UNKNOWN_TYPE)

#define FCGI_HEADER_LENGTH       8

typedef enum { cgi_TIMEOUT = -3, cgi_FORCE_QUIT, cgi_ERROR, cgi_OKE, cgi_END_OF_DATA } cgi_result_t;

#define FCGI_BUFFER_SIZE    (64 * 1024 - 1)
#define FCGI_CHUNK_SIZE     (64 * 1024 - 8)
#define STREAM_BUFFER_SIZE  (32 * 1024)

typedef struct {
  int           sock;
  unsigned char data[FCGI_BUFFER_SIZE + 8];
  int           size;
  unsigned char type;
} fcgi_buffer_t;

typedef struct dap_fcgi_session_s {

  int   sock;
  char  *uploaded_file;
  time_t deadline;
  bool  force_quit;
  bool  read_header;
  char  header[FCGI_HEADER_LENGTH];
  size_t fcgi_data_len;

  char *input_buffer, *error_buffer;
  int   input_buffer_size, error_buffer_size;
  uint32_t input_len, error_len, content_offset;

  dap_http_client_t *client;

} dap_fcgi_session_t;

#define DAP_FCGI_SESSION(a) ((dap_fcgi_session_t*) (a)->_inheritor)

fcgi_server_t fcgi_server;

void dap_http_fcgi_new( dap_http_client_t *cl_ht, void *arg )
{
  log_it( L_NOTICE, "dap_http_fcgi_new()" );
}

void dap_http_fcgi_delete( dap_http_client_t *cl_ht, void *arg )
{
  dap_fcgi_session_t *session = DAP_FCGI_SESSION( cl_ht );

  if ( session->input_buffer ) 
    free( session->input_buffer );

  if ( session->error_buffer ) 
    free( session->error_buffer );

  log_it( L_NOTICE, "dap_http_fcgi_delete()" );
}

void dap_http_fcgi_headers_read( dap_http_client_t *cl_ht, void *arg );
void dap_http_fcgi_headers_write( dap_http_client_t *cl_ht, void *arg );
void dap_http_fcgi_data_read( dap_http_client_t *cl_ht, void *arg );
void dap_http_fcgi_data_write( dap_http_client_t *cl_ht, void *arg );

#ifndef _WIN32
static int  connect_to_unix_socket( char *unix_socket )
{
  struct sockaddr_un sunix;
  int sock;

  if ( strlen(unix_socket) >= sizeof(sunix.sun_path) ) {
    return -1;
  }

  if ( (sock = socket(AF_UNIX, SOCK_STREAM, 0)) != -1 ) {
    sunix.sun_family = AF_UNIX;
    strcpy( sunix.sun_path, unix_socket );
    if ( connect(sock, (struct sockaddr*)&sunix, sizeof(struct sockaddr_un)) != 0 ) {
      close( sock );
      sock = -1;
    }
  }

  return sock;
}
#endif

static int write_buffer( int handle, const char *buffer, int size )
{
  int total_written = 0;
  ssize_t bytes_written;

  if (size <= 0 )
    return 0;

  while ( total_written < size ) {

    if ((bytes_written = write(handle, buffer + total_written, size - total_written)) == -1) {
      if ( errno != EINTR ) {
        return -1;
      }
    } else {
      total_written += bytes_written;
    }
  }

  return 0;
}

static void fcgi_set_padding( fcgi_buffer_t *fcgi_buffer )
{
  unsigned char padding;

  if ( (padding = fcgi_buffer->data[5] & 7) > 0 ) {
    padding = 8 - padding;
    memset( fcgi_buffer->data + fcgi_buffer->size, 0, (size_t)padding );
    fcgi_buffer->size += (int)padding;
  }
  fcgi_buffer->data[6] = padding;
}

static int send_fcgi_buffer( fcgi_buffer_t *fcgi_buffer, const char *buffer, int size )
{
  int written;

  if ( size > FCGI_BUFFER_SIZE ) {

    if ( fcgi_buffer->size > 0 ) {
      fcgi_set_padding( fcgi_buffer );
      if ( write_buffer(fcgi_buffer->sock, (char*)fcgi_buffer->data, fcgi_buffer->size) == -1 ) {
        return -1;
      }
      fcgi_buffer->size = 0;
    }

    memcpy( fcgi_buffer->data, "\x01\x00\x00\x01" "\xff\xff\x00\x00", 8 );

    fcgi_buffer->data[1] = fcgi_buffer->type;
    fcgi_buffer->data[4] = (FCGI_CHUNK_SIZE >> 8) & 255;
    fcgi_buffer->data[5] = FCGI_CHUNK_SIZE & 255;

    written = 0;
    do {
      if ( write_buffer(fcgi_buffer->sock, (char*)fcgi_buffer->data, 8) == -1 )
        return -1;
      if ( write_buffer(fcgi_buffer->sock, buffer + written, FCGI_CHUNK_SIZE) == -1 )
        return -1;
      written += FCGI_CHUNK_SIZE;
    } while ( size - written > FCGI_BUFFER_SIZE );

    if (send_fcgi_buffer(fcgi_buffer, buffer + written, size - written) == -1)
      return -1;

  } else if ( buffer == NULL ) {

    if ( fcgi_buffer->size > 0 ) {
      fcgi_set_padding( fcgi_buffer );
      if ( write_buffer(fcgi_buffer->sock, (char*)fcgi_buffer->data, fcgi_buffer->size) == -1 )
        return -1;
    }

    memcpy( fcgi_buffer->data, "\x01\x00\x00\x01" "\x00\x00\x00\x00", 8 );
    fcgi_buffer->data[1] = fcgi_buffer->type;
    if ( write_buffer(fcgi_buffer->sock, (char*)fcgi_buffer->data, 8) == -1 )
      return -1;

    fcgi_buffer->size = 0;
  } else {
    if ( (fcgi_buffer->size + size > FCGI_BUFFER_SIZE) && (fcgi_buffer->size > 0) ) {
      fcgi_set_padding( fcgi_buffer );
      if ( write_buffer(fcgi_buffer->sock, (char*)fcgi_buffer->data, fcgi_buffer->size) == -1 )
        return -1;
      fcgi_buffer->size = 0;
    }

    if ( fcgi_buffer->size == 0 ) {
      memcpy( fcgi_buffer->data, "\x01\x00\x00\x01" "\x00\x00\x00\x00", 8 );
      fcgi_buffer->data[1] = fcgi_buffer->type;
      fcgi_buffer->size = 8;
    }

    memcpy( fcgi_buffer->data + fcgi_buffer->size, buffer, size );
    fcgi_buffer->size += size;
    fcgi_buffer->data[4] = ((fcgi_buffer->size - 8) >> 8) & 255;
    fcgi_buffer->data[5] = (fcgi_buffer->size - 8) & 255;
  }

  return 0;
}

static int add_to_environment( fcgi_buffer_t *fcgi_buffer, char *key, char *value )
{
  char buffer[8];
  int len;
  size_t key_len, value_len;

  if ( (key_len = strlen(key)) <= 127 ) {
    *buffer = (unsigned char)key_len;
    len = 1;
  } else {
    *(buffer) = (unsigned char)((key_len >> 24) | 0x80);
    *(buffer + 1) = (unsigned char)((key_len >> 16) & 0xff);
    *(buffer + 2) = (unsigned char)((key_len >> 8) & 0xff);
    *(buffer + 3) = (unsigned char)(key_len & 0xff);
    len = 4;
  }

  if ( (value_len = strlen(value)) <= 127 ) {
    *(buffer + len) = (unsigned char)value_len;
    len++;
  } else {
    *(buffer + len) = (unsigned char)((value_len >> 24) | 0x80);
    *(buffer + len + 1) = (unsigned char)((value_len >> 16) & 0xff);
    *(buffer + len + 2) = (unsigned char)((value_len >> 8) & 0xff);
    *(buffer + len + 3) = (unsigned char)(value_len & 0xff);
    len += 4;
  }

  if ( send_fcgi_buffer(fcgi_buffer, buffer, len) == -1 ) {
    return -1;
  } else if ( send_fcgi_buffer(fcgi_buffer, key, key_len) == -1 ) {
    return -1;
  } else if ( send_fcgi_buffer(fcgi_buffer, value, value_len) == -1 ) {
    return -1;
  }

  return 0;
}

#define TLS_VAR_SIZE    512
#define MAX_HEADER_LEN  100

static void set_environment( dap_fcgi_session_t *session, fcgi_buffer_t *fcgi_buffer )
{
//  char ip[16], len[20], value[10], variable[MAX_HEADER_LEN];//, old;*data,
//  size_t len1;
//  bool has_path_info = false;
  dap_http_client_t *cl_ht = session->client;
  struct dap_http_header *http_headers;

  //t_http_header *http_headers;
  //  t_keyvalue *envir;

  add_to_environment( fcgi_buffer, "GATEWAY_INTERFACE", "CGI/1.1" );
  add_to_environment( fcgi_buffer, "REQUEST_METHOD", cl_ht->action );
  add_to_environment( fcgi_buffer, "REQUEST_URI", cl_ht->url_path );
  add_to_environment( fcgi_buffer, "SCRIPT_NAME", cl_ht->url_path );
  add_to_environment( fcgi_buffer, "SCRIPT_FILENAME", "/tmp/op" );

  if ( strlen(cl_ht->in_query_string) ) {
    add_to_environment(fcgi_buffer, "QUERY_STRING", cl_ht->in_query_string );
  }

  add_to_environment(fcgi_buffer, "SERVER_PORT", "9000" );
  add_to_environment(fcgi_buffer, "SERVER_NAME", "DAP HTTP SERVER" );
  add_to_environment(fcgi_buffer, "SERVER_PROTOCOL", "1.1" );
  add_to_environment(fcgi_buffer, "SERVER_SOFTWARE", "DAP HTTP SERVER" );

///   add_to_environment(fcgi_buffer, "REMOTE_ADDR", ip);

  /* Convert HTTP headers to HTTP_* environment variables
   */
  http_headers = cl_ht->in_headers;
  while (http_headers != NULL) {
    if (strcasecmp(http_headers->name, "Content-Type") != 0) {
        add_to_environment(fcgi_buffer, http_headers->name, http_headers->value );
    }
    http_headers = http_headers->next;
  }

}

static int  send_fcgi_request( dap_fcgi_session_t *session )
{
  fcgi_buffer_t fcgi_buffer;
  int sock = session->sock;
//  int bytes_read;

  fcgi_buffer.sock = sock;
  fcgi_buffer.size = 0;

  if ( write_buffer(sock, "\x01\x01\x00\x01" "\x00\x08\x00\x00" "\x00\x01\x00\x00" "\x00\x00\x00\x00", 16) == -1 )
    return -1;

  fcgi_buffer.type = FCGI_PARAMS;
  set_environment( session, &fcgi_buffer );

  if ( send_fcgi_buffer(&fcgi_buffer, NULL, 0) == -1 )
    return -1;

  return 0;
}

#define POLL_EVENT_BITS (POLLIN | POLLPRI | POLLHUP)

static cgi_result_t read_fcgi_socket( dap_fcgi_session_t *session, char *buffer, int size )
{
  int bytes_read;
  bool read_again;
  struct pollfd poll_data;

  poll_data.fd = session->sock;
  poll_data.events = POLL_EVENT_BITS;

  do {
    read_again = false;
    switch ( poll(&poll_data, 1, 1000) ) {
      case -1:
        if ( errno == EINTR )
          read_again = true;
        break;
      case 0:
        if (session->force_quit) {
          return cgi_FORCE_QUIT;
        } else if ( time(NULL) > session->deadline ) {
          return cgi_TIMEOUT;
        }
        read_again = true;
        break;
      default:
        do {
          read_again = false;
          if (( bytes_read = read(session->sock, buffer, size)) == -1 ) {
            if (errno != EINTR) {
              break;
            }
            read_again = true;
          }
        } while (read_again);
        return bytes_read;
    }
  } while (read_again);

  return cgi_ERROR;
}

#define DUMMY_BUFFER_SIZE   (64 * 1024)

static cgi_result_t read_from_fcgi_server( dap_fcgi_session_t *session )
{
  char *buffer, *dummy = NULL;
  bool read_again;
  int bytes_read, bytes_left;
  unsigned int content, padding, data_length;
  cgi_result_t result = cgi_OKE;

  /* Read header
   */
  if ( session->read_header ) {
    bytes_left = FCGI_HEADER_LENGTH;
    do {
      read_again = false;

      switch ( bytes_read = read_fcgi_socket( session, session->header + FCGI_HEADER_LENGTH - bytes_left, bytes_left) ) {
        case cgi_TIMEOUT:
          return cgi_TIMEOUT;
        case cgi_FORCE_QUIT:
          return cgi_FORCE_QUIT;
        case cgi_ERROR:
          return cgi_ERROR;
        case 0:
          return cgi_END_OF_DATA;
        default:
          if ( (bytes_left -= bytes_read) > 0 ) {
            read_again = true;
          }
      }
    } while ( read_again );

    session->read_header = false;
    session->fcgi_data_len = 0;
  }

  if ( ((unsigned char)session->header[1]) == FCGI_END_REQUEST ) {
    return cgi_END_OF_DATA;
  }

  /* Determine the size of the data
   */
  content = 256 * (unsigned char)session->header[4] + (unsigned char)session->header[5];
  padding = (unsigned char)session->header[6];

  if ( (data_length = content + padding) <= 0 ) {
    session->read_header = true;
    return cgi_OKE;
  }

  switch ( (unsigned char)session->header[1] ) {
    case FCGI_STDOUT:
      buffer = session->input_buffer + session->input_len;
      bytes_left = session->input_buffer_size - session->input_len;
      break;
    case FCGI_STDERR:
      buffer = session->error_buffer + session->error_len;
      bytes_left = session->error_buffer_size - session->error_len;
      break;
    default:
      /* Unsupported type, so skip data
       */
      if ( (dummy = (char*)malloc(DUMMY_BUFFER_SIZE)) == NULL ) {
        return cgi_ERROR;
      }
      buffer = dummy;
      bytes_left = DUMMY_BUFFER_SIZE;
  }

  /* Read data
   */
  if ( (unsigned int)bytes_left > (data_length - session->fcgi_data_len) ) {
    bytes_left = data_length - session->fcgi_data_len;
  }

  switch ( bytes_read = read_fcgi_socket(session, buffer, bytes_left) ) {
    case cgi_TIMEOUT:
      result = cgi_TIMEOUT;
      break;
    case cgi_FORCE_QUIT:
      result = cgi_FORCE_QUIT;
      break;
    case cgi_ERROR:
      result = cgi_ERROR;
      break;
    case 0:
      result = cgi_END_OF_DATA;
      break;
    default:
      if ( (session->fcgi_data_len += bytes_read) == data_length ) {
        session->read_header = true;
      }
      if ( session->fcgi_data_len > content ) {
        /* Read more then content (padding)
         */
        if ( session->fcgi_data_len - bytes_read < content ) {
          bytes_read = content - (session->fcgi_data_len - bytes_read);
        } else {
          bytes_read = 0;
        }
      }
      switch ( (unsigned char)session->header[1] ) {
        case FCGI_STDOUT:
          session->input_len += bytes_read;
          break;
        case FCGI_STDERR:
          session->error_len += bytes_read;
          break;
        default:
          break;
      }
  }

  if ( dummy )
    free( dummy );

  return result;
}

static int fix_crappy_cgi_headers( dap_fcgi_session_t *session )
{
  char *header_end, *new_buffer, *src, *dst;
  int count = 0, header_len, new_size;

  if ( (header_end = strstr(session->input_buffer, "\n\n")) == NULL ) {
    return 1;
  }
  header_end += 2;

  /* Count EOL
   */
  src = session->input_buffer;
  while ( src < header_end ) {
    if (*src == '\n') {
      count++;
    }
    src++;
  }

  /* Allocate new buffer
   */
  new_size = session->input_len + count;
  if ( (new_buffer = (char*)malloc(new_size + 1)) == NULL ) {
    return -1;
  }

  /* Fix EOL
   */
  src = session->input_buffer;
  dst = new_buffer;
  while ( src < header_end ) {
    if ( *src == '\n' ) {
      *(dst++) = '\r';
    }
    *(dst++) = *(src++);
  }

  /* Copy request body
   */
  header_len = header_end - session->input_buffer;
  memcpy( new_buffer + header_len + count, header_end, session->input_len - header_len + 1 );

  free( session->input_buffer );
  session->input_buffer = new_buffer;
  session->input_len += count;
  session->input_buffer_size = new_size;

  return 0;
}

const char *strncasestr(const char *haystack, const char *needle, int len) {
  int i, steps, needle_len;

  if ((haystack == NULL) || (needle == NULL)) {
    return NULL;
  }

  needle_len = strlen(needle);
  steps = len - needle_len + 1;

  for (i = 0; i < steps; i++) {
    if (strncasecmp(haystack + i, needle, needle_len) == 0) {
      return haystack + i;
    }
  }

  return NULL;
}

static char *find_cgi_header( char *buffer, int size, char *header )
{
  char *start, *pos;

  if ( !header || !buffer )
    return NULL;

  start = buffer;
  while ( (pos = strncasestr(buffer, header, size)) != NULL ) {

    if ( pos == start )
      return start;

    if ( *(pos - 1) == '\n' )
      return pos;

    size -= pos + 1 - buffer;
    buffer = pos + 1;
  }

  return NULL;
}

static int extract_http_code( char *data )
{
  int result = -1;
  char *code, c;

  while ( *data == ' ' ) 
    data ++;

  code = data;

  while ( *data ) {
    if ( *data == '\r' || *data == ' ' ) {
      c = *data;
      *data = 0;
      result = atoi( code );
      *data = c;
      break;
    }
    data ++;
  }

  return result;
}

int dap_http_fcgi_init( dap_server_t *dap_server, dap_config_t *g_config )
{
  log_it(L_NOTICE,"Initialized FCGI server module");

  fcgi_server.fcgi_path   = (char *)dap_config_get_item_str_default(   g_config, "fcgi", "bin_path", "/fcgi/helloworld" );
  fcgi_server.fcgi_url    = (char *)dap_config_get_item_str_default(   g_config, "fcgi", "fcgi_url", "/fcgi" );
  fcgi_server.port        = dap_config_get_item_int32_default( g_config, "fcgi", "port", 9000 );
  fcgi_server.unix_socket = (char *)dap_config_get_item_str_default(   g_config, "fcgi", "socket", "/tmp/fcgi.socket" );

  log_it( L_DEBUG, "fcgi_server.fcgi_path = %s", fcgi_server.fcgi_path );
  log_it( L_DEBUG, "fcgi_server.fcgi_url = %s", fcgi_server.fcgi_url );
  log_it( L_DEBUG, "fcgi_server.unix_socket = %s", fcgi_server.unix_socket );
  log_it( L_DEBUG, "fcgi_server.port = %u", fcgi_server.port );

  dap_http_t *sh = DAP_HTTP(dap_server);

  dap_http_add_proc( sh, fcgi_server.fcgi_url, &fcgi_server, 
                    dap_http_fcgi_new, 
                    dap_http_fcgi_delete, 
                    dap_http_fcgi_headers_read, 
                    dap_http_fcgi_headers_write,
                    dap_http_fcgi_data_read, 
                    dap_http_fcgi_data_write, 
                    NULL );
  return 0;
}

/**
typedef struct dap_http_client
{
    char action[128]; // Type of HTTP action (GET, PUT and etc)
    char url_path[2048]; // URL path of requested document
    uint32_t http_version_major; // Major version of HTTP protocol
    uint32_t http_version_minor; // Minor version of HTTP protocol
    bool keep_alive;

    dap_http_client_state_t state_read;
    dap_http_client_state_t state_write;

    struct dap_http_header *in_headers;
    uint64_t in_content_length;

    char in_content_type[256];
    char in_query_string[1024];
    char in_cookie[1024];

    struct dap_http_header *out_headers;
    uint64_t out_content_length;
    bool out_content_ready;
    char out_content_type[256];
    time_t out_last_modified;
    bool out_connection_close;

    dap_client_remote_t *client;
    struct dap_http *http;

    uint16_t reply_status_code;
    char reply_reason_phrase[256];

    struct dap_http_url_proc * proc;

    void *_inheritor;
    void *_internal;

} dap_http_client_t;

//Structure for holding HTTP header in the bidirectional list
typedef struct dap_http_header{
    char *name;
    char *value;
    struct dap_http_header *next;
    struct dap_http_header *prev;
} dap_http_header_t;

*/

void dap_http_fcgi_headers_read( dap_http_client_t *cl_ht, void *arg )
{
/*
  log_it( L_DEBUG, "dap_http_fcgi_headers_read" );
  log_it( L_DEBUG, "arg = %X", arg );

  log_it( L_DEBUG, "action: %s", cl_ht->action );
  log_it( L_DEBUG, "url_path: %s", cl_ht->url_path );
  log_it( L_DEBUG, "keep_alive: %u", cl_ht->keep_alive );
  log_it( L_DEBUG, "http version: %u.%u", cl_ht->http_version_major, cl_ht->http_version_minor );
  log_it( L_DEBUG, "http version: %u.%u", cl_ht->http_version_major, cl_ht->http_version_minor );
  log_it( L_DEBUG, "in_content_type: %s", cl_ht->in_content_type );
  log_it( L_DEBUG, "in_query_string: %s", cl_ht->in_query_string );
  log_it( L_DEBUG, "in_cookie      : %s", cl_ht->in_cookie );
  log_it( L_DEBUG, "in_content_length: %u", cl_ht->in_content_length );

  log_it( L_DEBUG, "reply_reason_phrase: %s", cl_ht->reply_reason_phrase );
  log_it( L_DEBUG, "reply_status_code: %u", cl_ht->reply_status_code );
  int i = 0;
  for ( dap_http_header_t *dh = cl_ht->in_headers; dh; dh = dh->next ) {

    log_it( L_DEBUG, "Header %u) name: %s", i, dh->name );
    log_it( L_DEBUG, "Header %u) value: %s", i, dh->value );
    ++ i;
  }
*/
  cl_ht->state_write = DAP_HTTP_CLIENT_STATE_START;
  cl_ht->state_read  = cl_ht->keep_alive ? DAP_HTTP_CLIENT_STATE_START : DAP_HTTP_CLIENT_STATE_NONE;

  dap_client_remote_ready_to_write( cl_ht->client, true );
  dap_client_remote_ready_to_read( cl_ht->client, cl_ht->keep_alive );
}


void dap_http_fcgi_headers_write( dap_http_client_t *cl_ht, void *arg )
{
  dap_fcgi_session_t *session;
  int result, header_length, delta, return_code = 200;
  char *end_of_header, *code, *str_end, *str;//, *code, c, *str, *sendfile = NULL;
  bool content_type_set = false;
//  bool content_ready = false;
  cgi_result_t cgi_result;

  log_it( L_DEBUG, "dap_http_fcgi_headers_write" );

  cl_ht->_inheritor = DAP_NEW_Z( dap_fcgi_session_t );

  session = DAP_FCGI_SESSION( cl_ht );
  session->client = cl_ht;

//  session->sock = connect_to_unix_socket( fcgi_server.unix_socket );
//  if ( session->sock == -1 ) {
//    log_it( L_ERROR, "can't connect to \"%s\"", fcgi_server.unix_socket );
//    return_code = 500;
//    goto done;
//  }

  log_it( L_ERROR, "can't connect to \"%s\"", fcgi_server.unix_socket );
  return_code = 500;
  goto done;

  session->deadline = time(NULL) + 16;

  #define CGI_BUFFER_SIZE   (32 * 1024)

  session->input_buffer_size = session->error_buffer_size = CGI_BUFFER_SIZE;
  session->input_len = session->error_len = 0;

  if ( (session->input_buffer = (char*)malloc(session->input_buffer_size + 1)) == NULL ) {
    return_code = 500;
    goto done;
  }
  if ( (session->error_buffer = (char*)malloc(session->error_buffer_size + 1)) == NULL ) {
    free(session->input_buffer);
    session->input_buffer = NULL;
    return_code = 500;
    goto done;
  }

  log_it( L_NOTICE, "connected to \"%s\"", fcgi_server.unix_socket );

  if ( send_fcgi_request(session) == -1 ) {
    log_it( L_ERROR, "send_fcgi_request: error");
    return_code = 500;
    goto done;
    return;
  }

//  send_in_chunks = false;
  session->read_header = true;

//  if ( time(NULL) > session->deadline )
//    cgi_result = cgi_TIMEOUT;
//  else

  cgi_result = read_from_fcgi_server( session );

  switch ( cgi_result ) {
    case cgi_ERROR:
      log_it( L_ERROR, "FCGI error \n");
      return_code = 500;
      break;
    case cgi_TIMEOUT:
      log_it( L_ERROR, "FCGI timeout \n");
        return_code = 500;
      break;
    case cgi_FORCE_QUIT:
      break;
    case cgi_OKE:
      log_it(L_NOTICE, "[[[[ cgi_OKE ]]]" );
/*
        if (session->error_len > 0) {
          //Error received from CGI
        }
*/
      if ( !session->input_len ) break;

      *(session->input_buffer + session->input_len) = 0;

      // Serching end_of_header
      if ( (end_of_header = strstr(session->input_buffer, "\r\n\r\n")) == NULL ) {

        if ( (result = fix_crappy_cgi_headers(session)) == -1 ) {
          log_it( L_ERROR, "error fixing crappy CGI headers");
          return_code = 500;
          break;
        }
        if ( !result )
          end_of_header = strstr( session->input_buffer, "\r\n\r\n" );
      }
      else
        log_it( L_NOTICE, "end_of_header found, header size = %u", end_of_header - session->input_buffer );

      if ( !end_of_header ) break;

      return_code = 200;

      if ( (strncmp(session->input_buffer, "HTTP/1.0 ", 9) == 0) || (strncmp(session->input_buffer, "HTTP/1.1 ", 9) == 0) ) {

        if ( (str_end = strstr(session->input_buffer, "\r\n")) != NULL ) {
          return_code = extract_http_code( session->input_buffer + 9 );
  
          delta = str_end - session->input_buffer + 2;
          memmove( session->input_buffer, session->input_buffer + delta, session->input_len - delta );
          session->input_len -= delta;
          end_of_header -= delta;
        }
      }

      header_length = end_of_header + 4 - session->input_buffer;

      if ( return_code == 200 ) {
        if ( (code = find_cgi_header(session->input_buffer, header_length, "Status:")) != NULL ) {
          return_code = extract_http_code( code + 7 );
        }
      }

      if ( return_code <= 0 || return_code > 999 ) {
        log_it( L_ERROR, "invalid status code received from CGI: %u", return_code );
      }

      if ( (code = find_cgi_header(session->input_buffer, header_length, "Content-type:")) != NULL ) {
        code += 13;
        if ( (str = strstr(code, "\r\n")) != NULL ) {
          *str = 0;
          strcpy( cl_ht->out_content_type, code );
          *str = '\r';
          content_type_set = true;
          log_it( L_ERROR, "Content-type: detected: '%s'", cl_ht->out_content_type );
        }
      }

      delta = header_length;
      memmove( session->input_buffer, session->input_buffer + delta, session->input_len - delta );
      session->input_len -= delta;
      //end_of_header -= delta;

      break;
    case cgi_END_OF_DATA:

      break;

    } /* switch */

done:;

  cl_ht->reply_status_code = return_code;
  if ( return_code != 200 ) {
    cl_ht->client->signal_close = true;
    return;
  }

  cl_ht->out_last_modified = time( NULL );
  cl_ht->out_content_length = 0;
  cl_ht->out_content_ready = true;

  if ( !content_type_set ) {
    strcpy( cl_ht->out_content_type, "text/html" );
  }

  log_it( L_ERROR, "DEBUG 0000" );

  return;
}

void dap_http_fcgi_data_read( dap_http_client_t *cl_ht, void *arg )
{
  int *bytes_return = (int*) arg; // Return number of read bytes
  //Do nothing
  *bytes_return = cl_ht->client->buf_in_size;
}

void dap_http_fcgi_data_write( dap_http_client_t *cl_ht, void *arg )
{
  dap_fcgi_session_t *session = DAP_FCGI_SESSION( cl_ht );
  cgi_result_t cgi_result;

  log_it( L_DEBUG, "dap_http_fcgi_data_write");

  if ( session->input_len ) {

    memcpy( cl_ht->client->buf_out, session->input_buffer, session->input_len );
    cl_ht->client->buf_out_size = session->input_len;
    session->input_len = 0;
    return;
  }

  session->input_len = 0;

  cgi_result = read_from_fcgi_server( session );

  switch ( cgi_result ) {
    case cgi_ERROR:
      log_it( L_ERROR, "FCGI error");
      goto done;
      break;
    case cgi_TIMEOUT:
      log_it( L_ERROR, "FCGI timeout");
      goto done;
      break;
    case cgi_FORCE_QUIT:
      goto done;
      break;
    case cgi_OKE:
//      log_it( L_ERROR, "FCGI OK");
//      log_it( L_DEBUG, "session->input_len = %u", session->input_len );
      if ( session->input_len ) {

        memcpy( cl_ht->client->buf_out, session->input_buffer, session->input_len );
        cl_ht->client->buf_out_size = session->input_len;
        session->input_len = 0;
        return;
      }
      else 
        goto done;
      break;

    case cgi_END_OF_DATA:
//      log_it( L_DEBUG, "[[[[[[ end of data ]]]]]]" );
      break;
  }

  return;

/**
    (void) arg;
    dap_http_file_t * cl_ht_file= DAP_HTTP_FILE(cl_ht);
    cl_ht->client->buf_out_size=fread(cl_ht->client->buf_out,1,sizeof(cl_ht->client->buf_out),cl_ht_file->fd);
    cl_ht_file->position+=cl_ht->client->buf_out_size;

    if(feof(cl_ht_file->fd)!=0){
        log_it(L_INFO, "All the file %s is sent out",cl_ht_file->local_path);
        //strncat(cl_ht->client->buf_out+cl_ht->client->buf_out_size,"\r\n",sizeof(cl_ht->client->buf_out));
        fclose(cl_ht_file->fd);
        dap_client_remote_ready_to_write(cl_ht->client,false);
        cl_ht->client->signal_close=!cl_ht->keep_alive;
        cl_ht->state_write=DAP_HTTP_CLIENT_STATE_NONE;
    }
**/

done:;

  dap_client_remote_ready_to_write( cl_ht->client, false );
  cl_ht->state_write = DAP_HTTP_CLIENT_STATE_NONE;
  cl_ht->client->signal_close = true;
  close( session->sock );

//  log_it( L_DEBUG, "dap_http_fcgi_data_write" );
}

void dap_http_fcgi_deinit( void )
{

  log_it(L_NOTICE,"DeInitialized FCGI server module");
  return;
}

