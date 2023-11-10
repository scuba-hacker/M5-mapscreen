#include <M5StickCPlus.h>
#include <math.h>
#include <map>
#include <set>
#include <memory>

#include "navigation_waypoints.h"

#include "MapScreen.h"

uint8_t currentMap = 0;
uint8_t zoom = 0;

std::unique_ptr<MapScreen> mapScreen;

void setup()
{
  M5.begin();

  M5.Lcd.setRotation(0);

  mapScreen.reset(new MapScreen(&M5.Lcd,&M5));
}

void loop()
{
//  M5.update();
//  mapScreen->testDrawingMapsAndFeatures(currentMap,zoom);

    mapScreen->drawFeaturesOnMapToScreen(s_maps+currentMap);
    mapScreen->testAnimatingDiverSpriteOnCurrentMap();
    
    currentMap++;
    if (currentMap == 7)
      currentMap=0;
    delay(1000);
}

/*

// need an out of map option - the top middle triangle for example
uint8_t selectNextMap(const pixel loc, uint8_t thisMap)
{
  uint8_t nextMap = thisMap;

  switch(thisMap)
  {
    case 0:
    {
      if ((loc.x >= 116 && loc.y >= 118) || loc.y >= 215)   // go right from 0 to 1
        nextMap=1;
      break;
    }
    case 1:
    {
      if (loc.x >= 130 && loc.y >= 55)      // go right from 1 to 2
        nextMap=2;
      else if (loc.x <=4 || (loc.y <= 30 && loc.x <=100))  // go left from 1 to 0
        nextMap=0;
      break;
    }
    case 2:
    {
      if (loc.x >= 97 || loc.y >=175)     // go right from 2 to 3
        nextMap=3;
      else if (loc.x <=5)
        nextMap=2;                        // go left from 2 to 1
      break;
    }
    case 3:
    {
      if (loc.x <= 39 || loc.y <= 119)    // go left from 3 to 2
        nextMap=2;
      break;
    }
    default:
    {
      break;
    }
  }
  
  return nextMap;
}
*/
