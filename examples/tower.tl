mesh Tower {
  create() : mesh, error {
    r0 = ring(0, 0, 1.0, 1.0, 32);
    r1 = grow_out(r0, -0.05, 0.2);
    r2 = grow_out(r1, -0.05, 0.2);
    r3 = grow_out(r2, -0.05, 0.2);
    r4 = grow_out(r3, -0.05, 0.2);
    r5 = grow_out(r4, -0.05, 0.2);
    r6 = grow_out(r5, -0.05, 0.2);

    rings = [r0, r1, r2, r3, r4, r5, r6];
    return stitch(rings), nil;
  }
}
