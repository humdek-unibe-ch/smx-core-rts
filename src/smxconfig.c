/**
 * @author  Simon Maurer
 * @license
 *  This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this file,
 *  You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * Helper functions for parsing configuration files of the runtime systemr
 * library of Streamix.
 */

#include <stdarg.h>
#include "smxconfig.h"
#include "smxutils.h"
#include "smxlog.h"

/******************************************************************************/
int smx_config_data_maps_apply( smx_config_data_maps_t* maps,
        bson_t* src_payload )
{
    int i, rc, err = 0;
    bson_t* src_payload_item = src_payload;

    if( maps == NULL )
        return false;

    if( maps->is_extended )
    {
        return smx_config_data_maps_apply_ext( maps, src_payload_item );
    }
    else
    {
        for( i = 0; i < maps->count; i++ )
        {
            if( maps->items[i].src_payload != NULL )
            {
                src_payload_item = maps->items[i].src_payload;
            }
            if( src_payload_item != NULL && maps->items[i].src_path != NULL )
            {
                rc = smx_config_data_maps_apply_base( &maps->items[i],
                        src_payload_item );
                if( rc < 0 )
                {
                    err = rc;
                }
            }
        }
    }

    return err;
}

/******************************************************************************/
int smx_config_data_maps_apply_base( smx_config_data_map_t* key_map,
        bson_t* src_payload )
{
    char src_path[256];
    const char* src_path_ptr = key_map->src_path;
    bson_iter_t* iter;

    if( strcmp( key_map->src_path, "." ) == 0 )
    {
        return SMX_CONFIG_MAP_ERROR_BAD_ROOT_TYPE;
    }

    if( BSON_ITER_HOLDS_BOOL( &key_map->tgt_iter ) )
    {
        bson_iter_overwrite_bool( &key_map->tgt_iter,
                key_map->fallback.v_bool );
    }
    else if( BSON_ITER_HOLDS_DOUBLE( &key_map->tgt_iter ) )
    {
        bson_iter_overwrite_double( &key_map->tgt_iter,
                key_map->fallback.v_double );
    }
    else if( BSON_ITER_HOLDS_INT64( &key_map->tgt_iter ) )
    {
        bson_iter_overwrite_int64( &key_map->tgt_iter,
                key_map->fallback.v_int64 );
    }
    else if( BSON_ITER_HOLDS_INT32( &key_map->tgt_iter ) )
    {
        bson_iter_overwrite_int32( &key_map->tgt_iter,
                key_map->fallback.v_int32 );
    }

    if( key_map->src_prefix != NULL )
    {
        sprintf( src_path, "%s%s", key_map->src_prefix, key_map->src_path );
        src_path_ptr = src_path;
    }

    if( smx_config_data_map_get_iter( src_payload, src_path_ptr,
                &key_map->src_iter ) )
    {
        key_map->is_src_iter_set = true;
        iter = &key_map->src_iter;
        if( BSON_ITER_HOLDS_BOOL( &key_map->tgt_iter ) )
        {
            if( smx_config_data_map_can_write_bool( iter ) )
            {
                bson_iter_overwrite_bool( &key_map->tgt_iter,
                        bson_iter_as_bool( iter ) );
            }
        }
        else if( BSON_ITER_HOLDS_DOUBLE( &key_map->tgt_iter ) )
        {
            if( smx_config_data_map_can_write_double( iter ) )
            {
                bson_iter_overwrite_double( &key_map->tgt_iter,
                        bson_iter_as_double( iter ) );
            }
        }
        else if( BSON_ITER_HOLDS_INT64( &key_map->tgt_iter ) )
        {
            if( smx_config_data_map_can_write_int64( iter ) )
            {
                bson_iter_overwrite_int64( &key_map->tgt_iter,
                        bson_iter_as_int64( iter ) );
            }
        }
        else if( BSON_ITER_HOLDS_INT32( &key_map->tgt_iter ) )
        {
            if( smx_config_data_map_can_write_int32( iter ) )
            {
                bson_iter_overwrite_int32( &key_map->tgt_iter,
                    bson_iter_int32( iter ) );
            }
        }
    }
    else
    {
        return SMX_CONFIG_MAP_ERROR_MISSING_SRC_KEY;
    }
    return 0;
}

/*****************************************************************************/
int smx_config_data_maps_apply_ext( smx_config_data_maps_t* maps,
        bson_t* src_payload )
{
    bson_iter_t i_tgt;

    if( maps->h )
    {
        SMX_LOG_NET( maps->h, debug, "apply maps in extendend mode" );
    }

    bson_iter_init( &i_tgt, maps->tgt_payload );
    bson_destroy( &maps->mapped_payload );
    bson_init( &maps->mapped_payload );

    return smx_config_data_maps_apply_ext_iter( &i_tgt, src_payload,
            &maps->mapped_payload, "", maps );
}

/*****************************************************************************/
int smx_config_data_maps_apply_ext_iter( bson_iter_t* i_tgt,
        bson_t* src_payload, bson_t* payload, const char* key,
        smx_config_data_maps_t* maps )
{
    int rc;
    bson_iter_t child;
    bson_t child_payload;
    char dot_key[1000];
    const char* iter_key;
    while( bson_iter_next( i_tgt ) )
    {
        strcpy( dot_key, key );
        iter_key = bson_iter_key( i_tgt );
        strcat( dot_key, iter_key );
        rc = smx_config_data_map_append_val( dot_key, iter_key, src_payload,
                payload, maps );
        if( rc == 0 )
        {
            if( maps->h )
            {
                SMX_LOG_NET( maps->h, debug, "appended mapped value at '%s'",
                        dot_key );
            }
            continue;
        }
        if( BSON_ITER_HOLDS_DOCUMENT( i_tgt )
                && bson_iter_recurse( i_tgt, &child ) )
        {
            strcat( dot_key, "." );
            bson_init( &child_payload );
            smx_config_data_maps_apply_ext_iter( &child,
                    src_payload, &child_payload, dot_key, maps );
            BSON_APPEND_DOCUMENT( payload, iter_key, &child_payload );
            bson_destroy( &child_payload );
        }
        else if( BSON_ITER_HOLDS_ARRAY( i_tgt )
                && bson_iter_recurse( i_tgt, &child ) )
        {
            strcat( dot_key, "." );
            bson_init( &child_payload );
            smx_config_data_maps_apply_ext_iter( &child,
                    src_payload, &child_payload, dot_key, maps );
            BSON_APPEND_ARRAY( payload, iter_key, &child_payload );
            bson_destroy( &child_payload );
        }
        else
        {
            BSON_APPEND_VALUE( payload, iter_key, bson_iter_value( i_tgt ) );
            if( maps->h )
            {
                SMX_LOG_NET( maps->h, debug, "appended unmapped value at '%s'",
                        dot_key );
            }
        }
    }
    return 0;
}

/******************************************************************************/
void smx_config_data_maps_cleanup( smx_config_data_maps_t* maps )
{
    int i;
    if( maps == NULL )
        return;

    for( i = 0 ; i < maps->count; i++ )
    {
        if( maps->items[i].src_payload != NULL )
        {
            bson_destroy( maps->items[i].src_payload );
        }
    }
    bson_destroy( &maps->mapped_payload );
    free( maps );
}

/******************************************************************************/
bson_t* smx_config_data_maps_get_mapped_payload(
        smx_config_data_maps_t* maps )
{
    if( maps ==  NULL )
        return NULL;

    return &maps->mapped_payload;
}

/******************************************************************************/
smx_config_data_map_t* smx_config_data_maps_get_map_by_key(
        smx_config_data_maps_t* maps, const char* key )
{
    int i;

    for( i = 0; i < maps->count; i++ )
    {
        if( maps->items[i].key != NULL
                && strcmp( key, maps->items[i].key ) == 0 )
        {
            return &maps->items[i];
        }
    }

    return NULL;
}

/******************************************************************************/
int smx_config_data_maps_init( bson_iter_t* i_fields, bson_t* data,
        smx_config_data_maps_t* maps )
{
    int rc, i;
    maps->h = NULL;
    maps->count = 0;
    maps->is_extended = false;
    maps->tgt_payload = data;
    bson_copy_to( data, &maps->mapped_payload );
    bson_iter_t i_field;

    for( i = 0; i < SMX_CONFIG_MAX_MAP_ITEMS; i++ )
    {
        maps->items[i].is_src_iter_set = false;
        maps->items[i].key = NULL;
        maps->items[i].src_path = NULL;
        maps->items[i].src_prefix = NULL;
        maps->items[i].tgt_path = NULL;
        maps->items[i].src_payload = NULL;
        maps->items[i].type = BSON_TYPE_UNDEFINED;
        maps->items[i].h = NULL;
    }

    while( bson_iter_next( i_fields ) )
    {
        if( maps->count >= SMX_CONFIG_MAX_MAP_ITEMS )
        {
            return SMX_CONFIG_MAP_ERROR_MAP_COUNT_EXCEEDED;
        }
        if( BSON_ITER_HOLDS_DOCUMENT( i_fields )
                && bson_iter_recurse( i_fields, &i_field ) )
        {
            rc = smx_config_data_map_init( &maps->mapped_payload, &i_field,
                    &maps->is_extended, &maps->items[maps->count] );
            if( rc < 0 )
            {
                return rc;
            }
            maps->count++;
        }
        else
        {
            return SMX_CONFIG_MAP_ERROR_BAD_MAP_TYPE;
        }
    }

    if( maps->count == 0 ) {
        return SMX_CONFIG_MAP_ERROR_NO_MAP_ITEM;
    }

    return 0;
}

/******************************************************************************/
void smx_config_data_maps_init_net_handler( smx_config_data_maps_t* maps,
        void* h )
{
    int i;

    maps->h = h;
    for( i = 0; i < maps->count; i++ )
    {
        maps->items[i].h = h;
    }
}

/*****************************************************************************/
int smx_config_data_map_append_val( const char* dot_key,
        const char* iter_key, bson_t* src_payload, bson_t* payload,
        smx_config_data_maps_t* maps )
{
    int i;
    const bson_oid_t* oid;
    bson_oid_t oid_init;
    char oid_str[25];
    bson_t* src_payload_item = src_payload;
    bson_iter_t* src_child;
    char src_path[256];
    const char* src_path_ptr;
    uint32_t data_len;
    const uint8_t* data_doc;
    bson_t doc;

    for( i = 0; i < maps->count; i++ )
    {
        if( maps->items[i].src_payload != NULL )
        {
            src_payload_item = maps->items[i].src_payload;
        }
        if( src_payload_item == NULL || maps->items[i].src_path == NULL )
        {
            if( maps->h )
            {
                SMX_LOG_NET( maps->h, warn, "no src payload or src path"
                        " defined, ignoring map %d at '%s'", i, dot_key );
            }
            continue;
        }
        if( strcmp( dot_key, maps->items[i].tgt_path ) == 0 )
        {
            if( strcmp( maps->items[i].src_path, "." ) == 0 )
            {
                BSON_APPEND_DOCUMENT( payload, iter_key, src_payload_item );
                return 0;
            }

            src_path_ptr = maps->items[i].src_path;
            if( maps->items[i].src_prefix != NULL )
            {
                sprintf( src_path, "%s%s", maps->items[i].src_prefix,
                        maps->items[i].src_path );
                src_path_ptr = src_path;
            }

            if( smx_config_data_map_get_iter( src_payload_item, src_path_ptr,
                        &maps->items[i].src_iter ) )
            {
                maps->items[i].is_src_iter_set = true;
                src_child = &maps->items[i].src_iter;
                if( maps->items[i].type == BSON_TYPE_UNDEFINED )
                {
                    BSON_APPEND_VALUE( payload, iter_key,
                            bson_iter_value( src_child ) );
                }
                else if( maps->items[i].type == BSON_TYPE_UTF8 )
                {
                    if( BSON_ITER_HOLDS_OID( src_child ) )
                    {
                        oid = bson_iter_oid( src_child );
                        bson_oid_to_string( oid, oid_str );
                        BSON_APPEND_UTF8( payload, iter_key, oid_str );
                    }
                    else if( BSON_ITER_HOLDS_UTF8( src_child ) )
                    {
                        BSON_APPEND_UTF8( payload, iter_key,
                                bson_iter_utf8( src_child, NULL ) );
                    }
                }
                else if( maps->items[i].type == BSON_TYPE_OID )
                {
                    if( BSON_ITER_HOLDS_UTF8( src_child ) )
                    {
                        bson_oid_init_from_string( &oid_init,
                                bson_iter_utf8( src_child, NULL ) );
                        BSON_APPEND_OID( payload, iter_key, &oid_init );
                    }
                    else if( BSON_ITER_HOLDS_OID( src_child ) )
                    {
                        BSON_APPEND_OID( payload, iter_key,
                                bson_iter_oid( src_child ) );
                    }
                }
                else if( maps->items[i].type == BSON_TYPE_INT32 )
                {
                    if( BSON_ITER_HOLDS_INT32( src_child ) )
                    {
                        BSON_APPEND_INT32( payload, iter_key,
                                bson_iter_int32( src_child ) );
                    }
                    else if( BSON_ITER_HOLDS_INT64( src_child ) )
                    {
                        BSON_APPEND_INT64( payload, iter_key,
                                bson_iter_int64( src_child ) );
                    }
                    else if( BSON_ITER_HOLDS_DOUBLE( src_child ) )
                    {
                        BSON_APPEND_DOUBLE( payload, iter_key,
                                bson_iter_double( src_child ) );
                    }
                }
                else if( maps->items[i].type == BSON_TYPE_INT64 )
                {
                    if( BSON_ITER_HOLDS_INT32( src_child ) )
                    {
                        BSON_APPEND_INT32( payload, iter_key,
                                bson_iter_as_int64( src_child ) );
                    }
                    else if( BSON_ITER_HOLDS_INT64( src_child ) )
                    {
                        BSON_APPEND_INT64( payload, iter_key,
                                bson_iter_int64( src_child ) );
                    }
                    else if( BSON_ITER_HOLDS_DOUBLE( src_child ) )
                    {
                        BSON_APPEND_DOUBLE( payload, iter_key,
                                bson_iter_double( src_child ) );
                    }
                }
                else if( maps->items[i].type == BSON_TYPE_DOUBLE )
                {
                    if( smx_config_data_map_can_write_double( src_child ) )
                    {
                        BSON_APPEND_DOUBLE( payload, iter_key,
                                bson_iter_as_double( src_child ) );
                    }
                }
                else if( maps->items[i].type == BSON_TYPE_BOOL )
                {
                    if( smx_config_data_map_can_write_bool( src_child ) )
                    {
                        BSON_APPEND_BOOL( payload, iter_key,
                                bson_iter_as_bool( src_child ) );
                    }
                }
                else if( maps->items[i].type == BSON_TYPE_ARRAY )
                {
                    if( BSON_ITER_HOLDS_ARRAY( src_child ) )
                    {
                        bson_iter_array( src_child, &data_len, &data_doc );
                        bson_init_static( &doc, data_doc, data_len );
                        BSON_APPEND_ARRAY( payload, iter_key, &doc );
                        bson_destroy( &doc );
                    }
                }
                else if( maps->items[i].type == BSON_TYPE_DOCUMENT )
                {
                    if( BSON_ITER_HOLDS_DOCUMENT( src_child ) )
                    {
                        bson_iter_document( src_child, &data_len, &data_doc );
                        bson_init_static( &doc, data_doc, data_len );
                        BSON_APPEND_DOCUMENT( payload, iter_key, &doc );
                        bson_destroy( &doc );
                    }
                }
                return 0;
            }
            else
            {
                return SMX_CONFIG_MAP_ERROR_MISSING_SRC_KEY;
            }
        }
    }

    return SMX_CONFIG_MAP_ERROR_MISSING_TGT_KEY;
}

/******************************************************************************/
bool smx_config_data_map_can_write_bool( bson_iter_t* i_src )
{
    if( BSON_ITER_HOLDS_BOOL( i_src )
            || BSON_ITER_HOLDS_DOUBLE( i_src )
            || BSON_ITER_HOLDS_INT64( i_src )
            || BSON_ITER_HOLDS_UTF8( i_src )
            || BSON_ITER_HOLDS_INT32( i_src )
            || BSON_ITER_HOLDS_NULL( i_src )
            || BSON_ITER_HOLDS_UNDEFINED( i_src ) )
    {
        return true;
    }
    else
    {
        return false;
    }
}

/******************************************************************************/
bool smx_config_data_map_can_write_double( bson_iter_t* i_src )
{
    if( BSON_ITER_HOLDS_BOOL( i_src )
            || BSON_ITER_HOLDS_DOUBLE( i_src )
            || BSON_ITER_HOLDS_INT64( i_src )
            || BSON_ITER_HOLDS_INT32( i_src ) )
    {
        return true;
    }
    else
    {
        return false;
    }
}

/******************************************************************************/
bool smx_config_data_map_can_write_int64( bson_iter_t* i_src )
{
    if( BSON_ITER_HOLDS_BOOL( i_src )
            || BSON_ITER_HOLDS_DOUBLE( i_src )
            || BSON_ITER_HOLDS_INT64( i_src )
            || BSON_ITER_HOLDS_INT32( i_src ) )
    {
        return true;
    }
    else
    {
        return false;
    }
}

/******************************************************************************/
bool smx_config_data_map_can_write_int32( bson_iter_t* i_src )
{
    if( BSON_ITER_HOLDS_INT32( i_src ) )
    {
        return true;
    }
    else
    {
        return false;
    }
}

/******************************************************************************/
bool smx_config_data_map_get_iter( bson_t* data, const char* map,
        bson_iter_t* child )
{
    if( map == NULL )
    {
        return false;
    }
    bson_iter_t iter;
    return bson_iter_init( &iter, data )
            && bson_iter_find_descendant( &iter, map, child );
}

/******************************************************************************/
int smx_config_data_map_init( bson_t* payload,
        bson_iter_t* i_map, bool* is_extended, smx_config_data_map_t* map )
{
    int rc;
    const char* key;
    bson_iter_t i_tgt;
    map->is_src_iter_set = false;
    map->key = NULL;
    map->src_path = NULL;
    map->src_prefix = NULL;
    map->tgt_path = NULL;
    map->src_payload = NULL;
    map->type = BSON_TYPE_UNDEFINED;
    map->h = NULL;

    while( bson_iter_next( i_map ) )
    {
        key = bson_iter_key( i_map );
        if( strcmp( key, "tgt" ) == 0 )
        {
            if( BSON_ITER_HOLDS_UTF8( i_map ) )
            {
                rc = smx_config_data_map_init_tgt_utf8( payload, map,
                        bson_iter_utf8( i_map, NULL ), is_extended );
                if( rc < 0 )
                {
                    return rc;
                }
            }
            else if( BSON_ITER_HOLDS_DOCUMENT( i_map )
                    && bson_iter_recurse( i_map, &i_tgt ) )
            {
                rc = smx_config_data_map_init_tgt_doc( payload, map,
                        &i_tgt, is_extended );
                if( rc < 0 )
                {
                    return rc;
                }
            }
        }
        else if( ( strcmp( key, "src" ) == 0 )
                && BSON_ITER_HOLDS_UTF8( i_map ) )
        {
            map->src_path = bson_iter_utf8( i_map, NULL );
        }
        else if( ( strcmp( key, "key" ) == 0 )
                && BSON_ITER_HOLDS_UTF8( i_map ) )
        {
            map->key = bson_iter_utf8( i_map, NULL );
        }
    }

    if( map->src_path == NULL )
    {
        return SMX_CONFIG_MAP_ERROR_MISSING_SRC_DEF;
    }

    if( map->tgt_path == NULL )
    {
        return SMX_CONFIG_MAP_ERROR_MISSING_TGT_DEF;
    }

    return 0;
}

/******************************************************************************/
void smx_config_data_map_init_src_prefix( smx_config_data_map_t* map,
        const char* prefix )
{
    map->src_prefix = prefix;
}

/******************************************************************************/
int smx_config_data_map_init_tgt_doc( bson_t* payload,
        smx_config_data_map_t* map, bson_iter_t* i_tgt, bool* is_extended )
{
    int rc;
    const char* key;
    const char* type;

    while( bson_iter_next( i_tgt ) )
    {
        key = bson_iter_key( i_tgt );
        if( ( strcmp( key, "path" ) == 0 ) && BSON_ITER_HOLDS_UTF8( i_tgt ) )
        {
            rc = smx_config_data_map_init_tgt_utf8( payload, map,
                    bson_iter_utf8( i_tgt, NULL ), is_extended );
            if( rc < 0 )
            {
                return rc;
            }
        }
        else if( ( strcmp( key, "type" ) == 0 )
                && BSON_ITER_HOLDS_UTF8( i_tgt ) )
        {
            *is_extended = true;
            type = bson_iter_utf8( i_tgt, NULL );
            if( strcmp( type, "int32" ) == 0 )
            {
                map->type = BSON_TYPE_INT32;
            }
            else if( strcmp( type, "int64" ) == 0 )
            {
                map->type = BSON_TYPE_INT64;
            }
            else if( strcmp( type, "double" ) == 0 )
            {
                map->type = BSON_TYPE_DOUBLE;
            }
            else if( strcmp( type, "bool" ) == 0 )
            {
                map->type = BSON_TYPE_BOOL;
            }
            else if( strcmp( type, "utf8" ) == 0 )
            {
                map->type = BSON_TYPE_UTF8;
            }
            else if( strcmp( type, "oid" ) == 0 )
            {
                map->type = BSON_TYPE_OID;
            }
            else if( strcmp( type, "array" ) == 0 )
            {
                map->type = BSON_TYPE_ARRAY;
            }
            else if( strcmp( type, "object" ) == 0 )
            {
                map->type = BSON_TYPE_DOCUMENT;
            }
            else
            {
                return SMX_CONFIG_MAP_ERROR_BAD_TYPE_OPTION;
            }
        }
    }

    return 0;
}

/******************************************************************************/
int smx_config_data_map_init_tgt_utf8( bson_t* data,
        smx_config_data_map_t* map, const char* tgt_path, bool* is_extended )
{
    map->tgt_path = tgt_path;
    bson_type_t type;
    if( !smx_config_data_map_get_iter( data, map->tgt_path,
                &map->tgt_iter ) )
    {
        return SMX_CONFIG_MAP_ERROR_MISSING_TGT_KEY;
    }
    if( BSON_ITER_HOLDS_BOOL( &map->tgt_iter ) )
    {
        map->fallback.v_bool = bson_iter_bool( &map->tgt_iter );
        type = BSON_TYPE_BOOL;
    }
    else if( BSON_ITER_HOLDS_INT32( &map->tgt_iter ) )
    {
        map->fallback.v_int32 = bson_iter_int32( &map->tgt_iter );
        type = BSON_TYPE_INT32;
    }
    else if( BSON_ITER_HOLDS_INT64( &map->tgt_iter ) )
    {
        map->fallback.v_int64 = bson_iter_int64( &map->tgt_iter );
        type = BSON_TYPE_INT64;
    }
    else if( BSON_ITER_HOLDS_DOUBLE( &map->tgt_iter ) )
    {
        map->fallback.v_double = bson_iter_double(
                &map->tgt_iter );
        type = BSON_TYPE_DOUBLE;
    }
    else
    {
        *is_extended = true;
        if( BSON_ITER_HOLDS_ARRAY( &map->tgt_iter ) )
        {
            type = BSON_TYPE_ARRAY;
        }
        else if( BSON_ITER_HOLDS_DOCUMENT( &map->tgt_iter ) )
        {
            type = BSON_TYPE_DOCUMENT;
        }
        else if( BSON_ITER_HOLDS_UTF8( &map->tgt_iter ) )
        {
            type = BSON_TYPE_UTF8;
        }
        else if( BSON_ITER_HOLDS_OID( &map->tgt_iter ) )
        {
            type = BSON_TYPE_OID;
        }
    }

    if( map->type == BSON_TYPE_UNDEFINED )
    {
        map->type = type;
    }

    return 0;
}

/******************************************************************************/
const char* smx_config_data_map_strerror( int code )
{
    const char* undefined = "undefined";

    switch( code )
    {
        case SMX_CONFIG_MAP_ERROR_NO_ERROR:
            return "no error";
        case SMX_CONFIG_MAP_ERROR_BAD_ROOT_TYPE:
            return "target must be of type document when mapping the"
                " entire source payload";
        case SMX_CONFIG_MAP_ERROR_MISSING_SRC_KEY:
            return "missing key in source BSON structure";
        case SMX_CONFIG_MAP_ERROR_MISSING_SRC_DEF:
            return "missing source path definition in key map value";
        case SMX_CONFIG_MAP_ERROR_MISSING_TGT_KEY:
            return "missing key in target BSON structure";
        case SMX_CONFIG_MAP_ERROR_MISSING_TGT_DEF:
            return "missing target path definition in key map value";
        case SMX_CONFIG_MAP_ERROR_MAP_COUNT_EXCEEDED:
            return "number of allowed key mappings exceeded";
        case SMX_CONFIG_MAP_ERROR_BAD_MAP_TYPE:
            return "a key mapping in config item `map` must be an object";
        case SMX_CONFIG_MAP_ERROR_NO_MAP_ITEM:
            return "at least one valid mapping is required when defining a"
                " data map";
        case SMX_CONFIG_MAP_ERROR_BAD_TYPE_OPTION:
            return "undefined type option in key map value";
        default:
            return undefined;
    }

    return undefined;
}

/*****************************************************************************/
bool smx_config_get_bool( bson_t* conf, const char* search )
{
    smx_config_error_t err;
    return smx_config_get_bool_err( conf, search, &err );
}

/*****************************************************************************/
bool smx_config_get_bool_err( bson_t* conf, const char* search,
        smx_config_error_t* err )
{
    bson_iter_t iter;
    bson_iter_t child;
    *err = SMX_CONFIG_ERROR_NO_ERROR;
    if( bson_iter_init( &iter, conf ) && bson_iter_find_descendant( &iter,
                search, &child ) )
    {
        if( BSON_ITER_HOLDS_BOOL( &child ) )
            return bson_iter_bool( &child );
        else
            *err = SMX_CONFIG_ERROR_BAD_TYPE;
    }
    else
        *err = SMX_CONFIG_ERROR_NO_VALUE;
    return false;
}

/*****************************************************************************/
int smx_config_get_int( bson_t* conf, const char* search )
{
    smx_config_error_t err;
    return smx_config_get_int_err( conf, search, &err );
}

/*****************************************************************************/
int smx_config_get_int_err( bson_t* conf, const char* search,
        smx_config_error_t* err )
{
    bson_iter_t iter;
    bson_iter_t child;
    *err = SMX_CONFIG_ERROR_NO_ERROR;
    if( bson_iter_init( &iter, conf ) && bson_iter_find_descendant( &iter,
                search, &child ) )
    {
        if( BSON_ITER_HOLDS_INT32( &child ) )
            return bson_iter_int32( &child );
        else
            *err = SMX_CONFIG_ERROR_BAD_TYPE;
    }
    else
        *err = SMX_CONFIG_ERROR_NO_VALUE;
    return 0;
}

/*****************************************************************************/
double smx_config_get_double( bson_t* conf, const char* search )
{
    smx_config_error_t err;
    return smx_config_get_double_err( conf, search, &err );
}

/*****************************************************************************/
double smx_config_get_double_err( bson_t* conf, const char* search,
        smx_config_error_t* err )
{
    bson_iter_t iter;
    bson_iter_t child;
    *err = SMX_CONFIG_ERROR_NO_ERROR;
    if( bson_iter_init( &iter, conf ) && bson_iter_find_descendant( &iter,
                search, &child ) )
    {
        if( BSON_ITER_HOLDS_DOUBLE( &child )
                || BSON_ITER_HOLDS_BOOL( &child )
                || BSON_ITER_HOLDS_INT32( &child )
                || BSON_ITER_HOLDS_INT64( &child ) )
            return bson_iter_as_double( &child );
        else
            *err = SMX_CONFIG_ERROR_BAD_TYPE;
    }
    else
        *err = SMX_CONFIG_ERROR_NO_VALUE;
    return 0;
}

/*****************************************************************************/
const char* smx_config_get_string( bson_t* conf, const char* search,
        unsigned int* len )
{
    smx_config_error_t err;
    return smx_config_get_string_err( conf, search, len, &err );
}

/*****************************************************************************/
const char* smx_config_get_string_err( bson_t* conf, const char* search,
        unsigned int* len, smx_config_error_t* err )
{
    bson_iter_t iter;
    bson_iter_t child;
    *err = SMX_CONFIG_ERROR_NO_ERROR;
    if( bson_iter_init( &iter, conf ) && bson_iter_find_descendant( &iter,
                search, &child ) )
    {
        if( BSON_ITER_HOLDS_UTF8( &child ) )
            return bson_iter_utf8( &child, len );
        else
            *err = SMX_CONFIG_ERROR_BAD_TYPE;
    }
    else
        *err = SMX_CONFIG_ERROR_NO_VALUE;
    return NULL;
}

/*****************************************************************************/
int smx_config_init_bool( bson_t* conf, const char* search, bool* val )
{
    bool res;
    smx_config_error_t err;
    res = smx_config_get_bool_err( conf, search, &err );
    if( err != SMX_CONFIG_ERROR_NO_ERROR )
    {
        return err;
    }
    *val = res;
    return 0;
}

/*****************************************************************************/
int smx_config_init_double( bson_t* conf, const char* search, double* val )
{
    double res;
    smx_config_error_t err;
    res = smx_config_get_double_err( conf, search, &err );
    if( err != SMX_CONFIG_ERROR_NO_ERROR )
    {
        return err;
    }
    *val = res;
    return 0;
}

/*****************************************************************************/
int smx_config_init_int( bson_t* conf, const char* search, int* val )
{
    int res;
    smx_config_error_t err;
    res = smx_config_get_int_err( conf, search, &err );
    if( err != SMX_CONFIG_ERROR_NO_ERROR )
    {
        return err;
    }
    *val = res;
    return 0;
}

/*****************************************************************************/
const char* smx_config_strerror( smx_config_error_t err )
{
    const char* unknown = "unknown error";
    switch( err )
    {
        case SMX_CONFIG_ERROR_NO_ERROR:
            return "no error";
        case SMX_CONFIG_ERROR_BAD_TYPE:
            return "bad type";
        case SMX_CONFIG_ERROR_NO_VALUE:
            return "no value found";
        default:
            return unknown;
    }
    return unknown;
}
