#ifndef lint
static char vcid[] = "$Id: da2.c,v 1.32 1996/01/27 04:56:41 bsmith Exp $";
#endif
 
/*
  Code for manipulating distributed regular arrays in parallel.
*/

#include "daimpl.h"    /*I   "da.h"   I*/

/*@
   DAGetCorners - Returns the global (x,y,z) indices of the lower left
   corner of the local region, excluding ghost points.

   Input Parameter:
.  da - the distributed array

   Output Parameters:
.  x,y,z - the corner indices. y and z are optional.
.  m,n,p - widths in the corresponding directions. n and p are optional.

.keywords: distributed array, get, corners, nodes, local indices

.seealso: DAGetGhostCorners()
@*/
int DAGetCorners(DA da,int *x,int *y,int *z,int *m, int *n, int *p)
{
  int w;

  PETSCVALIDHEADERSPECIFIC(da,DA_COOKIE);
  /* since the xs, xe ... have all been multiplied by the number of degrees 
     of freedom per cell, w = da->w, we divide that out before returning.*/
  w = da->w;  
  *x = da->xs/w; *m = (da->xe - da->xs)/w;
  if (y) *y = da->ys/w; if (n) *n = (da->ye - da->ys)/w;
  if (z) *z = da->zs/w; if (p) *p = (da->ze - da->zs)/w; 
  return 0;
} 

/*@
    DAGetGhostCorners - Returns the global (x,y,z) indices of the lower left
    corner of the local region, including ghost points.

   Input Parameter:
.  da - the distributed array

   Output Parameters:
.  x,y,z - the corner indices. y and z are optional.
.  m,n,p - widths in the corresponding directions. n and p are optional.

.keywords: distributed array, get, ghost, corners, nodes, local indices

.seealso: DAGetCorners()
@*/
int DAGetGhostCorners(DA da,int *x,int *y,int *z,int *m, int *n, int *p)
{
  int w;

  PETSCVALIDHEADERSPECIFIC(da,DA_COOKIE);
  /* since the xs, xe ... have all been multiplied by the number of degrees 
     of freedom per cell, w = da->w, we divide that out before returning.*/
  w = da->w;  
  *x = da->Xs/w; *m = (da->Xe - da->Xs)/w;
  if (y) *y = da->Ys/w; if (n) *n = (da->Ye - da->Ys)/w;
  if (z) *z = da->Zs/w; if (p) *p = (da->Ze - da->Zs)/w; 
  return 0;
}

