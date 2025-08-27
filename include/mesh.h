#ifndef MESH_H
#define MESH_H

#include <stdbool.h>
#include <raymath.h>

typedef struct {
    int a, b, c, d;
} Quad;

typedef struct {
    Vector3 *v;
    int vCount, vCap;
    Quad *q;
    int qCount, qCap;
} QMesh;

typedef struct {
    int *idx;
    int count, cap;
} QRing;

#endif
