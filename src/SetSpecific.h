#ifndef SETSPECIFIC_H
  #define SETSPECIFIC_H

  #ifdef BED
    #define SENSORNAME "Bed"
    #define NUMLEDS 150
    #define LEDGROUP 1

  #elif COUCH
    #define SENSORNAME "Couch"
    #define NUMLEDS 150
    #define LEDGROUP 2

  #elif TV
    #define SENSORNAME "TV"
    #define NUMLEDS 150
    #define LEDGROUP 3

  #elif TVSTAND
    #define SENSORNAME "TV Stand"
    #define NUMLEDS 150
    #define LEDGROUP 4

  #elif WALLDESK
    #define SENSORNAME "Wall Desk"
    #define NUMLEDS 150
    #define LEDGROUP 5

  #elif GLASSDESK
    #define SENSORNAME "Glass Desk"
    #define NUMLEDS 150
    #define LEDGROUP 6

  #endif

#endif
