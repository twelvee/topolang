mesh Tower {
  const dz   =  0.2;
  const step = -0.05;

  make_rings(ring r, number n) : ringlist, error {
    out = ringlist(r);
    for i in 1..=n {
      r = grow_out(r, -0.05, dz);
      out = ringlist_push(out, r);
    }
    return out, nil;
  }

  create() : mesh, error {
    r = ring(0, 0, 1.0, 1.0, 32);
    rings = make_rings(r, 6);
    side   = stitch(rings);
    bottom = cap_plane(first(rings));
    top    = cap_plane(last(rings));

    return weld(merge(side, bottom, top), 0.001), nil;
  }
}
