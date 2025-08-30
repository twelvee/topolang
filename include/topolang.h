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
    float *vertices;
    int vCount;
    int *quads;
    int qCount;
} TopoMesh;

typedef struct {
    TopoMesh *meshes;
    int count;
} TopoScene;

typedef struct TopoProgram TopoProgram;

typedef struct {
    const char *path;
    const char *code;
} TopoSource;

typedef struct {
    const char **include_dirs;
    int include_dir_count;

    bool (*read_file)(const char *requested_path,
                      const char *from_path,
                      const char **out_buf,
                      const char **out_name,
                      void *user);

    void *user;
} TopoOptions;

bool topo_compile(const TopoSource *sources, int nSources, TopoArena *A, TopoProgram **outProg, TopoError *err);

bool
topo_compile_ex(const TopoSource *sources, int nSources, const TopoOptions *opt, TopoArena *A, TopoProgram **outProg,
                TopoError *err);

bool topo_execute(const TopoProgram *prog, const char *entryMeshName,
                  TopoArena *A, TopoScene *outScene, TopoError *err);

bool topo_export_gltf(const TopoScene *scene, const char *outGltfPath, TopoError *err);

bool topo_export_obj_ex(const TopoScene *scene, const char *outObjPath, int triangulate, TopoError *err);

bool topo_export_obj(const TopoScene *scene, const char *outObjPath, TopoError *err);

void topo_free_scene(TopoScene *s);

void topo_free_mesh(TopoMesh *m);

#ifdef __cplusplus
}
#endif
#endif