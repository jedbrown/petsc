#ifndef lint
static char vcid[] = "$Id: index.c,v 1.37 1996/12/16 19:30:59 balay Exp balay $";
#endif
/*  
   Defines the abstract operations on index sets, i.e. the public interface. 
*/
#include "src/is/isimpl.h"      /*I "is.h" I*/

#undef __FUNCTION__  
#define __FUNCTION__ "ISIdentity"
/*@C
   ISIdentity - Determines whether index set is the identity mapping.

   Input Parmeters:
.  is - the index set

   Output Parameters:
.  ident - PETSC_TRUE if an identity, else PETSC_FALSE

.keywords: IS, index set, identity

.seealso: ISSetIdentity()
@*/
int ISIdentity(IS is,PetscTruth *ident)
{
  PetscValidHeaderSpecific(is,IS_COOKIE);
  PetscValidIntPointer(ident);
  *ident = (PetscTruth) is->isidentity;
  return 0;
}

#undef __FUNCTION__  
#define __FUNCTION__ "ISSetIdentity"
/*@
   ISSetIdentity - Informs the index set that it is an identity.

   Input Parmeters:
.  is - the index set

.keywords: IS, index set, identity

.seealso: ISIdentity()
@*/
int ISSetIdentity(IS is)
{
  PetscValidHeaderSpecific(is,IS_COOKIE);
  is->isidentity = 1;
  return 0;
}

#undef __FUNCTION__  
#define __FUNCTION__ "ISPermutation"
/*@C
   ISPermutation - PETSC_TRUE or PETSC_FALSE depending on whether the 
   index set has been declared to be a permutation.

   Input Parmeters:
.  is - the index set

   Output Parameters:
.  perm - PETSC_TRUE if a permutation, else PETSC_FALSE

.keywords: IS, index set, permutation

.seealso: ISSetPermutation()
@*/
int ISPermutation(IS is,PetscTruth *perm)
{
  PetscValidHeaderSpecific(is,IS_COOKIE);
  PetscValidIntPointer(perm);
  *perm = (PetscTruth) is->isperm;
  return 0;
}

#undef __FUNCTION__  
#define __FUNCTION__ "ISSetPermutation"
/*@
   ISSetPermutation - Informs the index set that it is a permutation.

   Input Parmeters:
.  is - the index set

.keywords: IS, index set, permutation

.seealso: ISPermutation()
@*/
int ISSetPermutation(IS is)
{
  PetscValidHeaderSpecific(is,IS_COOKIE);
  is->isperm = 1;
  return 0;
}

#undef __FUNCTION__  
#define __FUNCTION__ "ISDestroy"
/*@C
   ISDestroy - Destroys an index set.

   Input Parameters:
.  is - the index set

.keywords: IS, index set, destroy

.seealso: ISCreateSeq(), ISCreateStride()
@*/
int ISDestroy(IS is)
{
  PetscValidHeaderSpecific(is,IS_COOKIE);
  return (*is->destroy)((PetscObject) is);
}

#undef __FUNCTION__  
#define __FUNCTION__ "ISInvertPermutation"
/*@C
   ISInvertPermutation - Creates a new permutation that is the inverse of 
                         a given permutation.

   Input Parameter:
.  is - the index set

   Output Parameter:
.  isout - the inverse permutation

.keywords: IS, index set, invert, inverse, permutation
@*/
int ISInvertPermutation(IS is,IS *isout)
{
  PetscValidHeaderSpecific(is,IS_COOKIE);
  if (!is->isperm) SETERRQ(1,"not a permutation");
  return (*is->ops.invertpermutation)(is,isout);
}

#undef __FUNCTION__  
#define __FUNCTION__ "ISGetSize"
/*@
   ISGetSize - Returns the global length of an index set. 

   Input Parameter:
.  is - the index set

   Output Parameter:
.  size - the global size

.keywords: IS, index set, get, global, size

@*/
int ISGetSize(IS is,int *size)
{
  PetscValidHeaderSpecific(is,IS_COOKIE);
  PetscValidIntPointer(size);
  return (*is->ops.getsize)(is,size);
}

#undef __FUNCTION__  
#define __FUNCTION__ "ISGetIndices"
/*@C
   ISGetIndices - Returns a pointer to the indices.  The user should call 
   ISRestoreIndices() after having looked at the indices.  The user should 
   NOT change the indices.

   Input Parameter:
.  is - the index set

   Output Parameter:
.  ptr - the location to put the pointer to the indices


   Fortran Note:
   The Fortran interface is slightly different from that given below.
   See the Fortran chapter of the users manual and 
   petsc/src/is/examples for details.

.keywords: IS, index set, get, indices

.seealso: ISRestoreIndices()
@*/
int ISGetIndices(IS is,int **ptr)
{
  PetscValidHeaderSpecific(is,IS_COOKIE);
  PetscValidPointer(ptr);
  return (*is->ops.getindices)(is,ptr);
} 

#undef __FUNCTION__  
#define __FUNCTION__ "ISRestoreIndices"
/*@C
   ISRestoreIndices - Restores an index set to a usable state after a call 
                      to ISGetIndices().

   Input Parameters:
.  is - the index set
.  ptr - the pointer obtained by ISGetIndices()

   Fortran Note:
   The Fortran interface is slightly different from that given below.
   See the users manual and petsc/src/is/examples for details.

.keywords: IS, index set, restore, indices

.seealso: ISGetIndices()
@*/
int ISRestoreIndices(IS is,int **ptr)
{
  PetscValidHeaderSpecific(is,IS_COOKIE);
  PetscValidPointer(ptr);
  if (is->ops.restoreindices) return (*is->ops.restoreindices)(is,ptr);
  else return 0;
}

#undef __FUNCTION__  
#define __FUNCTION__ "ISView"
/*@
   ISView - Displays an index set.

   Input Parameters:
.  is - the index set
.  viewer - viewer used to display the set, for example VIEWER_STDOUT_SELF.

.keywords: IS, index set, indices

.seealso: ViewerFileOpenASCII()
@*/
int ISView(IS is, Viewer viewer)
{
  PetscValidHeaderSpecific(is,IS_COOKIE);
  if (viewer) {PetscValidHeader(viewer);}
  else { viewer = VIEWER_STDOUT_SELF; }
  return (*is->view)((PetscObject)is,viewer);
}

#undef __FUNCTION__  
#define __FUNCTION__ "ISSort"
/*@
   ISSort - Sorts the indices of an index set.

   Input Parameters:
.  is - the index set

.keywords: IS, index set, sort, indices

.seealso: ISSorted()
@*/
int ISSort(IS is)
{
  PetscValidHeaderSpecific(is,IS_COOKIE);
  return (*is->ops.sortindices)(is);
}

#undef __FUNCTION__  
#define __FUNCTION__ "ISSorted"
/*@C
   ISSorted - Checks the indices to determine whether they have been sorted.

   Input Parameters:
.  is - the index set

   Output Parameters:
.  flg - output flag, either
$     PETSC_TRUE if the index set is sorted;
$     PETSC_FALSE otherwise.

.keywords: IS, index set, sort, indices

.seealso: ISSort()
@*/
int ISSorted(IS is, PetscTruth *flg)
{
  PetscValidHeaderSpecific(is,IS_COOKIE);
  PetscValidIntPointer(flg);
  return (*is->ops.sorted)(is, flg);
}
