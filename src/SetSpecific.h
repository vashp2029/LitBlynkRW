#ifndef SETSPECIFIC_H
  #define SETSPECIFIC_H

  #ifdef BED
    #define SENSORNAME "bed"
    #define NUMLEDS 150
    #define LEDGROUP 2 //LEDGROUP 1 is reserved as "global" or "all groups"

  #elif COUCH
    #define SENSORNAME "couch"
    #define NUMLEDS 150
    #define LEDGROUP 3

  #elif TV
    #define SENSORNAME "tv"
    #define NUMLEDS 150
    #define LEDGROUP 4

  #elif TVSTAND
    #define SENSORNAME "tv-stand"
    #define NUMLEDS 150
    #define LEDGROUP 5

  #elif WALLDESK
    #define SENSORNAME "wall-desk"
    #define NUMLEDS 150
    #define LEDGROUP 6

  #elif GLASSDESK
    #define SENSORNAME "glass-desk"
    #define NUMLEDS 150
    #define LEDGROUP 7

  #endif

#endif
