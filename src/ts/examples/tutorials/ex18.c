static char help[] = "Hybrid Finite Element-Finite Volume Example.\n";
/*F
  Here we are advecting a passive tracer in a harmonic velocity field, defined by
a forcing function $f$:
\begin{align}
  -\Delta \mathbf{u} + f &= 0 \\
  \frac{\partial\phi}{\partial t} - \nabla\cdot \phi \mathbf{u} &= 0
\end{align}
F*/
#include <petscdmplex.h>
#include <petscds.h>
#include <petscts.h>

#include <petsc-private/dmpleximpl.h> /* For DotD */

#define ALEN(a) (sizeof(a)/sizeof((a)[0]))

PetscInt spatialDim = 0;

typedef enum {ZERO, CONSTANT, GAUSSIAN, TILTED} PorosityDistribution;

typedef struct {
  /* Domain and mesh definition */
  PetscInt       dim;               /* The topological mesh dimension */
  DMBoundaryType xbd, ybd;          /* The boundary type for the x- and y-boundary */
  char           filename[2048];    /* The optional ExodusII file */
  /* Problem definition */
  PetscBool      useFV;             /* Use a finite volume scheme for advection */
  void         (*exactFuncs[2])(const PetscReal x[], PetscScalar *u, void *ctx);
  void         (*initialGuess[2])(const PetscReal x[], PetscScalar *u, void *ctx);
  PorosityDistribution porosityDist;
} AppCtx;

#undef __FUNCT__
#define __FUNCT__ "ProcessOptions"
static PetscErrorCode ProcessOptions(MPI_Comm comm, AppCtx *options)
{
  const char    *porosityDist[4]  = {"zero", "constant", "gaussian", "tilted"};
  PetscInt       bd, pd;
  PetscErrorCode ierr;

  PetscFunctionBeginUser;
  options->dim          = 2;
  options->xbd          = DM_BOUNDARY_PERIODIC;
  options->ybd          = DM_BOUNDARY_PERIODIC;
  options->filename[0]  = '\0';
  options->useFV        = PETSC_FALSE;
  options->porosityDist = ZERO;

  ierr = PetscOptionsBegin(comm, "", "Magma Dynamics Options", "DMPLEX");CHKERRQ(ierr);
  ierr = PetscOptionsInt("-dim", "The topological mesh dimension", "ex18.c", options->dim, &options->dim, NULL);CHKERRQ(ierr);
  spatialDim = options->dim;
  bd   = options->xbd;
  ierr = PetscOptionsEList("-x_bd_type", "The x-boundary type", "ex18.c", DMBoundaryTypes, 5, DMBoundaryTypes[options->xbd], &bd, NULL);CHKERRQ(ierr);
  options->xbd = (DMBoundaryType) bd;
  bd   = options->ybd;
  ierr = PetscOptionsEList("-y_bd_type", "The y-boundary type", "ex18.c", DMBoundaryTypes, 5, DMBoundaryTypes[options->ybd], &bd, NULL);CHKERRQ(ierr);
  options->ybd = (DMBoundaryType) bd;
  ierr = PetscOptionsString("-f", "Exodus.II filename to read", "ex18.c", options->filename, options->filename, sizeof(options->filename), NULL);CHKERRQ(ierr);
  ierr = PetscOptionsBool("-use_fv", "Use the finite volume method for advection", "ex18.c", options->useFV, &options->useFV, NULL);CHKERRQ(ierr);
  pd   = options->porosityDist;
  ierr = PetscOptionsEList("-porosity_dist","Initial porosity distribution type","ex18.c",porosityDist,4,porosityDist[options->porosityDist],&pd,NULL);CHKERRQ(ierr);
  options->porosityDist = (PorosityDistribution) pd;
  ierr = PetscOptionsEnd();

  PetscFunctionReturn(0);
}

static void f0_lap_u(const PetscScalar u[], const PetscScalar u_t[], const PetscScalar u_x[], const PetscScalar a[], const PetscScalar a_t[], const PetscScalar a_x[], const PetscReal x[], PetscScalar f0[])
{
  const PetscInt Nc = spatialDim;
  PetscInt       comp;
#if 0
  for (comp = 0; comp < Nc; ++comp) f0[comp] = 4.0;
#else
  for (comp = 0; comp < Nc; ++comp) f0[comp] = u[comp];
#endif
}

static void f1_lap_u(const PetscScalar u[], const PetscScalar u_t[], const PetscScalar u_x[], const PetscScalar a[], const PetscScalar a_t[], const PetscScalar a_x[], const PetscReal x[], PetscScalar f1[])
{
  const PetscInt Nc  = spatialDim;
  const PetscInt dim = spatialDim;
  PetscInt       comp, d;

  for (comp = 0; comp < Nc; ++comp) {
    for (d = 0; d < dim; ++d) {
#if 0
      f1[comp*dim+d] = u_x[comp*dim+d];
#else
      f1[comp*dim+d] = 0.0;
#endif
    }
  }
}

static void f0_lap_periodic_u(const PetscScalar u[], const PetscScalar u_t[], const PetscScalar u_x[], const PetscScalar a[], const PetscScalar a_t[], const PetscScalar a_x[], const PetscReal x[], PetscScalar f0[])
{
  f0[0] = -sin(2.0*PETSC_PI*x[0]);
  f0[1] = 2.0*PETSC_PI*x[1]*cos(2.0*PETSC_PI*x[0]);
}

static void f0_lap_doubly_periodic_u(const PetscScalar u[], const PetscScalar u_t[], const PetscScalar u_x[], const PetscScalar a[], const PetscScalar a_t[], const PetscScalar a_x[], const PetscReal x[], PetscScalar f0[])
{
  f0[0] = -2.0*sin(2.0*PETSC_PI*x[0])*cos(2.0*PETSC_PI*x[1]);
  f0[1] =  2.0*sin(2.0*PETSC_PI*x[1])*cos(2.0*PETSC_PI*x[0]);
}

static void f0_advection(const PetscScalar u[], const PetscScalar u_t[], const PetscScalar u_x[],
                         const PetscScalar a[], const PetscScalar a_t[], const PetscScalar a_x[], const PetscReal x[], PetscScalar f0[])
{
  PetscInt d;
  f0[0] = u_t[spatialDim];
  for (d = 0; d < spatialDim; ++d) f0[0] += u[spatialDim]*u_x[d*spatialDim+d] + u_x[spatialDim*spatialDim+d]*u[d];
}

static void f1_advection(const PetscScalar u[], const PetscScalar u_t[], const PetscScalar u_x[],
                         const PetscScalar a[], const PetscScalar a_t[], const PetscScalar a_x[], const PetscReal x[], PetscScalar f1[])
{
  PetscInt d;
  for (d = 0; d < spatialDim; ++d) f1[0] = 0.0;
}

static void riemann_advection(const PetscReal *qp, const PetscReal *n, const PetscScalar *uL, const PetscScalar *uR, PetscScalar *flux, void *ctx)
{
  PetscReal wind[3] = {0.0, 1.0, 0.0};
  PetscReal wn = DMPlex_DotD_Internal(spatialDim, wind /*uL*/, n);

  flux[0] = (wn > 0 ? uL[spatialDim] : uR[spatialDim]) * wn;
}

/*
  In 2D we use the exact solution:

    u   = x^2 + y^2
    v   = 2 x^2 - 2xy
    phi = h(x + y + (u + v) t)
    f_x = f_y = 4

  so that

    -\Delta u + f = <-4, -4> + <4, 4> = 0
    {\partial\phi}{\partial t} - \nabla\cdot \phi u = 0
    h_t(x + y + (u + v) t) - u . grad phi - phi div u
  = u h' + v h'              - u h_x - v h_y
  = 0

We will conserve phi since

    \nabla \cdot u = 2x - 2x = 0

Also try h((x + ut)^2 + (y + vt)^2), so that

    h_t((x + ut)^2 + (y + vt)^2) - u . grad phi - phi div u
  = 2 h' (u (x + ut) + v (y + vt)) - u h_x - v h_y
  = 2 h' (u (x + ut) + v (y + vt)) - u h' 2 (x + u t) - v h' 2 (y + vt)
  = 2 h' (u (x + ut) + v (y + vt)  - u (x + u t) - v (y + vt))
  = 0

*/
void quadratic_u_2d(const PetscReal x[], PetscScalar *u, void *ctx)
{
#if 0
  u[0] = x[0]*x[0] + x[1]*x[1];
  u[1] = 2.0*x[0]*x[0] - 2.0*x[0]*x[1];
#else
  u[0] = 0.0;
  u[1] = 0.0;
#endif
}

/*
  In 2D we use the exact, periodic solution:

    u   =  sin(2 pi x)/4 pi^2
    v   = -y cos(2 pi x)/2 pi
    phi = h(x + y + (u + v) t)
    f_x = -sin(2 pi x)
    f_y = 2 pi y cos(2 pi x)

  so that

    -\Delta u + f = <sin(2pi x),  -2pi y cos(2pi x)> + <-sin(2pi x), 2pi y cos(2pi x)> = 0

We will conserve phi since

    \nabla \cdot u = cos(2pi x)/2pi - cos(2pi x)/2pi = 0
*/
void periodic_u_2d(const PetscReal x[], PetscScalar *u, void *ctx)
{
  u[0] = sin(2.0*PETSC_PI*x[0])/PetscSqr(2.0*PETSC_PI);
  u[1] = -x[1]*cos(2.0*PETSC_PI*x[0])/(2.0*PETSC_PI);
}

/*
  In 2D we use the exact, doubly periodic solution:

    u   =  sin(2 pi x) cos(2 pi y)/4 pi^2
    v   = -sin(2 pi y) cos(2 pi x)/4 pi^2
    phi = h(x + y + (u + v) t)
    f_x = -2sin(2 pi x) cos(2 pi y)
    f_y =  2sin(2 pi y) cos(2 pi x)

  so that

    -\Delta u + f = <2 sin(2pi x) cos(2pi y),  -2 sin(2pi y) cos(2pi x)> + <-2 sin(2pi x) cos(2pi y), 2 sin(2pi y) cos(2pi x)> = 0

We will conserve phi since

    \nabla \cdot u = cos(2pi x) cos(2pi y)/2pi - cos(2pi y) cos(2pi x)/2pi = 0
*/
void doubly_periodic_u_2d(const PetscReal x[], PetscScalar *u, void *ctx)
{
  u[0] =  sin(2.0*PETSC_PI*x[0])*cos(2.0*PETSC_PI*x[1])/PetscSqr(2.0*PETSC_PI);
  u[1] = -sin(2.0*PETSC_PI*x[1])*cos(2.0*PETSC_PI*x[0])/PetscSqr(2.0*PETSC_PI);
}

void initialVelocity(const PetscReal x[], PetscScalar *u, void *ctx)
{
  PetscInt d;
  for (d = 0; d < spatialDim; ++d) u[d] = 0.0;
}

void zero_phi(const PetscReal x[], PetscScalar *u, void *ctx)
{
  u[0] = 0.0;
}

void constant_phi(const PetscReal x[], PetscScalar *u, void *ctx)
{
  u[0] = 1.0;
}

void gaussian_phi_2d(const PetscReal x[], PetscScalar *u, void *ctx)
{
  const PetscReal t     = *((PetscReal *) ctx);
  const PetscReal xi    = x[0] + (sin(2.0*PETSC_PI*x[0])/(4.0*PETSC_PI*PETSC_PI))*t - 0.5;
  const PetscReal eta   = x[1] + (-x[1]*cos(2.0*PETSC_PI*x[0])/(2.0*PETSC_PI))*t - 0.5;
  const PetscReal r2    = xi*xi + eta*eta;
  const PetscReal sigma = 1.0/6.0;

  u[0] = PetscExpReal(-r2/(2.0*sigma*sigma))/(sigma*sqrt(2.0*PETSC_PI));
}

void tilted_phi_2d(const PetscReal x[], PetscScalar *u, void *ctx)
{
  PetscReal       x0[3];
  const PetscReal wind[3] = {0.0, 1.0, 0.0};
  const PetscReal t       = *((PetscReal *) ctx);

  DMPlex_WaxpyD_Internal(2, -t, wind, x, x0);
  if (x0[1] > 0) u[0] =  1.0*x[0] + 3.0*x[1];
  else           u[0] = -2.0;
}

#undef __FUNCT__
#define __FUNCT__ "advect_inflow"
static PetscErrorCode advect_inflow(PetscReal time, const PetscReal *c, const PetscReal *n, const PetscScalar *xI, PetscScalar *xG, void *ctx)
{
  PetscFunctionBeginUser;
  xG[0] = -2.0;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "advect_outflow"
static PetscErrorCode advect_outflow(PetscReal time, const PetscReal *c, const PetscReal *n, const PetscScalar *xI, PetscScalar *xG, void *ctx)
{
  PetscFunctionBeginUser;
  xG[0] = xI[0];
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "CreateMesh"
static PetscErrorCode CreateMesh(MPI_Comm comm, AppCtx *user, DM *dm)
{
  DM              distributedMesh = NULL;
  PetscBool       periodic        = user->xbd == DM_BOUNDARY_PERIODIC || user->xbd == DM_BOUNDARY_TWIST || user->ybd == DM_BOUNDARY_PERIODIC || user->ybd == DM_BOUNDARY_TWIST ? PETSC_TRUE : PETSC_FALSE;
  const char     *filename        = user->filename;
  const PetscInt  cells[3]        = {3, 3, 3};
  const PetscReal L[3]            = {1.0, 1.0, 1.0};
  PetscReal       maxCell[3];
  PetscInt        d;
  size_t          len;
  PetscErrorCode  ierr;

  PetscFunctionBeginUser;
  ierr = PetscStrlen(filename, &len);CHKERRQ(ierr);
  if (!len) {
    ierr = DMPlexCreateHexBoxMesh(comm, user->dim, cells, user->xbd, user->ybd, DM_BOUNDARY_NONE, dm);CHKERRQ(ierr);
    ierr = PetscObjectSetName((PetscObject) *dm, "Mesh");CHKERRQ(ierr);
  } else {
    ierr = DMPlexCreateFromFile(comm, filename, PETSC_TRUE, dm);CHKERRQ(ierr);
  }
  if (periodic) {for (d = 0; d < 3; ++d) maxCell[d] = 1.1*(L[d]/cells[d]); ierr = DMSetPeriodicity(*dm, maxCell, L);CHKERRQ(ierr);}
  /* Distribute mesh */
  ierr = DMPlexDistribute(*dm, 0, NULL, &distributedMesh);CHKERRQ(ierr);
  if (distributedMesh) {
    ierr = DMDestroy(dm);CHKERRQ(ierr);
    *dm  = distributedMesh;
  }
  /* Localize coordinates */
  ierr = DMPlexLocalizeCoordinates(*dm);CHKERRQ(ierr);
  ierr = DMViewFromOptions(*dm, NULL, "-orig_dm_view");CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "SetupBC"
static PetscErrorCode SetupBC(DM dm, AppCtx *user)
{
  DMLabel        label;
  PetscBool      check;
  PetscErrorCode ierr;

  PetscFunctionBeginUser;
  /* Set initial guesses and exact solutions */
  switch (user->dim) {
  case 2:
    user->initialGuess[0] = initialVelocity;
    switch(user->porosityDist) {
    case ZERO:     user->initialGuess[1] = zero_phi;break;
    case CONSTANT: user->initialGuess[1] = constant_phi;break;
    case GAUSSIAN: user->initialGuess[1] = gaussian_phi_2d;break;
    case TILTED:   user->initialGuess[1] = tilted_phi_2d;break;
    }
    switch (user->xbd) {
    case DM_BOUNDARY_PERIODIC:
      switch (user->ybd) {
      case DM_BOUNDARY_PERIODIC:
        user->exactFuncs[0] = doubly_periodic_u_2d;
        user->exactFuncs[1] = user->initialGuess[1];
        break;
      default:
        user->exactFuncs[0] = periodic_u_2d;
        user->exactFuncs[1] = user->initialGuess[1];
        break;
      }
      break;
    default:
      user->exactFuncs[0] = quadratic_u_2d;
      user->exactFuncs[1] = user->initialGuess[1];
      break;
    }
    break;
  default:
    SETERRQ1(PETSC_COMM_WORLD, PETSC_ERR_ARG_OUTOFRANGE, "Invalid dimension %d", user->dim);
  }
  ierr = PetscOptionsHasName(NULL, "-dmts_check", &check);CHKERRQ(ierr);
  if (check) {
    user->initialGuess[0] = user->exactFuncs[0];
    user->initialGuess[1] = user->exactFuncs[1];
  }
  /* Set BC */
  ierr = DMPlexGetLabel(dm, "marker", &label);CHKERRQ(ierr);
  if (label) {
    const PetscInt id = 1;

    ierr = DMPlexAddBoundary(dm, PETSC_TRUE, "wall", "marker", 0, user->exactFuncs[0], 1, &id, user);CHKERRQ(ierr);
  }
  ierr = DMPlexGetLabel(dm, "Face Sets", &label);CHKERRQ(ierr);
  if (label) {
    const PetscInt inflowids[] = {100,200,300}, outflowids[] = {101};

    ierr = DMPlexAddBoundary(dm, PETSC_TRUE, "inflow",  "Face Sets", 1, (void (*)()) advect_inflow,  ALEN(inflowids),  inflowids,  user);CHKERRQ(ierr);
    ierr = DMPlexAddBoundary(dm, PETSC_TRUE, "outflow", "Face Sets", 1, (void (*)()) advect_outflow, ALEN(outflowids), outflowids, user);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "SetupProblem"
static PetscErrorCode SetupProblem(DM dm, AppCtx *user)
{
  PetscDS        prob;
  PetscErrorCode ierr;

  PetscFunctionBeginUser;
  ierr = DMGetDS(dm, &prob);CHKERRQ(ierr);
  switch (user->xbd) {
  case DM_BOUNDARY_PERIODIC:
    switch (user->ybd) {
    case DM_BOUNDARY_PERIODIC:
      ierr = PetscDSSetResidual(prob, 0, f0_lap_doubly_periodic_u, f1_lap_u);CHKERRQ(ierr);
      break;
    default:
      ierr = PetscDSSetResidual(prob, 0, f0_lap_periodic_u, f1_lap_u);CHKERRQ(ierr);
      break;
    }
    break;
  default:
    ierr = PetscDSSetResidual(prob, 0, f0_lap_u, f1_lap_u);CHKERRQ(ierr);
    break;
  }
  ierr = PetscDSSetResidual(prob, 1, f0_advection, f1_advection);CHKERRQ(ierr);
  ierr = PetscDSSetRiemannSolver(prob, 1, riemann_advection);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "SetupDiscretization"
PetscErrorCode SetupDiscretization(DM dm, AppCtx *user)
{
  DM              cdm = dm;
  const PetscInt  dim = user->dim;
  PetscQuadrature q;
  PetscFE         fe[2];
  PetscFV         fv;
  PetscDS         prob;
  PetscInt        order;
  PetscErrorCode  ierr;

  PetscFunctionBeginUser;
  /* Create finite element */
  ierr = PetscFECreateDefault(dm, dim, dim, PETSC_FALSE, "velocity_", -1, &fe[0]);CHKERRQ(ierr);
  ierr = PetscObjectSetName((PetscObject) fe[0], "velocity");CHKERRQ(ierr);
  ierr = PetscFEGetQuadrature(fe[0], &q);CHKERRQ(ierr);
  ierr = PetscQuadratureGetOrder(q, &order);CHKERRQ(ierr);
  ierr = PetscFECreateDefault(dm, dim, 1, PETSC_FALSE, "porosity_", order, &fe[1]);CHKERRQ(ierr);
  ierr = PetscObjectSetName((PetscObject) fe[1], "porosity");CHKERRQ(ierr);

  ierr = PetscFVCreate(PetscObjectComm((PetscObject) dm), &fv);CHKERRQ(ierr);
  ierr = PetscObjectSetName((PetscObject) fv, "porosity");CHKERRQ(ierr);
  ierr = PetscFVSetFromOptions(fv);CHKERRQ(ierr);
  ierr = PetscFVSetNumComponents(fv, 1);CHKERRQ(ierr);
  ierr = PetscFVSetSpatialDimension(fv, dim);CHKERRQ(ierr);
  ierr = PetscFVSetQuadrature(fv, q);CHKERRQ(ierr);

  /* Set discretization and boundary conditions for each mesh */
  while (cdm) {
    ierr = DMGetDS(cdm, &prob);CHKERRQ(ierr);
    ierr = PetscDSSetDiscretization(prob, 0, (PetscObject) fe[0]);CHKERRQ(ierr);
    if (user->useFV) {ierr = PetscDSSetDiscretization(prob, 1, (PetscObject) fv);CHKERRQ(ierr);}
    else             {ierr = PetscDSSetDiscretization(prob, 1, (PetscObject) fe[1]);CHKERRQ(ierr);}

    ierr = SetupProblem(cdm, user);CHKERRQ(ierr);
    ierr = DMPlexGetCoarseDM(cdm, &cdm);CHKERRQ(ierr);

    /* Coordinates were never localized for coarse meshes */
    if (cdm) {ierr = DMPlexLocalizeCoordinates(cdm);CHKERRQ(ierr);}
  }
  ierr = PetscFEDestroy(&fe[0]);CHKERRQ(ierr);
  ierr = PetscFEDestroy(&fe[1]);CHKERRQ(ierr);
  ierr = PetscFVDestroy(&fv);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "CreateDM"
static PetscErrorCode CreateDM(MPI_Comm comm, AppCtx *user, DM *dm)
{
  PetscErrorCode ierr;

  PetscFunctionBeginUser;
  ierr = CreateMesh(comm, user, dm);CHKERRQ(ierr);
  /* Setup BC */
  ierr = SetupBC(*dm, user);CHKERRQ(ierr);
  /* Handle refinement, BC ids, etc. */
  ierr = DMSetFromOptions(*dm);CHKERRQ(ierr);
  /* Construct ghost cells */
  if (user->useFV) {
    DM gdm;

    ierr = DMPlexConstructGhostCells(*dm, NULL, NULL, &gdm);CHKERRQ(ierr);
    ierr = DMDestroy(dm);CHKERRQ(ierr);
    *dm  = gdm;
  }
  ierr = DMViewFromOptions(*dm, NULL, "-dm_view");CHKERRQ(ierr);
  /* Setup problem */
  ierr = SetupDiscretization(*dm, user);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "SetInitialConditionFVM"
PetscErrorCode SetInitialConditionFVM(DM dm, Vec X, PetscInt field, void (*func)(const PetscReal [], PetscScalar *, void *), void *ctx)
{
  PetscDS            prob;
  DM                 dmCell;
  Vec                cellgeom;
  const PetscScalar *cgeom;
  PetscScalar       *x;
  PetscInt           cStart, cEnd, cEndInterior, c, off;
  PetscErrorCode     ierr;

  PetscFunctionBeginUser;
  ierr = DMGetDS(dm, &prob);CHKERRQ(ierr);
  ierr = PetscDSGetFieldOffset(prob, field, &off);CHKERRQ(ierr);
  ierr = DMPlexGetHybridBounds(dm, &cEndInterior, NULL, NULL, NULL);CHKERRQ(ierr);
  ierr = DMPlexTSGetGeometryFVM(dm, NULL, &cellgeom, NULL);CHKERRQ(ierr);
  ierr = VecGetDM(cellgeom, &dmCell);CHKERRQ(ierr);
  ierr = DMPlexGetHeightStratum(dm, 0, &cStart, &cEnd);CHKERRQ(ierr);
  ierr = VecGetArrayRead(cellgeom, &cgeom);CHKERRQ(ierr);
  ierr = VecGetArray(X, &x);CHKERRQ(ierr);
  for (c = cStart; c < cEndInterior; ++c) {
    const PetscFVCellGeom *cg;
    PetscScalar           *xc;

    ierr = DMPlexPointLocalRead(dmCell, c, cgeom, &cg);CHKERRQ(ierr);
    ierr = DMPlexPointGlobalRef(dm, c, x, &xc);CHKERRQ(ierr);
    if (xc) (*func)(cg->centroid, &xc[off], ctx);
  }
  ierr = VecRestoreArrayRead(cellgeom, &cgeom);CHKERRQ(ierr);
  ierr = VecRestoreArray(X, &x);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "MonitorFunctionals"
static PetscErrorCode MonitorFunctionals(TS ts, PetscInt stepnum, PetscReal time, Vec X, void *ctx)
{
#if 0
  AppCtx            *user   = (AppCtx *) ctx;
#endif
  char              *ftable = NULL;
  DM                 dm;
  PetscSection       s;
  Vec                cellgeom;
  const PetscScalar *x;
  PetscReal         *xnorms;
  PetscInt           pStart, pEnd, p, Nf, f, cEndInterior;
  PetscErrorCode     ierr;

  PetscFunctionBeginUser;
  ierr = VecGetDM(X, &dm);CHKERRQ(ierr);
  ierr = DMPlexTSGetGeometryFVM(dm, NULL, &cellgeom, NULL);CHKERRQ(ierr);
  ierr = DMPlexGetHybridBounds(dm, &cEndInterior, NULL, NULL, NULL);CHKERRQ(ierr);
  ierr = DMGetDefaultSection(dm, &s);CHKERRQ(ierr);
  ierr = PetscSectionGetNumFields(s, &Nf);CHKERRQ(ierr);
  ierr = PetscSectionGetChart(s, &pStart, &pEnd);CHKERRQ(ierr);
  ierr = PetscCalloc1(Nf, &xnorms);CHKERRQ(ierr);
  ierr = VecGetArrayRead(X, &x);CHKERRQ(ierr);
  for (p = pStart; p < pEnd; ++p) {
    for (f = 0; f < Nf; ++f) {
      PetscInt dof, off,d;

      ierr = PetscSectionGetFieldDof(s, p, f, &dof);CHKERRQ(ierr);
      ierr = PetscSectionGetFieldOffset(s, p, f, &off);CHKERRQ(ierr);
      for (d = 0; d < dof; ++d) xnorms[f] = PetscMax(xnorms[f], PetscAbsScalar(x[off+d]));
    }
  }
  ierr = VecRestoreArrayRead(X, &x);CHKERRQ(ierr);
  if (stepnum >= 0) {           /* No summary for final time */
#if 0
    Model             mod = user->model;
    PetscInt          c,cStart,cEnd,fcount,i;
    size_t            ftableused,ftablealloc;
    const PetscScalar *cgeom,*x;
    DM                dmCell;
    PetscReal         *fmin,*fmax,*fintegral,*ftmp;
    fcount = mod->maxComputed+1;
    ierr   = PetscMalloc4(fcount,&fmin,fcount,&fmax,fcount,&fintegral,fcount,&ftmp);CHKERRQ(ierr);
    for (i=0; i<fcount; i++) {
      fmin[i]      = PETSC_MAX_REAL;
      fmax[i]      = PETSC_MIN_REAL;
      fintegral[i] = 0;
    }
    ierr = DMPlexGetHeightStratum(dm,0,&cStart,&cEnd);CHKERRQ(ierr);
    ierr = VecGetDM(cellgeom,&dmCell);CHKERRQ(ierr);
    ierr = VecGetArrayRead(cellgeom,&cgeom);CHKERRQ(ierr);
    ierr = VecGetArrayRead(X,&x);CHKERRQ(ierr);
    for (c = cStart; c < cEndInterior; ++c) {
      const PetscFVCellGeom *cg;
      const PetscScalar     *cx;
      ierr = DMPlexPointLocalRead(dmCell,c,cgeom,&cg);CHKERRQ(ierr);
      ierr = DMPlexPointGlobalRead(dm,c,x,&cx);CHKERRQ(ierr);
      if (!cx) continue;        /* not a global cell */
      for (i=0; i<mod->numCall; i++) {
        FunctionalLink flink = mod->functionalCall[i];
        ierr = (*flink->func)(mod,time,cg->centroid,cx,ftmp,flink->ctx);CHKERRQ(ierr);
      }
      for (i=0; i<fcount; i++) {
        fmin[i]       = PetscMin(fmin[i],ftmp[i]);
        fmax[i]       = PetscMax(fmax[i],ftmp[i]);
        fintegral[i] += cg->volume * ftmp[i];
      }
    }
    ierr = VecRestoreArrayRead(cellgeom,&cgeom);CHKERRQ(ierr);
    ierr = VecRestoreArrayRead(X,&x);CHKERRQ(ierr);
    ierr = MPI_Allreduce(MPI_IN_PLACE,fmin,fcount,MPIU_REAL,MPI_MIN,PetscObjectComm((PetscObject)ts));CHKERRQ(ierr);
    ierr = MPI_Allreduce(MPI_IN_PLACE,fmax,fcount,MPIU_REAL,MPI_MAX,PetscObjectComm((PetscObject)ts));CHKERRQ(ierr);
    ierr = MPI_Allreduce(MPI_IN_PLACE,fintegral,fcount,MPIU_REAL,MPI_SUM,PetscObjectComm((PetscObject)ts));CHKERRQ(ierr);

    ftablealloc = fcount * 100;
    ftableused  = 0;
    ierr        = PetscMalloc1(ftablealloc,&ftable);CHKERRQ(ierr);
    for (i=0; i<mod->numMonitored; i++) {
      size_t         countused;
      char           buffer[256],*p;
      FunctionalLink flink = mod->functionalMonitored[i];
      PetscInt       id    = flink->offset;
      if (i % 3) {
        ierr = PetscMemcpy(buffer,"  ",2);CHKERRQ(ierr);
        p    = buffer + 2;
      } else if (i) {
        char newline[] = "\n";
        ierr = PetscMemcpy(buffer,newline,sizeof newline-1);CHKERRQ(ierr);
        p    = buffer + sizeof newline - 1;
      } else {
        p = buffer;
      }
      ierr = PetscSNPrintfCount(p,sizeof buffer-(p-buffer),"%12s [%10.7g,%10.7g] int %10.7g",&countused,flink->name,(double)fmin[id],(double)fmax[id],(double)fintegral[id]);CHKERRQ(ierr);
      countused += p - buffer;
      if (countused > ftablealloc-ftableused-1) { /* reallocate */
        char *ftablenew;
        ftablealloc = 2*ftablealloc + countused;
        ierr = PetscMalloc(ftablealloc,&ftablenew);CHKERRQ(ierr);
        ierr = PetscMemcpy(ftablenew,ftable,ftableused);CHKERRQ(ierr);
        ierr = PetscFree(ftable);CHKERRQ(ierr);
        ftable = ftablenew;
      }
      ierr = PetscMemcpy(ftable+ftableused,buffer,countused);CHKERRQ(ierr);
      ftableused += countused;
      ftable[ftableused] = 0;
    }
    ierr = PetscFree4(fmin,fmax,fintegral,ftmp);CHKERRQ(ierr);
#endif
    ierr = PetscPrintf(PetscObjectComm((PetscObject) ts), "% 3D  time %8.4g  |x| (", stepnum, (double) time);CHKERRQ(ierr);
    for (f = 0; f < Nf; ++f) {
      if (f > 0) {ierr = PetscPrintf(PetscObjectComm((PetscObject) ts), ", ");CHKERRQ(ierr);}
      ierr = PetscPrintf(PetscObjectComm((PetscObject) ts), "%8.4g", (double) xnorms[f]);CHKERRQ(ierr);
    }
    ierr = PetscPrintf(PetscObjectComm((PetscObject) ts), ")  %s\n", ftable ? ftable : "");CHKERRQ(ierr);
    ierr = PetscFree(ftable);CHKERRQ(ierr);
  }
  ierr = PetscFree(xnorms);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "main"
int main(int argc, char **argv)
{
  MPI_Comm       comm;
  TS             ts;
  DM             dm;
  Vec            u;
  AppCtx         user;
  PetscReal      t0, t = 0.0;
  void          *ctxs[2] = {&t, &t};
  PetscErrorCode ierr;

  ierr = PetscInitialize(&argc, &argv, (char*) 0, help);CHKERRQ(ierr);
  comm = PETSC_COMM_WORLD;
  ierr = ProcessOptions(comm, &user);CHKERRQ(ierr);
  ierr = TSCreate(comm, &ts);CHKERRQ(ierr);
  ierr = TSSetType(ts, TSBEULER);CHKERRQ(ierr);
  ierr = CreateDM(comm, &user, &dm);CHKERRQ(ierr);
  ierr = TSSetDM(ts, dm);CHKERRQ(ierr);

  ierr = DMCreateGlobalVector(dm, &u);CHKERRQ(ierr);
  ierr = PetscObjectSetName((PetscObject) u, "solution");CHKERRQ(ierr);
  //ierr = DMTSSetIFunctionLocal(dm, DMPlexTSComputeIFunctionFEM, &user);CHKERRQ(ierr);
  ierr = DMTSSetRHSFunctionLocal(dm, DMPlexTSComputeRHSFunctionFVM, &user);CHKERRQ(ierr);
  if (user.useFV) {ierr = TSMonitorSet(ts, MonitorFunctionals, &user, NULL);CHKERRQ(ierr);}
  ierr = TSSetDuration(ts, 1, 2.0);CHKERRQ(ierr);
  ierr = TSSetInitialTimeStep(ts, 0.0, 0.01);CHKERRQ(ierr);
  ierr = TSSetFromOptions(ts);CHKERRQ(ierr);

  ierr = DMPlexProjectFunction(dm, user.exactFuncs, ctxs, INSERT_ALL_VALUES, u);CHKERRQ(ierr);
  ierr = DMPlexProjectFunction(dm, user.initialGuess, ctxs, INSERT_VALUES, u);CHKERRQ(ierr);
  ierr = SetInitialConditionFVM(dm, u, 1, user.initialGuess[1], ctxs[1]);CHKERRQ(ierr);
  ierr = VecViewFromOptions(u, "init_", "-vec_view");CHKERRQ(ierr);
  ierr = TSGetTime(ts, &t);CHKERRQ(ierr);
  t0   = t;
  ierr = DMTSCheckFromOptions(ts, u, user.exactFuncs, ctxs);CHKERRQ(ierr);
  ierr = TSSolve(ts, u);CHKERRQ(ierr);
  ierr = TSGetTime(ts, &t);CHKERRQ(ierr);
  if (t > t0) {ierr = DMTSCheckFromOptions(ts, u, user.exactFuncs, ctxs);CHKERRQ(ierr);}
  ierr = VecViewFromOptions(u, "sol_", "-vec_view");CHKERRQ(ierr);
  {
    PetscReal ftime;
    PetscInt  nsteps;
    TSConvergedReason reason;

    ierr = TSGetSolveTime(ts, &ftime);CHKERRQ(ierr);
    ierr = TSGetTimeStepNumber(ts, &nsteps);CHKERRQ(ierr);
    ierr = TSGetConvergedReason(ts, &reason);CHKERRQ(ierr);
    ierr = PetscPrintf(PETSC_COMM_WORLD, "%s at time %g after %D steps\n", TSConvergedReasons[reason], (double) ftime, nsteps);CHKERRQ(ierr);
  }

  ierr = VecDestroy(&u);CHKERRQ(ierr);
  ierr = DMDestroy(&dm);CHKERRQ(ierr);
  ierr = TSDestroy(&ts);CHKERRQ(ierr);
  ierr = PetscFinalize();
  return(0);
}
