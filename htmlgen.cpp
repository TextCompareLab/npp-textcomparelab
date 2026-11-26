#include "src/diff.h"
#include <fstream>
#include <sstream>
static std::string rd(const char*p){std::ifstream f(p,std::ios::binary);std::stringstream s;s<<f.rdbuf();return s.str();}
int main(int c,char**v){std::string A=rd(v[1]),B=rd(v[2]);auto r=tcl::compare(A,B);std::ofstream f(v[3],std::ios::binary);f<<tcl::htmlReport(A,B,v[4],v[5],r);}
