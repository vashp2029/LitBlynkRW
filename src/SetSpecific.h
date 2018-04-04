#ifndef SETSPECIFIC_H
  #define SETSPECIFIC_H

  #ifdef BED
    #define SENSORNAME "Bed"
    #define NUMLEDS 150
    #define LEDGROUP 2 //LEDGROUP 1 is reserved as "global" or "all groups"

  #elif COUCH
    #define SENSORNAME "Couch"
    #define NUMLEDS 150
    #define LEDGROUP 3

  #elif TV
    #define SENSORNAME "TV"
    #define NUMLEDS 150
    #define LEDGROUP 4

  #elif TVSTAND
    #define SENSORNAME "TV Stand"
    #define NUMLEDS 150
    #define LEDGROUP 5

  #elif WALLDESK
    #define SENSORNAME "Wall Desk"
    #define NUMLEDS 150
    #define LEDGROUP 6

  #elif GLASSDESK
    #define SENSORNAME "Glass Desk"
    #define NUMLEDS 150
    #define LEDGROUP 7

  #endif

#endif
