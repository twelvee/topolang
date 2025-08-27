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