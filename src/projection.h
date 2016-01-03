/* Copyright 2013 Bernhard R. Fischer, 2048R/5C5FFD47 <bf@abenteuerland.at>
 *
 * This file is part of Parsefsh.
 *
 * Smrender is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License.
 *
 * Parsefsh is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Parsefsh. If not, see <http://www.gnu.org/licenses/>.
 */

/*! This file contains the main() function, the output functions and the
 *  functions for the coordinate transformations.
 *
 *  @author Bernhard R. Fischer
 */

/*#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/types.h>
#include <errno.h>
*/

// ellipsoid parameters for WGS84. e is calculated by init_ellipsoid()
#define WGS84 {6378137, 6356752.3142, 0}
// maximum iterations to prevent from endless loops
#define MAX_IT 32
// iteration accuracy for reverse Mercator,
// approx 10cm in radians: 10cm / 100 / 1852 / 60 / 180 * M_PI
#define IT_ACCURACY 1.5E-8


// structure to keep ellipsoid data
typedef struct ellipsoid
{
   double a;   //!< semi-major axis in m (equatorial)
   double b;   //!< semi-minor axis in m (polar)
   double e;   //!< eccentricity (this is derived from a and b, call init_ellipsoid())
} ellipsoid_t;


// structure to keep latitude and longitude
struct coord
{
   double lat, lon;
};


// structure to keeping bearing and distance
struct pcoord
{
   double bearing, dist;
};


void init_ellipsoid(ellipsoid_t *);
double phi_iterate_merc(const ellipsoid_t *, double );
double northing(const ellipsoid_t *, double );
struct pcoord coord_diff(const struct coord *, const struct coord *);

