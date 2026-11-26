#include "src/diff.h"
#include <fstream>
#include <sstream>
#include <cstdio>
static std::string rd(const char*p){std::ifstream f(p,std::ios::binary);std::stringstream s;s<<f.rdbuf();return s.str();}
int main(int c,char**v){std::string out;int r=tcl::unifiedDiff(rd(v[1]),rd(v[2]),v[3],v[4],out);
 if(r==0){std::ofstream f(v[5],std::ios::binary);f<<out;} return r;}
