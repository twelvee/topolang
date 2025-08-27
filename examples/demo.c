#include "topolang.h"
#include <stdio.h>
#include <stdlib.h>

static char *load_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = (char *) malloc(sz + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    size_t rd = fread(buf, 1, sz, f);
    buf[rd] = '\0';
    fclose(f);
    return buf;
}

int main(void) {
    const char *filename = "box.tl";

    char *code = load_file(filename);
    if (!code) {
        fprintf(stderr, "Cannot open %s\n", filename);
        return 1;
    }

    TopoArena *A = topo_arena_create(16 * 1024 * 1024);
    TopoProgram *prog = NULL;
    TopoError err = {0};
    TopoSource src = {.path = filename, .code = code};

    if (!topo_compile(&src, 1, A, &prog, &err)) {
        fprintf(stderr, "Compile %d:%d %s\n", err.line, err.col, err.msg);
        free(code);
        topo_arena_destroy(A);
        return 1;
    }

    TopoScene scene = {0};
    if (!topo_execute(prog, "Cube", A, &scene, &err)) {
        fprintf(stderr, "Execute: %s\n", err.msg);
        free(code);
        topo_arena_destroy(A);
        return 1;
    }

    if (!topo_export_gltf(&scene, "cube.gltf", &err)) {
        fprintf(stderr, "GLTF: %s\n", err.msg);
        free(code);
        topo_free_scene(&scene);
        topo_arena_destroy(A);
        return 1;
    }

    printf("OK: cube.gltf (+ .bin)\n");
    topo_free_scene(&scene);
    topo_arena_destroy(A);
    free(code);
    return 0;
}
