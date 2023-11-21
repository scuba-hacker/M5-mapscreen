#include <M5StickCPlus.h>
#include <memory.h>
#include "esp_heap_caps.h"

#include "navigation_waypoints.h"
#include "dive_track.h"

#include "MapScreen.h"

std::unique_ptr<MapScreen> mapScreen;

class geo_location
{
  public:
    double la;
    double lo;
    int heading;
};

geo_location pos;

int trackIndex=0;
int trackLength=sizeof(diveTrack)/sizeof(location);

int testIteration=0;

void setup()
{
  M5.begin();
  
  M5.Lcd.setRotation(0);
  
  mapScreen.reset(new MapScreen(&M5.Lcd,&M5));

  multi_heap_info_t info;
  heap_caps_get_info(&info, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT); // internal RAM, memory capable to store data or to create new task
  info.total_free_bytes;   // total currently free in all non-continues blocks
  info.minimum_free_bytes;  // minimum free ever
  info.largest_free_block;   // largest continues block to allocate big array

  // Serial.println("Managed to allocate 2 x 8-bit 320x240 = 76800 bytes");
  // Serial.printf("free bytes: %i  largest free block: %i\n",  info.total_free_bytes, info.largest_free_block);
  
  mapScreen->setUseDiverHeading(true);

  pos.la = 51.4601028571429;    // spitfire car
  pos.lo = -0.54883835;

  pos.la = 51.4605855;    // lightning boat
  pos.lo = -0.54890166666666; 

  mapScreen->setTargetWaypointByLabel("Sub"); // Cafe Jetty
}

void cycleTrackIndex()
{
  trackIndex = (trackIndex + 1) % trackLength;

  pos.lo += 0.00004;
  pos.la -= 0.00002;
}

void loop()
{
//  if (testIteration == 300)
//  {
//      mapScreen->setTargetWaypointByLabel("Sub"); // Mid Jetty
//  }
  
  M5.update();

  if (M5.BtnA.isPressed() && M5.BtnB.isPressed())
  {
     pos.lo -= 0.0002;
     pos.la += 0.0002;
  }
  else if (M5.BtnA.isPressed())
     pos.lo -= 0.0001;
  else if (M5.BtnB.isPressed())
     pos.la += 0.0001;
  else if (M5.Axp.GetBtnPress() == 0x02)
  {
    mapScreen->cycleZoom();
    while(M5.Axp.GetBtnPress() == 0x02);
  }

  bool useTrack = true;
  
  if (useTrack)
  {
    mapScreen->drawDiverOnBestFeaturesMapAtCurrentZoom(diveTrack[trackIndex]._la,diveTrack[trackIndex]._lo,diveTrack[trackIndex]._h);
  }
  else
  {
    mapScreen->drawDiverOnBestFeaturesMapAtCurrentZoom(pos.la,pos.lo,pos.heading);
    const uint16_t headingStep = 5;

    pos.heading+=headingStep;
    pos.heading%=360;
  }
    
  cycleTrackIndex();
  testIteration++;
//  delay(50);
}
