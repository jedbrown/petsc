#!/usr/bin/env python

import os,shutil, string, re
from distutils.sysconfig import parse_makefile
import sys
import logging, time
import types
sys.path.insert(0, os.path.abspath(os.path.dirname(__file__)))
from cmakegen import Mistakes, stripsplit, AUTODIRS, SKIPDIRS
from cmakegen import defaultdict # collections.defaultdict, with fallback for python-2.4
from gmakegen import *

import inspect
thisscriptdir = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))
sys.path.insert(0,thisscriptdir) 
import testparse
import example_template

class generateExamples(Petsc):
  """
    gmakegen.py has basic structure for finding the files, writing out
      the dependencies, etc.
  """
  def __init__(self,petsc_dir=None, petsc_arch=None, verbose=False, single_ex=False):
    super(generateExamples, self).__init__(petsc_dir=None, petsc_arch=None, verbose=False)

    self.single_ex=single_ex
    self.arch_dir=os.path.join(self.petsc_dir,self.petsc_arch)
    self.ptNaming=True
    # Whether to write out a useful debugging
    #if verbose: self.summarize=True
    self.summarize=True

    # For help in setting the requirements
    self.precision_types="single double __float128 int32".split()
    self.integer_types="int32 int64".split()
    self.languages="fortran cuda cxx".split()    # Always requires C so do not list

    # Things that are not test
    self.buildkeys=testparse.buildkeys

    # Adding a dictionary for storing sources, objects, and tests
    # to make building the dependency tree easier
    self.sources={}
    self.objects={}
    self.tests={}
    for pkg in PKGS:
      self.sources[pkg]={}
      self.objects[pkg]=[]
      self.tests[pkg]={}
      for lang in LANGS:
        self.sources[pkg][lang]={}
        self.sources[pkg][lang]['srcs']=[]
        self.tests[pkg][lang]={}

    # Do some initialization
    self.testroot_dir=os.path.join(self.arch_dir,"tests")
    if not os.path.isdir(self.testroot_dir): os.makedirs(self.testroot_dir)

    self.indent="   "
    return

  def nameSpace(self,srcfile,srcdir):
    """
    Because the scripts have a non-unique naming, the pretty-printing
    needs to convey the srcdir and srcfile.  There are two ways of doing this.
    """
    if self.ptNaming:
      cdir=srcdir.split('src')[1].lstrip("/").rstrip("/")
      prefix=cdir.replace('/examples/','_').replace("/","_")+"-"
      nameString=prefix+srcfile
    else:
      #nameString=srcdir+": "+srcfile
      nameString=srcfile
    return nameString

  def getLanguage(self,srcfile):
    """
    Based on the source, determine associated language as found in gmakegen.LANGS
    Can we just return srcext[1:\] now?
    """
    langReq=None
    srcext=os.path.splitext(srcfile)[-1]
    if srcext in ".F90".split(): langReq="F90"
    if srcext in ".F".split(): langReq="F"
    if srcext in ".cxx".split(): langReq="cxx"
    if srcext == ".cu": langReq="cu"
    if srcext == ".c": langReq="c"
    #if not langReq: print "ERROR: ", srcext, srcfile
    return langReq

  def _getLoopVars(self,inDict,testname, isSubtest=False):
    """
    Given: 'args: -bs {{1 2 3 4 5}} -pc_type {{cholesky sor}} -ksp_monitor'
    Return: 
      inDict['args']: -ksp_monitor
      inDict['subargs']: -bs ${bs} -pc_type ${pc_type}
      loopVars['subargs']['varlist']=['bs' 'pc_type']   # Don't worry about OrderedDict
      loopVars['subargs']['bs']=[["bs"],["1 2 3 4 5"]]
      loopVars['subargs']['pc_type']=[["pc_type"],["cholesky sor"]]
    subst should be passed in instead of inDict
    """
    loopVars={}; newargs=""
    lkeys=inDict.keys()
    lsuffix='_'
    from testparse import parseLoopArgs
    for key in lkeys:
      if type(inDict[key])!=types.StringType: continue
      keystr = str(inDict[key])
      akey=('subargs' if key=='args' else key)  # what to assign
      if akey not in inDict: inDict[akey]=''
      varlist=[]
      for varset in re.split('-(?=[a-zA-Z])',keystr):
        if not varset.strip(): continue
        if '{{' in varset:
          keyvar,lvars,ftype=parseLoopArgs(varset)
          if akey not in loopVars: loopVars[akey]={}
          varlist.append(keyvar)
          loopVars[akey][keyvar]=[keyvar,lvars]
          if akey=='nsize':
            inDict[akey] = '${' + keyvar + '}'
            lsuffix+=akey+'-'+inDict[akey]+'_'
          else:
            inDict[akey] += ' -'+keyvar+' ${' + keyvar + '}'
            lsuffix+=keyvar+'-${' + keyvar + '}_'
        else:
          if key=='args': newargs+=" -"+varset.strip()
        if len(varlist)>0: loopVars[akey]['varlist']=varlist

      
    # For subtests, args are always substituted in (not top level)
    if isSubtest:
      inDict['subargs']+=" "+newargs.strip()
      inDict['args']=''
      if 'label_suffix' in inDict:
        inDict['label_suffix']+=lsuffix.rstrip('_')
      else:
        inDict['label_suffix']=lsuffix.rstrip('_')
    else:
      if len(loopVars.keys())>0: 
        inDict['args']=newargs.strip()
        inDict['label_suffix']=lsuffix.rstrip('_')
    if len(loopVars.keys())>0:
      return loopVars
    else:
      return None

  def getArgLabel(self,testDict):
    """
    In all of the arguments in the test dictionary, create a simple
    string for searching within the makefile system.  For simplicity in
    search, remove "-", for strings, etc.
    Also, concatenate the arg commands
    For now, ignore nsize -- seems hard to search for anyway
    """
    # Collect all of the args associated with a test
    argStr=("" if 'args' not in testDict else testDict['args'])
    if 'subtests' in testDict:
      for stest in testDict["subtests"]:
         sd=testDict[stest]
         argStr=argStr+("" if 'args' not in sd else sd['args'])

    # Now go through and cleanup
    argStr=re.sub('{{(.*?)}}',"",argStr)
    argStr=re.sub('-'," ",argStr)
    for digit in string.digits: argStr=re.sub(digit," ",argStr)
    argStr=re.sub("\.","",argStr)
    argStr=re.sub(",","",argStr)
    argStr=re.sub('\+',' ',argStr)
    argStr=re.sub(' +',' ',argStr)  # Remove repeated white space
    return argStr.strip()

  def addToSources(self,exfile,root,srcDict):
    """
      Put into data structure that allows easy generation of makefile
    """
    pkg=self.relpath(self.petsc_dir,root).split("/")[1]
    fullfile=os.path.join(root,exfile)
    relpfile=self.relpath(self.petsc_dir,fullfile)
    lang=self.getLanguage(exfile)
    if not lang: return
    self.sources[pkg][lang]['srcs'].append(relpfile)
    if 'depends' in srcDict:
      depSrc=srcDict['depends']
      depObj=os.path.splitext(depSrc)[0]+".o"
      self.sources[pkg][lang][exfile]=depObj

    # In gmakefile, ${TESTDIR} var specifies the object compilation
    testsdir=self.relpath(self.petsc_dir,root)+"/"
    objfile="${TESTDIR}/"+testsdir+os.path.splitext(exfile)[0]+".o"
    self.objects[pkg].append(objfile)
    return

  def addToTests(self,test,root,exfile,execname,testDict):
    """
      Put into data structure that allows easy generation of makefile
      Organized by languages to allow testing of languages
    """
    pkg=self.relpath(self.petsc_dir,root).split("/")[1]
    #nmtest=self.nameSpace(test,root)
    rpath=self.relpath(self.petsc_dir,root)
    nmtest=os.path.join(rpath,test)
    lang=self.getLanguage(exfile)
    if not lang: return
    self.tests[pkg][lang][nmtest]={}
    self.tests[pkg][lang][nmtest]['exfile']=os.path.join(rpath,exfile)
    self.tests[pkg][lang][nmtest]['exec']=execname
    self.tests[pkg][lang][nmtest]['argLabel']=self.getArgLabel(testDict)
    return

  def getExecname(self,exfile,root):
    """
      Generate bash script using template found next to this file.  
      This file is read in at constructor time to avoid file I/O
    """
    rpath=self.relpath(self.petsc_dir,root)
    if self.single_ex:
      execname=rpath.split("/")[1]+"-ex"
    else:
      execname=os.path.splitext(exfile)[0]
    return execname

  def getSubstVars(self,testDict,rpath,testname):
    """
      Create a dictionary with all of the variables that get substituted
      into the template commands found in example_template.py
    """
    subst={}

    # Handle defaults of testparse.acceptedkeys (e.g., ignores subtests)
    if 'nsize' not in testDict: testDict['nsize']=1
    for ak in testparse.acceptedkeys: 
      if ak=='test': continue
      subst[ak]=(testDict[ak] if ak in testDict else '')

    # Now do other variables
    subst['execname']=testDict['execname']
    if 'filter' in testDict:
      subst['filter']="'"+testDict['filter']+"'"   # Quotes are tricky - overwrite

    # Others
    subst['subargs']=''  # Default.  For variables override
    subst['srcdir']=os.path.join(self.petsc_dir,rpath)
    subst['label_suffix']=''
    subst['comments']="\n#".join(subst['comments'].split("\n"))
    if subst['comments']: subst['comments']="#"+subst['comments']
    subst['exec']="../"+subst['execname']
    subst['testroot']=self.testroot_dir
    subst['testname']=testname
    dp = self.conf.get('DATAFILESPATH','')
    subst['datafilespath_line'] = 'DATAFILESPATH=${DATAFILESPATH:-"'+dp+'"}'

    # This is used to label some matrices
    subst['petsc_index_size']=str(self.conf['PETSC_INDEX_SIZE'])
    subst['petsc_scalar_size']=str(self.conf['PETSC_SCALAR_SIZE'])

    # These can have for loops and are treated separately later
    subst['nsize']=str(subst['nsize'])

    #Conf vars
    if self.petsc_arch.find('valgrind')>=0:
      subst['mpiexec']='petsc_mpiexec_valgrind ' + self.conf['MPIEXEC']
    else:
      subst['mpiexec']=self.conf['MPIEXEC']
    subst['petsc_dir']=self.petsc_dir # not self.conf['PETSC_DIR'] as this could be windows path
    subst['diff']=self.conf['DIFF']
    subst['rm']=self.conf['RM']
    subst['grep']=self.conf['GREP']
    subst['petsc_lib_dir']=self.conf['PETSC_LIB_DIR']
    subst['wpetsc_dir']=self.conf['wPETSC_DIR']

    # Output file is special because of subtests override
    defroot=(re.sub("run","",testname) if testname.startswith("run") else testname)
    if not "_" in defroot: defroot=defroot+"_1"
    subst['defroot']=defroot
    subst['label']=self.nameSpace(defroot,subst['srcdir'])
    subst['redirect_file']=defroot+".tmp"
    if 'output_file' not in testDict: 
      subst['output_file']="output/"+defroot+".out"
    # Add in the full path here.
    subst['output_file']=os.path.join(subst['srcdir'],subst['output_file'])
    if not os.path.isfile(os.path.join(self.petsc_dir,subst['output_file'])):
      if not subst['TODO']:
        print "Warning: "+subst['output_file']+" not found."
    # Worry about alt files here -- see
    #   src/snes/examples/tutorials/output/ex22*.out
    altlist=[subst['output_file']]
    for i in range(1,3):
      altroot=defroot+"_alt"
      if i==2: altroot=altroot+"_2"
      af="output/"+altroot+".out"
      srcaf=os.path.join(subst['srcdir'],af)
      fullaf=os.path.join(self.petsc_dir,srcaf)
      if os.path.isfile(fullaf): altlist.append(srcaf)
    if len(altlist)>1: subst['altfiles']=altlist
    #if len(altlist)>1: print "Found alt files: ",altlist

    return subst

  def getCmds(self,subst,i):
    """
      Generate bash script using template found next to this file.  
      This file is read in at constructor time to avoid file I/O
    """
    indnt=self.indent
    nindnt=i # the start and has to be consistent with below
    cmdLines=""

    # MPI is the default -- but we have a few odd commands
    if not subst['command']:
      cmd=indnt*nindnt+self._substVars(subst,example_template.mpitest)
    else:
      cmd=indnt*nindnt+self._substVars(subst,example_template.commandtest)
    cmdLines+=cmd+"\n\n"

    if not subst['filter_output']:
      if 'altfiles' not in subst:
        cmd=indnt*nindnt+self._substVars(subst,example_template.difftest)
      else:
        # Have to do it by hand a bit because of variable number of alt files
        rf=subst['redirect_file']
        cmd=indnt*nindnt+example_template.difftest.split('@')[0]
        for i in range(len(subst['altfiles'])):
          af=subst['altfiles'][i]
          cmd+=af+' '+rf+' > diff-${testname}-'+str(i)+'.out 2> diff-${testname}-'+str(i)+'.out'
          if i!=len(subst['altfiles'])-1:
            cmd+=' || ${diff_exe} '
          else:
            cmd+='" diff-${testname}.out diff-${testname}.out diff-${label}'
            cmd+=subst['label_suffix']+' ""'  # Quotes are painful
    else:
      cmd=indnt*nindnt+self._substVars(subst,example_template.filterdifftest)
    cmdLines+=cmd+"\n"
    return cmdLines

  def _substVars(self,subst,origStr):
    """
      Substitute variables
    """
    Str=origStr
    for subkey in subst:
      if type(subst[subkey])!=types.StringType: continue
      patt="@"+subkey.upper()+"@"
      Str=re.sub(patt,subst[subkey],Str)
    return Str

  def _writeTodoSkip(self,fh,tors,reasons,footer):
    """
    Write out the TODO and SKIP lines in the file
    The TODO or SKIP variable, tors, should be lower case
    """
    TORS=tors.upper()
    template=eval("example_template."+tors+"line")
    tsStr=re.sub("@"+TORS+"COMMENT@",', '.join(reasons),template)
    tab = ''
    if reasons == ["Requires DATAFILESPATH"]:
      # The only reason not to run is DATAFILESPATH, which we check at run-time
      tab = '    '
      fh.write('if test -z "${DATAFILESPATH}"; then\n')
    if reasons:
      fh.write(tab+tsStr+"\ntotal=1; "+tors+"=1\n")
      fh.write(tab+footer+"\n")
      fh.write(tab+"exit\n")
    if reasons == ["Requires DATAFILESPATH"]:
      fh.write('fi\n')
    fh.write('\n\n')
    return

  def getLoopVarsHead(self,loopVars,i):
    """
    Generate a nicely indented string with the format loops
    Here is what the data structure looks like
      loopVars['subargs']['varlist']=['bs' 'pc_type']   # Don't worry about OrderedDict
      loopVars['subargs']['bs']=["i","1 2 3 4 5"]
      loopVars['subargs']['pc_type']=["j","cholesky sor"]
    """
    outstr=''; indnt=self.indent
    for key in loopVars:
      for var in loopVars[key]['varlist']:
        varval=loopVars[key][var]
        outstr += indnt * i + "for "+varval[0]+" in "+varval[1]+"; do\n"
        i = i + 1
    return (outstr,i)

  def getLoopVarsFoot(self,loopVars,i):
    outstr=''; indnt=self.indent
    for key in loopVars:
      for var in loopVars[key]['varlist']:
        i = i - 1
        outstr += indnt * i + "done\n"
    return (outstr,i)

  def genRunScript(self,testname,root,isRun,srcDict):
    """
      Generate bash script using template found next to this file.  
      This file is read in at constructor time to avoid file I/O
    """
    # runscript_dir directory has to be consistent with gmakefile
    testDict=srcDict[testname]
    rpath=self.relpath(self.petsc_dir,root)
    runscript_dir=os.path.join(self.testroot_dir,rpath)
    if not os.path.isdir(runscript_dir): os.makedirs(runscript_dir)
    fh=open(os.path.join(runscript_dir,testname+".sh"),"w")
    petscvarfile=os.path.join(self.arch_dir,'lib','petsc','conf','petscvariables')

    # Get variables to go into shell scripts.  last time testDict used
    subst=self.getSubstVars(testDict,rpath,testname)
    loopVars = self._getLoopVars(subst,testname)  # Alters subst as well
    #if '33_' in testname: print subst['subargs']

    #Handle runfiles
    for lfile in subst.get('localrunfiles','').split():
      fullfile=os.path.join(self.petsc_dir,rpath,lfile)
      shutil.copy(fullfile,runscript_dir)
    # Check subtests for local runfiles
    for stest in subst.get("subtests",[]):
      for lfile in testDict[stest].get('localrunfiles','').split():
        fullfile=os.path.join(self.petsc_dir,rpath,lfile)
        shutil.copy(fullfile,self.runscript_dir)

    # Now substitute the key variables into the header and footer
    header=self._substVars(subst,example_template.header)
    # The header is done twice to enable @...@ in header
    header=self._substVars(subst,header)
    footer=re.sub('@TESTROOT@',subst['testroot'],example_template.footer)

    # Start writing the file
    fh.write(header+"\n")

    # If there is a TODO or a SKIP then we do it before writing out the
    # rest of the command (which is useful for working on the test)
    # SKIP and TODO can be for the source file or for the runs
    self._writeTodoSkip(fh,'todo',[s for s in [srcDict.get('TODO',''), testDict.get('TODO','')] if s],footer)
    self._writeTodoSkip(fh,'skip',srcDict.get('SKIP',[]) + testDict.get('SKIP',[]),footer)

    j=0  # for indentation 

    if loopVars:
      (loopHead,j) = self.getLoopVarsHead(loopVars,j)
      if (loopHead): fh.write(loopHead+"\n")

    # Subtests are special
    if 'subtests' in testDict:
      substP=subst   # Subtests can inherit args but be careful
      for stest in testDict["subtests"]:
        subst=substP.copy()
        subst.update(testDict[stest])
        subst['nsize']=str(subst['nsize'])
        sLoopVars = self._getLoopVars(subst,testname,isSubtest=True)
        #if '10_9' in testname: print sLoopVars
        if sLoopVars: 
          (sLoopHead,j) = self.getLoopVarsHead(sLoopVars,j)
          fh.write(sLoopHead+"\n")
        fh.write(self.getCmds(subst,j)+"\n")
        if sLoopVars: 
          (sLoopFoot,j) = self.getLoopVarsFoot(sLoopVars,j)
          fh.write(sLoopFoot+"\n")
    else:
      fh.write(self.getCmds(subst,j)+"\n")

    if loopVars: 
      (loopFoot,j) = self.getLoopVarsFoot(loopVars,j)
      fh.write(loopFoot+"\n")

    fh.write(footer+"\n")
    os.chmod(os.path.join(runscript_dir,testname+".sh"),0755)
    #if '10_9' in testname: sys.exit()
    return

  def  genScriptsAndInfo(self,exfile,root,srcDict):
    """
    Generate scripts from the source file, determine if built, etc.
     For every test in the exfile with info in the srcDict:
      1. Determine if it needs to be run for this arch
      2. Generate the script
      3. Generate the data needed to write out the makefile in a
         convenient way
     All tests are *always* run, but some may be SKIP'd per the TAP standard
    """
    debug=False
    fileIsTested=False
    execname=self.getExecname(exfile,root)
    isBuilt=self._isBuilt(exfile,srcDict)
    for test in srcDict:
      if test in self.buildkeys: continue
      if debug: print self.nameSpace(exfile,root), test
      srcDict[test]['execname']=execname   # Convenience in generating scripts
      isRun=self._isRun(srcDict[test])
      self.genRunScript(test,root,isRun,srcDict)
      srcDict[test]['isrun']=isRun
      if isRun: fileIsTested=True
      self.addToTests(test,root,exfile,execname,srcDict[test])

    # This adds to datastructure for building deps
    if fileIsTested and isBuilt: self.addToSources(exfile,root,srcDict)
    #print self.nameSpace(exfile,root), fileIsTested
    return

  def _isBuilt(self,exfile,srcDict):
    """
    Determine if this file should be built. 
    """
    # Get the language based on file extension
    srcDict['SKIP'] = []
    lang=self.getLanguage(exfile)
    if (lang=="F" or lang=="F90") and not self.have_fortran: 
      srcDict["SKIP"].append("Fortran required for this test")
    if lang=="cu" and 'PETSC_HAVE_CUDA' not in self.conf: 
      srcDict["SKIP"].append("CUDA required for this test")
    if lang=="cxx" and 'PETSC_HAVE_CXX' not in self.conf: 
      srcDict["SKIP"].append("C++ required for this test")

    # Deprecated source files
    if srcDict.get("TODO"):
      return False

    # isRun can work with srcDict to handle the requires
    if "requires" in srcDict: 
      if len(srcDict["requires"])>0: 
        return self._isRun(srcDict)

    return srcDict['SKIP'] == []


  def _isRun(self,testDict):
    """
    Based on the requirements listed in the src file and the petscconf.h
    info, determine whether this test should be run or not.
    """
    indent="  "
    debug=False

    if 'SKIP' not in testDict:
      testDict['SKIP'] = []
    # MPI requirements
    if testDict.get('nsize',1)>1 and 'MPI_IS_MPIUNI' in self.conf:
      if debug: print indent+"Cannot run parallel tests"
      testDict['SKIP'].append("Parallel test with serial build")
 
    # The requirements for the test are the sum of all the run subtests
    if 'subtests' in testDict:
      if 'requires' not in testDict: testDict['requires']=""
      for stest in testDict['subtests']:
        if 'requires' in testDict[stest]:
          testDict['requires']+=" "+testDict[stest]['requires']


    # Now go through all requirements
    if 'requires' in testDict:
      for requirement in testDict['requires'].split():
        requirement=requirement.strip()
        if not requirement: continue
        if debug: print indent+"Requirement: ", requirement
        isNull=False
        if requirement.startswith("!"):
          requirement=requirement[1:]; isNull=True
        # Precision requirement for reals
        if requirement in self.precision_types:
          if self.conf['PETSC_PRECISION']==requirement:
            if isNull:
              testDict['SKIP'].append("not "+requirement+" required")
              continue
            continue  # Success
          elif not isNull:
            testDict['SKIP'].append(requirement+" required")
            continue
        # Precision requirement for ints
        if requirement in self.integer_types:
          if requirement=="int32":
            if self.conf['PETSC_SIZEOF_INT']==4:
              if isNull:
                testDict['SKIP'].append("not int32 required")
                continue
              continue  # Success
            elif not isNull:
              testDict['SKIP'].append("int32 required")
              continue
          if requirement=="int64":
            if self.conf['PETSC_SIZEOF_INT']==8:
              if isNull:
                testDict['SKIP'].append("NOT int64 required")
                continue
              continue  # Success
            elif not isNull:
              testDict['SKIP'].append("int64 required")
              continue
        # Datafilespath
        if requirement=="datafilespath" and not isNull:
          testDict['SKIP'].append("Requires DATAFILESPATH")
          continue
        # Defines -- not sure I have comments matching
        if "define(" in requirement.lower():
          reqdef=requirement.split("(")[1].split(")")[0]
          if reqdef in self.conf:
            if isNull:
              testDict['SKIP'].append("Null requirement not met: "+requirement)
              continue
            continue  # Success
          elif not isNull:
            testDict['SKIP'].append("Required: "+requirement)
            continue

        # Rest should be packages that we can just get from conf
        if requirement == "complex":
          petscconfvar="PETSC_USE_COMPLEX"
        else:
          petscconfvar="PETSC_HAVE_"+requirement.upper()
        if self.conf.get(petscconfvar):
          if isNull:
            testDict['SKIP'].append("Not "+petscconfvar+" requirement not met")
            continue
          continue  # Success
        elif not isNull:
          if debug: print "requirement not found: ", requirement
          testDict['SKIP'].append(petscconfvar+" requirement not met")
          continue

    return testDict['SKIP'] == []

  def genPetscTests_summarize(self,dataDict):
    """
    Required method to state what happened
    """
    if not self.summarize: return
    indent="   "
    fhname=os.path.join(self.testroot_dir,'GenPetscTests_summarize.txt')
    fh=open(fhname,"w")
    #print "See ", fhname
    for root in dataDict:
      relroot=self.relpath(self.petsc_dir,root)
      pkg=relroot.split("/")[1]
      fh.write(relroot+"\n")
      allSrcs=[]
      for lang in LANGS: allSrcs=allSrcs+self.sources[pkg][lang]['srcs']
      for exfile in dataDict[root]:
        # Basic  information
        fullfile=os.path.join(root,exfile)
        rfile=self.relpath(self.petsc_dir,fullfile)
        builtStatus=(" Is built" if rfile in allSrcs else " Is NOT built")
        fh.write(indent+exfile+indent*4+builtStatus+"\n")

        for test in dataDict[root][exfile]:
          if test in self.buildkeys: continue
          line=indent*2+test
          fh.write(line+"\n")
          # Looks nice to have the keys in order
          #for key in dataDict[root][exfile][test]:
          for key in "isrun abstracted nsize args requires script".split():
            if key not in dataDict[root][exfile][test]: continue
            line=indent*3+key+": "+str(dataDict[root][exfile][test][key])
            fh.write(line+"\n")
          fh.write("\n")
        fh.write("\n")
      fh.write("\n")
    #fh.write("\nClass Sources\n"+str(self.sources)+"\n")
    #fh.write("\nClass Tests\n"+str(self.tests)+"\n")
    fh.close()
    return

  def genPetscTests(self,root,dirs,files,dataDict):
    """
     Go through and parse the source files in the directory to generate
     the examples based on the metadata contained in the source files
    """
    debug=False
    # Use examplesAnalyze to get what the makefles think are sources
    #self.examplesAnalyze(root,dirs,files,anlzDict)

    dataDict[root]={}

    for exfile in files:
      #TST: Until we replace files, still leaving the orginals as is
      #if not exfile.startswith("new_"+"ex"): continue
      if not exfile.startswith("ex"): continue

      # Convenience
      fullex=os.path.join(root,exfile)
      relpfile=self.relpath(self.petsc_dir,fullex)
      if debug: print relpfile
      dataDict[root].update(testparse.parseTestFile(fullex,0))
      # Need to check and make sure tests are in the file
      # if verbosity>=1: print relpfile
      if exfile in dataDict[root]:
        self.genScriptsAndInfo(exfile,root,dataDict[root][exfile])

    return

  def walktree(self,top,action="printFiles"):
    """
    Walk a directory tree, starting from 'top'
    """
    #print "action", action
    # Goal of action is to fill this dictionary
    dataDict={}
    for root, dirs, files in os.walk(top, topdown=False):
      if not "examples" in root: continue
      if not os.path.isfile(os.path.join(root,"makefile")): continue
      bname=os.path.basename(root.rstrip("/"))
      if bname=="tests" or bname=="tutorials":
        eval("self."+action+"(root,dirs,files,dataDict)")
      if type(top) != types.StringType:
          raise TypeError("top must be a string")
    # Now summarize this dictionary
    eval("self."+action+"_summarize(dataDict)")
    return dataDict

  def gen_gnumake(self, fd):
    """
     Overwrite of the method in the base PETSc class 
    """
    def write(stem, srcs):
        for lang in LANGS:
            fd.write('%(stem)s.%(lang)s := %(srcs)s\n' % dict(stem=stem, lang=lang, srcs=' '.join(srcs[lang]['srcs'])))
    for pkg in PKGS:
        srcs = self.gen_pkg(pkg)
        write('testsrcs-' + pkg, srcs)
    return self.gendeps

  def gen_pkg(self, pkg):
    """
     Overwrite of the method in the base PETSc class 
    """
    return self.sources[pkg]

  def write_gnumake(self,dataDict):
    """
     Write out something similar to files from gmakegen.py

     Test depends on script which also depends on source
     file, but since I don't have a good way generating
     acting on a single file (oops) just depend on
     executable which in turn will depend on src file
    """
    # Different options for how to set up the targets
    compileExecsFirst=False

    # Open file
    arch_files = self.arch_path('lib','petsc','conf', 'testfiles')
    fd = open(arch_files, 'w')

    # Write out the sources
    gendeps = self.gen_gnumake(fd)

    # Write out the tests and execname targets
    fd.write("\n#Tests and executables\n")    # Delimiter

    for pkg in PKGS:
      # These grab the ones that are built
      for lang in LANGS:
        testdeps=[]
        for ftest in self.tests[pkg][lang]:
          test=os.path.basename(ftest)
          basedir=os.path.dirname(ftest)
          testdeps.append(self.nameSpace(test,basedir))
        fd.write("test-"+pkg+"."+lang+" := "+' '.join(testdeps)+"\n")
        fd.write('test-%s.%s : $(test-%s.%s)\n' % (pkg, lang, pkg, lang))

        # test targets
        for ftest in self.tests[pkg][lang]:
          test=os.path.basename(ftest)
          basedir=os.path.dirname(ftest)
          testdir="${TESTDIR}/"+basedir+"/"
          nmtest=self.nameSpace(test,basedir)
          rundir=os.path.join(testdir,test)
          #print test, nmtest
          script=test+".sh"

          # Deps
          exfile=self.tests[pkg][lang][ftest]['exfile']
          fullex=os.path.join(self.petsc_dir,exfile)
          localexec=self.tests[pkg][lang][ftest]['exec']
          execname=os.path.join(testdir,localexec)
          fullscript=os.path.join(testdir,script)
          tmpfile=os.path.join(testdir,test,test+".tmp")

          # *.counts depends on the script and either executable (will
          # be run) or the example source file (SKIP or TODO)
          fd.write('%s.counts : %s %s\n'
                   % (os.path.join('$(TESTDIR)/counts', nmtest),
                      fullscript,
                      execname if exfile in self.sources[pkg][lang]['srcs'] else fullex))
          # Now write the args:
          fd.write(nmtest+"_ARGS := '"+self.tests[pkg][lang][ftest]['argLabel']+"'\n")

    fd.close()
    return

  def writeHarness(self,output,dataDict):
    """
     This is set up to write out multiple harness even if only gnumake
     is supported now
    """
    eval("self.write_"+output+"(dataDict)")
    return

def main(petsc_dir=None, petsc_arch=None, output=None, verbose=False, single_ex=False):
    if output is None:
        output = 'gnumake'


    pEx=generateExamples(petsc_dir=petsc_dir, petsc_arch=petsc_arch, verbose=verbose, single_ex=single_ex)
    dataDict=pEx.walktree(os.path.join(pEx.petsc_dir,'src'),action="genPetscTests")
    pEx.writeHarness(output,dataDict)

if __name__ == '__main__':
    import optparse
    parser = optparse.OptionParser()
    parser.add_option('--verbose', help='Show mismatches between makefiles and the filesystem', action='store_true', default=False)
    parser.add_option('--petsc-arch', help='Set PETSC_ARCH different from environment', default=os.environ.get('PETSC_ARCH'))
    parser.add_option('--output', help='Location to write output file', default=None)
    parser.add_option('-s', '--single_executable', dest='single_executable', action="store_false", help='Whether there should be single executable per src subdir.  Default is false')
    opts, extra_args = parser.parse_args()
    if extra_args:
        import sys
        sys.stderr.write('Unknown arguments: %s\n' % ' '.join(extra_args))
        exit(1)
    main(petsc_arch=opts.petsc_arch, output=opts.output, verbose=opts.verbose, single_ex=opts.single_executable)
