#ifndef lint
static char vcid[] = "$Id: da2.c,v 1.32 1996/01/27 04:56:41 bsmith Exp $";
#endif
 
/*
  Code for manipulating distributed regular arrays in parallel.
*/

#include "daimpl.h"    /*I   "da.h"   I*/

/*@
   DAGlobalToLocalBegin - Maps values from the global vector to the local
   patch, the ghost points are included. Must be followed by 
   DAGlobalToLocalEnd() to complete the exchange.

   Input Parameters:
.  da - the distributed array context
.  g - the global vector
.  mode - one of INSERT_VALUES or ADD_VALUES

   Output Parameter:
.  l  - the local values

.keywords: distributed array, global to local, begin

.seealso: DAGlobalToLocalEnd(), DALocalToGlobal(), DACreate2d()
@*/
int DAGlobalToLocalBegin(DA da,Vec g, InsertMode mode,Vec l)
{
  int ierr;
  PETSCVALIDHEADERSPECIFIC(da,DA_COOKIE);
  ierr = VecScatterBegin(g,l,mode,SCATTER_ALL,da->gtol); CHKERRQ(ierr);
  return 0;
}

/*@
   DAGlobalToLocalEnd - Maps values from the global vector to the local
   patch, the ghost points are included. Must be preceeded by 
   DAGlobalToLocalBegin().

   Input Parameters:
.  da - the distributed array context
.  g - the global vector
.  mode - one of INSERT_VALUES or ADD_VALUES

   Output Parameter:
.  l  - the local values

.keywords: distributed array, global to local, end

.seealso: DAGlobalToLocalBegin(), DALocalToGlobal(), DACreate2d()
@*/
int DAGlobalToLocalEnd(DA da,Vec g, InsertMode mode,Vec l)
{
  int ierr;
  PETSCVALIDHEADERSPECIFIC(da,DA_COOKIE);
  ierr = VecScatterEnd(g,l,mode,SCATTER_ALL,da->gtol); CHKERRQ(ierr);
  return 0;
}

