/*T
   Concepts: BiGraph
   Processors: multiple
T*/

/*
  Create a series of parallel BiGraphs suitable for testing the Delta routines.
*/

static char help[] = "Constructs a series of parallel BiGraphs and performs ParDelta routines.\n\n";

#include <ParDelta.hh>
#include <ALE.hh>


typedef ALE::Two::BiGraph<int,ALE::Two::Rec<int>,ALE::def::Point,ALE::Two::Rec<ALE::def::Point>,int> PointBiGraph;
typedef ALE::Two::ParDelta<PointBiGraph>                                 PointParDelta;
typedef PointParDelta::overlap_type                                      PointOverlap;
typedef PointParDelta::fusion_type                                       PointConeFusion;

PetscErrorCode   testBiGraphHat(MPI_Comm comm);

#undef __FUNCT__
#define __FUNCT__ "main"
int main(int argc, char *argv[])
{
  MPI_Comm       comm;
  PetscTruth     flag;
  PetscInt       verbosity;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = PetscInitialize(&argc, &argv, (char *) 0, help);CHKERRQ(ierr);
  verbosity = 1;
  ierr = PetscOptionsGetInt(PETSC_NULL, "-verbosity", &verbosity, &flag); CHKERRQ(ierr);
  comm = PETSC_COMM_WORLD;

  ierr = testBiGraphHat(comm); CHKERRQ(ierr);

  ierr = PetscFinalize();CHKERRQ(ierr);
  PetscFunctionReturn(0);
}/* main() */

#undef  __FUNCT__
#define __FUNCT__ "testBiGraphHat"
PetscErrorCode testBiGraphHat(MPI_Comm comm) {
  int rank;
  PetscErrorCode ierr;
  int debug;
  PetscTruth flag;
  PetscFunctionBegin;
  ALE::Obj<PointBiGraph> bg = PointBiGraph(comm);

  ierr = MPI_Comm_rank(comm, &rank); CHKERRQ(ierr);
  
  debug = 0;
  ierr = PetscOptionsGetInt(PETSC_NULL, "-debug", &debug, &flag); CHKERRQ(ierr);
  ierr = PetscPrintf(comm, "%s: using debug value of %d\n", __FUNCT__, debug); CHKERRQ(ierr);

  // Add three arrows from a single cap point rank to global points with the indices 2*rank, 2*rank+1, 2*rank+2 
  for(int i = 0; i < 3; i++) {
    bg->addArrow(rank, ALE::def::Point(-1,2*rank+i), -rank);
  }
  
  // View
  bg->parView("Hat bigraph");

  // Construct a Delta object and a base overlap object
  PointParDelta delta(bg, 1);
  ALE::Obj<PointOverlap>   overlap = delta.overlap();
  // View
  overlap->parView("Hat overlap");
  ALE::Obj<PointConeFusion> fusion   = delta.fusion(overlap);
  // View
  fusion->parView("Hat cone fusion");

  PetscFunctionReturn(0);
}/* testBiGraphHat() */
