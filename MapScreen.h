#include <M5StickCPlus.h>
#include <math.h>
#include <memory.h>
#include <cstddef>

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
    const char* label;
    const uint16_t backColour;
    const char* backText;
    const bool surveyMap;
    const bool swapBytes;
    const float mapLongitudeLeft;
    const float mapLongitudeRight;
    const float mapLatitudeBottom;
  
    geo_map() : mapData(0),backColour(0),backText((const char*)0), surveyMap(false),swapBytes(false), mapLongitudeLeft(0),mapLongitudeRight(0),mapLatitudeBottom(0)
    {}
    
    geo_map(const uint16_t*  md, const char* l, uint16_t bc,const char* bt, bool sm, bool sb, float ll, float lr, float lb) : mapData(md),label(l),backColour(bc),backText(bt),surveyMap(sm),swapBytes(sb),mapLongitudeLeft(ll),mapLongitudeRight(lr),mapLatitudeBottom(lb)
    {}
};

static const geo_map s_maps[] =
{
  [0] = { .mapData = w1_1_16, .label="North", .backColour=TFT_BLACK, .backText="", .surveyMap=false, .swapBytes=false, .mapLongitudeLeft = -0.55, .mapLongitudeRight = -0.548, .mapLatitudeBottom = 51.4604},
  [1] = { .mapData = w1_2_16, .label="Cafe", .backColour=TFT_BLACK, .backText="", .surveyMap=false, .swapBytes=false, .mapLongitudeLeft = -0.5495, .mapLongitudeRight = -0.5475, .mapLatitudeBottom = 51.4593},
  [2] = { .mapData = w1_3_16, .label="Mid", .backColour=TFT_BLACK, .backText="", .surveyMap=false, .swapBytes=false, .mapLongitudeLeft = -0.5478, .mapLongitudeRight = -0.5458, .mapLatitudeBottom = 51.4588},
  [3] = { .mapData = w1_4_16, .label="South", .backColour=TFT_BLACK, .backText="", .surveyMap=false, .swapBytes=false, .mapLongitudeLeft = -0.5471, .mapLongitudeRight = -0.5451, .mapLatitudeBottom = 51.4583},
  [4] = { .mapData = wraysbury_x1, .label="All", .backColour=TFT_BLACK, .backText="", .surveyMap=false, .swapBytes=true, .mapLongitudeLeft = -0.5499, .mapLongitudeRight = -0.5452, .mapLatitudeBottom = 51.457350},
  [5] = { .mapData = nullptr, .label="Canoe", .backColour=TFT_CYAN, .backText="Canoe",.surveyMap=true, .swapBytes=false, .mapLongitudeLeft = -0.54910, .mapLongitudeRight = -0.54880, .mapLatitudeBottom = 51.46190}, // Canoe area
  [6] = { .mapData = nullptr, .label="Sub",  .backColour=TFT_CYAN, .backText="Sub",.surveyMap=true, .swapBytes=false, .mapLongitudeLeft = -0.54931, .mapLongitudeRight = -0.54900, .mapLatitudeBottom = 51.4608}, // Sub area
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
    MapScreen(TFT_eSPI* tft, M5StickCPlus* m5) : _allLakeMap(s_maps+4), 
                                                  _canoeZoneMap(s_maps+5),
                                                  _subZoneMap(s_maps+6),
                                                  _zoom(1),
                                                  _tileX(0),
                                                  _tileY(0)
    {
      _tft = tft;
      _m5 = m5;
      
      _currentMap = _previousMap = nullptr;

      _cleanMapAndFeaturesSprite.reset(new TFT_eSprite(_tft));
      _compositedScreenSprite.reset(new TFT_eSprite(_tft));
      _diverSprite.reset(new TFT_eSprite(_tft));
      _featureSprite.reset(new TFT_eSprite(_tft));

      initSprites();
    }

    ~MapScreen()
    {

    }

    void initCurrentMap(const double diverLatitude, const double diverLongitude);
    void clearMap();
    void drawFeaturesOnSpecifiedMapToScreen(const geo_map* featureAreaToShow);
    void drawDiverOnBestFeaturesMapAtCurrentZoom(const double diverLatitude, const double diverLongitude);
    
    void testAnimatingDiverSpriteOnCurrentMap();
    void testDrawingMapsAndFeatures(uint8_t& currentMap, uint8_t& zoom);


  private:
    TFT_eSPI* _tft;
    M5StickCPlus* _m5;
    
    std::unique_ptr<TFT_eSprite> _cleanMapAndFeaturesSprite;
    std::unique_ptr<TFT_eSprite> _compositedScreenSprite;
    std::unique_ptr<TFT_eSprite> _diverSprite;
    std::unique_ptr<TFT_eSprite> _featureSprite;

    static const uint16_t s_imgHeight = 240;
    static const uint16_t s_imgWidth = 135;
    static const uint8_t s_diverSpriteRadius = 15;
    static const uint8_t s_featureSpriteRadius = 5;
    static const uint16_t s_diverSpriteColour = TFT_BLUE;
    static const uint16_t s_featureSpriteColour = TFT_MAGENTA;
    static const bool     s_useSpriteForFeatures = true;

    const geo_map *_currentMap, *_previousMap;

    const geo_map* _northMap=s_maps;          const uint8_t _northMapIndex = 0;
    const geo_map* _cafeJettyMap=s_maps+1;   const uint8_t _cafeJettyMapIndex = 1;
    const geo_map* _midJettyMap=s_maps+2;    const uint8_t _midJettyMapIndex = 2;
    const geo_map* _southMap=s_maps+3;       const uint8_t _southMapIndex = 3;
    const geo_map* _allLakeMap=s_maps+4;     const uint8_t _allLakeMapIndex = 4;
    const geo_map* _canoeZoneMap=s_maps+5;   const uint8_t _canoeZoneMapIndex = 5;
    const geo_map* _subZoneMap=s_maps+6;     const uint8_t _subZoneMapIndex = 6;

    uint8_t _zoom;
    uint8_t _tileX;
    uint8_t _tileY;
    
    void initSprites();
    void drawFeaturesOnCleanMapSprite(const geo_map* featureMap,uint8_t zoom=1,int tileX=0,int tileY=0);
    pixel convertGeoToPixel(double latitude, double longitude, const geo_map* mapToPlot) const;
    const geo_map* getNextMapByPixelLocation(const MapScreen::pixel loc, const geo_map* thisMap) const;

    bool isPixelInCanoeZone(const MapScreen::pixel loc, const geo_map* thisMap) const;
    bool isPixelInSubZone(const MapScreen::pixel loc, const geo_map* thisMap) const;
    bool isPixelOutsideScreenExtent(const MapScreen::pixel loc) const;

    void debugPixelMapOutput(const MapScreen::pixel loc, const geo_map* thisMap, const geo_map* nextMap) const;
    void debugPixelFeatureOutput(navigationWaypoint* waypoint, MapScreen::pixel loc, const geo_map* thisMap) const;
};

void MapScreen::clearMap()
{
  _currentMap = _previousMap = nullptr;
  _m5->Lcd.fillScreen(TFT_BLACK);
}

void MapScreen::initCurrentMap(const double diverLatitude, const double diverLongitude)
{
  Serial.println("enter initCurrentMap");  
  // set zoom=1, tileX=0, tileY=0
  // find which map the lat/long is on. If outside all map area then choose all lake map.
  _zoom = 1;
  _tileX = _tileY = 0;
  
  _currentMap = _previousMap = _allLakeMap;

  pixel p;
  
  // identify first map that includes diver location within extent
  for (uint8_t i=_northMapIndex; i<=_allLakeMapIndex; i++)
  {
    p = convertGeoToPixel(diverLatitude, diverLongitude, s_maps+i);
    if (p.x >= 0 && p.x < s_imgWidth && p.y >=0 && p.y < s_imgHeight)
    {
      Serial.printf("hit map: %i x=%hu y=%hu\n",i,p.x,p.y);
      _currentMap = s_maps+i;
      break;
    }
    else
    {
      Serial.printf("miss map: %i x=%hu y=%hu\n",i,p.x,p.y);
    }
  }

  if (_currentMap == _northMap)
  {
    _previousMap = _cafeJettyMap;
  }
  else if (_currentMap == _cafeJettyMap)
  {
    _previousMap = _northMap;
  }
  else if (_currentMap == _midJettyMap)
  {
    _previousMap = _cafeJettyMap;
  }
  else if (_currentMap == _southMap)
  {
    _previousMap = _midJettyMap;
  }
  else if (_currentMap == _allLakeMap)
  {
    _previousMap = _allLakeMap;
    Serial.println("ALL Lake");
  }
  else if (_currentMap == _canoeZoneMap)
  {
    _previousMap = _canoeZoneMap;
    Serial.println("Canoe Zone");
  }
  else if (_currentMap == _subZoneMap)
  {
    _previousMap = _subZoneMap;
    Serial.println("Sub Zone");
  }
  else
  {
    Serial.println("Unknown map");
  }
  Serial.println("exit initCurrentMap");  
}

/* Requirements:
 *  
 *  _currentMap starts at NULL.
 *  survey maps are checked for presence before non-survey maps.
 *  survey maps show all features within map extent, plus diver, without last visited feature shown.
 *  survey maps only switch to last zoom non-survey map when out of area.
 *  at zoom level 1 and 2, base map gets switched once the selectMap function has detected diver moved to edge as defined by current map.
 *  at zoom level 2 switch between tiles within a base map is done at a tile boundary.
 *  for non-survey maps, last feature and next feature are shown in different colours.
 *  diver sprite flashes blue/green.
 */

void MapScreen::drawDiverOnBestFeaturesMapAtCurrentZoom(const double diverLatitude, const double diverLongitude)
{
    if (_currentMap == nullptr || _currentMap == _allLakeMap)
    {
      initCurrentMap(diverLatitude, diverLongitude);
    }
    
    pixel p = convertGeoToPixel(diverLatitude, diverLongitude, _currentMap);
    const geo_map* nextMap = getNextMapByPixelLocation(p, _currentMap);

    if (nextMap != _currentMap)
      _previousMap = _currentMap;

    // draw diver and feature map at pixel
    
    if (nextMap->mapData)
    {
      _cleanMapAndFeaturesSprite->pushImageScaled(0, 0, s_imgWidth, s_imgHeight, _zoom, _tileX, _tileY, 
                                                  nextMap->mapData, nextMap->swapBytes);
      drawFeaturesOnCleanMapSprite(nextMap);    // temp
    }
    else
    {
      _cleanMapAndFeaturesSprite->fillSprite(nextMap->backColour);
      
      drawFeaturesOnCleanMapSprite(nextMap);
    }
    
    _cleanMapAndFeaturesSprite->pushToSprite(_compositedScreenSprite.get(),0,0);
    
    p = convertGeoToPixel(diverLatitude, diverLongitude, nextMap);
    _diverSprite->pushToSprite(_compositedScreenSprite.get(),p.x-s_diverSpriteRadius,p.y-s_diverSpriteRadius,TFT_BLACK); // BLACK is the transparent colour

    _compositedScreenSprite->pushSprite(0,0);

    if (*nextMap->backText)
    {
      _m5->Lcd.setCursor(5,5);
      _m5->Lcd.setTextSize(3);
      _m5->Lcd.setTextColor(TFT_BLACK, nextMap->backColour);
      _m5->Lcd.println(nextMap->backText);
    }

    _currentMap = nextMap;
}

void MapScreen::debugPixelMapOutput(const MapScreen::pixel loc, const geo_map* thisMap, const geo_map* nextMap) const
{
  Serial.printf("%s %i, %i --> %s\n",thisMap->label,loc.x,loc.y,nextMap->label);
}

void MapScreen::debugPixelFeatureOutput(navigationWaypoint* waypoint, MapScreen::pixel loc, const geo_map* thisMap) const
{
  Serial.printf("x=%i y=%i %s %s \n",loc.x,loc.y,thisMap->label,waypoint->_label);
}

bool MapScreen::isPixelInCanoeZone(const MapScreen::pixel loc, const geo_map* thisMap) const
{
  bool result = false;
  
  if (thisMap == _northMap)
  {
    const int en = 3;    // enlarge extent by pixels
    result =( loc.x > 61 && loc.x < 80 && loc.y > 48-en && loc.y < 69+en);
  }

  return result;
}

bool MapScreen::isPixelInSubZone(const MapScreen::pixel loc, const geo_map* thisMap) const
{
  bool result = false;
  
  const int en = 3;    // enlarge extent by pixels
  if (thisMap == _northMap)
  {
    return (loc.x > 47 && loc.x < 66 && loc.y > 170-en && loc.y < 189+en);;
  }
  else if (thisMap == _cafeJettyMap)
  {
    return (loc.x > 13 && loc.x < 32 && loc.y > 51-en && loc.y < 70+en);
  }  
  
  return result;
}

bool MapScreen::isPixelOutsideScreenExtent(const MapScreen::pixel loc) const
{
  return (loc.x <= 0 || loc.x >= s_imgWidth || loc.y <=0 || loc.y >= s_imgHeight); 
}

const geo_map* MapScreen::getNextMapByPixelLocation(const MapScreen::pixel loc, const geo_map* thisMap) const
{
  const geo_map* nextMap = thisMap;

  if ((thisMap == _canoeZoneMap || thisMap == _subZoneMap) && isPixelOutsideScreenExtent(loc))
  {
    nextMap = (thisMap == _canoeZoneMap ? _northMap : _cafeJettyMap);   // go back to previous map?
  }
  else if (thisMap == _northMap)   // go right from 0 to 1
  {
    if (isPixelInCanoeZone(loc, thisMap))
      nextMap = _canoeZoneMap;
    else if (isPixelInSubZone(loc, thisMap))
      nextMap = _subZoneMap;
    else if ((loc.x >= 116 && loc.y >= 118) || 
           loc.x >= 30 && loc.y >= 215)
    {
      nextMap=_cafeJettyMap;
    }
  }
  else if (thisMap == _cafeJettyMap)
  { 
    if (isPixelInSubZone(loc, thisMap))
      nextMap = _subZoneMap;
    else if (loc.x >= 125 && loc.y >= 55 || 
        loc.x >= 135)      // go right from 1 to 2
      nextMap=_midJettyMap;
    else if (loc.x <=4 && loc.y <= 122 || 
            loc.x <= 83 && loc.y <= 30 )  // go left from 1 to 0
      nextMap=_northMap;
  }
  else if (thisMap == _midJettyMap)
  {
    if (loc.x >= 97 && loc.y >= 48|| 
        loc.y >= 175 && loc.x >= 42) // go right from 2 to 3
      nextMap=_southMap;
    else if (loc.x <= 0)
      nextMap=_cafeJettyMap;          // go left from 2 to 1
  }
  else if (thisMap == _southMap)
  {
    if  (loc.x <= 0 && loc.y <= 193 || 
         loc.x <= 39 && loc.y <= 119) // go left from 3 to 2
      nextMap = _midJettyMap;
  }

//  debugPixelMapOutput(loc, thisMap, nextMap);

  return nextMap;
}

void MapScreen::initSprites()
{
  _cleanMapAndFeaturesSprite->setColorDepth(16);
  _cleanMapAndFeaturesSprite->createSprite(135,240);

  _compositedScreenSprite->setColorDepth(16);
  _compositedScreenSprite->createSprite(135,240);

  _diverSprite->setColorDepth(16);
  _diverSprite->createSprite(s_diverSpriteRadius*2,s_diverSpriteRadius*2);
  _diverSprite->fillCircle(s_diverSpriteRadius,s_diverSpriteRadius,s_diverSpriteRadius,s_diverSpriteColour);

  _featureSprite->setColorDepth(16);
  _featureSprite->createSprite(s_featureSpriteRadius*2+1,s_featureSpriteRadius*2+1);
  _featureSprite->fillCircle(s_featureSpriteRadius,s_featureSpriteRadius,s_featureSpriteRadius,s_featureSpriteColour);
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
      if (s_useSpriteForFeatures)
        _featureSprite->pushToSprite(_cleanMapAndFeaturesSprite.get(),p.x - s_featureSpriteRadius, p.y - s_featureSpriteRadius,TFT_BLACK);
      else
        _cleanMapAndFeaturesSprite->fillCircle(p.x,p.y,s_featureSpriteRadius,s_featureSpriteColour);
        
//      debugPixelFeatureOutput(waypoints+i, p, featureMap);
    }
  }
}

//#define PI 3.141592653589793

MapScreen::pixel MapScreen::convertGeoToPixel(double latitude, double longitude, const geo_map* mapToPlot) const
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

  int16_t x = (longitude - mapLngLeft) * ((double)mapWidth / mapLngDelta);
  int16_t y = (double)mapHeight - ((worldMapWidth / 2.0L * log((1.0 + sin(latitudeRad)) / (1.0 - sin(latitudeRad)))) - (double)mapOffsetY);

  return pixel(x,y);
}

void MapScreen::drawFeaturesOnSpecifiedMapToScreen(const geo_map* featureAreaToShow)
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
