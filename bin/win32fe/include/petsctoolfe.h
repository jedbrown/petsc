/* $Id: toolfe.h,v 1.3 2001/04/16 22:17:10 buschelm Exp $ */
#ifndef PETScToolFE_h_
#define PETScToolFE_h_

#include "fe.h"

namespace PETScFE {

  class tool {
  public:
    static int Create(tool*&,string);
    virtual void Parse(void);
    virtual void Execute(void);
    virtual void GetArgs(int argc,char *argv[]);
    virtual void Destroy(void) {delete this;}
  protected:
    tool();
    virtual ~tool() {}
    virtual void Help(void);

    void PrintListString(list<string> &);
    virtual void ProtectQuotes(string &);
    virtual void ReplaceSlashWithBackslash(string &);
    virtual void GetShortPath(string &);
    virtual void Merge(string &,list<string> &,LI &);

    list<string> arg;
    int verbose;
    int helpfound;
  private:
    void FoundHelp(LI &);
    void FoundUse(LI &);
    void FoundVerbose(LI &);

    string OptionTags;
    typedef void (PETScFE::tool::*ptm)(LI &);
    map<char,ptm> Options;
  };

}
#endif
