import "legs.tl";
import "back.tl";

mesh Chair {
  part Seat(number w=0.46, number d=0.48, number t=0.035): mesh {
    r0 = ring(0, 0, w * 0.5, d * 0.5, 48);
    r1 = lift_z(r0, t);
    wall = stitch([r0, r1]);
    cap0 = cap_plane(r0);
    cap1 = cap_plane(r1);
    slab = merge(wall, cap0, cap1);
    return weld(slab, 0.001);
  }

  create(): mesh {
    w     = 0.46;
    d     = 0.48;
    tSeat = 0.035;
    hLeg  = 0.45;

    seat = move(Seat(w, d, tSeat), 0, 0, hLeg);

    rLeg   = 0.014;
    insetL = 0.090;
    legs = ChairLegs.Legs(w, d, hLeg, insetL, rLeg);

    backW = w * 0.90;
    backH = 0.70;
    rBack = 0.012;

    back = ChairBack.Back(backW, backH, rBack);

    contact = 0.09;
    backY   = -d * 0.5 + contact - rBack;
    backZ   =  hLeg + 0.001;
    back    = move(back, 0, backY, backZ);

    return weld(merge(legs, seat, back), 0.001);
  }
}
