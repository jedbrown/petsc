#ifdef PETSC_RCS_HEADER
"$Id: petscconf.h,v 1.18 1999/07/07 14:42:09 bsmith Exp balay $"
"Defines the configuration for this machine"
#endif

#if !defined(INCLUDED_PETSCCONF_H)
#define INCLUDED_PETSCCONF_H

#define PARCH_sun4 
#define PETSC_HAVE_LIMITS_H
#define PETSC_HAVE_STDLIB_H
#define PETSC_HAVE_STROPTS_H 
#define PETSC_HAVE_SEARCH_H 
#define PETSC_HAVE_PWD_H 
#define PETSC_HAVE_STRING_H 
#define PETSC_HAVE_MALLOC_H 
#define PETSC_HAVE_X11 
#define PETSC_HAVE_STRINGS_H 
#define PETSC_HAVE_DRAND48 
#define PETSC_HAVE_GETDOMAINNAME  
#define PETSC_HAVE_UNISTD_H 
#define PETSC_HAVE_SYS_TIME_H 
#define PETSC_NEEDS_GETTIMEOFDAY_PROTO
#define PETSC_HAVE_UNAME

#define PETSC_HAVE_FORTRAN_UNDERSCORE

#define PETSC_HAVE_READLINK
#define PETSC_HAVE_GETWD
#define PETSC_HAVE_REALPATH
#define PETSC_HAVE_SLOW_NRM2

#define PETSC_HAVE_DOUBLE_ALIGN
#define PETSC_HAVE_DOUBLE_ALIGN_MALLOC

#define PETSC_HAVE_MEMALIGN

#define PETSC_HAVE_MALLOC_VERIFY
#define PETSC_HAVE_SYS_RESOURCE_H
#define PETSC_SIZEOF_VOIDP 4
#define PETSC_SIZEOF_INT 4
#define PETSC_SIZEOF_DOUBLE 8

#define PETSC_WORDS_BIGENDIAN 1

#define PETSC_USE_DYNAMIC_LIBRARIES 1
#define PETSC_HAVE_TEMPLATED_COMPLEX

#define PETSC_HAVE_4ARG_SIGNAL_HANDLER
#define PETSC_NEED_SETSOCKETOPT_PROTO
#define PETSC_NEED_CONNECT_PROTO
#define PETSC_NEED_SOCKET_PROTO

#endif
