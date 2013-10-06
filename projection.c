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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/types.h>
#include <errno.h>

#include "projection.h"

#define DEGSCALE (M_PI / 180.0)
#define DEG2RAD(x) ((x) * DEGSCALE)
#define RAD2DEG(x) ((x) / DEGSCALE)
#define DEG2M(x) ((x) * 60 * 1852)



/*! This function derives the ellipsoid parameters from the semi-major and
 * semi-minor axis.
 * @param el A pointer to an ellipsoid structure. el->a and el->b MUST be
 * pre-initialized.
 */
void init_ellipsoid(ellipsoid_t *el)
{
   el->e = sqrt(1 - pow(el->b / el->a, 2));
}


/*! This function calculates the nearest geographic latitude to the reference
 * latitude phi0 from the Northing N based on the ellipsoid el. It must be
 * called iteratively to gain an appropriate accuracy.
 * @param el Pointer to the ellipsoid data.
 * @param N Northing according to the Mercator projection.
 * @param phi0 Reference latitude in radians. If phi0 = 0 (e.g. initially) a
 * spherical reverse Mercator is calculated.
 * @return Returns the latitude in radians.
 */
static double phi_rev_merc(const ellipsoid_t *el, double N, double phi0)
{
   double esin = el->e * sin(phi0);
   return M_PI_2 - 2.0 * atan(exp(-N / el->a) * pow((1 - esin) / (1 + esin), el->e / 2));
}


/*! This function derives the geographic latitude from the Mercator Northing N.
 * It iteratively calls phi_rev_merc(). At a maximum it iterates either MAX_IT
 * times or until the accuracy is less than IT_ACCURACY.
 * @param el Pointer to the ellipsoid data.
 * @param N Mercator Northing.
 * @return Returns the latitude in radians.
 */
double phi_iterate_merc(const ellipsoid_t *el, double N)
{
   double phi, phi0;
   int i;

   for (phi = 0, phi0 = M_PI, i = 0; fabs(phi - phi0) > IT_ACCURACY && i < MAX_IT; i++)
   {
      phi0 = phi;
      phi = phi_rev_merc(el, N, phi0);
   }

   return phi;
}


double northing(const ellipsoid_t *el, double lat)
{
   return el->a * log(tan(M_PI_4 + lat / 2) * pow((1 - el->e * sin(lat)) / (1 + el->e * sin(lat)), el->e / 2));
}


/*! Calculate bearing and distance from src to dst.
 *  @param src Source coodinates (struct coord).
 *  @param dst Destination coordinates (struct coord).
 *  @return Returns a struct pcoord. Pcoord contains the orthodrome distance in
 *  degrees and the bearing, 0 degress north, clockwise.
 */
struct pcoord coord_diff(const struct coord *src, const struct coord *dst)
{
   struct pcoord pc;
   double dlat, dlon;

   dlat = dst->lat - src->lat;
   dlon = (dst->lon - src->lon) * cos(DEG2RAD((src->lat + dst->lat) / 2.0));

   pc.bearing = RAD2DEG(atan2(dlon, dlat));
   pc.dist = RAD2DEG(acos(
      sin(DEG2RAD(src->lat)) * sin(DEG2RAD(dst->lat)) + 
      cos(DEG2RAD(src->lat)) * cos(DEG2RAD(dst->lat)) * cos(DEG2RAD(dst->lon - src->lon))));

   if (pc.bearing  < 0)
      pc.bearing += 360.0;

   return pc;
}


//#define TEST_PROJECTION
#ifdef TEST_PROJECTION

int main(int argc, char **argv)
{
   ellipsoid_t el = WGS84;
   int north = 6855295; //!< 52.48999447
   double lat;

   init_ellipsoid(&el);

   lat = phi_iterate_merc(&el, north);
   printf("northing = %d, latitude = %f\n", north, lat * 180 / M_PI);
   north = round(northing(&el, lat));
   printf("northing = %d, latitude = %f\n", north, lat * 180 / M_PI);

   lat = 54.06193333 / 180 * M_PI;
   north = round(northing(&el, lat));
   printf("northing = %d, latitude = %f\n", north, lat * 180 / M_PI);

   return 0;
}

#endif

