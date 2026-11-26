#include "src/diff.h"
#include <cstdio>
#include <random>
#include <cassert>
using namespace tcl;
// reference LCS length via DP
static size_t lcs(const std::vector<uint32_t>&a,const std::vector<uint32_t>&b){
 std::vector<std::vector<int>> d(a.size()+1,std::vector<int>(b.size()+1,0));
 for(size_t i=1;i<=a.size();i++)for(size_t j=1;j<=b.size();j++)
  d[i][j]=a[i-1]==b[j-1]?d[i-1][j-1]+1:std::max(d[i-1][j],d[i][j-1]);
 return d[a.size()][b.size()];}
int main(){
 std::mt19937 g(1);
 for(int it=0;it<20000;it++){
  int n=g()%12,m=g()%12,al=2+g()%4;
  std::vector<uint32_t>a(n),b(m);
  for(auto&x:a)x=g()%al; for(auto&x:b)x=g()%al;
  std::vector<char>ka,kb; workBudget()=1000000; bool ok=myers(a,b,ka,kb,4000); assert(ok);
  size_t ca=0,cb=0; std::vector<uint32_t> sa,sb;
  for(int i=0;i<n;i++)if(ka[i]){ca++;sa.push_back(a[i]);}
  for(int j=0;j<m;j++)if(kb[j]){cb++;sb.push_back(b[j]);}
  if(!(ca==cb&&sa==sb&&ca==lcs(a,b))){printf("FAIL it=%d n=%d m=%d ca=%zu cb=%zu lcs=%zu\n",it,n,m,ca,cb,lcs(a,b));return 1;}
 }
 puts("myers ok (20000 random cases, matches DP LCS)");
 auto r=compare("one\ntwo three\nfour\r\nfive\n","one\ntwo 3 three\nfour\nsix\nfive\n");
 printf("changed A:");for(int x:r.changedA)printf(" %d",x);printf(" | added B:");for(int x:r.addedLines)printf(" %d",x);
 printf(" | removed A:");for(int x:r.removedLines)printf(" %d",x);printf(" | inlineB:");for(auto&x:r.inlineB)printf(" [%zu+%zu]",x.pos,x.len);puts("");
 auto s=compare("a\nb","a\nb"); printf("identical=%d\n",s.identical);
 // big
 std::string A,B; for(int i=0;i<50000;i++){A+="line "+std::to_string(i)+"\n"; B+=(i%1000==0?"changed ":"line ")+std::to_string(i)+"\n";}
 auto t=compare(A,B); printf("big: changedA=%zu tooDifferent=%d\n",t.changedA.size(),t.tooDifferent);
}
