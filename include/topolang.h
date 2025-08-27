#ifndef TOPOLANG_H
#define TOPOLANG_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int line, col;
    char msg[256];
} TopoError;

typedef struct TopoArena TopoArena;

TopoArena *topo_arena_create(size_t bytes);

void topo_arena_reset(TopoArena *A);

void topo_arena_destroy(TopoArena *A);

typedef struct {
    float *vertices;   // xyz * vCount
    int vCount;
    int *quads;      // 4* qCount
    int qCount;
} TopoMesh;

typedef struct {
    TopoMesh *meshes;
    int count;
} TopoScene;

typedef struct TopoProgram TopoProgram;

typedef struct {
    const char *path;
    const char *code; //utf8
} TopoSource;

bool topo_compile(const TopoSource *sources, int nSources, TopoArena *A, TopoProgram **outProg, TopoError *err);

bool topo_execute(const TopoProgram *prog, const char *entryMeshName,
                  TopoArena *A, TopoScene *outScene, TopoError *err);

bool topo_export_gltf(const TopoScene *scene, const char *outGltfPath, TopoError *err);

void topo_free_scene(TopoScene *s);

void topo_free_mesh(TopoMesh *m);

#ifdef __cplusplus
}
#endif
#endif
