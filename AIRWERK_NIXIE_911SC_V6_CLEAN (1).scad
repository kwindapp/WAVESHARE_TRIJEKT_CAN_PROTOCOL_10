// AIRWERK NIXIE V6 - clean bezel without four outer mounting holes
// Concept dimensions in mm. Check actual DIN aperture and chosen tube sockets.
part="assembly"; // assembly, bezel, reflector, housing, back, tubes
$fn=72;
front_w=190; front_h=58; front_t=4;
case_w=177; case_h=47; case_depth=85; wall=2.5;
// Visible window left of center, leaving blank metal on right
window_w=128; window_h=39; window_x=-19; radius=5;
reflect_depth=17; slope=5; tray_wall=3; floor_t=3;
tube_d=19; tube_pitch=27; colon_gap=9;
// Nixie tubes are represented by solid mockups, NOT through-holes in the reflector floor.
module rr(w,h,r){offset(r=r) square([w-2*r,h-2*r],center=true);}
module plate(w,h,r,t){linear_extrude(t) rr(w,h,r);}
module tapered_cavity(){
  // Wide opening at front, smaller opening at rear, with genuine sloped sidewalls
  hull(){
    translate([0,0,reflect_depth-0.02]) linear_extrude(0.04) rr(window_w,window_h,radius);
    translate([0,0,floor_t]) linear_extrude(0.04) rr(window_w-2*slope,window_h-2*slope,max(1,radius-2));
  }
  translate([0,0,reflect_depth-0.02]) linear_extrude(1) rr(window_w,window_h,radius);
}
module bezel(){
  difference(){
    plate(front_w,front_h,3,front_t);
    translate([window_x,0,-1]) plate(window_w,window_h,radius,front_t+2);

  }
}
module reflector(){
  translate([window_x,0,-reflect_depth]) difference(){
    plate(window_w+2*tray_wall,window_h+2*tray_wall,radius+tray_wall,reflect_depth);
    tapered_cavity();
    // small wiring pass-throughs ONLY; sockets/PCB mounted behind tray
    for(x=[-1.5*tube_pitch,-.5*tube_pitch,.5*tube_pitch,1.5*tube_pitch])
      translate([x,0,-0.5]) cylinder(d=5,h=floor_t+1);
    for(y=[-colon_gap/2,colon_gap/2])
      translate([0,y,-0.5]) cylinder(d=2.5,h=floor_t+1);
  }
}
module housing(){
  translate([0,0,-case_depth]) difference(){
    plate(case_w,case_h,3,case_depth-1);
    translate([0,0,-1]) plate(case_w-2*wall,case_h-2*wall,1.2,case_depth+2);
  }
}
module back(){
  difference(){
    plate(case_w,case_h,3,3);
    for(x=[-79,79],y=[-17,17]) translate([x,y,-1]) cylinder(d=3.2,h=5);
    for(x=[-55:11:55]) translate([x,0,-1]) plate(4,17,1.5,5);
  }
}
// Tube mockups rotated 90 degrees: axes now lie along Y, parallel to the face.
// Visual placeholders only; final socket/PCB geometry depends on tube selection.
module tubes(){
  for(x=[-1.5*tube_pitch,-.5*tube_pitch,.5*tube_pitch,1.5*tube_pitch]){
    color([0.23,0.13,0.08,0.55])
      translate([window_x+x,0,-reflect_depth+floor_t+6])
        rotate([90,0,0]) cylinder(d=tube_d,h=29,center=true);
  }
  color([1,0.42,0.04]) for(y=[-colon_gap/2,colon_gap/2])
    translate([window_x,y,-reflect_depth+floor_t+2]) sphere(d=2.8);
}
module assembly(){
  color([0.72,0.73,0.74]) bezel();
  color([0.65,0.31,0.10]) reflector();
  color([0.14,0.15,0.16]) housing();
  tubes();
  color([0.2,0.21,0.22]) translate([0,0,-case_depth-3]) back();
}
if(part=="assembly") assembly();
else if(part=="bezel") bezel();
else if(part=="reflector") reflector();
else if(part=="housing") housing();
else if(part=="back") back();
else if(part=="tubes") tubes();
