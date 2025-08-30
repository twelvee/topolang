mesh ChairLegs {
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

  part TubeY(number len, number r): mesh {
    b = rotate_x(Cylinder(len, r, 24), -1.57079633);
    return move(b, 0, -len * 0.5, 0);
  }

  part TubeZ(number len, number r): mesh {
    return Cylinder(len, r, 24);
  }

  part Leg(number r=0.014, number h=0.45): mesh {
    return TubeZ(h, r);
  }

 part Legs(number w, number d, number z0, number inset, number rLeg): mesh {
   mn = w; if (d < w) { mn = d; }

   want = inset;
   if (want < 0) { want = mn * 0.08; }

   maxInset = mn * 0.5 - rLeg - 0.006;
   if (want > maxInset) { want = maxInset; }
   minInset = rLeg + 0.006;
   if (want < minInset) { want = minInset; }

   dx = w * 0.5 - want;
   dy = d * 0.5 - want;

   leg = Leg(rLeg, z0);

   railR    = rLeg * 0.70;
   railZ    = z0 * 0.78;
   railLenY = d - want * 2.0;

   sideRails = merge(
     move(TubeY(railLenY, railR),  dx, 0, railZ),
     move(TubeY(railLenY, railR), -dx, 0, railZ)
   );

   L = merge(
     move(leg,  dx,  dy, 0),
     move(leg, -dx,  dy, 0),
     move(leg,  dx, -dy, 0),
     move(leg, -dx, -dy, 0),
     sideRails
   );

   return weld(L, 0.001);
 }
}
