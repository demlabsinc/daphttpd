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

#ifndef WIN32
#include "pthread.h"
#else
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <winsock2.h>
#include <windows.h>
#include <mswsock.h>
#include <ws2tcpip.h>
#include <io.h>
#include "wrappers.h"
#include "../pthread-win32/pthread.h"
#include <wepoll.h>
#endif

#include "uthash.h"
#include "dap_common.h"

#include "../http_server/http_client/dap_http_client.h"
#include "../http_server/http_client/dap_http_header.h"

#include "dap_enc.h"
#include "dap_enc_ks.h"
#include "dap_enc_key.h"
#include "dap_memcached.h"

#define LOG_TAG "dap_enc_ks"

static dap_enc_ks_key_t * _ks = NULL;
static bool s_memcache_enable = false;
static time_t s_memcache_expiration_key = 0;

int dap_enc_ks_init(bool a_memcache_backup_enable, time_t a_memcache_expiration_key)
{
    if(a_memcache_backup_enable) {
        if(dap_memcache_is_enable()) {
            s_memcache_expiration_key = a_memcache_expiration_key;
            s_memcache_enable = true;
        } else {
            log_it(L_ERROR, "Can't init memcache backup. Memcache module is not activated.");
            return -1;
        }
    }
    return 0;
}

void _enc_key_free(dap_enc_ks_key_t **ptr);

void dap_enc_ks_deinit()
{
    if (_ks) {
        dap_enc_ks_key_t *cur_item, *tmp;
        HASH_ITER(hh, _ks, cur_item, tmp) {
            HASH_DEL(_ks, cur_item);
            _enc_key_free(&cur_item);
        }
    }
}

inline static void s_gen_session_id(char a_id_buf[DAP_ENC_KS_KEY_ID_SIZE])
{
    for(short i = 0; i < DAP_ENC_KS_KEY_ID_SIZE; i++)
        a_id_buf[i] = 65 + rand() % 25;
}

void s_save_key_in_storge(dap_enc_ks_key_t *a_key)
{
    HASH_ADD_STR(_ks,id,a_key);
    if(s_memcache_enable) {
        dap_enc_key_serealize_t* l_serealize_key = dap_enc_key_serealize(a_key->key);
        dap_memcache_put(a_key->id, l_serealize_key, sizeof (dap_enc_key_serealize_t), s_memcache_expiration_key);
        free(l_serealize_key);
    }
}


dap_enc_ks_key_t * dap_enc_ks_find(const char * v_id)
{
    dap_enc_ks_key_t * ret = NULL;
    HASH_FIND_STR(_ks,v_id,ret);
    if(ret == NULL) {
        if(s_memcache_enable) {
            void* l_key_buf;
            size_t l_val_length;
            bool find = dap_memcache_get(v_id, &l_val_length, (void**)&l_key_buf);
            if(find) {
                if(l_val_length != sizeof (dap_enc_key_serealize_t)) {
                    log_it(L_WARNING, "Data can be broken");
                }
                dap_enc_key_t* key = dap_enc_key_deserealize(l_key_buf, l_val_length);
                ret = DAP_NEW_Z(dap_enc_ks_key_t);
                strncpy(ret->id, v_id, DAP_ENC_KS_KEY_ID_SIZE);
                pthread_mutex_init(&ret->mutex,NULL);
                ret->key = key;
                HASH_ADD_STR(_ks,id,ret);
                free(l_key_buf);
                return ret;
            }
        }
        log_it(L_WARNING, "Key not found");
    }
    return ret;
}

dap_enc_key_t * dap_enc_ks_find_http(struct dap_http_client * a_http_client)
{
    dap_http_header_t * hdr_key_id=dap_http_header_find(a_http_client->in_headers,"KeyID");

    if(hdr_key_id){
        
        dap_enc_ks_key_t * ks_key=dap_enc_ks_find(hdr_key_id->value);
        if(ks_key)
            return ks_key->key;
        else{
            log_it(L_WARNING, "Not found keyID %s in storage", hdr_key_id->value);
            return NULL;
        }
    }else{
        log_it(L_WARNING, "No KeyID in HTTP headers");
        return NULL;
    }
}

dap_enc_ks_key_t * dap_enc_ks_new()
{
    dap_enc_ks_key_t * ret = DAP_NEW_Z(dap_enc_ks_key_t);
    s_gen_session_id(ret->id);
    pthread_mutex_init(&ret->mutex,NULL);
    return ret;
}

bool dap_enc_ks_save_in_storage(dap_enc_ks_key_t* key)
{
    if(dap_enc_ks_find(key->id) != NULL) {
        log_it(L_WARNING, "key is already saved in storage");
        return false;
    }
    s_save_key_in_storge(key);
    return true;
}

dap_enc_ks_key_t * dap_enc_ks_add(struct dap_enc_key * key)
{
    dap_enc_ks_key_t * ret = DAP_NEW_Z(dap_enc_ks_key_t);
    ret->key = key;
    pthread_mutex_init(&ret->mutex, NULL);
    s_gen_session_id(ret->id);
    dap_enc_ks_save_in_storage(ret);
    return ret;
}

void dap_enc_ks_delete(const char *id)
{
    dap_enc_ks_key_t *delItem = dap_enc_ks_find(id);
    if (delItem) {
        HASH_DEL (_ks, delItem);
        if(s_memcache_enable && dap_memcache_delete(id) == false) {
            log_it(L_WARNING, "Cant delete key from memcache. Not found");
        }
        pthread_mutex_destroy(&delItem->mutex);
        _enc_key_free(&delItem);
        return;
    }
    log_it(L_WARNING, "Can't delete key by id: %s. Key not found", id);
}

void _enc_key_free(dap_enc_ks_key_t **ptr)
{
    if (*ptr){
        if((*ptr)->key)
            dap_enc_key_delete((*ptr)->key);
        free (*ptr);
        //*ptr = NULL; //not need
    }
}
