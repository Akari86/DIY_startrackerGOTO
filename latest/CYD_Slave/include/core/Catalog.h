#pragma once
// Local browse index. Coordinates are requested from the mount on selection.
// A constellation entry aims at the explicitly named representative object.
struct Target { const char *name,*detail; int messier,category; double ra_h, dec_d; };
static const Target targets[]={
 {"Orion","Region / M42 nebula",42,1, 5.58814, -5.39111},
 {"Taurus","Region / M45 Pleiades",45,1, 3.78361, 24.11667},
 {"Andromeda","Region / M31 galaxy",31,1, 0.71230, 41.26875},
 {"Lyra","Region / M57 ring",57,1, 18.8930, 33.0283},
 {"Hercules","Region / M13 cluster",13,1, 16.6950, 36.4600},
 {"Sagittarius","Region / M8 lagoon",8,1, 18.0600, -24.3800},
 {"Cygnus","Region / M29 cluster",29,1, 20.3980, 38.5300},
 {"Pegasus","Region / M15 cluster",15,1, 21.5000, 12.1670},
 {"M45 Pleiades","Open star cluster",45,2, 3.7836, 24.1167},
 {"M44 Beehive","Open star cluster",44,2, 8.6670, 19.6670},
 {"M13 Hercules","Globular cluster",13,2, 16.6950, 36.4600},
 {"M3","Globular cluster",3,2, 13.7040, 28.3760},
 {"M5","Globular cluster",5,2, 15.3100, 2.0810},
 {"M15","Globular cluster",15,2, 21.5000, 12.1670},
 {"M31 Andromeda","Spiral galaxy",31,3, 0.7123, 41.2688},
 {"M33 Triangulum","Spiral galaxy",33,3, 1.5640, 30.6600},
 {"M51 Whirlpool","Interacting galaxies",51,3, 13.4980, 47.1950},
 {"M81 Bode","Spiral galaxy",81,3, 9.9210, 69.0650},
 {"M82 Cigar","Starburst galaxy",82,3, 9.9300, 69.6780},
 {"M42 Orion","Emission nebula",42,4, 5.5881, -5.3911},
 {"M57 Ring","Planetary nebula",57,4, 18.8930, 33.0283},
 {"M27 Dumbbell","Planetary nebula",27,4, 19.9910, 22.7210},
 {"M8 Lagoon","Emission nebula",8,4, 18.0600, -24.3800},
 {"M20 Trifid","Nebula",20,4, 18.0440, -22.9530},
 {"M1 Crab","Supernova remnant",1,4, 5.5750, 22.0140}
};
constexpr int TARGET_COUNT=sizeof(targets)/sizeof(targets[0]);
