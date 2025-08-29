mesh Tower {
  create() : mesh, error {
    step = -0.05;
    dz   =  0.2;

    r = ring(0, 0, 1.0, 1.0, 32);
    rings = ringlist(r);

    for i in 1..=6 {
      r = grow_out(r, step, dz);
      rings = ringlist_push(rings, r);
    }

    side   = stitch(rings);
    bottom = cap_plane(first(rings));
    top    = cap_plane(r);

    return weld(merge(side, bottom, top), 0.001), nil;
  }
}
