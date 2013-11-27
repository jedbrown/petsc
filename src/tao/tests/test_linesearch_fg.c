/*
  Include "tao.h" so we can use TAO solvers with PETSc support.  
  Include "petscdmda.h" so that we can use distributed arrays (DMs) for managing
  the parallel mesh.
*/

#include "petscdmda.h"
#include "petscksp.h"
#include "taosolver.h"

static  char help[]=
"This example demonstrates use of the TAO package to \n\
solve a bound constrained minimization problem.  This example is based on \n\
the problem DPJB from the MINPACK-2 test suite.  This pressure journal \n\
bearing problem is an example of elliptic variational problem defined over \n\
a two dimensional rectangle.  By discretizing the domain into triangular \n\
elements, the pressure surrounding the journal bearing is defined as the \n\
minimum of a quadratic function whose variables are bounded below by zero.\n\
The command line options are:\n\
  -mx <xg>, where <xg> = number of grid points in the 1st coordinate direction\n\
  -my <yg>, where <yg> = number of grid points in the 2nd coordinate direction\n\
 \n";


/* 
   User-defined application context - contains data needed by the 
   application-provided call-back routines, FormFunctionGradient(),
   FormHessian().
*/
typedef struct {
  /* problem parameters */
  PetscReal      ecc;          /* test problem parameter */
  PetscReal      b;            /* A dimension of journal bearing */
  PetscInt       nx,ny;        /* discretization in x, y directions */

  /* Working space */
  DM          dm;           /* distributed array data structure */
  Mat         A;            /* Quadratic Objective term */
  Vec         B;            /* Linear Objective term */
  Vec         W;            /* work vector */
} AppCtx;

/* User-defined routines */
static PetscReal p(PetscReal xi, PetscReal ecc);
static PetscErrorCode FormFunctionGradient(TaoSolver, Vec, PetscReal *,Vec,void *);
static PetscErrorCode FormLSFunctionGradient(TaoLineSearch, Vec, PetscReal *, Vec, void*);
static PetscErrorCode FormHessian(TaoSolver,Vec,Mat *, Mat *, MatStructure *, void *);
static PetscErrorCode ComputeB(AppCtx*);

#undef __FUNCT__
#define __FUNCT__ "main"
int main( int argc, char **argv )
{
  PetscErrorCode        ierr;               /* used to check for functions returning nonzeros */
  PetscInt        Nx, Ny;             /* number of processors in x- and y- directions */
  PetscInt        m, N;               /* number of local and global elements in vectors */
  Vec        x;                  /* variables vector */
  Vec        xl,xu;                  /* bounds vectors */
  PetscReal d1000 = 1000;
  PetscBool   flg;              /* A return variable when checking for user options */
  TaoSolver tao;                /* TaoSolver solver context */

  TaoSolverTerminationReason reason;
  AppCtx     user;               /* user-defined work context */
  PetscReal     zero=0.0;           /* lower bound on all variables */
  TaoLineSearch    ls;

  
  /* Initialize PETSC and TAO */
  PetscInitialize( &argc, &argv,(char *)0,help );
  TaoInitialize( &argc, &argv,(char *)0,help );

  /* Set the default values for the problem parameters */
  user.nx = 50; user.ny = 50; user.ecc = 0.1; user.b = 10.0;

  /* Check for any command line arguments that override defaults */
  ierr = PetscOptionsGetInt(PETSC_NULL,"-mx",&user.nx,&flg); CHKERRQ(ierr);
  ierr = PetscOptionsGetInt(PETSC_NULL,"-my",&user.ny,&flg); CHKERRQ(ierr);
  ierr = PetscOptionsGetReal(PETSC_NULL,"-ecc",&user.ecc,&flg); CHKERRQ(ierr);
  ierr = PetscOptionsGetReal(PETSC_NULL,"-b",&user.b,&flg); CHKERRQ(ierr);


  PetscPrintf(PETSC_COMM_WORLD,"\n---- Journal Bearing Problem SHB-----\n");
  PetscPrintf(PETSC_COMM_WORLD,"mx: %D,  my: %D,  ecc: %G \n\n",
	      user.nx,user.ny,user.ecc);

  /* Calculate any derived values from parameters */
  N = user.nx*user.ny; 

  /* Let Petsc determine the grid division */
  Nx = PETSC_DECIDE; Ny = PETSC_DECIDE;

  /*
     A two dimensional distributed array will help define this problem,
     which derives from an elliptic PDE on two dimensional domain.  From
     the distributed array, Create the vectors.
  */
  ierr = DMDACreate2d(PETSC_COMM_WORLD,DMDA_BOUNDARY_NONE,DMDA_BOUNDARY_NONE,DMDA_STENCIL_STAR,
		      user.nx,user.ny,Nx,Ny,1,1,PETSC_NULL,PETSC_NULL,
		      &user.dm); CHKERRQ(ierr);

  /*
     Extract global and local vectors from DM; the vector user.B is
     used solely as work space for the evaluation of the function, 
     gradient, and Hessian.  Duplicate for remaining vectors that are 
     the same types.
  */
  ierr = DMCreateGlobalVector(user.dm,&x); CHKERRQ(ierr); /* Solution */
  ierr = VecDuplicate(x,&user.B); CHKERRQ(ierr); /* Linear objective */
  ierr = VecDuplicate(x,&user.W); CHKERRQ(ierr); /* Linear objective */


  /*  Create matrix user.A to store quadratic, Create a local ordering scheme. */
  ierr = VecGetLocalSize(x,&m); CHKERRQ(ierr);
  ierr = DMCreateMatrix(user.dm,&user.A);

  /* User defined function -- compute linear term of quadratic */
  ierr = ComputeB(&user); CHKERRQ(ierr);

  /* The TAO code begins here */

  /* 
     Create the optimization solver, Petsc application 
     Suitable methods: "tao_gpcg","tao_bqpip","tao_tron","tao_blmvm" 
  */
  ierr = TaoCreate(PETSC_COMM_WORLD,&tao); CHKERRQ(ierr);
  ierr = TaoSetType(tao,"tao_blmvm"); CHKERRQ(ierr);


  /* Set the initial vector */
  ierr = VecSet(x, zero); CHKERRQ(ierr);
  ierr = TaoSetInitialVector(tao,x); CHKERRQ(ierr);

  /* Set the user function, gradient, hessian evaluation routines and data structures */
  ierr = TaoSetObjectiveAndGradientRoutine(tao,FormFunctionGradient,(void*) &user);
  CHKERRQ(ierr);
  
  ierr = TaoSetHessianRoutine(tao,user.A,user.A,FormHessian,(void*)&user); CHKERRQ(ierr);

  /* Set a routine that defines the bounds */
  ierr = VecDuplicate(x,&xl); CHKERRQ(ierr);
  ierr = VecDuplicate(x,&xu); CHKERRQ(ierr);
  ierr = VecSet(xl, zero); CHKERRQ(ierr);
  ierr = VecSet(xu, d1000); CHKERRQ(ierr);
  ierr = TaoSetVariableBounds(tao,xl,xu); CHKERRQ(ierr);

  /* Check for any tao command line options */
  ierr = TaoSetFromOptions(tao); CHKERRQ(ierr);

  ierr = TaoGetLineSearch(tao,&ls); CHKERRQ(ierr);
  ierr = TaoLineSearchSetObjectiveAndGradientRoutine(ls,FormLSFunctionGradient,&user); CHKERRQ(ierr);

  /* Solve the bound constrained problem */
  ierr = TaoSolve(tao); CHKERRQ(ierr);

  ierr = TaoGetTerminationReason(tao,&reason); CHKERRQ(ierr);
  if (reason <= 0)
    PetscPrintf(PETSC_COMM_WORLD,"Try a different TAO method, adjust some parameters, or check the function evaluation routines\n");


  /* Free PETSc data structures */
  ierr = VecDestroy(&x); CHKERRQ(ierr); 
  ierr = VecDestroy(&xl); CHKERRQ(ierr); 
  ierr = VecDestroy(&xu); CHKERRQ(ierr); 
  ierr = MatDestroy(&user.A); CHKERRQ(ierr);
  ierr = VecDestroy(&user.B); CHKERRQ(ierr); 
  ierr = VecDestroy(&user.W); CHKERRQ(ierr);
  /* Free TAO data structures */
  ierr = TaoDestroy(&tao); CHKERRQ(ierr);

  ierr = DMDestroy(&user.dm); CHKERRQ(ierr);

  TaoFinalize();
  PetscFinalize();

  return 0;
}


static PetscReal p(PetscReal xi, PetscReal ecc)
{ 
  PetscReal t=1.0+ecc*PetscCosScalar(xi); 
  return (t*t*t); 
}

#undef __FUNCT__
#define __FUNCT__ "ComputeB"
PetscErrorCode ComputeB(AppCtx* user)
{
  PetscErrorCode ierr;
  PetscInt i,j,k;
  PetscInt nx,ny,xs,xm,gxs,gxm,ys,ym,gys,gym;
  PetscReal two=2.0, pi=4.0*atan(1.0);
  PetscReal hx,hy,ehxhy;
  PetscReal temp,*b;
  PetscReal ecc=user->ecc;

  nx=user->nx;
  ny=user->ny;
  hx=two*pi/(nx+1.0);
  hy=two*user->b/(ny+1.0);
  ehxhy = ecc*hx*hy;


  /*
     Get local grid boundaries
  */
  ierr = DMDAGetCorners(user->dm,&xs,&ys,PETSC_NULL,&xm,&ym,PETSC_NULL); CHKERRQ(ierr);
  ierr = DMDAGetGhostCorners(user->dm,&gxs,&gys,PETSC_NULL,&gxm,&gym,PETSC_NULL); CHKERRQ(ierr);
  

  /* Compute the linear term in the objective function */  
  ierr = VecGetArray(user->B,&b); CHKERRQ(ierr);
  for (i=xs; i<xs+xm; i++){
    temp=PetscSinScalar((i+1)*hx);
    for (j=ys; j<ys+ym; j++){
      k=xm*(j-ys)+(i-xs);
      b[k]=  - ehxhy*temp;
    }
  }
  ierr = VecRestoreArray(user->B,&b); CHKERRQ(ierr);
  ierr = PetscLogFlops(5*xm*ym+3*xm); CHKERRQ(ierr);

  return 0;
}

#undef __FUNCT__
#define __FUNCT__ "FormFunctionGradient"
PetscErrorCode FormFunctionGradient(TaoSolver tao, Vec X, PetscReal *fcn,Vec G,void *ptr)
{
  AppCtx* user=(AppCtx*)ptr;
  PetscErrorCode ierr;
  PetscInt i,j,k,kk;
  PetscInt col[5],row,nx,ny,xs,xm,gxs,gxm,ys,ym,gys,gym;
  PetscReal one=1.0, two=2.0, six=6.0,pi=4.0*atan(1.0);
  PetscReal hx,hy,hxhy,hxhx,hyhy;
  PetscReal xi,v[5];
  PetscReal ecc=user->ecc, trule1,trule2,trule3,trule4,trule5,trule6;
  PetscReal vmiddle, vup, vdown, vleft, vright;
  PetscReal tt,f1,f2;
  PetscReal *x,*g,zero=0.0;
  Vec localX;

  PetscFunctionBegin;
  ierr = PetscPrintf(PETSC_COMM_WORLD,"FunctionGradient\n"); CHKERRQ(ierr);
  nx=user->nx;
  ny=user->ny;
  hx=two*pi/(nx+1.0);
  hy=two*user->b/(ny+1.0);
  hxhy=hx*hy;
  hxhx=one/(hx*hx);
  hyhy=one/(hy*hy);

  ierr = DMGetLocalVector(user->dm,&localX);CHKERRQ(ierr);

  ierr = DMGlobalToLocalBegin(user->dm,X,INSERT_VALUES,localX); CHKERRQ(ierr);
  ierr = DMGlobalToLocalEnd(user->dm,X,INSERT_VALUES,localX); CHKERRQ(ierr);

  ierr = VecSet(G, zero); CHKERRQ(ierr);
  /*
    Get local grid boundaries
  */
  ierr = DMDAGetCorners(user->dm,&xs,&ys,PETSC_NULL,&xm,&ym,PETSC_NULL); CHKERRQ(ierr);
  ierr = DMDAGetGhostCorners(user->dm,&gxs,&gys,PETSC_NULL,&gxm,&gym,PETSC_NULL); CHKERRQ(ierr);
  
  ierr = VecGetArray(localX,&x); CHKERRQ(ierr);
  ierr = VecGetArray(G,&g); CHKERRQ(ierr);

  for (i=xs; i< xs+xm; i++){
    xi=(i+1)*hx;
    trule1=hxhy*( p(xi,ecc) + p(xi+hx,ecc) + p(xi,ecc) ) / six; /* L(i,j) */
    trule2=hxhy*( p(xi,ecc) + p(xi-hx,ecc) + p(xi,ecc) ) / six; /* U(i,j) */
    trule3=hxhy*( p(xi,ecc) + p(xi+hx,ecc) + p(xi+hx,ecc) ) / six; /* U(i+1,j) */
    trule4=hxhy*( p(xi,ecc) + p(xi-hx,ecc) + p(xi-hx,ecc) ) / six; /* L(i-1,j) */
    trule5=trule1; /* L(i,j-1) */
    trule6=trule2; /* U(i,j+1) */

    vdown=-(trule5+trule2)*hyhy;
    vleft=-hxhx*(trule2+trule4);
    vright= -hxhx*(trule1+trule3);
    vup=-hyhy*(trule1+trule6);
    vmiddle=(hxhx)*(trule1+trule2+trule3+trule4)+hyhy*(trule1+trule2+trule5+trule6);

    for (j=ys; j<ys+ym; j++){
      
      row=(j-gys)*gxm + (i-gxs);
       v[0]=0; v[1]=0; v[2]=0; v[3]=0; v[4]=0;
       
       k=0;
       if (j>gys){ 
	 v[k]=vdown; col[k]=row - gxm; k++;
       }
       
       if (i>gxs){
	 v[k]= vleft; col[k]=row - 1; k++;
       }

       v[k]= vmiddle; col[k]=row; k++;
       
       if (i+1 < gxs+gxm){
	 v[k]= vright; col[k]=row+1; k++;
       }
       
       if (j+1 <gys+gym){
	 v[k]= vup; col[k] = row+gxm; k++;
       }
       tt=0;
       for (kk=0;kk<k;kk++){
	 tt+=v[kk]*x[col[kk]];
       }
       row=(j-ys)*xm + (i-xs);
       g[row]=tt;

     }

  }

  ierr = VecRestoreArray(localX,&x); CHKERRQ(ierr);
  ierr = VecRestoreArray(G,&g); CHKERRQ(ierr);

  ierr = DMRestoreLocalVector(user->dm,&localX); CHKERRQ(ierr);

  ierr = VecDot(X,G,&f1); CHKERRQ(ierr);
  ierr = VecDot(user->B,X,&f2); CHKERRQ(ierr);
  ierr = VecAXPY(G, one, user->B); CHKERRQ(ierr);
  *fcn = f1/2.0 + f2;
  

  ierr = PetscLogFlops((91 + 10*ym) * xm); CHKERRQ(ierr);
  PetscFunctionReturn(0);

}

#undef __FUNCT__
#define __FUNCT__ "FormLSFunctionGradient"
PetscErrorCode FormLSFunctionGradient(TaoLineSearch ls, Vec X, PetscReal *fcn,Vec G,void *ptr)
{
  AppCtx* user=(AppCtx*)ptr;
  PetscErrorCode ierr;
  PetscInt i,j,k,kk;
  PetscInt col[5],row,nx,ny,xs,xm,gxs,gxm,ys,ym,gys,gym;
  PetscReal one=1.0, two=2.0, six=6.0,pi=4.0*atan(1.0);
  PetscReal hx,hy,hxhy,hxhx,hyhy;
  PetscReal xi,v[5];
  PetscReal ecc=user->ecc, trule1,trule2,trule3,trule4,trule5,trule6;
  PetscReal vmiddle, vup, vdown, vleft, vright;
  PetscReal tt,f1,f2;
  PetscReal *x,*g,zero=0.0;
  Vec localX;
  PetscReal steplength;
  Vec ls_x,ls_s;

  PetscFunctionBegin;
  nx=user->nx;
  ny=user->ny;
  hx=two*pi/(nx+1.0);
  hy=two*user->b/(ny+1.0);
  hxhy=hx*hy;
  hxhx=one/(hx*hx);
  hyhy=one/(hy*hy);

  ierr = TaoLineSearchGetStartingVector(ls,&ls_x); CHKERRQ(ierr);
  ierr = TaoLineSearchGetStepDirection(ls,&ls_s); CHKERRQ(ierr);
  ierr = TaoLineSearchGetStepLength(ls,&steplength); CHKERRQ(ierr);
  ierr = VecCopy(ls_x,user->W); CHKERRQ(ierr);
  ierr = VecAXPY(user->W,steplength,ls_s); CHKERRQ(ierr);

  ierr = DMGetLocalVector(user->dm,&localX);CHKERRQ(ierr);

  ierr = DMGlobalToLocalBegin(user->dm,user->W,INSERT_VALUES,localX); CHKERRQ(ierr);
  ierr = DMGlobalToLocalEnd(user->dm,user->W,INSERT_VALUES,localX); CHKERRQ(ierr);

  ierr = VecSet(G, zero); CHKERRQ(ierr);
  /*
    Get local grid boundaries
  */
  ierr = DMDAGetCorners(user->dm,&xs,&ys,PETSC_NULL,&xm,&ym,PETSC_NULL); CHKERRQ(ierr);
  ierr = DMDAGetGhostCorners(user->dm,&gxs,&gys,PETSC_NULL,&gxm,&gym,PETSC_NULL); CHKERRQ(ierr);
  
  ierr = VecGetArray(localX,&x); CHKERRQ(ierr);
  ierr = VecGetArray(G,&g); CHKERRQ(ierr);

  for (i=xs; i< xs+xm; i++){
    xi=(i+1)*hx;
    trule1=hxhy*( p(xi,ecc) + p(xi+hx,ecc) + p(xi,ecc) ) / six; /* L(i,j) */
    trule2=hxhy*( p(xi,ecc) + p(xi-hx,ecc) + p(xi,ecc) ) / six; /* U(i,j) */
    trule3=hxhy*( p(xi,ecc) + p(xi+hx,ecc) + p(xi+hx,ecc) ) / six; /* U(i+1,j) */
    trule4=hxhy*( p(xi,ecc) + p(xi-hx,ecc) + p(xi-hx,ecc) ) / six; /* L(i-1,j) */
    trule5=trule1; /* L(i,j-1) */
    trule6=trule2; /* U(i,j+1) */

    vdown=-(trule5+trule2)*hyhy;
    vleft=-hxhx*(trule2+trule4);
    vright= -hxhx*(trule1+trule3);
    vup=-hyhy*(trule1+trule6);
    vmiddle=(hxhx)*(trule1+trule2+trule3+trule4)+hyhy*(trule1+trule2+trule5+trule6);

    for (j=ys; j<ys+ym; j++){
      
      row=(j-gys)*gxm + (i-gxs);
       v[0]=0; v[1]=0; v[2]=0; v[3]=0; v[4]=0;
       
       k=0;
       if (j>gys){ 
	 v[k]=vdown; col[k]=row - gxm; k++;
       }
       
       if (i>gxs){
	 v[k]= vleft; col[k]=row - 1; k++;
       }

       v[k]= vmiddle; col[k]=row; k++;
       
       if (i+1 < gxs+gxm){
	 v[k]= vright; col[k]=row+1; k++;
       }
       
       if (j+1 <gys+gym){
	 v[k]= vup; col[k] = row+gxm; k++;
       }
       tt=0;
       for (kk=0;kk<k;kk++){
	 tt+=v[kk]*x[col[kk]];
       }
       row=(j-ys)*xm + (i-xs);
       g[row]=tt;

     }

  }

  ierr = VecRestoreArray(localX,&x); CHKERRQ(ierr);
  ierr = VecRestoreArray(G,&g); CHKERRQ(ierr);

  ierr = DMRestoreLocalVector(user->dm,&localX); CHKERRQ(ierr);

  ierr = VecDot(X,G,&f1); CHKERRQ(ierr);
  ierr = VecDot(user->B,X,&f2); CHKERRQ(ierr);
  ierr = VecAXPY(G, one, user->B); CHKERRQ(ierr);
  *fcn = f1/2.0 + f2;
  

  ierr = PetscLogFlops((91 + 10*ym) * xm); CHKERRQ(ierr);
  PetscFunctionReturn(0);

}



#undef __FUNCT__
#define __FUNCT__ "FormHessian"
/* 
   FormHessian computes the quadratic term in the quadratic objective function 
   Notice that the objective function in this problem is quadratic (therefore a constant
   hessian).  If using a nonquadratic solver, then you might want to reconsider this function
*/
PetscErrorCode FormHessian(TaoSolver tao,Vec X,Mat *H, Mat *Hpre, MatStructure *flg, void *ptr)
{
  AppCtx* user=(AppCtx*)ptr;
  PetscErrorCode ierr;
  PetscInt i,j,k;
  PetscInt col[5],row,nx,ny,xs,xm,gxs,gxm,ys,ym,gys,gym;
  PetscReal one=1.0, two=2.0, six=6.0,pi=4.0*atan(1.0);
  PetscReal hx,hy,hxhy,hxhx,hyhy;
  PetscReal xi,v[5];
  PetscReal ecc=user->ecc, trule1,trule2,trule3,trule4,trule5,trule6;
  PetscReal vmiddle, vup, vdown, vleft, vright;
  Mat hes=*H;
  PetscBool assembled;

  nx=user->nx;
  ny=user->ny;
  hx=two*pi/(nx+1.0);
  hy=two*user->b/(ny+1.0);
  hxhy=hx*hy;
  hxhx=one/(hx*hx);
  hyhy=one/(hy*hy);

  *flg=SAME_NONZERO_PATTERN;
  /*
    Get local grid boundaries
  */
  ierr = DMDAGetCorners(user->dm,&xs,&ys,PETSC_NULL,&xm,&ym,PETSC_NULL); CHKERRQ(ierr);
  ierr = DMDAGetGhostCorners(user->dm,&gxs,&gys,PETSC_NULL,&gxm,&gym,PETSC_NULL); CHKERRQ(ierr);
  
  ierr = MatAssembled(hes,&assembled); CHKERRQ(ierr);
  if (assembled){ierr = MatZeroEntries(hes);  CHKERRQ(ierr);}

  for (i=xs; i< xs+xm; i++){
    xi=(i+1)*hx;
    trule1=hxhy*( p(xi,ecc) + p(xi+hx,ecc) + p(xi,ecc) ) / six; /* L(i,j) */
    trule2=hxhy*( p(xi,ecc) + p(xi-hx,ecc) + p(xi,ecc) ) / six; /* U(i,j) */
    trule3=hxhy*( p(xi,ecc) + p(xi+hx,ecc) + p(xi+hx,ecc) ) / six; /* U(i+1,j) */
    trule4=hxhy*( p(xi,ecc) + p(xi-hx,ecc) + p(xi-hx,ecc) ) / six; /* L(i-1,j) */
    trule5=trule1; /* L(i,j-1) */
    trule6=trule2; /* U(i,j+1) */

    vdown=-(trule5+trule2)*hyhy;
    vleft=-hxhx*(trule2+trule4);
    vright= -hxhx*(trule1+trule3);
    vup=-hyhy*(trule1+trule6);
    vmiddle=(hxhx)*(trule1+trule2+trule3+trule4)+hyhy*(trule1+trule2+trule5+trule6);
    v[0]=0; v[1]=0; v[2]=0; v[3]=0; v[4]=0;

    for (j=ys; j<ys+ym; j++){
      row=(j-gys)*gxm + (i-gxs);
       
      k=0;
      if (j>gys){ 
	v[k]=vdown; col[k]=row - gxm; k++;
      }
       
      if (i>gxs){
	v[k]= vleft; col[k]=row - 1; k++;
      }

      v[k]= vmiddle; col[k]=row; k++;
       
      if (i+1 < gxs+gxm){
	v[k]= vright; col[k]=row+1; k++;
      }
       
      if (j+1 <gys+gym){
	v[k]= vup; col[k] = row+gxm; k++;
      }
      ierr = MatSetValuesLocal(hes,1,&row,k,col,v,INSERT_VALUES); CHKERRQ(ierr);
       
    }

  }

  /* 
     Assemble matrix, using the 2-step process:
     MatAssemblyBegin(), MatAssemblyEnd().
     By placing code between these two statements, computations can be
     done while messages are in transition.
  */
  ierr = MatAssemblyBegin(hes,MAT_FINAL_ASSEMBLY); CHKERRQ(ierr);
  ierr = MatAssemblyEnd(hes,MAT_FINAL_ASSEMBLY); CHKERRQ(ierr);

  /*
    Tell the matrix we will never add a new nonzero location to the
    matrix. If we do it will generate an error.
  */
  ierr = MatSetOption(hes,MAT_NEW_NONZERO_LOCATION_ERR,PETSC_TRUE); CHKERRQ(ierr);
  ierr = MatSetOption(hes,MAT_SYMMETRIC,PETSC_TRUE); CHKERRQ(ierr);

  ierr = PetscLogFlops(9*xm*ym+49*xm); CHKERRQ(ierr);
  ierr = MatNorm(hes,NORM_1,&hx); CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
