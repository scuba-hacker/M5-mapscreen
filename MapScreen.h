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

// array of geomap pointers

class geoRef
{
  public:
    int geoMaps[4];
    
};

geoRef s_featureToMaps[waypointCount];

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

const uint8_t exitWaypointCount = 4;
navigationWaypoint* exitWaypoints[exitWaypointCount];

/*  ************* Features to add ***********
 *  1. DONE for non-survey maps, last feature and next feature are shown in different colours.
 *  2. diver sprite flashes blue/green.
 *  3. DONE - diver sprite is rotated according to compass direction.
 *  4. direction line to target
 *  5. direction line of current heading
 *  
*/

class MapScreen
{     
  public:
    class pixel
    {
      public:
        pixel() : x(0), y(0), colour(0) {}
        pixel(int16_t xx, int16_t yy, uint16_t colourr) : x(xx), y(yy), colour(colourr) {}
        pixel(int16_t xx, int16_t yy) : x(xx), y(yy), colour(0) {}

        int16_t x;
        int16_t y;
        uint16_t colour;
    };

  public:
    MapScreen(TFT_eSPI* tft, M5StickCPlus* m5) : _allLakeMap(s_maps+4), 
                                                  _canoeZoneMap(s_maps+5),
                                                  _subZoneMap(s_maps+6),
                                                  _zoom(1),
                                                  _priorToZoneZoom(1),
                                                  _tileXToDisplay(0),
                                                  _tileYToDisplay(0),
                                                  _showAllLake(false),
                                                  _lastDiverLatitude(0),
                                                  _lastDiverLongitude(0),
                                                  _lastDiverHeading(0),
                                                  _useDiverHeading(false),
                                                  _targetWaypoint(nullptr),
                                                  _closestExitWaypoint(nullptr),
                                                  _prevWaypoint(nullptr)
    {
      _tft = tft;
      _m5 = m5;
      
      _currentMap = _previousMap = nullptr;

      _cleanMapAndFeaturesSprite.reset(new TFT_eSprite(_tft));
      _compositedScreenSprite.reset(new TFT_eSprite(_tft));
      _diverSprite.reset(new TFT_eSprite(_tft));
      _diverPlainSprite.reset(new TFT_eSprite(_tft));
      _diverRotatedSprite.reset(new TFT_eSprite(_tft));
      _featureSprite.reset(new TFT_eSprite(_tft));

      _targetSprite.reset(new TFT_eSprite(_tft));
      _lastTargetSprite.reset(new TFT_eSprite(_tft));

      initSprites();
      initFeatureToMapsLookup();
      initExitWaypoints();
    }

    void setTargetWaypointByLabel(const char* label)
    {
      _prevWaypoint = _targetWaypoint;
      _targetWaypoint = nullptr;
      // find targetWayPoint in the navigation_waypoints array by first 3 chars
      for (int i=0; i < waypointCount; i++)
      {
        if (strncmp(waypoints[i]._label, label, 3) == 0)
        {
          _targetWaypoint=waypoints+i;
          break;
        }
      }
    }

    ~MapScreen()
    {

    }
    
    void setUseDiverHeading(const bool use)
    {
      _useDiverHeading = use;
    }
    
    void initCurrentMap(const double diverLatitude, const double diverLongitude);
    void clearMap();
    void drawFeaturesOnSpecifiedMapToScreen(const geo_map* featureAreaToShow, int16_t zoom=1, int16_t tileX=0, int16_t tileY=0);

    void drawDiverOnBestFeaturesMapAtCurrentZoom(const double diverLatitude, const double diverLongitude, const double diverHeading = 0);
    
    void drawDiverOnCompositedMapSprite(const double latitude, const double longitude, const double heading, const geo_map* featureMap);

    double distanceBetween(double lat1, double long1, double lat2, double long2) const;
    double degreesCourseTo(double lat1, double long1, double lat2, double long2) const;
    double radiansCourseTo(double lat1, double long1, double lat2, double long2) const;


    navigationWaypoint* getClosestJetty(double& distance);

    int drawDirectionLineOnCompositeSprite(const double diverLatitude, const double diverLongitude, 
                                                    const geo_map* featureMap, navigationWaypoint* waypoint, uint16_t colour, int indicatorLength);

    void drawHeadingLineOnCompositeMapSprite(const double diverLatitude, const double diverLongitude, 
                                            const double heading, const geo_map* featureMap);

    void drawRegistrationPixelsOnCleanMapSprite(const geo_map* featureMap);

    void cycleZoom();
    
    void testAnimatingDiverSpriteOnCurrentMap();
    void testDrawingMapsAndFeatures(uint8_t& currentMap, int16_t& zoom);

  private:
    TFT_eSPI* _tft;
    M5StickCPlus* _m5;
    
    std::unique_ptr<TFT_eSprite> _cleanMapAndFeaturesSprite;
    std::unique_ptr<TFT_eSprite> _compositedScreenSprite;
    std::unique_ptr<TFT_eSprite> _diverSprite;
    std::unique_ptr<TFT_eSprite> _diverPlainSprite;
    std::unique_ptr<TFT_eSprite> _diverRotatedSprite;
    std::unique_ptr<TFT_eSprite> _featureSprite;
    std::unique_ptr<TFT_eSprite> _targetSprite;
    std::unique_ptr<TFT_eSprite> _lastTargetSprite;

    static const int16_t s_imgHeight = 240;
    static const int16_t s_imgWidth = 135;
    static const uint8_t s_diverSpriteRadius = 15;
    static const uint8_t s_featureSpriteRadius = 5;
    static const uint16_t s_diverSpriteColour = TFT_BLUE;
    static const uint16_t s_featureSpriteColour = TFT_MAGENTA;
    static const bool     s_useSpriteForFeatures = true;

    double _lastDiverLatitude;
    double _lastDiverLongitude;
    double _lastDiverHeading;

    bool _useDiverHeading;
    
    const geo_map *_currentMap, *_previousMap;

    bool _showAllLake;

    const geo_map* _northMap=s_maps;          const uint8_t _northMapIndex = 0;
    const geo_map* _cafeJettyMap=s_maps+1;   const uint8_t _cafeJettyMapIndex = 1;
    const geo_map* _midJettyMap=s_maps+2;    const uint8_t _midJettyMapIndex = 2;
    const geo_map* _southMap=s_maps+3;       const uint8_t _southMapIndex = 3;
    const geo_map* _allLakeMap=s_maps+4;     const uint8_t _allLakeMapIndex = 4;
    const geo_map* _canoeZoneMap=s_maps+5;   const uint8_t _canoeZoneMapIndex = 5;
    const geo_map* _subZoneMap=s_maps+6;     const uint8_t _subZoneMapIndex = 6;

    navigationWaypoint* _targetWaypoint;
    navigationWaypoint* _prevWaypoint;
    navigationWaypoint* _closestExitWaypoint;
   
    int16_t _zoom;
    int16_t _priorToZoneZoom;
    int16_t _tileXToDisplay;
    int16_t _tileYToDisplay;
    
    void initSprites();
    void initFeatureToMapsLookup();
    void initExitWaypoints();
    void initMapsForFeature(navigationWaypoint* waypoint, geoRef& ref);

    void drawFeaturesOnCleanMapSprite(const geo_map* featureMap);
//    void drawFeaturesOnCleanMapSprite(const geo_map* featureMap,uint8_t zoom, uint8_t tileX, uint8_t tileY);
    pixel convertGeoToPixelDouble(double latitude, double longitude, const geo_map* mapToPlot) const;
    pixel convertGeoToPixelFloat(float latitude, float longitude, const geo_map* mapToPlot) const;
    
    const geo_map* getNextMapByPixelLocation(const MapScreen::pixel loc, const geo_map* thisMap);
    MapScreen::pixel scalePixelForZoomedInTile(const pixel p, int16_t& tileX, int16_t& tileY) const;

    bool isPixelInCanoeZone(const MapScreen::pixel loc, const geo_map* thisMap) const;
    bool isPixelInSubZone(const MapScreen::pixel loc, const geo_map* thisMap) const;
    bool isPixelOutsideScreenExtent(const MapScreen::pixel loc) const;

    void debugPixelMapOutput(const MapScreen::pixel loc, const geo_map* thisMap, const geo_map* nextMap) const;
    void debugPixelFeatureOutput(navigationWaypoint* waypoint, MapScreen::pixel loc, const geo_map* thisMap) const;
    void debugScaledPixelForTile(pixel p, pixel pScaled, int16_t tileX,int16_t tileY) const;
};


const int16_t mX = 135,  hX = 67;
const int16_t mY = 240,  hY = 120;
const int16_t o = 10;

const int registrationPixelsSize=16;
const MapScreen::pixel registrationPixels[registrationPixelsSize] =
{
  [0] = { .x = o,    .y = o,    .colour = 0xFF00},
  [1] = { .x = hX-o, .y = o,    .colour = 0xFF00},
  [2] = { .x = o,    .y = hY-o, .colour = 0xFF00},
  [3] = { .x = hX-o, .y = hY-o, .colour = 0xFF00},
  
  [4] = { .x = hX+o, .y = o,    .colour = 0xFFFF},
  [5] = { .x = mX-o, .y = o,    .colour = 0xFFFF},
  [6] = { .x = hX+o, .y = hY-o, .colour = 0xFFFF},
  [7] = { .x = mX-o, .y = hY-o, .colour = 0xFFFF},

  [8]  = { .x = o,    .y = hY+o, .colour = 0x00FF},
  [9]  = { .x = hX-o, .y = hY+o, .colour = 0x00FF},
  [10] = { .x = o,    .y = mY-o, .colour = 0x00FF},
  [11] = { .x = hX-o, .y = mY-o, .colour = 0x00FF},

  [12] = { .x = hX+o, .y = hY+o, .colour = 0x0000},
  [13] = { .x = mX-o, .y = hY+o, .colour = 0x0000},
  [14] = { .x = hX+o, .y = mY-o, .colour = 0x0000},
  [15] = { .x = mX-o, .y = mY-o, .colour = 0x0000},
};

const MapScreen::pixel mapOffsets[16]=
{
  [0]  = { .x=0, .y=0 },   // map 0 to 0
  [1]  = { .x=30, .y=116 },   // map 0 to 1
  [2]  = { .x=144, .y=171 },   // map 0 to 2
  [3]  = { .x=186, .y=219 },   // map 0 to 3
     
  [4]  = { .x=-30,.y=-116 }, // map 1 to 0
  [5]  = { .x=0, .y=0},   // map 1 to 1
  [6]  = { .x=114, .y=55 },   // map 1 to 2
  [7]  = { .x=156, .y=103 },   // map 1 to 3

  [8]  = { .x=-144, .y=-171},   // map 2 to 0
  [9]  = { .x=-114, .y=-55 },   // map 2 to 1
  [10] = { .x=0, .y=0 },   // map 2 to 2
  [11] = { .x=42, .y=48 },   // map 2 to 3

  [12] = { .x=-186, .y=-219 },   // map 3 to 0
  [13] = { .x=-156, .y=-103 },   // map 3 to 1
  [14] = { .x=-42, .y=-48 },   // map 3 to 2
  [15] = { .x=0, .y=0 },   // map 3 to 3
};
    
void MapScreen::initSprites()
{
  _cleanMapAndFeaturesSprite->setColorDepth(16);
  _cleanMapAndFeaturesSprite->createSprite(135,240);

  _compositedScreenSprite->setColorDepth(16);
  _compositedScreenSprite->createSprite(135,240);

   uint16_t s_headingIndicatorColour=TFT_RED;
   uint16_t s_headingIndicatorRadius=8;
   uint16_t s_headingIndicatorOffsetX=s_diverSpriteRadius;
   uint16_t s_headingIndicatorOffsetY=0;

  _diverSprite->setColorDepth(16);
  _diverSprite->createSprite(s_diverSpriteRadius*2,s_diverSpriteRadius*2);
  _diverSprite->fillCircle(s_diverSpriteRadius,s_diverSpriteRadius,s_diverSpriteRadius,s_diverSpriteColour);
  
  _diverPlainSprite->setColorDepth(16);
  _diverPlainSprite->createSprite(s_diverSpriteRadius*2,s_diverSpriteRadius*2);
  _diverSprite->pushToSprite(_diverPlainSprite.get(),0,0);

  _diverSprite->fillCircle(s_headingIndicatorOffsetX,s_headingIndicatorOffsetY,s_headingIndicatorRadius,s_headingIndicatorColour);


  _diverRotatedSprite->setColorDepth(16);
  _diverRotatedSprite->createSprite(s_diverSpriteRadius*2,s_diverSpriteRadius*2);  
  
  _featureSprite->setColorDepth(16);
  _featureSprite->createSprite(s_featureSpriteRadius*2+1,s_featureSpriteRadius*2+1);
  _featureSprite->fillCircle(s_featureSpriteRadius,s_featureSpriteRadius,s_featureSpriteRadius,s_featureSpriteColour);

  _targetSprite->setColorDepth(16);
  _targetSprite->createSprite(s_featureSpriteRadius*2+1,s_featureSpriteRadius*2+1);
  _targetSprite->fillCircle(s_featureSpriteRadius,s_featureSpriteRadius,s_featureSpriteRadius,TFT_RED);

  _lastTargetSprite->setColorDepth(16);
  _lastTargetSprite->createSprite(s_featureSpriteRadius*2+1,s_featureSpriteRadius*2+1);
  _lastTargetSprite->fillCircle(s_featureSpriteRadius,s_featureSpriteRadius,s_featureSpriteRadius,TFT_BLUE);
}

void MapScreen::initFeatureToMapsLookup()
{
  for (int i=0; i<waypointCount; i++)
  {
    initMapsForFeature(waypoints+i,s_featureToMaps[i]);
  }
}

void MapScreen::initMapsForFeature(navigationWaypoint* waypoint, geoRef& ref)
{
  int refIndex = 0;
  
  pixel p;
  
  for (uint8_t i=_northMapIndex; i<_allLakeMapIndex; i++)
  {
    p = convertGeoToPixelDouble(waypoint->_lat, waypoint->_long, s_maps+i);
    if (p.x >= 0 && p.x < s_imgWidth && p.y >=0 && p.y < s_imgHeight)
    {
      ref.geoMaps[refIndex++] = i;
    }
    else
    {
      ref.geoMaps[refIndex++] = -1;
    }
  }
}


void MapScreen::initExitWaypoints()
{
  int currentExitIndex=0;
  
  for (int i=0; i<waypointCount; i++)
  {
    if (strncmp(waypoints[i]._label, "Z0", 2) == 0)
    {
      exitWaypoints[currentExitIndex++] = waypoints+i;
      if (currentExitIndex == exitWaypointCount - 1)
      {
        exitWaypoints[currentExitIndex] = nullptr;
        break;
      }
    }
  }
}

void MapScreen::initCurrentMap(const double diverLatitude, const double diverLongitude)
{  
  _currentMap = _previousMap = _allLakeMap;

  pixel p;
  
  // identify first map that includes diver location within extent
  for (uint8_t i=_northMapIndex; i<=_allLakeMapIndex; i++)
  {
    p = convertGeoToPixelDouble(diverLatitude, diverLongitude, s_maps+i);
    if (p.x >= 0 && p.x < s_imgWidth && p.y >=0 && p.y < s_imgHeight)
    {
      scalePixelForZoomedInTile(p,_tileXToDisplay, _tileYToDisplay);
      _currentMap = s_maps+i;
      break;
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

void MapScreen::clearMap()
{
  _currentMap = _previousMap = nullptr;
  _priorToZoneZoom = _zoom = 1;
  _tileXToDisplay = _tileXToDisplay = 0;
  _m5->Lcd.fillScreen(TFT_BLACK);
}

void MapScreen::cycleZoom()
{            
  if (_showAllLake)
  {
    _showAllLake = false;
    _zoom = 1;
    _previousMap=_allLakeMap; 
    _currentMap=nullptr;
    Serial.println("switch to zoom 1 normal map\n");
  }
  else if (!_showAllLake && _zoom == 4)
  {
    _showAllLake = true;
    _zoom = 1;
    _currentMap = _allLakeMap;
    Serial.println("switch to zoom 4 normal map\n");
  }
  else if (!_showAllLake && _zoom == 3)
  {
    _showAllLake = false;
    _zoom = 4;
    Serial.println("switch to zoom 3 normal map\n");
  }
  else if (!_showAllLake && _zoom == 2)
  {
    _showAllLake = false;
    _zoom = 3;
    Serial.println("switch to zoom 2 normal map\n");
  }
  else if (!_showAllLake && _zoom == 1)
  {
    _showAllLake = false;
    _zoom = 2;
    Serial.println("switch to zoom 2 normal map\n");
  }
  }

/* Requirements:
 *  
 *  DONE - survey maps are checked for presence before non-survey maps.
 *  DONE - survey maps show all features within map extent, plus diver, without last visited feature shown.
 *  DONE - survey maps only switch to last zoom non-survey map when out of area.
 *  DONE - at zoom level 1 and 2, base map gets switched once the selectMap function has detected diver moved to edge as defined by current map.
 *  DONE 0 at zoom level 2 switch between tiles within a base map is done at a tile boundary.
 *  DONE - for non-survey maps, last feature and next feature are shown in different colours.
 *  DONE - Heading indicator in blue
 *  DONE - Direction Line in red to next feature spanning maps and tiles at any zoom
 *  TODO - Green Line pointing to nearest exit Cafe or Mid Jetty
 *  TODO - Diver sprite flashes blue/green.
 */

navigationWaypoint* MapScreen::getClosestJetty(double& shortestDistance)
{
  shortestDistance = 1e10;
  navigationWaypoint* closest = nullptr;

  navigationWaypoint** next = exitWaypoints;
  
  while (*next)
  {
    double distance = distanceBetween(_lastDiverLatitude, _lastDiverLongitude, (*next)->_lat, (*next)->_long);
  
    if (distance < shortestDistance)
    {
      shortestDistance =  distance;
      _closestExitWaypoint = *next;
    }
    next++;
  }
    
  return _closestExitWaypoint;
}

void MapScreen::drawDiverOnBestFeaturesMapAtCurrentZoom(const double diverLatitude, const double diverLongitude, const double diverHeading)
{
    _lastDiverLatitude = diverLatitude;
    _lastDiverLongitude = diverLongitude;
    _lastDiverHeading = diverHeading;
    
    if (_currentMap == nullptr)
    {
      initCurrentMap(diverLatitude, diverLongitude);
    }
    
    pixel p = convertGeoToPixelDouble(diverLatitude, diverLongitude, _currentMap);
    const geo_map* nextMap = getNextMapByPixelLocation(p, _currentMap);

    if (nextMap != _currentMap)
    {
       p = convertGeoToPixelDouble(diverLatitude, diverLongitude, nextMap);
      _previousMap = _currentMap;
    }

    p = scalePixelForZoomedInTile(p,_tileXToDisplay,_tileYToDisplay);

    // draw diver and feature map at pixel
    if (nextMap->mapData)
    {
      
      _cleanMapAndFeaturesSprite->pushImageScaled(0, 0, s_imgWidth, s_imgHeight, _zoom, _tileXToDisplay, _tileYToDisplay, 
                                                  nextMap->mapData, nextMap->swapBytes);
      drawFeaturesOnCleanMapSprite(nextMap);
//      drawRegistrationPixelsOnCleanMapSprite(nextMap);
    }
    else
    {
      _cleanMapAndFeaturesSprite->fillSprite(nextMap->backColour);
      
      drawFeaturesOnCleanMapSprite(nextMap);  // need to revert zoom to 1
    }
    
    _cleanMapAndFeaturesSprite->pushToSprite(_compositedScreenSprite.get(),0,0);

    double distanceToClosestJetty = 0.0;
    double bearing = 0.0;
    bearing = drawDirectionLineOnCompositeSprite(diverLatitude, diverLongitude, nextMap,getClosestJetty(distanceToClosestJetty), TFT_DARKGREEN, 70);

    bearing = drawDirectionLineOnCompositeSprite(diverLatitude, diverLongitude, nextMap,_targetWaypoint, TFT_RED, 100);
    
    drawHeadingLineOnCompositeMapSprite(diverLatitude, diverLongitude, diverHeading, nextMap); // was diverHeading
        
    drawDiverOnCompositedMapSprite(diverLatitude, diverLongitude, diverHeading, nextMap);

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

const geo_map* MapScreen::getNextMapByPixelLocation(const MapScreen::pixel loc, const geo_map* thisMap)
{
  const geo_map* nextMap = thisMap;

  if (thisMap == _allLakeMap)
    return _allLakeMap;
    
  if ((thisMap == _canoeZoneMap || thisMap == _subZoneMap) && isPixelOutsideScreenExtent(loc))
  {
    nextMap = (thisMap == _canoeZoneMap ? _northMap : _cafeJettyMap);   // go back to previous map?
    _zoom = _priorToZoneZoom;
  }
  else if (thisMap == _northMap)   // go right from 0 to 1
  {
    if (isPixelInCanoeZone(loc, thisMap))
    {
      _priorToZoneZoom=_zoom;
      _zoom = 1;
      nextMap = _canoeZoneMap;
    }
    else if (isPixelInSubZone(loc, thisMap))
    {
      _priorToZoneZoom=_zoom;
      _zoom = 1;
      nextMap = _subZoneMap;
    }
    else if ((loc.x >= 116 && loc.y >= 118) || 
           loc.x >= 30 && loc.y >= 215)
    {
      nextMap=_cafeJettyMap;
    }
  }
  else if (thisMap == _cafeJettyMap)
  { 
    if (isPixelInSubZone(loc, thisMap))
    {
      _priorToZoneZoom=_zoom;
      _zoom = 1;
      nextMap = _subZoneMap;                  // reset zoom to 1
    }
    else if (loc.x >= 125 && loc.y >= 55 || 
        loc.x >= 135)      // go right from 1 to 2
    {
      nextMap=_midJettyMap;
    }
    else if (loc.x <=4 && loc.y <= 122 || 
            loc.x <= 83 && loc.y <= 30 )  // go left from 1 to 0
    {
      nextMap=_northMap;
    }
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

MapScreen::pixel MapScreen::scalePixelForZoomedInTile(const pixel p, int16_t& tileX, int16_t& tileY) const
{
  tileX = p.x / (s_imgWidth / _zoom);
  tileY = p.y / (s_imgHeight / _zoom);

  pixel pScaled;
  if (tileX < _zoom && tileY < _zoom)
  {
    pScaled.x = p.x * _zoom - (s_imgWidth)  * tileX;
    pScaled.y = p.y * _zoom - (s_imgHeight) * tileY;
  }
  else
  {
    pScaled.x = p.x * _zoom;
    pScaled.y = p.y * _zoom;
    tileX = tileY = 0;
  }

  pScaled.colour = p.colour;

//  debugScaledPixelForTile(p, pScaled, tileX,tileY);

  return pScaled;
}

double MapScreen::distanceBetween(double lat1, double long1, double lat2, double long2) const
{
  // returns distance in meters between two positions, both specified
  // as signed decimal-degrees latitude and longitude. Uses great-circle
  // distance computation for hypothetical sphere of radius 6372795 meters.
  // Because Earth is no exact sphere, rounding errors may be up to 0.5%.
  // Courtesy of Maarten Lamers
  double delta = radians(long1-long2);
  double sdlong = sin(delta);
  double cdlong = cos(delta);
  lat1 = radians(lat1);
  lat2 = radians(lat2);
  double slat1 = sin(lat1);
  double clat1 = cos(lat1);
  double slat2 = sin(lat2);
  double clat2 = cos(lat2);
  delta = (clat1 * slat2) - (slat1 * clat2 * cdlong);
  delta = sq(delta);
  delta += sq(clat2 * sdlong);
  delta = sqrt(delta);
  double denom = (slat1 * slat2) + (clat1 * clat2 * cdlong);
  delta = atan2(delta, denom);
  return delta * 6372795.0;
}

double MapScreen::degreesCourseTo(double lat1, double long1, double lat2, double long2) const
{
  return radiansCourseTo( lat1,  long1,  lat2,  long2) / PI * 180;
}

double MapScreen::radiansCourseTo(double lat1, double long1, double lat2, double long2) const
{
  // returns course in degrees (North=0, West=270) from position 1 to position 2,
  // both specified as signed decimal-degrees latitude and longitude.
  // Because Earth is no exact sphere, calculated course may be off by a tiny fraction.
  // Courtesy of Maarten Lamers
  double dlon = radians(long2-long1);
  lat1 = radians(lat1);
  lat2 = radians(lat2);
  double a1 = sin(dlon) * cos(lat2);
  double a2 = sin(lat1) * cos(lat2) * cos(dlon);
  a2 = cos(lat1) * sin(lat2) - a2;
  a2 = atan2(a1, a2);
  if (a2 < 0.0)
  {
    a2 += TWO_PI;
  }
  return a2;
}


int MapScreen::drawDirectionLineOnCompositeSprite(const double diverLatitude, const double diverLongitude, 
                                                            const geo_map* featureMap, navigationWaypoint* waypoint, uint16_t colour, int indicatorLength)
{
  // 1. WORKS CORRECTLY IF TARGET ON SAME MAP and TILE AS DIVER, at zoom 1 or zoom 2.
  // 2. If at zoom 1 and on different map, direction line angle is not right as it does not account for x and y offsets across maps.
  // 3. At zoom 2 the issues arising from (2) have twice an impact in incorrect locations.
  int heading = 0;
  
  if (waypoint)
  {
    pixel pDiver = convertGeoToPixelDouble(diverLatitude, diverLongitude, featureMap);
    int16_t diverTileX=0,diverTileY=0;
    pDiver = scalePixelForZoomedInTile(pDiver,diverTileX,diverTileY);

    int16_t targetTileX=0,targetTileY=0;
    pixel pTarget = convertGeoToPixelDouble(waypoint->_lat, waypoint->_long, featureMap);

    
    /*
    int waypointIndex = _targetWaypoint-waypoints;
    // 1. get the maps for target way point
    geoRef* mapsForTarget = s_featureToMaps+waypointIndex;

    // 2. choose the single map of interest to calculate target direction line

    int diverMapIndex = featureMap - s_maps;
// START OPTION 1 //////////////////////
    int targetMapIndex = -1;
    
    // check for current diver map
    if (mapsForTarget->geoMaps[diverMapIndex] != -1)
      targetMapIndex = diverMapIndex;
    else
    {
      int t = diverMapIndex;
      
      if (t == 0)
      {
        // search for first map with feature to the right   
        if (mapsForTarget->geoMaps[t+1] != -1)
          targetMapIndex=t+1;
        else if (mapsForTarget->geoMaps[t+2] != -1)
          targetMapIndex=t+2;
        else if (mapsForTarget->geoMaps[t+3] != -1)
          targetMapIndex=t+3;
      }
      else if (t == 1)
      {
        if (mapsForTarget->geoMaps[t-1] != -1)
          targetMapIndex=t-1;
        else if (mapsForTarget->geoMaps[t+1] != -1)
          targetMapIndex=t+1;
        else if (mapsForTarget->geoMaps[t+2] != -1)
          targetMapIndex=t+2;
      }
      else if (t == 2)
      {
        if (mapsForTarget->geoMaps[t-1] != -1)
          targetMapIndex=t-1;
        else if (mapsForTarget->geoMaps[t+1] != -1)
          targetMapIndex=t+1;
        else if (mapsForTarget->geoMaps[t-2] != -1)
          targetMapIndex=t-2;
      }
      else if (t == 3)
      {
        if (mapsForTarget->geoMaps[t-1] != -1)
          targetMapIndex=t-1;
        else if (mapsForTarget->geoMaps[t-2] != -1)
          targetMapIndex=t-2;
        else if (mapsForTarget->geoMaps[t-3] != -1)
          targetMapIndex=t-3;
      }
    }

    if (targetMapIndex == -1)
      return 0.0;
    // 4. get the x/y translation between the two maps
    pixel translation = mapOffsets[diverMapIndex*4 + targetMapIndex];

    // 5. translate target pixel location 
    pTarget.x += translation.x;
    pTarget.y += translation.y;
    
//    pTarget= scalePixelForZoomedInTile(pTarget,targetTileX,targetTileY);  // that gives pixel for the appropriate tile, but we need it relative to the entire image.

    pTarget.x = pTarget.x * _zoom - s_imgWidth * diverTileX;     // t.x = 10; tileX=0; tileX = 1;
    pTarget.y = pTarget.y * _zoom - s_imgHeight * diverTileY;   /// there was a bug here

//    geo_map* targetMap = nullptr;//s_mapForFeatures[_targetWaypoint-waypoints];   // look up the map that the // features reside on multiple maps

//    pTarget.x = pTarget.x + mapOffsets[featureMap-s_maps][targetMap-s_maps];
//    pTarget.y = pTarget.y + mapOffsets[featureMap-s_maps][targetMap-s_maps];
      
    _compositedScreenSprite->drawLine(pDiver.x, pDiver.y, pTarget.x,pTarget.y,TFT_RED);    
      
    _compositedScreenSprite->drawLine(pDiver.x-2, pDiver.y-2, pTarget.x,pTarget.y,TFT_RED);
    _compositedScreenSprite->drawLine(pDiver.x-2, pDiver.y+2, pTarget.x,pTarget.y,TFT_RED);
    _compositedScreenSprite->drawLine(pDiver.x+2, pDiver.y-2, pTarget.x,pTarget.y,TFT_RED);
    _compositedScreenSprite->drawLine(pDiver.x+2, pDiver.y+2, pTarget.x,pTarget.y,TFT_RED);


    if (pTarget.y < pDiver.y)
      return (int)(atan((double)(pTarget.x - pDiver.x) / (double)(-(pTarget.y - pDiver.y))) * 180.0 / PI) % 360;
    else if (pTarget.y > pDiver.y)
      return (int)(180.0 + atan((double)(pTarget.x - pDiver.x) / (double)(-(pTarget.y - pDiver.y))) * 180.0 / PI);
    else
      return 0;
  }
  
  return 0;
}
// END OPTION 1 //////////////////////
*/

// START OPTION 2 //////////////////////

    if (!isPixelOutsideScreenExtent(convertGeoToPixelDouble(waypoint->_lat, waypoint->_long, featureMap)))
    {
      // use line between diver and target locations
      pTarget.x = pTarget.x * _zoom - s_imgWidth * diverTileX;
      pTarget.y = pTarget.y * _zoom - s_imgHeight * diverTileY;

      _compositedScreenSprite->drawLine(pDiver.x, pDiver.y, pTarget.x,pTarget.y,colour);
  
      _compositedScreenSprite->drawLine(pDiver.x-2, pDiver.y-2, pTarget.x,pTarget.y,colour);
      _compositedScreenSprite->drawLine(pDiver.x-2, pDiver.y+2, pTarget.x,pTarget.y,colour);
      _compositedScreenSprite->drawLine(pDiver.x+2, pDiver.y-2, pTarget.x,pTarget.y,colour);
      _compositedScreenSprite->drawLine(pDiver.x+2, pDiver.y+2, pTarget.x,pTarget.y,colour);

      if (pTarget.y < pDiver.y)
        heading = (int)(atan((double)(pTarget.x - pDiver.x) / (double)(-(pTarget.y - pDiver.y))) * 180.0 / PI) % 360;
      else if (pTarget.y > pDiver.y)
        heading = (int)(180.0 + atan((double)(pTarget.x - pDiver.x) / (double)(-(pTarget.y - pDiver.y))) * 180.0 / PI);
    }
    else
    {
      heading = degreesCourseTo(diverLatitude,diverLongitude,waypoint->_lat,waypoint->_long);
  
      // use lat/long to draw outside map area with arbitrary length.
      pixel pHeading;
    
      double rads = heading * PI / 180.0;  
      pHeading.x = pDiver.x + indicatorLength * sin(rads);
      pHeading.y = pDiver.y - indicatorLength * cos(rads);

      _compositedScreenSprite->drawLine(pDiver.x, pDiver.y, pHeading.x,pHeading.y,colour);
    
      _compositedScreenSprite->drawLine(pDiver.x-2, pDiver.y-2, pHeading.x,pHeading.y,colour);
      _compositedScreenSprite->drawLine(pDiver.x-2, pDiver.y+2, pHeading.x,pHeading.y,colour);
      _compositedScreenSprite->drawLine(pDiver.x+2, pDiver.y-2, pHeading.x,pHeading.y,colour);
      _compositedScreenSprite->drawLine(pDiver.x+2, pDiver.y+2, pHeading.x,pHeading.y,colour);
    }
  }
  return heading;
// END OPTION 2 //////////////////////
}

void MapScreen::drawHeadingLineOnCompositeMapSprite(const double diverLatitude, const double diverLongitude, 
                                                            const double heading, const geo_map* featureMap)
{
  int16_t tileX=0,tileY=0;
  pixel pDiver = convertGeoToPixelDouble(diverLatitude, diverLongitude, featureMap);
  pDiver = scalePixelForZoomedInTile(pDiver,tileX,tileY);
  
  const double hypotoneuse=50;
  pixel pHeading;

  double rads = heading * PI / 180.0;  
  pHeading.x = pDiver.x + hypotoneuse * sin(rads);
  pHeading.y = pDiver.y - hypotoneuse * cos(rads);

  _compositedScreenSprite->drawLine(pDiver.x, pDiver.y, pHeading.x,pHeading.y,TFT_BLUE);

  _compositedScreenSprite->drawLine(pDiver.x-2, pDiver.y-2, pHeading.x,pHeading.y,TFT_BLUE);
  _compositedScreenSprite->drawLine(pDiver.x-2, pDiver.y+2, pHeading.x,pHeading.y,TFT_BLUE);
  _compositedScreenSprite->drawLine(pDiver.x+2, pDiver.y-2, pHeading.x,pHeading.y,TFT_BLUE);
  _compositedScreenSprite->drawLine(pDiver.x+2, pDiver.y+2, pHeading.x,pHeading.y,TFT_BLUE);
}

void MapScreen::drawDiverOnCompositedMapSprite(const double latitude, const double longitude, const double heading, const geo_map* featureMap)
{
    pixel pDiver = convertGeoToPixelDouble(latitude, longitude, featureMap);

    int16_t diverTileX=0, diverTileY=0;
    pDiver = scalePixelForZoomedInTile(pDiver, diverTileX, diverTileY);

    if (_targetWaypoint)
    {
      pixel p = convertGeoToPixelDouble(_targetWaypoint->_lat, _targetWaypoint->_long, featureMap);
      int16_t tileX=0,tileY=0;
      p = scalePixelForZoomedInTile(p,tileX,tileY);
  
      if (tileX == diverTileX && tileY == diverTileY)  // only show target sprite on screen if tiles match
        _targetSprite->pushToSprite(_compositedScreenSprite.get(), p.x-s_featureSpriteRadius,p.y-s_featureSpriteRadius,TFT_BLACK);
     }

    if (_prevWaypoint)
    {
      pixel p = convertGeoToPixelDouble(_prevWaypoint->_lat, _prevWaypoint->_long, featureMap);
      int16_t tileX=0,tileY=0;
      p = scalePixelForZoomedInTile(p,tileX,tileY);
      if (tileX == diverTileX && tileY == diverTileY)  // only show last target sprite on screen if tiles match
        _lastTargetSprite->pushToSprite(_compositedScreenSprite.get(), p.x-s_featureSpriteRadius,p.y-s_featureSpriteRadius,TFT_BLACK);
    }

    // draw direction line to next target.
    if (_useDiverHeading)
    {
      _diverSprite->pushRotated(_diverRotatedSprite.get(),heading,TFT_BLACK); // BLACK is the transparent colour
      _diverRotatedSprite->pushToSprite(_compositedScreenSprite.get(),pDiver.x-s_diverSpriteRadius,pDiver.y-s_diverSpriteRadius,TFT_BLACK); // BLACK is the transparent colour
    }
    else
    {
      _diverPlainSprite->pushToSprite(_compositedScreenSprite.get(),pDiver.x-s_diverSpriteRadius,pDiver.y-s_diverSpriteRadius,TFT_BLACK); // BLACK is the transparent colour
    }
}
/*
void MapScreen::drawFeaturesOnCleanMapSprite(const geo_map* featureMap)
{
  drawFeaturesOnCleanMapSprite(featureMap,_zoom, _tileX, _tileY);
}
*/

void MapScreen::drawRegistrationPixelsOnCleanMapSprite(const geo_map* featureMap)
{
  for(int i=0;i<registrationPixelsSize;i++)
  {
    pixel p = registrationPixels[i];
  
    int16_t tileX=0,tileY=0;
    p = scalePixelForZoomedInTile(p,tileX,tileY);
    if (tileX != _tileXToDisplay || tileY != _tileYToDisplay)
      continue;
  
  //    Serial.printf("%i,%i      s: %i,%i\n",p.x,p.y,sP.x,sP.y);
    if (p.x >= 0 && p.x < s_imgWidth && p.y >=0 && p.y < s_imgHeight)   // CHANGE these to take account of tile shown  
    {
      if (s_useSpriteForFeatures)
        _featureSprite->pushToSprite(_cleanMapAndFeaturesSprite.get(),p.x - s_featureSpriteRadius, p.y - s_featureSpriteRadius,TFT_BLACK);
      else
        _cleanMapAndFeaturesSprite->fillCircle(p.x,p.y,s_featureSpriteRadius,p.colour);
        
  //      debugPixelFeatureOutput(waypoints+i, p, featureMap);
    }
  }
}

void MapScreen::drawFeaturesOnCleanMapSprite(const geo_map* featureMap)
{
  for(int i=0;i<waypointCount;i++)
  {
    pixel p = convertGeoToPixelDouble(waypoints[i]._lat, waypoints[i]._long, featureMap);

    int16_t tileX=0,tileY=0;
    p = scalePixelForZoomedInTile(p,tileX,tileY);

//    Serial.printf("%i,%i      s: %i,%i\n",p.x,p.y,sP.x,sP.y);
    if (tileX == _tileXToDisplay && tileY == _tileYToDisplay && p.x >= 0 && p.x < s_imgWidth && p.y >=0 && p.y < s_imgHeight)   // CHANGE these to take account of tile shown  
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

MapScreen::pixel MapScreen::convertGeoToPixelDouble(double latitude, double longitude, const geo_map* mapToPlot) const
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


void MapScreen::debugScaledPixelForTile(pixel p, pixel pScaled, int16_t tileX,int16_t tileY) const
{
  Serial.printf("dspt x=%i y=%i --> x=%i y=%i  tx=%i ty=%i\n",p.x,p.y,pScaled.x,pScaled.y,tileX,tileY);
}

void MapScreen::debugPixelMapOutput(const MapScreen::pixel loc, const geo_map* thisMap, const geo_map* nextMap) const
{
  Serial.printf("dpmo %s %i, %i --> %s\n",thisMap->label,loc.x,loc.y,nextMap->label);
}

void MapScreen::debugPixelFeatureOutput(navigationWaypoint* waypoint, MapScreen::pixel loc, const geo_map* thisMap) const
{
  Serial.printf("dpfo x=%i y=%i %s %s \n",loc.x,loc.y,thisMap->label,waypoint->_label);
}

///////////////
///////////////
///////////////
///////////////
///////////////
///////////////
///////////////
///////////////
///////////////
///////////////
///////////////
///////////////
///////////////
///////////////














MapScreen::pixel MapScreen::convertGeoToPixelFloat(float latitude, float longitude, const geo_map* mapToPlot) const
{
  const int16_t mapWidth = s_imgWidth; // in pixels
  const int16_t mapHeight = s_imgHeight; // in pixels
  float mapLngLeft = mapToPlot->mapLongitudeLeft; // in degrees. the longitude of the left side of the map (i.e. the longitude of whatever is depicted on the left-most part of the map image)
  float mapLngRight = mapToPlot->mapLongitudeRight; // in degrees. the longitude of the right side of the map
  float  mapLatBottom = mapToPlot->mapLatitudeBottom; // in degrees.  the latitude of the bottom of the map

  float  mapLatBottomRad = mapLatBottom * PI / 180.0;
  float  latitudeRad = latitude * PI / 180.0;
  float  mapLngDelta = (mapLngRight - mapLngLeft);

  float  worldMapWidth = ((mapWidth / mapLngDelta) * 360.0) / (2.0 * PI);
  float  mapOffsetY = (worldMapWidth / 2.0 * log((1.0 + sin(mapLatBottomRad)) / (1.0 - sin(mapLatBottomRad))));

  int16_t x = (longitude - mapLngLeft) * ((float)mapWidth / mapLngDelta);
  int16_t y = (float)mapHeight - ((worldMapWidth / 2.0 * log((1.0 + sin(latitudeRad)) / (1.0 - sin(latitudeRad)))) - (double)mapOffsetY);

  return pixel(x,y);
}

void MapScreen::drawFeaturesOnSpecifiedMapToScreen(const geo_map* featureAreaToShow, int16_t zoom, int16_t tileX, int16_t tileY)
{
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
    
//    drawFeaturesOnCleanMapSprite(featureAreaToShow,_zoom,_tileX,_tileY);

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
    
    pixel p = convertGeoToPixelDouble(latitude, longitude, featureAreaToShow);
    _diverSprite->pushToSprite(_compositedScreenSprite.get(),p.x-s_diverSpriteRadius,p.y-s_diverSpriteRadius,TFT_BLACK); // BLACK is the transparent colour

    _compositedScreenSprite->pushSprite(0,0);

    latitude+=0.0001;
    longitude+=0.0001;
  }
}

void MapScreen::testDrawingMapsAndFeatures(uint8_t& currentMap, int16_t& zoom)
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

//        drawFeaturesOnCleanMapSprite(featureAreaToShow, zoom, tileX, tileY);
    
        double latitude = featureAreaToShow->mapLatitudeBottom;
        double longitude = featureAreaToShow->mapLongitudeLeft;
      
        for(int i=0;i<20;i++)
        {
          _cleanMapAndFeaturesSprite->pushToSprite(_compositedScreenSprite.get(),0,0);
          
          pixel p = convertGeoToPixelDouble(latitude, longitude, featureAreaToShow);
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

//    drawFeaturesOnCleanMapSprite(featureAreaToShow,1,0,0);

    double latitude = featureAreaToShow->mapLatitudeBottom;
    double longitude = featureAreaToShow->mapLongitudeLeft;
  
    for(int i=0;i<25;i++)
    {
      _cleanMapAndFeaturesSprite->pushToSprite(_compositedScreenSprite.get(),0,0);
      
      pixel p = convertGeoToPixelDouble(latitude, longitude, featureAreaToShow);
      _diverSprite->pushToSprite(_compositedScreenSprite.get(),p.x-s_diverSpriteRadius,p.y-s_diverSpriteRadius,TFT_BLACK); // BLACK is the transparent colour

      _compositedScreenSprite->pushSprite(0,0);

      latitude+=0.0002;
      longitude+=0.0002;
    }
  } 
}
