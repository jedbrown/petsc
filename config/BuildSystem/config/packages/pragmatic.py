import config.package

class Configure(config.package.CMakePackage):
  def __init__(self, framework):
    import os
    config.package.CMakePackage.__init__(self, framework)
    self.download          = ['git://https://github.com/meshadaptation/pragmatic.git']
    self.gitcommit         = '29b85a2c29b3344c67abdf6dc010a998bec03cf0'
    self.functions         = ['pragmatic_2d_init']
    self.includes          = ['pragmatic.h']
    self.liblist           = [['libpragmatic.a']]
    self.includedir        = os.path.join('include', 'pragmatic')
    return

  def setupDependencies(self, framework):
    config.package.CMakePackage.setupDependencies(self, framework)
    self.compilerFlags   = framework.require('config.compilerFlags', self)
    self.sharedLibraries = framework.require('PETSc.options.sharedLibraries', self)
    self.scalartypes     = framework.require('PETSc.options.scalarTypes',self)
    self.indexTypes      = framework.require('PETSc.options.indexTypes', self)
    self.metis           = framework.require('config.packages.metis', self)
    self.eigen           = framework.require('config.packages.eigen', self)
    self.mathlib         = framework.require('config.packages.mathlib',self)
    self.deps            = [self.metis, self.eigen, self.mathlib]
    return

  def formCMakeConfigureArgs(self):
    if not self.cmake.found:
      raise RuntimeError('CMake > 2.5 is needed to build Pragmatic')

    args = config.package.CMakePackage.formCMakeConfigureArgs(self)
    args.append('-DMETIS_DIR='+self.metis.getInstallDir())
    args.append('-DENABLE_VTK=OFF')
    args.append('-DENABLE_OPENMP=OFF')
    args.append('-DEIGEN_INCLUDE_DIR='+self.eigen.include[0])
    if not self.compilerFlags.debugging:
      args.append('-DCMAKE_BUILD_TYPE=Release')
    if self.checkSharedLibrariesEnabled():
      args.append('-DCMAKE_INSTALL_RPATH_USE_LINK_PATH:BOOL=ON')
    if self.indexTypes.integerSize == 64:
      raise RuntimeError('Pragmatic cannot be built with 64-bit integers')
    if self.scalartypes.precision == 'single':
      raise RuntimeError('Pragmatic cannot be built with single precision')
    elif self.scalartypes.precision == 'quad':
      raise RuntimeError('Pragmatic cannot be built with quad precision')
    return args
