#!/usr/bin/env python
"""
Parse the test file and return a dictionary.

Quick usage::

  bin/maint/testparse.py -t src/ksp/ksp/examples/tutorials/ex1.c

From the command line, it prints out the dictionary.  
This is meant to be used by other scripts, but it is 
useful to debug individual files.



Example language
----------------
/*T
   Concepts:
   requires: moab
T*/



/*TEST

   # This is equivalent to test:
   testset:
      args: -pc_type mg -ksp_type fgmres -da_refine 2 -ksp_monitor_short -mg_levels_ksp_monitor_short -mg_levels_ksp_norm_type unpreconditioned -ksp_view -pc_mg_type full

   testset:
      suffix: 2
      nsize: 2
      args: -pc_type mg -ksp_type fgmres -da_refine 2 -ksp_monitor_short -mg_levels_ksp_monitor_short -mg_levels_ksp_norm_type unpreconditioned -ksp_view -pc_mg_type full

   testset:
      suffix: 2
      nsize: 2
      args: -pc_type mg -ksp_type fgmres -da_refine 2 -ksp_monitor_short -mg_levels_ksp_monitor_short -mg_levels_ksp_norm_type unpreconditioned -ksp_view -pc_mg_type full
      test:

TEST*/

"""

import os, re, glob, types
from distutils.sysconfig import parse_makefile
import sys
import logging
sys.path.insert(0, os.path.abspath(os.path.dirname(__file__)))

import inspect
thisscriptdir = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))
maintdir=os.path.join(os.path.join(os.path.dirname(thisscriptdir),'bin'),'maint')
sys.path.insert(0,maintdir) 
# Don't print out trace when raise Exceptions 
sys.tracebacklimit = 0

# These are special keys describing build
buildkeys="requires TODO SKIP depends".split()

acceptedkeys="test nsize requires command suffix args filter filter_output localrunfiles comments TODO SKIP output_file".split()
appendlist="args requires comments".split()

import re

def _stripIndent(block,srcfile,entireBlock=False,fileNums=[]):
  """
  Go through and remove a level of indentation
  Also strip of trailing whitespace
  """
  # The first entry should be test: but it might be indented. 
  ext=os.path.splitext(srcfile)[1]
  stripstr=" "
  if len(fileNums)>0: lineNum=fileNums[0]-1
  for lline in block.split("\n"):
    if len(fileNums)>0: lineNum+=1
    line=lline[1:] if lline.startswith("!") else lline
    if not line.strip(): continue
    if line.strip().startswith('#'): continue
    if entireBlock:
      var=line.split(":")[0].strip()
      if not (var=='test' or var=='testset'): 
        raise Exception("Formatting error: Cannot find test in file: "+srcfile+" at line: "+str(lineNum)+"\n")
    nspace=len(line)-len(line.lstrip(stripstr))
    newline=line[nspace:]
    break

  # Strip off any indentation for the whole string and any trailing
  # whitespace for convenience
  newTestStr="\n"
  if len(fileNums)>0: lineNum=fileNums[0]-1
  firstPass=True
  for lline in block.split("\n"):
    if len(fileNums)>0: lineNum+=1
    line=lline[1:] if lline.startswith("!") else lline
    if not line.strip(): continue
    if line.strip().startswith('#'): 
      newTestStr+=line+'\n'
    else:
      newline=line[nspace:]
      newTestStr+=newline.rstrip()+"\n"
    # Do some basic indentation checks
    if entireBlock:
      # Don't need to check comment lines
      if line.strip().startswith('#'): continue
      if not newline.startswith(" "):
        var=newline.split(":")[0].strip()
        if not (var=='test' or var=='testset'): 
          err="Formatting error in file "+srcfile+" at line: " +line+"\n"
          if len(fileNums)>0:
            raise Exception(err+"Check indentation at line number: "+str(lineNum))
          else:
            raise Exception(err)
      else:
        var=line.split(":")[0].strip()
        if var=='test' or var=='testset':
          subnspace=len(line)-len(line.lstrip(stripstr))
          if firstPass:
            firstsubnspace=subnspace
            firstPass=False
          else:
            if firstsubnspace!=subnspace:
              err="Formatting subtest error in file "+srcfile+" at line: " +line+"\n"
              if len(fileNums)>0:
                raise Exception(err+"Check indentation at line number: "+str(lineNum))
              else:
                raise Exception(err)


  return newTestStr

def parseLoopArgs(varset):
  """
  Given:   String containing loop variables
  Return: tuple containing separate/shared and string of loop vars
  """
  keynm=varset.split("{{")[0].strip()
  if not keynm.strip(): keynm='nsize'
  lvars=varset.split('{{')[1].split('}')[0]
  suffx=varset.split('{{')[1].split('}')[1]
  ftype='separate' if suffx.startswith('separate') else 'shared' 
  return keynm,lvars,ftype

def _getSeparateTestvars(testDict):
  """
  Given: dictionary that may have 
  Return:  Variables that cause a test split
  """
  vals=None
  sepvars=[]
  # Check nsize
  if testDict.has_key('nsize'): 
    varset=testDict['nsize']
    if '{{' in varset:
      keynm,lvars,ftype=parseLoopArgs(varset)
      if ftype=='separate': sepvars.append(keynm)

  # Now check args
  if not testDict.has_key('args'): return sepvars
  for varset in re.split('-(?=[a-zA-Z])',testDict['args']):
    if not varset.strip(): continue
    if '{{' in varset:
      # Assuming only one for loop per var specification
      keynm,lvars,ftype=parseLoopArgs(varset)
      if ftype=='separate': sepvars.append(keynm)

  return sepvars

def _getVarVals(findvar,testDict):
  """
  Given: variable that is either nsize or in args
  Return:  Values to loop over
  """
  vals=None
  newargs=''
  if findvar=='nsize':
    varset=testDict[findvar]
    keynm,vals,ftype=parseLoopArgs('nsize '+varset)
  else:
    varlist=[]
    for varset in re.split('-(?=[a-zA-Z])',testDict['args']):
      if not varset.strip(): continue
      if '{{' in varset:
        keyvar,vals,ftype=parseLoopArgs(varset)
        if keyvar!=findvar: 
          newargs+="-"+varset.strip()+" "
          continue
      else:
        newargs+="-"+varset.strip()+" "

  if not vals: raise StandardError("Could not find separate_testvar: "+findvar)
  return vals,newargs

def genTestsSeparateTestvars(intests,indicts):
  """
  Given: testname, sdict with 'separate_testvars
  Return: testnames,sdicts: List of generated tests
  """
  testnames=[]; sdicts=[]
  for i in range(len(intests)):
    testname=intests[i]; sdict=indicts[i]; i+=1
    separate_testvars=_getSeparateTestvars(sdict)
    if len(separate_testvars)>0:
      for kvar in separate_testvars:
        kvals,newargs=_getVarVals(kvar,sdict)
        # No errors means we are good to go
        for val in kvals.split():
          kvardict=sdict.copy()
          gensuffix="_"+kvar+"-"+val
          newtestnm=testname+gensuffix
          if sdict.has_key('suffix'):
            kvardict['suffix']=sdict['suffix']+gensuffix
          else:
            kvardict['suffix']=gensuffix
          if kvar=='nsize':
            kvardict[kvar]=val
          else:
            kvardict['args']=newargs+"-"+kvar+" "+val
          testnames.append(newtestnm)
          sdicts.append(kvardict)
    else:
      testnames.append(testname)
      sdicts.append(sdict)
  return testnames,sdicts

def genTestsSubtestSuffix(testnames,sdicts):
  """
  Given: testname, sdict with separate_testvars
  Return: testnames,sdicts: List of generated tests
  """
  tnms=[]; sdcts=[]
  for i in range(len(testnames)):
    testname=testnames[i]
    rmsubtests=[]; keepSubtests=False
    if sdicts[i].has_key('subtests'):
      for stest in sdicts[i]["subtests"]:
        if sdicts[i][stest].has_key('suffix'):
          rmsubtests.append(stest)
          gensuffix="_"+sdicts[i][stest]['suffix']
          newtestnm=testname+gensuffix
          tnms.append(newtestnm)
          newsdict=sdicts[i].copy()
          del newsdict['subtests']
          # Have to hand update
          # Append
          for kup in appendlist:
            if sdicts[i][stest].has_key(kup):
              if sdicts[i].has_key(kup):
                newsdict[kup]=sdicts[i][kup]+" "+sdicts[i][stest][kup]
              else:
                newsdict[kup]=sdicts[i][stest][kup]
          # Promote
          for kup in acceptedkeys:
            if kup in appendlist: continue
            if sdicts[i][stest].has_key(kup): 
              newsdict[kup]=sdicts[i][stest][kup]
          # Cleanup
          for st in sdicts[i]["subtests"]: del newsdict[st]
          sdcts.append(newsdict)
        else:
          keepSubtests=True
    else:
      tnms.append(testnames[i])
      sdcts.append(sdicts[i])
    # If a subtest without a suffix exists, then save it
    if keepSubtests:
      tnms.append(testnames[i])
      newsdict=sdicts[i].copy()
      # Prune the tests to prepare for keeping
      for rmtest in rmsubtests:
        newsdict['subtests'].remove(rmtest)
        del newsdict[rmtest]
      sdcts.append(newsdict)
    i+=1
  return tnms,sdcts

def splitTests(testname,sdict):
  """
  Given: testname and YAML-generated dictionary
  Return: list of names and dictionaries corresponding to each test
          given that the YAML language allows for multiple tests
  """

  # Order: Parent sep_tv, subtests suffix, subtests sep_tv
  testnames,sdicts=genTestsSeparateTestvars([testname],[sdict])
  testnames,sdicts=genTestsSubtestSuffix(testnames,sdicts)
  testnames,sdicts=genTestsSeparateTestvars(testnames,sdicts)

  # Because I am altering the list, I do this in passes.  Inelegant 

  return testnames, sdicts

def parseTest(testStr,srcfile,verbosity):
  """
  This parses an individual test
  YAML is hierarchial so should use a state machine in the general case,
  but in practice we only support two levels of test:
  """
  basename=os.path.basename(srcfile)
  # Handle the new at the begininng
  bn=re.sub("new_","",basename)
  # This is the default
  testname="run"+os.path.splitext(bn)[0]

  # Tests that have default everything (so empty effectively)
  if len(testStr)==0: return testname, {}

  striptest=_stripIndent(testStr,srcfile)

  # go through and parse
  subtestnum=0
  subdict={}
  comments=[]
  indentlevel=0
  for ln in striptest.split("\n"):
    line=ln.split('#')[0].rstrip()
    if verbosity>2: print line
    comment=("" if len(ln.split("#"))>0 else " ".join(ln.split("#")[1:]).strip())
    if comment: comments.append(comment)
    if not line.strip(): continue
    lsplit=line.split(':')
    if len(lsplit)==0: raise Exception("Missing : in line: "+line)
    indentcount=lsplit[0].count(" ")
    var=lsplit[0].strip()
    val=line[line.find(':')+1:].strip()
    if not var in acceptedkeys: raise Exception("Not a defined key: "+var+" from:  "+line)
    # Start by seeing if we are in a subtest
    if line.startswith(" "):
      subdict[subtestname][var]=val
      if not indentlevel: indentlevel=indentcount
      #if indentlevel!=indentcount: print "Error in indentation:", ln
    # Determine subtest name and make dict
    elif var=="test":
      subtestname="test"+str(subtestnum)
      subdict[subtestname]={}
      if not subdict.has_key("subtests"): subdict["subtests"]=[]
      subdict["subtests"].append(subtestname)
      subtestnum=subtestnum+1
    # The rest are easy
    else:
      # For convenience, it is sometimes convenient to list twice
      if subdict.has_key(var):
        if var in appendlist:
          subdict[var]+=" "+val
        else:
          raise Exception(var+" entered twice: "+line)
      else:
        subdict[var]=val
      if var=="suffix":
        if len(val)>0:
          testname=testname+"_"+val

  if len(comments): subdict['comments']="\n".join(comments).lstrip("\n")
  # A test block can create multiple tests.  This does
  # that logic
  testnames,subdicts=splitTests(testname,subdict)
  return testnames,subdicts

def parseTests(testStr,srcfile,fileNums,verbosity):
  """
  Parse the yaml string describing tests and return 
  a dictionary with the info in the form of:
    testDict[test][subtest]
  This is an inelegant parser as we do not wish to 
  introduce a new dependency by bringing in pyyaml.
  The advantage is that validation can be done as 
  it is parsed (e.g., 'test' is the only top-level node)
  """

  testDict={}

  # The first entry should be test: but it might be indented. 
  newTestStr=_stripIndent(testStr,srcfile,entireBlock=True,fileNums=fileNums)
  if verbosity>2: print srcfile

  # Now go through each test.  First elem in split is blank
  for test in re.split("\ntest(?:set)?:",newTestStr)[1:]:
    testnames,subdicts=parseTest(test,srcfile,verbosity)
    for i in range(len(testnames)):
      if testDict.has_key(testnames[i]):
        raise Error("Multiple test names specified: "+testname+" in file: "+srcfile)
      testDict[testnames[i]]=subdicts[i]
      
  return testDict

def parseTestFile(srcfile,verbosity):
  """
  Parse single example files and return dictionary of the form:
    testDict[srcfile][test][subtest]
  """
  debug=False
  curdir=os.path.realpath(os.path.curdir)
  basedir=os.path.dirname(os.path.realpath(srcfile))
  basename=os.path.basename(srcfile)
  os.chdir(basedir)

  testDict={}
  sh=open(srcfile,"r"); fileStr=sh.read(); sh.close()

  ## Start with doing the tests
  #
  fsplit=fileStr.split("/*TEST\n")[1:]
  if len(fsplit)==0: 
    if debug: print "No test found in: "+srcfile
    return {}
  fstart=len(fileStr.split("/*TEST\n")[0].split("\n"))+1
  # Allow for multiple "/*TEST" blocks even though it really should be
  # one
  srcTests=[]
  for t in fsplit: srcTests.append(t.split("TEST*/")[0])
  testString=" ".join(srcTests)
  if len(testString.strip())==0:
    print "No test found in: "+srcfile
    return {}
  flen=len(testString.split("\n"))
  fend=fstart+flen-1
  fileNums=range(fstart,fend)
  testDict[basename]=parseTests(testString,srcfile,fileNums,verbosity)

  ## Check and see if we have build reuqirements
  #
  if "/*T\n" in fileStr or "/*T " in fileStr:
    # The file info is already here and need to append
    Part1=fileStr.split("T*/")[0]
    fileInfo=Part1.split("/*T")[1]
    for bkey in buildkeys:
      if bkey+":" in fileInfo:
        testDict[basename][bkey]=fileInfo.split(bkey+":")[1].split("\n")[0].strip()

  os.chdir(curdir)
  return testDict

def parseTestDir(directory,verbosity):
  """
  Parse single example files and return dictionary of the form:
    testDict[srcfile][test][subtest]
  """
  curdir=os.path.realpath(os.path.curdir)
  basedir=os.path.realpath(directory)
  os.chdir(basedir)

  tDict={}
  for test_file in glob.glob("new_ex*.*"):
    tDict.update(parseTestFile(test_file,verbosity))

  os.chdir(curdir)
  return tDict

def printExParseDict(rDict):
  """
  This is useful for debugging
  """
  indent="   "
  for sfile in rDict:
    print sfile
    sortkeys=rDict[sfile].keys()
    sortkeys.sort()
    for runex in sortkeys:
      print indent+runex
      if type(rDict[sfile][runex])==types.StringType:
        print indent*2+rDict[sfile][runex]
      else:
        for var in rDict[sfile][runex]:
          if var.startswith("test"): continue
          print indent*2+var+": "+str(rDict[sfile][runex][var])
        if rDict[sfile][runex].has_key('subtests'):
          for var in rDict[sfile][runex]['subtests']:
            print indent*2+var
            for var2 in rDict[sfile][runex][var]:
              print indent*3+var2+": "+str(rDict[sfile][runex][var][var2])
      print "\n"
  return

def main(directory='',test_file='',verbosity=0):

    if directory:
      tDict=parseTestDir(directory,verbosity)
    else:
      tDict=parseTestFile(test_file,verbosity)
    if verbosity>0: printExParseDict(tDict)

    return

if __name__ == '__main__':
    import optparse
    parser = optparse.OptionParser()
    parser.add_option('-d', '--directory', dest='directory',
                      default="", help='Directory containing files to parse')
    parser.add_option('-t', '--test_file', dest='test_file',
                      default="", help='Test file, e.g., ex1.c, to parse')
    parser.add_option('-v', '--verbosity', dest='verbosity',
                      help='Verbosity of output by level: 1, 2, or 3', default=0)
    opts, extra_args = parser.parse_args()

    if extra_args:
        import sys
        sys.stderr.write('Unknown arguments: %s\n' % ' '.join(extra_args))
        exit(1)
    if not opts.test_file and not opts.directory:
      print "test file or directory is required"
      parser.print_usage()
      sys.exit()

    # Need verbosity to be an integer
    try:
      verbosity=int(opts.verbosity)
    except:
      raise Exception("Error: Verbosity must be integer")

    main(directory=opts.directory,test_file=opts.test_file,verbosity=verbosity)
