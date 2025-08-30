mesh ChairBack {
  part Cylinder(number len, number r, number seg=28): mesh {
    r0 = ring(0, 0, r, r, seg);
    r1 = lift_z(r0, len);
    wall = stitch([r0, r1]);
    cap0 = cap_plane(r0);
    cap1 = cap_plane(r1);
    out = merge(wall, cap0, cap1);
    return weld(out, 0.001);
  }

  part TubeX(number len, number r): mesh {
    b = rotate_y(Cylinder(len, r, 24), 1.57079633);
    return move(b, -len * 0.5, 0, 0);
  }

  part TubeZ(number len, number r): mesh {
    return Cylinder(len, r, 24);
  }

  part Back(number width, number height, number rTube): mesh {
    w  = width;
    h  = height;
    r  = rTube;

    insetRaw = w * 0.16;
    maxInset = w * 0.5 - r - 0.008;
    if (insetRaw > maxInset) { insetRaw = maxInset; }
    if (insetRaw < r + 0.008) { insetRaw = r + 0.008; }

    dx = w * 0.5 - insetRaw;

    uprL = move(TubeZ(h, r), -dx, 0, 0);
    uprR = move(TubeZ(h, r),  dx, 0, 0);

    top   = move(TubeX(2.0 * dx, r), 0, 0, h * 0.96);
    mid   = move(TubeX(2.0 * dx, r * 0.8), 0, 0, h * 0.55);
    lower = move(TubeX(2.0 * dx, r * 0.8), 0, 0, h * 0.18);

    frame = merge(uprL, uprR, top, mid, lower);
    return weld(frame, 0.001);
  }
}
