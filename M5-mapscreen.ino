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

class geo_location
{
  public:
    double la;
    double lo;
};

const geo_location diveTrack[] =
{  
  [0] = { .la = 0, .lo =0},
};

int trackIndex=0;
int trackLength=sizeof(diveTrack)/sizeof(geo_location);

geo_location pos;

void setup()
{
  M5.begin();
  
  M5.Lcd.setRotation(0);

  mapScreen.reset(new MapScreen(&M5.Lcd,&M5));

  pos.la = 51.4601028571429;    // spitfire car
  pos.lo = -0.54883835;

  pos.la = 51.4605855;    // lightning boat
  pos.lo = -0.54890166666666; 
}

void cycleTrackIndex()
{
  trackIndex = (trackIndex + 1) % trackLength;

  pos.lo += 0.00004;
  pos.la -= 0.00002;
}

void loop()
{
  M5.update();

  if (M5.BtnA.isPressed() && M5.BtnB.isPressed())
  {
    /*
    pos.la = 51.4605855;    // lightning boat
    pos.lo = -0.54890166666666; 
    mapScreen->clearMap();
    */
     pos.lo -= 0.0002;
     pos.la += 0.0002;
    
  }
  else if (M5.BtnA.isPressed())
     pos.lo -= 0.0001;
  else if (M5.BtnB.isPressed())
     pos.la += 0.0001;
     



  bool useTrack = false;
  
  if (useTrack)
    mapScreen->drawDiverOnBestFeaturesMapAtCurrentZoom(diveTrack[trackIndex].la,diveTrack[trackIndex].lo);
  else
    mapScreen->drawDiverOnBestFeaturesMapAtCurrentZoom(pos.la,pos.lo);
    
  cycleTrackIndex();
  delay(100);
}


void loop_away()
{
//  M5.update();
//  mapScreen->testDrawingMapsAndFeatures(currentMap,zoom);

    mapScreen->drawFeaturesOnSpecifiedMapToScreen(s_maps+currentMap);
    
    mapScreen->testAnimatingDiverSpriteOnCurrentMap();
    
    currentMap++;
    if (currentMap == 7)
      currentMap=0;
    delay(1000);
}

/*

// need an out of map option - the top middle triangle for example

*/
