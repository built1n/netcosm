/*
 *   NetCosm - a MUD server
 *   Copyright (C) 2016 Franklin Wei
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "globals.h"

#include "server.h"
#include "server_reqs.h"
#include "hash.h"
#include "multimap.h"
#include "userdb.h"
#include "world.h"
#include "world_api.h"

static const struct world_api api = {
    obj_new,
    obj_dup,
    obj_copy,
    obj_free,
    room_user_teleport,
    room_obj_add,
    room_obj_add_alias,
    room_obj_del,
    room_obj_del_by_ptr,
    room_obj_get,
    room_obj_get_size,
    room_obj_count,
    room_obj_count_noalias,
    room_verb_add,
    room_verb_del,
    room_verb_map,
    room_get,
    room_get_id,
    world_verb_add,
    world_verb_del,
    world_verb_map,
    verb_new,
    verb_free,
    hash_djb,
    compare_strings,
    compare_strings_nocase,
    hash_init,
    hash_setfreedata_cb,
    hash_setfreekey_cb,
    hash_free,
    hash_insert,
    hash_overwrite,
    hash_lookup,
    hash_remove,
    hash_iterate,
    hash_insert_pairs,
    hash_getkeyptr,
    hash_dup,
    hash_setdupdata_cb,
    multimap_init,
    multimap_free,
    multimap_lookup,
    multimap_insert,
    multimap_delete,
    multimap_delete_all,
    multimap_iterate,
    multimap_size,
    multimap_setfreedata_cb,
    multimap_dup,
    multimap_setdupdata_cb,
    multimap_copy,
    send_msg,
    child_toggle_rawmode,
    userdb_lookup,
    userdb_remove,
    userdb_size,
    userdb_add,
    userdb_iterate,
    userdb_add_obj,
    userdb_del_obj,
    userdb_del_obj_by_ptr,
    error,
    all_upper,
    all_lower,
    write_string,
    read_string,
    write_roomid,
    read_roomid,
    write_bool,
    read_bool,
    write_uint32,
    read_uint32,
    write_uint64,
    read_uint64,
    write_size,
    read_size,
    write_int,
    read_int,
    is_vowel,
    strlcat,
    format_noun
};

const struct world_api *nc = &api;
