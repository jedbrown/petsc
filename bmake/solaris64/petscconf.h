#ifdef PETSC_RCS_HEADER
"$Id: petscconf.h,v 1.24 1999/08/16 18:35:06 balay Exp balay $"
"Defines the configuration for this machine"
#endif

#if !defined(INCLUDED_PETSCCONF_H)
#define INCLUDED_PETSCCONF_H

#define PARCH_solaris 

#define PETSC_USE_CTABLE
#define PETSC_HAVE_LIMITS_H
#define PETSC_HAVE_STROPTS_H 
#define PETSC_HAVE_SEARCH_H 
#define PETSC_HAVE_PWD_H 
#define PETSC_HAVE_STRING_H 
#define PETSC_HAVE_MALLOC_H
#define PETSC_HAVE_STDLIB_H
#define PETSC_HAVE_UNISTD_H 
#define PETSC_HAVE_DRAND48 
#define PETSC_HAVE_SYS_TIME_H
#define PETSC_HAVE_SYS_SYSTEMINFO_H
#define PETSC_HAVE_SYSINFO
#define PETSC_HAVE_SUNMATH_H
#define PETSC_HAVE_SUNMATHPRO
#define PETSC_HAVE_STD_COMPLEX

#define PETSC_HAVE_FORTRAN_UNDERSCORE

#define PETSC_HAVE_READLINK
#define PETSC_HAVE_MEMMOVE

#define PETSC_HAVE_DOUBLE_ALIGN
#define PETSC_HAVE_DOUBLE_ALIGN_MALLOC

#define PETSC_HAVE_MEMALIGN
#define PETSC_USE_DBX_DEBUGGER
#define PETSC_HAVE_SYS_RESOURCE_H

#define PETSC_HAVE_SYS_PROCFS_H
#define PETSC_HAVE_FCNTL_H
#define PETSC_SIZEOF_VOIDP  8
#define PETSC_SIZEOF_INT    4
#define PETSC_SIZEOF_DOUBLE 8

#define PETSC_WORDS_BIGENDIAN 1

#define PETSC_HAVE_RTLD_GLOBAL 1

#endif
