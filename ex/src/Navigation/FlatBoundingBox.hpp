#ifndef FLATBOUNDINGBOX_HPP
#define FLATBOUNDINGBOX_HPP

#include "FlatGeoPoint.hpp"
#include "FlatRay.hpp"
#include "TaskProjection.hpp"
#include "BoundingBoxDistance.hpp"
#include <algorithm>

class FlatBoundingBox {
public:
  FlatBoundingBox(const FLAT_GEOPOINT &ll,
                  const FLAT_GEOPOINT &ur):
    bb_ll(ll.Longitude,ll.Latitude),
    bb_ur(ur.Longitude,ur.Latitude) {};

  FlatBoundingBox(const FLAT_GEOPOINT &loc,
                  const unsigned range=0):
    bb_ll(loc.Longitude-range,loc.Latitude-range),
    bb_ur(loc.Longitude+range,loc.Latitude+range) 
  {

  }

  unsigned distance(const FlatBoundingBox &f) const;

  // used by KD
  struct kd_get_bounds {
    typedef int result_type;
    int operator() ( const FlatBoundingBox &d, const unsigned k) const {
      switch(k) {
      case 0:
        return d.bb_ll.Longitude;
      case 1:
        return d.bb_ll.Latitude;
      case 2:
        return d.bb_ur.Longitude;
      case 3:
        return d.bb_ur.Latitude;
      };
      return 0; 
    };
  };

  struct kd_distance {
    typedef BBDist distance_type;
    distance_type operator() (const int &a, const int &b, 
                              const size_t dim) const 
      {
        int val = 0;
        if (dim<2) {
          val= std::max(b-a,0);
        } else {
          val= std::max(a-b,0);
        }
        return BBDist(dim,val);
      }
  };

  bool intersects(const FlatRay& ray) const;

protected:
  FLAT_GEOPOINT bb_ll;
  FLAT_GEOPOINT bb_ur;
private:

  /** @link dependency */
  /*#  BBDist lnkBBDist; */
protected:
};

#endif
