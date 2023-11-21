#ifndef dive_track_h
#define dive_track_h

#include <string.h>

class location
{
  public:
    double _la;
    double _lo;
    float _d;
    float _h;
    char   _t[5];
    bool   _p;

    location() : _la(0.0), _lo(0.0), _d(0.0), _p(false)
    {
      _t[0] = '\0';
    }

    location(double lat, double lng, float depth, float heading, const char *target, bool pin)
      : _la(lat), _lo(lng), _d(depth), _h(heading),_p(pin)
    {
      strncpy(_t, target, sizeof(_t) - 1);
      _t[sizeof(_t)-1] = '\0';
    }

    location(double lat, double lng, float depth, float heading, bool pin)
      : _la(lat), _lo(lng), _d(depth), _h(heading), _p(pin)
    {
      _t[0]='\0';
    }

    location(const location& loc)
    {
      _la = loc._la;
      _lo = loc._la;
      _d = loc._d;
      _h = loc._h;
      _p = loc._p;
    //  strncpy(_t, loc._t, sizeof(_t) - 1);
      _t[sizeof(_t)-1] = '\0';
    }

    location& operator=(const location& loc)
    {
      if (this == &loc)
        return *this;

      _la = loc._la;
      _lo = loc._la;
      _d = loc._d;
      _h = loc._h;
      _p = loc._p;
      strncpy(_t, loc._t, sizeof(_t) - 1);
      _t[sizeof(_t)-1] = '\0';
    }

    bool isNull(const location& loc) const
    {
      return (_la == 0 && _lo == 0);
    }
};

extern const location diveTrack[];
extern size_t getSizeOfDiveTrack();
#endif
