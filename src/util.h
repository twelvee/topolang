#ifndef UTIL_H
#define UTIL_H

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "intrinsics.h"

static inline void strsncpy(char *dst, const char *src, size_t cap) {
    if (cap == 0) return;
    size_t n = strlen(src);
    if (n >= cap) n = cap - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}


static int map_type(const char *t) {
    if (!t) return -1;
    if (!strcmp(t, "number")) return VAL_NUMBER;
    if (!strcmp(t, "string")) return VAL_STRING;
    if (!strcmp(t, "ring")) return VAL_RING;
    if (!strcmp(t, "ringlist")) return VAL_RINGLIST;
    if (!strcmp(t, "mesh")) return VAL_MESH;
    if (!strcmp(t, "void")) return VAL_VOID;
    return -1;
}

static int value_is_kind(Value v, int k) { return k < 0 ? 1 : (int) v.k == k; }

static const char *val_kind_str(int k) {
    switch (k) {
        case VAL_NUMBER:
            return "number";
        case VAL_STRING:
            return "string";
        case VAL_RING:
            return "ring";
        case VAL_RINGLIST:
            return "ringlist";
        case VAL_MESH:
            return "mesh";
        case VAL_VOID:
            return "void";
        default:
            return "unknown";
    }
}

static void sappend(char *dst, size_t cap, const char *fmt, ...) {
    size_t len = strlen(dst);
    if (len >= cap) return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(dst + len, cap - len, fmt, ap);
    va_end(ap);
}

static void value_to_string(Host *H, Value v, char out[256]) {
    out[0] = 0;
    if (v.k == VAL_STRING) {
        snprintf(out, 256, "%s", v.str.s ? v.str.s : "");
        return;
    }
    if (v.k == VAL_NUMBER) {
        snprintf(out, 256, "number(%g)", v.num);
        return;
    }
    if (v.k == VAL_VOID) {
        snprintf(out, 256, "void");
        return;
    }
    if (v.k == VAL_RING) {
        int c = v.ring ? v.ring->count : 0;
        float cx = 0, cy = 0, cz = 0, r = 0;
        if (H && H->build && v.ring && v.ring->idx && c > 0) {
            for (int i = 0; i < c; i++) {
                int id = v.ring->idx[i];
                Vector3 p = H->build->v[id];
                cx += p.x;
                cy += p.y;
                cz += p.z;
            }
            cx /= (float) c;
            cy /= (float) c;
            cz /= (float) c;
            for (int i = 0; i < c; i++) {
                int id = v.ring->idx[i];
                Vector3 p = H->build->v[id];
                float dx = p.x - cx, dy = p.y - cy, dz = p.z - cz;
                float d = sqrtf(dx * dx + dy * dy + dz * dz);
                r += d;
            }
            r /= (float) c;
            snprintf(out, 256, "ring(count=%d, center=%.3f,%.3f,%.3f, r≈%.3f)", c, cx, cy, cz, r);
        } else {
            snprintf(out, 256, "ring(count=%d)", c);
        }
        return;
    }
    if (v.k == VAL_RINGLIST) {
        int n = v.ringlist.count;
        snprintf(out, 256, "ringlist(count=%d", n);
        if (n > 0) {
            sappend(out, 256, ", rings=[");
            int lim = n < 8 ? n : 8;
            for (int i = 0; i < lim; i++) {
                int c = v.ringlist.ptrs[i] ? v.ringlist.ptrs[i]->count : 0;
                if (i) sappend(out, 256, ",");
                sappend(out, 256, "%d", c);
            }
            if (n > lim) sappend(out, 256, ",+%d", n - lim);
            sappend(out, 256, "]");
        }
        sappend(out, 256, ")");
        return;
    }
    if (v.k == VAL_MESH) {
        int vc = v.mesh ? v.mesh->vCount : 0;
        int qc = v.mesh ? v.mesh->qCount : 0;
        if (v.mesh && vc > 0) {
            Vector3 mn = v.mesh->v[0], mx = v.mesh->v[0];
            for (int i = 1; i < vc; i++) {
                Vector3 p = v.mesh->v[i];
                if (p.x < mn.x) mn.x = p.x;
                if (p.y < mn.y) mn.y = p.y;
                if (p.z < mn.z) mn.z = p.z;
                if (p.x > mx.x) mx.x = p.x;
                if (p.y > mx.y) mx.y = p.y;
                if (p.z > mx.z) mx.z = p.z;
            }
            snprintf(out, 256, "mesh(v=%d,q=%d,bbox=[%.3f,%.3f,%.3f]-[%.3f,%.3f,%.3f])",
                     vc, qc, mn.x, mn.y, mn.z, mx.x, mx.y, mx.z);
        } else {
            snprintf(out, 256, "mesh(v=%d,q=%d)", vc, qc);
        }
        return;
    }
    snprintf(out, 256, "%s", val_kind_str(v.k));
}

#endif
