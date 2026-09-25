#pragma once
// Local browse index. Coordinates are requested from the mount on selection.
// A constellation entry aims at the explicitly named representative object.
struct Target { const char *name,*detail; int messier,category; };
static const Target targets[]={
 {"Orion","Region / M42 nebula",42,1},
 {"Taurus","Region / M45 Pleiades",45,1},
 {"Andromeda","Region / M31 galaxy",31,1},
 {"Lyra","Region / M57 ring",57,1},
 {"Hercules","Region / M13 cluster",13,1},
 {"Sagittarius","Region / M8 lagoon",8,1},
 {"Cygnus","Region / M29 cluster",29,1},
 {"Pegasus","Region / M15 cluster",15,1},
 {"M45 Pleiades","Open star cluster",45,2},
 {"M44 Beehive","Open star cluster",44,2},
 {"M13 Hercules","Globular cluster",13,2},
 {"M3","Globular cluster",3,2},
 {"M5","Globular cluster",5,2},
 {"M15","Globular cluster",15,2},
 {"M31 Andromeda","Spiral galaxy",31,3},
 {"M33 Triangulum","Spiral galaxy",33,3},
 {"M51 Whirlpool","Interacting galaxies",51,3},
 {"M81 Bode","Spiral galaxy",81,3},
 {"M82 Cigar","Starburst galaxy",82,3},
 {"M42 Orion","Emission nebula",42,4},
 {"M57 Ring","Planetary nebula",57,4},
 {"M27 Dumbbell","Planetary nebula",27,4},
 {"M8 Lagoon","Emission nebula",8,4},
 {"M20 Trifid","Nebula",20,4},
 {"M1 Crab","Supernova remnant",1,4}
};
constexpr int TARGET_COUNT=sizeof(targets)/sizeof(targets[0]);
