/* -*- mode:c; c-file-style:"k&r"; c-basic-offset: 4; tab-width:4; indent-tabs-mode:nil; mode:auto-fill; fill-column:78; -*- */
/* vim: set ts=4 sw=4 et tw=78 fo=cqt wm=0: */

/* Copyright (C) 2014 OSCAR lab, Stony Brook University
   This file is part of Graphene Library OS.

   Graphene Library OS is free software: you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   Graphene Library OS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/*
 * config.c
 *
 * This file contains functions to read app config (manifest) file and create
 * a tree to lookup / access config values.
 */

#include <linux_list.h>
#include <api.h>
#include <asm-errno.h>

struct config {
    const char * key, * val;
    int klen, vlen;
    char * buf;
    struct list_head list;
    struct list_head children, siblings;
};

static int __add_config (struct config_store * store,
                         const char * key, int klen,
                         const char * val, int vlen,
                         struct config ** entry)
{
    struct list_head * list = &store->root;
    struct config * e = NULL;

    while (klen) {
        if (e && e->val)
            return -EINVAL;

        const char * token = key;
        int len = 0;
        for ( ; len < klen ; len++)
            if (token[len] == '.')
                break;

        list_for_each_entry(e, list, siblings)
            if (e->klen == len && !memcmp(e->key, token, len))
                goto next;

        e = store->malloc(sizeof(struct config));
        if (!e)
            return -ENOMEM;

        e->key  = token;
        e->klen = len;
        e->val  = NULL;
        e->vlen = 0;
        e->buf  = NULL;
        INIT_LIST_HEAD(&e->list);
        list_add_tail(&e->list, &store->entries);
        INIT_LIST_HEAD(&e->children);
        INIT_LIST_HEAD(&e->siblings);
        list_add_tail(&e->siblings, list);

next:
        if (len < klen)
            len++;
        key += len;
        klen -= len;
        list = &e->children;
    }

    if (!e || e->val || !list_empty(&e->children))
        return -EINVAL;

    e->val  = val;
    e->vlen = vlen;

    if (entry)
        *entry = e;

    return 0;
}

static struct config * __get_config (struct config_store * store,
                                     const char * key)
{
    struct list_head * list = &store->root;
    struct config * e = NULL;

    while (*key) {
        const char * token = key;
        int len = 0;
        for ( ; token[len] ; len++)
            if (token[len] == '.')
                break;

        list_for_each_entry(e, list, siblings)
            if (e->klen == len && !memcmp(e->key, token, len))
                goto next;

        return NULL;

next:
        if (token[len])
            len++;
        key += len;
        list = &e->children;
    }

    return e;
}

int get_config (struct config_store * store, const char * key,
                char * val_buf, size_t size)
{
    struct config * e = __get_config(store, key);

    if (!e || !e->val)
        return -EINVAL;

    if (e->vlen >= size)
        return -ENAMETOOLONG;

    memcpy(val_buf, e->val, e->vlen);
    val_buf[e->vlen] = 0;
    return e->vlen;
}

int get_config_entries (struct config_store * store, const char * key,
                        char * key_buf, size_t size)
{
    struct config * e = __get_config(store, key);

    if (!e || e->val)
        return -EINVAL;

    struct list_head * children = &e->children;
    int nentries = 0;

    list_for_each_entry(e, children, siblings) {
        if (e->klen >= size)
            return -ENAMETOOLONG;

        memcpy(key_buf, e->key, e->klen);
        key_buf[e->klen] = 0;
        key_buf += e->klen + 1;
        size -= e->klen + 1;
        nentries++;
    }

    return nentries;
}

static int __del_config (struct config_store * store,
                         struct list_head * root, const char * key)
{
    struct config * e, * found = NULL;
    int len = 0;
    for ( ; key[len] ; len++)
        if (key[len] == '.')
            break;

    list_for_each_entry(e, root, siblings)
        if (e->klen == len && !memcmp(e->key, key, len)) {
            found = e;
            break;
        }

    if (!found)
        return -ENOENT;

    if (key[len]) {
        if (found->val)
            return -ENOTDIR;
        int ret = __del_config(store, &found->children, key + len + 1);
        if (ret < 0)
            return ret;
        if (!list_empty(&found->children))
            return 0;
    } else {
        if (!found->val)
            return -EISDIR;
    }

    list_del(&found->siblings);
    list_del(&found->list);
    if (found->buf)
        store->free(found->buf);
    store->free(found);

    return 0;
}

int set_config (struct config_store * store, const char * key, const char * val)
{
    if (!key)
        return -EINVAL;

    if (!val) { /* deletion */
        return __del_config(store, &store->root, key);
    }

    int klen = strlen(key);
    int vlen = strlen(val);

    char * buf = store->malloc(klen + vlen + 2);
    if (!buf)
        return -ENOMEM;

    memcpy(buf, key, klen + 1);
    memcpy(buf + klen + 1, val, vlen + 1);

    struct config * e = __get_config(store, key);
    if (e) {
        e->val  = buf + klen + 1;
        e->vlen = vlen;
        e->buf  = buf;
    } else {
        int ret = __add_config(store, buf, klen, buf + klen + 1, vlen, &e);
        if (ret < 0) {
            store->free(buf);
            return ret;
        }
        e->buf  = buf;
    }

    return 0;
}

int read_config (struct config_store * store,
                 int (*filter) (const char * key, int ken),
                 const char ** errstring)
{
    INIT_LIST_HEAD(&store->root);
    INIT_LIST_HEAD(&store->entries);

    char * ptr = store->raw_data;
    char * ptr_end = store->raw_data + store->raw_size;

    const char * err = "unknown error";

#define IS_SPACE(c) ((c) == ' '  || (c) == '\t')
#define IS_BREAK(c) ((c) == '\r' || (c) == '\n')
#define IS_VALID(c)                                                    \
    (((c) >= 'A' && (c) <= 'Z') || ((c) >= 'a' && (c) <= 'z') ||       \
     ((c) >= '0' && (c) <= '9') || (c) == '_')

    register int skipping = 0;

#define IS_SKIP(p)                                                     \
    (skipping ? ({ if (IS_BREAK(*(p))) skipping = 0; 1; })             \
     : ((*(p)) == '#' ? ({ skipping = 1; 1; }) : IS_BREAK(*(p))))

#define RANGE (ptr < ptr_end)
#define GOTO_INVAL(msg) ({ err = msg; goto inval; })
#define CHECK_PTR(msg) if (!RANGE) GOTO_INVAL(msg)

    while (RANGE) {
        for ( ; RANGE && (IS_SKIP(ptr) || IS_SPACE(*ptr)) ; ptr++);

        if (!(RANGE))
            break;

        if (!IS_VALID(*ptr))
            GOTO_INVAL("invalid start of key");

        char * key = ptr;
        for ( ; RANGE ; ptr++) {
            char * pptr = ptr;

            for ( ; RANGE && IS_VALID(*ptr) ; ptr++);
            CHECK_PTR("stream ended at key");

            if (pptr == ptr)
                GOTO_INVAL("key token with zero length");

            if (*ptr != '.')
                break;
        }

        int klen = ptr - key;

        for ( ; RANGE && IS_SPACE(*ptr) ; ptr++)
        CHECK_PTR("stream ended at key");

        if (*ptr != '=')
            GOTO_INVAL("equal mark expected");

        ptr++;

        for ( ; RANGE && IS_SPACE(*ptr) ; ptr++);
        CHECK_PTR("stream ended at equal mark");

        char * val = NULL;
        int vlen;
        if (*ptr == '"') {
            int shift = 0;
            val = (++ptr);
            for ( ; RANGE && *ptr != '"' ; ptr++) {
                if (*ptr == '\\') {
                    shift++;
                    ptr++;
                }
                if (shift)
                    *(ptr - shift) = *ptr;
            }
            CHECK_PTR("stream ended without closing quote");
            vlen = (ptr - shift) - val;
        } else {
            val = ptr;
            for ( ; RANGE && !IS_SKIP(ptr) ; ptr++);
            vlen = ptr - val;
        }
        ptr++;

        if (!filter || !filter(key, klen)) {
            int ret = __add_config(store, key, klen, val, vlen, NULL);
            if (ret < 0) {
                if (ret == -ENAMETOOLONG)
                    GOTO_INVAL("key too long");
                if (ret == -EINVAL)
                    GOTO_INVAL("key format invalid");

                GOTO_INVAL("unknown error");
            }
        }
    }

    return 0;

inval:
    if (errstring)
        *errstring = err;

    return -EINVAL;
}

int free_config (struct config_store * store)
{
    struct config * e, * n;
    list_for_each_entry_safe(e, n, &store->entries, list) {
        if (e->buf)
            store->free(e->buf);
        store->free(e);
    }

    INIT_LIST_HEAD(&store->root);
    INIT_LIST_HEAD(&store->entries);
    return 0;
}

static int __dup_config (const struct config_store * ss,
                         const struct list_head * sr,
                         struct config_store * ts,
                         struct list_head * tr,
                         void ** data, size_t * size)
{
    struct config * e, * new;

    list_for_each_entry(e, sr, siblings) {
        char * key = NULL, * val = NULL, * buf = NULL;
        int need = 0;

        if (e->key) {
            if (*size > e->klen) {
                key = *data;
                *data += e->klen;
                *size -= e->klen;
                memcpy(key, e->key, e->klen);
            } else
                need += e->klen;
        }
        if (e->val) {
            if (*size > e->vlen) {
                val = *data;
                *data += e->vlen;
                *size -= e->vlen;
                memcpy(val, e->val, e->vlen);
            } else
                need += e->vlen;
        }

        if (need) {
            buf = ts->malloc(need);
            if (!buf)
                return -ENOMEM;
        }

        if (e->key && !key) {
            key = buf;
            memcpy(key, e->key, e->klen);
        }

        if (e->val && !val) {
            val = buf + (key == buf ? e->klen : 0);
            memcpy(val, e->val, e->vlen);
        }

        new = ts->malloc(sizeof(struct config));
        if (!new)
            return -ENOMEM;

        new->key  = key;
        new->klen = e->klen;
        new->val  = val;
        new->vlen = e->vlen;
        new->buf  = buf;
        INIT_LIST_HEAD(&new->list);
        list_add_tail(&new->list, &ts->entries);
        INIT_LIST_HEAD(&new->children);
        INIT_LIST_HEAD(&new->siblings);
        list_add_tail(&new->siblings, tr);

        if (!list_empty(&e->children)) {
            int ret = __dup_config(ss, &e->children,
                                   ts, &new->children,
                                   data, size);
            if (ret < 0)
                return ret;
        }
    }

    return 0;
}

int copy_config (struct config_store * store, struct config_store * new_store)
{
    INIT_LIST_HEAD(&new_store->root);
    INIT_LIST_HEAD(&new_store->entries);

    struct config * e;
    int size = 0;

    list_for_each_entry(e, &store->entries, list) {
        if (e->key)
            size += e->klen;
        if (e->val)
            size += e->vlen;
    }

    void * data = new_store->malloc(size);

    if (!data)
        return -ENOMEM;

    void * dataptr = data;
    size_t datasz = size;

    new_store->raw_data = data;
    new_store->raw_size = size;

    return __dup_config(store, &store->root, new_store, &new_store->root,
                        &dataptr, &datasz);
}

static int __write_config (void * f, size_t (*write) (void *, void *, size_t),
                           struct config_store * store,
                           struct list_head * root,
                           char * keybuf, size_t klen,
                           unsigned long * offset)
{
    struct config * e;
    int ret;
    char * buf = NULL;
    int bufsz = 0;

    list_for_each_entry(e, root, siblings)
        if (e->val) {
            int total = klen + e->klen + e->vlen + 2;

            while (total > bufsz) {
                bufsz += CONFIG_MAX;
                buf = __alloca(CONFIG_MAX);
            }

            memcpy(buf, keybuf, klen);
            memcpy(buf + klen, e->key, e->klen);
            buf[klen + e->klen] = '=';
            memcpy(buf + total - e->vlen - 1, e->val, e->vlen);
            buf[total - 1] = '\n';

            ret = write(f, buf, total);
            if (ret < 0)
                return ret;

            *offset += total;
        } else {
            if (klen + e->klen + 1 > CONFIG_MAX)
                return -ENAMETOOLONG;

            memcpy(keybuf + klen, e->key, e->klen);
            keybuf[klen + e->klen] = '.';

            if ((ret = __write_config(f, write, store, &e->children, keybuf,
                                      klen + e->klen + 1, offset)) < 0)
                return ret;
        }

    return 0;
}

int write_config (void * f, size_t (*write) (void *, void *, size_t),
                  struct config_store * store)
{
    char buf[CONFIG_MAX];
    unsigned long offset = 0;

    return __write_config(f, write, store, &store->root, buf, 0, &offset);
}
