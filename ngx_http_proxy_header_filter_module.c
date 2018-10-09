//
//  ngx_http_proxy_header_filter_module.c
//  ngx_http_proxy_header_filter_module
//
//  Created by 高阳 on 2018/5/22.
//  Copyright © 2018年 friparia. All rights reserved.
//
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

#include "ngx_http_proxy_header_filter_module.h"
typedef struct{
    ngx_str_t key;
    ngx_str_t match;
    ngx_str_t value;
} ngx_http_proxy_header_filter_headers_elt_t;

typedef struct {
    ngx_list_t *header_filters;
} ngx_http_proxy_header_filter_loc_conf_t;

typedef struct {
    unsigned modified;
} ngx_http_proxy_header_filter_ctx_t;

static char *ngx_http_proxy_header_filter(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static void *ngx_http_proxy_header_filter_create_loc_conf(ngx_conf_t *cf);
static char *ngx_http_proxy_header_filter_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child);
static ngx_int_t ngx_http_proxy_header_filter_init(ngx_conf_t *cf);

//u_char *str_replace(u_char *search, u_char *replace, u_char *subject);

static ngx_command_t ngx_http_proxy_header_filter_commands[] = {
    {
        ngx_string("proxy_header_filter"),
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE3,
        ngx_http_proxy_header_filter,
        NGX_HTTP_LOC_CONF_OFFSET,
        0,
        NULL
    },
    ngx_null_command
};

static ngx_http_module_t ngx_http_proxy_header_filter_module_ctx = {
    NULL,
    ngx_http_proxy_header_filter_init,
    NULL,
    NULL,
    NULL,
    NULL,
    ngx_http_proxy_header_filter_create_loc_conf,
    ngx_http_proxy_header_filter_merge_loc_conf
};

ngx_module_t ngx_http_proxy_header_filter_module = {
    NGX_MODULE_V1,
    &ngx_http_proxy_header_filter_module_ctx,
    ngx_http_proxy_header_filter_commands,
    NGX_HTTP_MODULE,
    NULL,                               /* init master */
    NULL,                               /* init module */
    NULL,                               /* init process */
    NULL,                               /* init thread */
    NULL,                               /* exit thread */
    NULL,                               /* exit process */
    NULL,                               /* exit master */
    NGX_MODULE_V1_PADDING
};

static ngx_http_output_header_filter_pt ngx_http_next_header_filter;
u_char *str_replace(ngx_pool_t *pool, u_char *search, u_char *replace,  u_char *subject){
    char *old = NULL, *new_subject = NULL ;
    char *p = NULL;
    ngx_int_t c = 0 , search_size;
    search_size = ngx_strlen(search);
    for(p = ngx_strstr(subject, search); p != NULL ; p = ngx_strstr(p + search_size , search))
    {
        c++;
    }
    c = (ngx_strlen(replace) - search_size) * c + ngx_strlen(subject) + 1;
    new_subject = ngx_palloc(pool, c);
    
    memset(new_subject, '\0', c);
    old = (char *)subject;
    for(p = ngx_strstr(subject , search) ; p != NULL ; p = ngx_strstr(p + search_size , search))
    {
    
        strncpy(new_subject + strlen(new_subject) , old , p - old);
        strcpy(new_subject + strlen(new_subject) , (char*)replace);
        old = p + search_size;
    }
    
    strcpy(new_subject + strlen(new_subject) , old);
    
    return (u_char*)new_subject;
}

static ngx_int_t ngx_http_proxy_header_header_filter(ngx_http_request_t *r){
    ngx_list_part_t     *part, *filter_part;
    ngx_http_proxy_header_filter_headers_elt_t *filter_elt;
    ngx_table_elt_t *var;
    ngx_uint_t i;
    ngx_str_t key, value;
    ngx_http_proxy_header_filter_loc_conf_t *lccf;
    u_char *replaced_str;
    lccf = ngx_http_get_module_loc_conf(r, ngx_http_proxy_header_filter_module);
    if(lccf->header_filters == NULL){
        return ngx_http_next_header_filter(r);
    }
    
    
    filter_part = &lccf->header_filters->part;
    filter_elt = filter_part->elts;
    for(i = 0; ;i++){
        if(i >= filter_part->nelts){
            if(filter_part->next == NULL){
                break;
            }
            filter_part = filter_part->next;
            filter_elt = filter_part->elts;
            i = 0;
        }
        ngx_log_debug5(NGX_LOG_DEBUG_EVENT, r->connection->log, 0, "proxy header filter, get from config %p %p: %s %s %s", lccf->header_filters, filter_elt, filter_elt->key.data, filter_elt->match.data, filter_elt->value.data);
        part = &r->headers_out.headers.part;
        var = part->elts;
        for(i = 0; ;i++){
            if(i >= part->nelts){
                if(part->next == NULL){
                    break;
                }
                part = part->next;
                var = part->elts;
                i = 0;
            }
            
            key = var[i].key;
            if(!ngx_strcasecmp(key.data, filter_elt->key.data)){
                value = var[i].value;
                ngx_log_debug2(NGX_LOG_DEBUG_EVENT, r->connection->log, 0, "proxy header filter, origin header %s: %s", key.data, value.data);
                replaced_str = str_replace(r->pool, filter_elt->match.data, filter_elt->value.data, value.data);
                ngx_log_debug1(NGX_LOG_DEBUG_EVENT, r->connection->log, 0, "proxy header filter, replaced str: %s", replaced_str);
                var[i].value.data = replaced_str;
                var[i].value.len = ngx_strlen(replaced_str);
            }
            
        }
    }
    return ngx_http_next_header_filter(r);
}


static char *ngx_http_proxy_header_filter(ngx_conf_t *cf, ngx_command_t *cmd, void *conf){
    
    ngx_str_t *directive;
    ngx_str_t key, match, value;
    ngx_http_proxy_header_filter_loc_conf_t *lccf = conf;
    ngx_list_t *header_filters;
    ngx_http_proxy_header_filter_headers_elt_t *elt;
    
    header_filters = lccf->header_filters;
    if(header_filters == NULL){
        header_filters = ngx_list_create(cf->pool, 1, sizeof(ngx_http_proxy_header_filter_headers_elt_t));
    }
    
    
    directive = cf->args->elts;
    key = directive[1];
    match = directive[2];
    value = directive[3];
    elt = ngx_list_push(header_filters);
    elt->key = key;
    elt->match = match;
    elt->value = value;
    ngx_log_debug6(NGX_LOG_DEBUG_EVENT, cf->log, 0, "proxy header filter, set: %p %p %p,  %s %s %s", lccf, header_filters, elt, elt->key.data, elt->match.data, elt->value.data);
    
    lccf->header_filters = header_filters;
    return NGX_CONF_OK;
}

static void *ngx_http_proxy_header_filter_create_loc_conf(ngx_conf_t *cf){
    ngx_http_proxy_header_filter_loc_conf_t *conf;
    conf = ngx_palloc(cf->pool, sizeof(ngx_http_proxy_header_filter_loc_conf_t));
    if(conf == NULL){
        return NULL;
    }
    conf->header_filters = NULL;
    return conf;
}

static char *ngx_http_proxy_header_filter_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child){
    ngx_http_proxy_header_filter_loc_conf_t *parent_conf = parent, *child_conf = child;
    ngx_list_t *parent_list, *child_list, *temp_list;
    ngx_list_part_t *parent_part, *child_part, *temp_part;
    ngx_uint_t i, j;
    
    ngx_http_proxy_header_filter_headers_elt_t *parent_elt, *child_elt, *temp_elt, *new_elt;
    parent_list = parent_conf->header_filters;
    child_list = child_conf->header_filters;
    temp_list = ngx_list_create(cf->pool, 1, sizeof(ngx_http_proxy_header_filter_headers_elt_t));
    if(child_list == NULL){
        child_list = ngx_list_create(cf->pool, 1, sizeof(ngx_http_proxy_header_filter_headers_elt_t));
    }
    if(parent_list != NULL){
        parent_part = &parent_list->part;
        parent_elt = parent_part->elts;
        for(i = 0; ; i++){
            if(i >= parent_part->nelts){
                if(parent_part->next == NULL){
                    break;
                }
                parent_part = parent_part->next;
                parent_elt = parent_part->elts;
                i = 0;
            }
            ngx_log_debug6(NGX_LOG_DEBUG_EVENT,  cf->log , 0, "proxy header filter, get from parent %p %p %p: %s %s %s", parent_conf, parent_list, parent_elt, parent_elt->key.data, parent_elt->match.data, parent_elt->value.data);
            
            child_part = &child_list->part;
            child_elt = child_part->elts;
            ngx_uint_t in_array;
            in_array = 0;
            for(j = 0; ; j++){
                if(j >= child_part->nelts){
                    if(child_part->next == NULL){
                        break;
                    }
                    child_part = child_part->next;
                    child_elt = child_part->elts;
                    j = 0;
                }
                ngx_log_debug6(NGX_LOG_DEBUG_EVENT, cf->log, 0, "proxy header filter, get from child %p %p %p: %s %s %s", child_conf, child_list, child_elt, child_elt->key.data, child_elt->match.data, child_elt->value.data);
                ngx_uint_t r1, r2;
                r1 = ngx_strcasecmp(parent_elt->key.data, child_elt->key.data);
                r2 = ngx_strcasecmp(parent_elt->match.data, child_elt->match.data);
                ngx_log_debug6(NGX_LOG_DEBUG_EVENT, cf->log, 0, "proxy header filter, result %d (%s %s), %d (%s, %s)", (int)r1, parent_elt->key.data, child_elt->key.data, (int)r2, parent_elt->match.data, child_elt->match.data);
                if(!r1 && !r2){
                    in_array = 1;
                }
                
            }
            if(!in_array){
                ngx_log_debug3(NGX_LOG_DEBUG_EVENT, cf->log, 0, "proxy header filter, child not exists: %s %s %s", parent_elt->key.data, parent_elt->match.data, parent_elt->value.data);
                temp_elt = ngx_list_push(temp_list);
                ngx_str_set(&temp_elt->key, parent_elt->key.data);
                ngx_str_set(&temp_elt->match, parent_elt->match.data);
                ngx_str_set(&temp_elt->value, parent_elt->value.data);
            }
        }
        
    }else{
        ngx_log_debug2(NGX_LOG_DEBUG_EVENT, cf->log, 0, "proxy header filter, parent null, parent_conf: %p, child_conf: %p", parent_conf, child_conf);
    }
    temp_part = &temp_list->part;
    temp_elt = temp_part->elts;
    for(i = 0; ; i++){
        if(i >= temp_part->nelts){
            if(temp_part->next == NULL){
                break;
            }
            temp_part = temp_part->next;
            temp_elt = temp_part->elts;
            i = 0;
        }
        new_elt = ngx_list_push(child_list);
        ngx_str_set(&new_elt->key, temp_elt->key.data);
        ngx_str_set(&new_elt->match, temp_elt->match.data);
        ngx_str_set(&new_elt->value, temp_elt->value.data);
    }
    
    child_part  = &child_list->part;
    child_elt = child_part->elts;
    for(i = 0; ; i++){
        if(i >= child_part->nelts){
            if(child_part->next == NULL){
                break;
            }
            child_part = child_part->next;
            child_elt = child_part->elts;
            i = 0;
        }
        ngx_log_debug6(NGX_LOG_DEBUG_EVENT, cf->log, 0, "proxy header filter, merged: %p %p %p: %s %s %s", child_conf, child_list, child_elt, child_elt->key.data, child_elt->match.data, child_elt->value.data);
    }
    return NGX_CONF_OK;
}

static ngx_int_t ngx_http_proxy_header_filter_init(ngx_conf_t *cf){
    
    ngx_http_next_header_filter = ngx_http_top_header_filter;
    ngx_http_top_header_filter = ngx_http_proxy_header_header_filter;
    return NGX_OK;
}
