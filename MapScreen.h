#include <M5StickCPlus.h>
#include <math.h>
#include <memory.h>

#include "navigation_waypoints.h"

#include "Wraysbury_MapBox_135x240.png.h"
#include "w1_1_16.png.h"
#include "w1_2_16.png.h"
#include "w1_3_16.png.h"
#include "w1_4_16.png.h"

class geo_map
{
  public:
  const uint16_t* mapData;
  const uint16_t backColour;
  const bool swapBytes;
  const char* backText;
  const float mapLongitudeLeft;
  const float mapLongitudeRight;
  const float mapLatitudeBottom;

  geo_map() : mapData(0),backColour(0),backText((const char*)0), swapBytes(false), mapLongitudeLeft(0),mapLongitudeRight(0),mapLatitudeBottom(0)
  {}
  
  geo_map(const uint16_t*  md, uint16_t bc, const char* bt, bool sb, float ll, float lr, float lb) : mapData(md),backColour(bc),backText(bt),swapBytes(sb),mapLongitudeLeft(ll),mapLongitudeRight(lr),mapLatitudeBottom(lb)
  {}
};

static const geo_map s_maps[] =
{
  [0] = { .mapData = w1_1_16, .backColour=TFT_BLACK, .backText="", .swapBytes=false, .mapLongitudeLeft = -0.55, .mapLongitudeRight = -0.548, .mapLatitudeBottom = 51.4604},
  [1] = { .mapData = w1_2_16, .backColour=TFT_BLACK, .backText="", .swapBytes=false, .mapLongitudeLeft = -0.5495, .mapLongitudeRight = -0.5475, .mapLatitudeBottom = 51.4593},
  [2] = { .mapData = w1_3_16, .backColour=TFT_BLACK, .backText="", .swapBytes=false, .mapLongitudeLeft = -0.5478, .mapLongitudeRight = -0.5458, .mapLatitudeBottom = 51.4588},
  [3] = { .mapData = w1_4_16, .backColour=TFT_BLACK, .backText="", .swapBytes=false, .mapLongitudeLeft = -0.5471, .mapLongitudeRight = -0.5451, .mapLatitudeBottom = 51.4583},
  [4] = { .mapData = wraysbury_x1, .backColour=TFT_BLACK, .backText="", .swapBytes=true, .mapLongitudeLeft = -0.5499, .mapLongitudeRight = -0.5452, .mapLatitudeBottom = 51.457350},
  [5] = { .mapData = 0, .backColour=TFT_CYAN, .backText="Canoe",.swapBytes=false, .mapLongitudeLeft = -0.54910, .mapLongitudeRight = -0.54880, .mapLatitudeBottom = 51.46190}, // Canoe area
  [6] = { .mapData = 0, .backColour=TFT_CYAN, .backText="Sub",.swapBytes=false, .mapLongitudeLeft = -0.54931, .mapLongitudeRight = -0.54900, .mapLatitudeBottom = 51.4608}, // Sub area
};

class MapScreen
{     
  private:
    class pixel
    {
      public:
        pixel() : x(0), y(0) {}
        pixel(int16_t xx, int16_t yy) : x(xx), y(yy) {}
        
        int16_t x;
        int16_t y;
    };
    
  public:
    MapScreen(TFT_eSPI* tft, M5StickCPlus* m5) : _allLakeMap(s_maps+4), _canoeZoneMap(s_maps+5),_subZoneMap(s_maps+6), _currentMap(0)
    {
      _tft = tft;
      _m5 = m5;
      
      _cleanMapAndFeaturesSprite.reset(new TFT_eSprite(_tft));
      _compositedScreenSprite.reset(new TFT_eSprite(_tft));
      _diverSprite.reset(new TFT_eSprite(_tft));

      initSprites();
    }

    ~MapScreen()
    {

    }
    
    void drawFeaturesOnMapToScreen(const geo_map* featureAreaToSow);
    void testAnimatingDiverSpriteOnCurrentMap();

    void testDrawingMapsAndFeatures(uint8_t& currentMap, uint8_t& zoom);


  private:
    TFT_eSPI* _tft;
    M5StickCPlus* _m5;
    
    std::unique_ptr<TFT_eSprite> _cleanMapAndFeaturesSprite;
    std::unique_ptr<TFT_eSprite> _compositedScreenSprite;
    std::unique_ptr<TFT_eSprite> _diverSprite;

    const geo_map* _currentMap;

    static const uint16_t s_imgHeight = 240;
    static const uint16_t s_imgWidth = 135;
    static const uint8_t s_diverSpriteRadius = 15;
    static const uint8_t s_featureSpriteRadius = 5;
    static const uint16_t s_diverSpriteColour = TFT_BLUE;
    static const uint16_t s_featureSpriteColour = TFT_MAGENTA;
    
    const geo_map* _allLakeMap;
    const geo_map* _canoeZoneMap;
    const geo_map* _subZoneMap;
    
    void initSprites();
    void drawFeaturesOnCleanMapSprite(const geo_map* featureMap,uint8_t zoom=1,int tileX=0,int tileY=0);
    pixel convertGeoToPixel(double latitude, double longitude, const geo_map* mapToPlot);
 };


void MapScreen::initSprites()
{
  _cleanMapAndFeaturesSprite->setColorDepth(16);
  _cleanMapAndFeaturesSprite->createSprite(135,240);

  _compositedScreenSprite->setColorDepth(16);
  _compositedScreenSprite->createSprite(135,240);

  _diverSprite->setColorDepth(16);
  _diverSprite->createSprite(s_diverSpriteRadius*2,s_diverSpriteRadius*2);
  _diverSprite->fillCircle(s_diverSpriteRadius,s_diverSpriteRadius,s_diverSpriteRadius,s_diverSpriteColour);
}

void MapScreen::drawFeaturesOnCleanMapSprite(const geo_map* featureMap,uint8_t zoom,int tileX,int tileY)
{
  for(int i=0;i<waypointCount;i++)
  {
    pixel p = convertGeoToPixel(waypoints[i]._lat, waypoints[i]._long, featureMap);
    p.x = (p.x - (s_imgWidth/zoom)*tileX) * zoom;
    p.y = (p.y - (s_imgHeight/zoom)*tileY) * zoom;

    if (p.x >= 0 && p.x < s_imgWidth && p.y >=0 && p.y < s_imgHeight)
    {
      _cleanMapAndFeaturesSprite->fillCircle(p.x,p.y,s_featureSpriteRadius,s_featureSpriteColour);
    }
  }
}

//#define PI 3.141592653589793

MapScreen::pixel MapScreen::convertGeoToPixel(double latitude, double longitude, const geo_map* mapToPlot)
{
  const int16_t mapWidth = s_imgWidth; // in pixels
  const int16_t mapHeight = s_imgHeight; // in pixels
  double mapLngLeft = mapToPlot->mapLongitudeLeft; // in degrees. the longitude of the left side of the map (i.e. the longitude of whatever is depicted on the left-most part of the map image)
  double mapLngRight = mapToPlot->mapLongitudeRight; // in degrees. the longitude of the right side of the map
  double mapLatBottom = mapToPlot->mapLatitudeBottom; // in degrees.  the latitude of the bottom of the map

  double mapLatBottomRad = mapLatBottom * PI / 180.0;
  double latitudeRad = latitude * PI / 180.0;
  double mapLngDelta = (mapLngRight - mapLngLeft);

  double worldMapWidth = ((mapWidth / mapLngDelta) * 360.0) / (2.0 * PI);
  double mapOffsetY = (worldMapWidth / 2.0 * log((1.0 + sin(mapLatBottomRad)) / (1.0 - sin(mapLatBottomRad))));

  uint16_t x = (longitude - mapLngLeft) * ((double)mapWidth / mapLngDelta);
  uint16_t y = (double)mapHeight - ((worldMapWidth / 2.0L * log((1.0 + sin(latitudeRad)) / (1.0 - sin(latitudeRad)))) - (double)mapOffsetY);

  return pixel(x,y);
}

void MapScreen::drawFeaturesOnMapToScreen(const geo_map* featureAreaToShow)
{
    const uint8_t zoom=1, tileX=0, tileY=0;

    _currentMap = featureAreaToShow;

    if (featureAreaToShow->mapData)
    {
      _cleanMapAndFeaturesSprite->pushImageScaled(0, 0, s_imgWidth, s_imgHeight, zoom, tileX, tileY, 
                                                  featureAreaToShow->mapData, featureAreaToShow->swapBytes);
    }
    else
    {
      _cleanMapAndFeaturesSprite->fillSprite(featureAreaToShow->backColour);
    }
    
    drawFeaturesOnCleanMapSprite(featureAreaToShow);

    _cleanMapAndFeaturesSprite->pushSprite(0,0);

    if (*featureAreaToShow->backText)
    {
      _m5->Lcd.setCursor(5,5);
      _m5->Lcd.setTextSize(3);
      _m5->Lcd.setTextColor(TFT_BLACK, featureAreaToShow->backColour);
      _m5->Lcd.println(featureAreaToShow->backText);
    }
}

void MapScreen::testAnimatingDiverSpriteOnCurrentMap()
{
  const geo_map* featureAreaToShow = _currentMap;
  
  double latitude = featureAreaToShow->mapLatitudeBottom;
  double longitude = featureAreaToShow->mapLongitudeLeft;

  for(int i=0;i<20;i++)
  {
    _cleanMapAndFeaturesSprite->pushToSprite(_compositedScreenSprite.get(),0,0);
    
    pixel p = convertGeoToPixel(latitude, longitude, featureAreaToShow);
    _diverSprite->pushToSprite(_compositedScreenSprite.get(),p.x-s_diverSpriteRadius,p.y-s_diverSpriteRadius,TFT_BLACK); // BLACK is the transparent colour

    _compositedScreenSprite->pushSprite(0,0);

    latitude+=0.0001;
    longitude+=0.0001;
  }
}

void MapScreen::testDrawingMapsAndFeatures(uint8_t& currentMap, uint8_t& zoom)
{  
  if (_m5->BtnA.isPressed())
  {
    zoom = (zoom == 2 ? 1 : 2);
    _m5->update();
    while (_m5->BtnA.isPressed())
    {
      _m5->update();
    }    
  }
  else if (_m5->BtnB.isPressed())
  {
    zoom = (zoom > 0 ? 0 : 1);
    _m5->update();
    while (_m5->BtnB.isPressed())
    {
      _m5->update();
    }    
  }

  if (zoom)
  {
    for (int tileY=0; tileY < zoom; tileY++)
    {
      for (int tileX=0; tileX < zoom; tileX++)
      {
        _m5->update();

        if (_m5->BtnA.isPressed())
        {
          zoom = (zoom == 2 ? 1 : 2);
          tileX=zoom;
          tileY=zoom;
          _m5->update();
          while (_m5->BtnA.isPressed())
          {
            _m5->update();
          }    
          break;
        }
        else if (_m5->BtnB.isPressed())
        {
          zoom = (zoom > 0 ? 0 : 1);
          tileX=zoom;
          tileY=zoom;
          _m5->update();
          while (_m5->BtnB.isPressed())
          {
            _m5->update();
          }    
          break;
        }

        const geo_map* featureAreaToShow = s_maps+currentMap;        
        _cleanMapAndFeaturesSprite->pushImageScaled(0, 0, s_imgWidth, s_imgHeight, zoom, tileX, tileY, featureAreaToShow->mapData);

        drawFeaturesOnCleanMapSprite(featureAreaToShow, zoom, tileX, tileY);
    
        double latitude = featureAreaToShow->mapLatitudeBottom;
        double longitude = featureAreaToShow->mapLongitudeLeft;
      
        for(int i=0;i<20;i++)
        {
          _cleanMapAndFeaturesSprite->pushToSprite(_compositedScreenSprite.get(),0,0);
          
          pixel p = convertGeoToPixel(latitude, longitude, featureAreaToShow);
          _diverSprite->pushToSprite(_compositedScreenSprite.get(),p.x-s_diverSpriteRadius,p.y-s_diverSpriteRadius,TFT_BLACK); // BLACK is the transparent colour
    
          _compositedScreenSprite->pushSprite(0,0);
    
          latitude+=0.0001;
          longitude+=0.0001;
//          delay(50);
        }
      }
    }
  
    currentMap == 3 ? currentMap = 0 : currentMap++;
  }
  else
  {
    const geo_map* featureAreaToShow = s_maps+4;
    const bool swapBytes = true;    // as original PNG is in opposite endian format (as not suitable for DMA)

    _cleanMapAndFeaturesSprite->pushImageScaled(0, 0, s_imgWidth, s_imgHeight, 1, 0, 0, featureAreaToShow->mapData, swapBytes);

    drawFeaturesOnCleanMapSprite(featureAreaToShow);

    double latitude = featureAreaToShow->mapLatitudeBottom;
    double longitude = featureAreaToShow->mapLongitudeLeft;
  
    for(int i=0;i<25;i++)
    {
      _cleanMapAndFeaturesSprite->pushToSprite(_compositedScreenSprite.get(),0,0);
      
      pixel p = convertGeoToPixel(latitude, longitude, featureAreaToShow);
      _diverSprite->pushToSprite(_compositedScreenSprite.get(),p.x-s_diverSpriteRadius,p.y-s_diverSpriteRadius,TFT_BLACK); // BLACK is the transparent colour

      _compositedScreenSprite->pushSprite(0,0);

      latitude+=0.0002;
      longitude+=0.0002;
    }
  } 
}
