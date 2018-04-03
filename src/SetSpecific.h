//This file is to make it easier to change the API key being used for
//each ESP by just looking at what the SENSORENAME is and automatically
//picking the corresponding key.

#ifndef SETSPECIFIC_H
#define SETSPECIFIC_H

  #ifdef BED
    #define SENSORNAME "Bed"
    char blynkAuth[] = "1979ca5fb8754d5fbb0cba42cf4fa472";

    #define NUM_LEDS 150
    #define LED_SET 1

  #elif COUCH
    #define SENSORNAME "Couch"
    char blynkAuth[] = "e6005a1bfd13426da4ded915aafa9b65";

    #define NUM_LEDS 150
    #define LED_SET 2

  #elif TV
    #define SENSORNAME "TV"
    char blynkAuth[] = "504304753d1349ba9b2bea94cef62bf8";

    #define NUM_LEDS 150
    #define LED_SET 3

  #elif TVSTAND
    #define SENSORNAME "TV Stand"
    char blynkAuth[] = "f1d29048bea247baa6846383190608c5";

    #define NUM_LEDS 150
    #define LED_SET 4

  #elif WALLDESK
    #define SENSORNAME "Wall Desk"
    char blynkAuth[] = "425f435e1c514a3483ce42aa77504c82";

    #define NUM_LEDS 150
    #define LED_SET 5

  #elif GLASSDESK
    #define SENSORNAME "Glass Desk"
    char blynkAuth[] = "86771618ea7c4008b56c4379403386ee";

    #define NUM_LEDS 150
    #define LED_SET 6

  #elif TESTESP
    #define SENSORNAME "TestESP"
    char blynkAuth[] = "48acb8cd05e645eebdef6d873f4e2262";

    #define NUM_LEDS 150
    #define LED_SET 7

  #endif

#endif
