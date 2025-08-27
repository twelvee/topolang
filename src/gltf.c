#include "topolang.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    float *v;
    int vCount;
    unsigned int *idx;
    int idxCount;
} TriMesh;

static TriMesh tri_from_quad(const TopoMesh *qm) {
    TriMesh t = {0};
    t.vCount = qm->vCount;
    t.v = (float *) malloc(sizeof(float) * 3 * t.vCount);
    memcpy(t.v, qm->vertices, sizeof(float) * 3 * t.vCount);
    t.idxCount = qm->qCount * 6;
    t.idx = (unsigned int *) malloc(sizeof(unsigned int) * t.idxCount);
    for (int i = 0; i < qm->qCount; i++) {
        int a = qm->quads[i * 4 + 0], b = qm->quads[i * 4 + 1], c = qm->quads[i * 4 + 2], d = qm->quads[i * 4 + 3];
        t.idx[i * 6 + 0] = a;
        t.idx[i * 6 + 1] = b;
        t.idx[i * 6 + 2] = c;
        t.idx[i * 6 + 3] = a;
        t.idx[i * 6 + 4] = c;
        t.idx[i * 6 + 5] = d;
    }
    return t;
}

bool topo_export_gltf(const TopoScene *scene, const char *outGltfPath, TopoError *err) {
    TriMesh all = {0};
    int totalV = 0, totalI = 0;
    for (int i = 0; i < scene->count; i++) {
        totalV += scene->meshes[i].vCount;
        totalI += scene->meshes[i].qCount * 6;
    }
    all.v = (float *) malloc(sizeof(float) * 3 * totalV);
    all.idx = (unsigned int *) malloc(sizeof(unsigned int) * totalI);
    int vo = 0, io = 0, base = 0;
    for (int i = 0; i < scene->count; i++) {
        TriMesh t = tri_from_quad(&scene->meshes[i]);
        memcpy(all.v + vo * 3, t.v, sizeof(float) * 3 * t.vCount);
        for (int k = 0; k < t.idxCount; k++) all.idx[io + k] = t.idx[k] + base;
        vo += t.vCount;
        io += t.idxCount;
        base += t.vCount;
        free(t.v);
        free(t.idx);
    }

    char binPath[512];
    snprintf(binPath, sizeof(binPath), "%s.bin", outGltfPath);
    FILE *fb = fopen(binPath, "wb");
    if (!fb) {
        if (err) {
            err->line = 0;
            err->col = 0;
            strcpy(err->msg, "can't write bin");
        }
        return false;
    }
    size_t byteV = sizeof(float) * 3 * totalV;
    size_t byteI = sizeof(unsigned int) * totalI;
    fwrite(all.v, 1, byteV, fb);
    fwrite(all.idx, 1, byteI, fb);
    fclose(fb);

    FILE *fg = fopen(outGltfPath, "wb");
    if (!fg) {
        if (err) { strcpy(err->msg, "can't write gltf"); }
        return false;
    }
    fprintf(fg,
            "{\n"
            "  \"asset\": {\"version\": \"2.0\"},\n"
            "  \"buffers\": [ {\"uri\": \"%s.bin\", \"byteLength\": %zu} ],\n"
            "  \"bufferViews\": [\n"
            "    {\"buffer\":0, \"byteOffset\":0, \"byteLength\": %zu, \"target\":34962},\n"
            "    {\"buffer\":0, \"byteOffset\":%zu, \"byteLength\": %zu, \"target\":34963}\n"
            "  ],\n"
            "  \"accessors\": [\n"
            "    {\"bufferView\":0, \"componentType\":5126, \"count\": %d, \"type\":\"VEC3\"},\n"
            "    {\"bufferView\":1, \"componentType\":5125, \"count\": %d, \"type\":\"SCALAR\"}\n"
            "  ],\n"
            "  \"meshes\": [ {\"primitives\": [ {\"attributes\": {\"POSITION\":0}, \"indices\":1} ]} ],\n"
            "  \"nodes\": [ {\"mesh\":0} ],\n"
            "  \"scenes\": [ {\"nodes\": [0]} ],\n"
            "  \"scene\": 0\n"
            "}\n",
            outGltfPath, (size_t) (byteV + byteI),
            byteV, byteV, byteI,
            totalV, totalI
    );
    fclose(fg);
    free(all.v);
    free(all.idx);
    return true;
}
