# TopoLang

A tiny domain-specific language (DSL) and runtime for **procedural mesh generation** in C. It includes a hand-written lexer, recursive-descent parser, AST, small interpreter with built-in geometry intrinsics, and a minimal C API to compile and execute TopoLang sources and retrieve triangle/quad meshes.

> Status: library (no CLI yet). Designed to be embedded in tools and pipelines.

---

## Table of Contents

* [Features](#features)
* [Quick Example](#quick-example)
* [Language Overview](#language-overview)

    * [Tokens](#tokens)
    * [Expressions](#expressions)
    * [Statements](#statements)
    * [Blocks](#blocks)
    * [`mesh` declarations](#mesh-declarations)
    * [`create` sections](#create-sections)
    * [`part` and `override`](#part-and-override)
    * [`import`](#import)
    * [Annotations `: mesh, error`](#annotations--mesh-error)
* [Built-in Intrinsics](#built-in-intrinsics)
* [C API](#c-api)

    * [Arena lifecycle](#arena-lifecycle)
    * [Compilation](#compilation)
    * [Execution](#execution)
    * [Freeing results](#freeing-results)
* [Error Reporting](#error-reporting)
* [Implementation Notes](#implementation-notes)
* [Limitations & Roadmap](#limitations--roadmap)
* [Raylib](#raylib)

---

## Features

* Hand-written **lexer** with `//` line comments, strings, numbers, identifiers, and punctuation.
* **Recursive-descent parser** producing a compact AST; tolerates newlines around operators and ignores `create` annotations like `: mesh, error`.
* Minimal **interpreter** evaluating blocks and expressions, with:

    * variables and assignment,
    * numeric arithmetics (unary minus and `+`),
    * function calls to geometry intrinsics,
    * array literals (used for ring lists),
    * `return` with one or more expressions (first is used).
* Geometry **intrinsics** for building meshes: `vertex`, `quad`, `mesh`, `merge`, `mirror_x`, `move`, `scale`, and “ring” utilities (`ring`, `grow_out`, `lift_z`, `ringlist`, `stitch`).
* **Arena allocator** for all transient allocations.
* C **embed API** to compile multi-source programs and execute a named mesh to a concrete scene buffer.

---

## Quick Example

TopoLang (DSL):

```topolang
mesh Cube {
  create() : mesh, error {
    v0 = vertex(-0.5, -0.5, -0.5);
    v1 = vertex( 0.5, -0.5, -0.5);
    v2 = vertex( 0.5,  0.5, -0.5);
    v3 = vertex(-0.5,  0.5, -0.5);

    v4 = vertex(-0.5, -0.5,  0.5);
    v5 = vertex( 0.5, -0.5,  0.5);
    v6 = vertex( 0.5,  0.5,  0.5);
    v7 = vertex(-0.5,  0.5,  0.5);

    return
      quad(v0, v1, v2, v3) +
      quad(v4, v5, v6, v7) +
      quad(v0, v1, v5, v4) +
      quad(v2, v3, v7, v6) +
      quad(v1, v2, v6, v5) +
      quad(v0, v3, v7, v4),
      nil;
  }
}
```

C embedding:

```c
#include "topolang.h"
#include "arena.h"
#include <stdio.h>

int main(void) {
    TopoArena *A = topo_arena_create(1<<20);
    const char *code = /* Cube script from above */ "...";

    TopoSource src = { .code = code, .name = "cube.tl" };
    TopoProgram *prog = NULL;
    TopoError err = {0};

    if (!topo_compile(&src, 1, A, &prog, &err)) {
        fprintf(stderr, "%s %d:%d\n", err.msg, err.line, err.col);
        return 1;
    }

    TopoScene scene = {0};
    if (!topo_execute(prog, "Cube", A, &scene, &err)) {
        fprintf(stderr, "%s\n", err.msg);
        return 2;
    }

    printf("Mesh: %d verts, %d quads\n", scene.meshes[0].vCount, scene.meshes[0].qCount);
    topo_free_scene(&scene);
    topo_arena_destroy(A);
    return 0;
}
```

---

## Language Overview

### Tokens

* Numbers: decimal (`123`, `3.14`, `.5`)
* Strings: double-quoted, no escapes (`"file.tl"`)
* Identifiers: `[_A-Za-z][_A-Za-z0-9]*`
* Punct: `(){}[],:;.=+-`
* Newlines are tokenized as `TK_NEWLINE` and often skipped.

Line comments: `// ...` to end of line.

### Expressions

* Literals: numbers, strings
* Identifiers and function calls: `foo`, `foo(a, b)`
* Arrays: `[a, b, c]` (used with `ringlist`)
* Unary: `-expr`
* Binary: `lhs + rhs`

    * `+` is overloaded:

        * number + number → number
        * mesh + mesh → merged mesh
        * anything else → right operand (best-effort passthrough)
* Assignment: `name = expr`
* Precedence (high→low): unary `-`, then `+`, assignment is right-recursive via `parse_expr`.

### Statements

* Expression statement: `expr;`
* Return: `return expr[, expr2, ...];`

    * Only the **first** expression is used by the runtime.

Semicolons are **required**.

### Blocks

* Curly braces `{ ... }`, containing statements.
* Newlines are allowed freely around operators and statements.

### `mesh` declarations

```
mesh Name [: Parent] {
  items...
}
```

* `Name` registers a mesh node.
* Parent name (optional) is parsed but not currently used at runtime.
* Body may contain `create`, `part`, `override part`, `import` items.

### `create` sections

```
create(param0 = default, param1 = default, ...) : annotations {
  ... statements ...
  return mesh_expr [, ...] ;
}
```

* Parameter **names and default literals** are parsed; defaults are not yet evaluated at runtime.
* Annotations after `:` (e.g. `mesh, error`) are parsed and **ignored**.
* The block must `return` a **mesh**.

### `part` and `override`

```
part PartName(param = default, ...) [: annotations] { ... }
override PartName(...) [: annotations] { ... }
```

* Parsed into AST and stored under `mesh` items.
* Not executed by the current runtime (reserved for higher-level composition).

### `import`

```
import "path/to/file.tl";
```

* Captured in AST under a mesh item.
* Not auto-loaded by the runtime (hook point for host application).

### Annotations `: mesh, error`

* Allowed after `create(...)` and `part ... (...)`.
* Comma-separated identifiers, newlines allowed.
* They are **ignored** by the interpreter; the parser simply skips them until `{`.

---

## Built-in Intrinsics

All intrinsics are plain functions in the DSL.

| Name       | Signature                                                      | Description                                                                     |
| ---------- | -------------------------------------------------------------- | ------------------------------------------------------------------------------- |
| `vertex`   | `vertex(x,y,z) -> number`                                      | Appends a vertex to the current builder mesh; returns its index.                |
| `quad`     | `quad(a,b,c,d) -> mesh`                                        | Creates a mesh containing one quad by copying vertices `a..d` from the builder. |
| `mesh`     | `mesh([m0, m1, ...]) -> mesh`                                  | Merges any mesh arguments into one; no args returns an empty mesh.              |
| `merge`    | `merge(mesh, ...) -> mesh`                                     | Explicit merge of meshes.                                                       |
| `mirror_x` | `mirror_x(mesh, weld=1e-6) -> mesh`                            | Mirrors across X and welds near-origin.                                         |
| `move`     | `move(mesh, dx,dy,dz) -> mesh`                                 | Translates the mesh.                                                            |
| `scale`    | `scale(mesh, sx,sy,sz) -> mesh`                                | Scales the mesh.                                                                |
| `ring`     | `ring(cx,cy,rx,ry,segments) -> ring`                           | Builds an ellipse ring.                                                         |
| `grow_out` | `grow_out(ring, step, dz) -> ring`                             | Grows a ring outward and elevates by `dz`.                                      |
| `lift_z`   | `lift_z(ring, dz) -> ring`                                     | Lifts a ring along Z.                                                           |
| `ringlist` | `ringlist(r0, r1, ...) -> ringlist`                            | Packs rings into a ring list.                                                   |
| `stitch`   | `stitch(ringA, ringB) -> mesh` or `stitch([rings...]) -> mesh` | Stitches adjacent rings into quads.                                             |

Notes:

* There is an implicit **builder mesh** per execution. `vertex` writes into it; `quad` copies those vertices into a new mesh output.
* Arrays (e.g. `[r0, r1, r2]`) are evaluated and internally forwarded to the `ringlist` intrinsic.

---

## C API

Public entry points (from `topolang.h` and `topolang.c`):

### Arena lifecycle

```c
TopoArena *topo_arena_create(size_t bytes);
void topo_arena_reset(TopoArena *A);
void topo_arena_destroy(TopoArena *A);
```

### Compilation

```c
typedef struct {
    const char *name;  // optional, used only by host
    const char *code;  // TopoLang source buffer
} TopoSource;

typedef struct {
    char msg[256];
    int line, col;
} TopoError;

typedef struct TopoProgram TopoProgram;

bool topo_compile(const TopoSource *sources, int nSources,
                  TopoArena *A, TopoProgram **outProg, TopoError *err);
```

* Compiles one or more sources into a `TopoProgram`.
* On parse error, fills `err->msg`, `line`, `col`.

### Execution

```c
typedef struct {
    int vCount;
    float *vertices; // xyz interleaved
    int qCount;
    int *quads;      // a,b,c,d indices
} TopoMesh;

typedef struct {
    int count;
    TopoMesh *meshes; // currently always count==1
} TopoScene;

bool topo_execute(const TopoProgram *prog, const char *entryMeshName,
                  TopoArena *A, TopoScene *outScene, TopoError *err);
```

* Looks up a `mesh <Name> { create { ... } }` by `entryMeshName`.
* Executes its `create` block and expects it to `return` a mesh.
* Converts the internal mesh into a CPU `TopoScene`.

### Freeing results

```c
void topo_free_mesh(TopoMesh *m);
void topo_free_scene(TopoScene *s);
```

* Frees heap memory inside the result structures.

---

## Error Reporting

* Lexer and parser produce messages like `expected }` with 1-based `line:col`.
* `topo_compile` propagates parse errors in `TopoError`.
* `topo_execute` reports runtime problems:

    * `mesh not found`
    * `no create() in mesh`
    * `create() did not return mesh`
    * intrinsic-specific messages (e.g., `quad: index out of range`)

---

## Implementation Notes

* **Arena allocation** (`TopoArena`) backs AST, values, and temporary evaluation data. Results returned to the host (`TopoScene`) are on the heap and must be freed.
* **Parser** is newline-tolerant around `+` and `=` and requires semicolons.
* `create(...) : <annotations>` is supported and **ignored**; the parser skips tokens until it sees `{`.
* Operator `+` is intentionally limited to `number+number` and `mesh+mesh`. Other pairs degrade to right-hand value to make expressions like `mesh + nil` harmless.
* `return a, b, c;` is parsed, but only the **first** value is used by `topo_execute`.

---

## Limitations & Roadmap

* No CLI tool; embed as a library.
* Strings do not support escapes.
* No operator precedence beyond unary `-` and `+`.
* Only `+` is implemented; no `*`, `/`, `-` binary.
* `part`, `override`, `import`, `mesh : Parent` are parsed but not executed/linked yet.
* `create` parameters are parsed; defaults are not applied at runtime.
* Arrays are only meaningful for `ringlist`.

Planned:

* CLI runner and file `import` resolution.
* Proper expression precedence and more operators.
* Executable parts/overrides and mesh composition.
* String escapes and diagnostics improvements.
* Optional triangles output and normals/UVs in the runtime mesh.

---


## Raylib

This library is currently depends on RayMath library. I will remove it eventually. I hope.
