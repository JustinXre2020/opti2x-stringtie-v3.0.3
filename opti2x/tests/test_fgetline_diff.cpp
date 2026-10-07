// Differential test: fgetline must return the same lines, lengths and file positions, and leave the stream at the
// same offset, as upstream StringTie v3.0.3 fgetline for any mix of \n, \r\n, lone \r, NUL, 0xFF and long lines.
#include "GBase.h"
#include <cstdio>
#include <cstring>
#include <random>
#include <string>

// Verbatim from upstream gclib/GBase.cpp.
static char* upstreamFgetline(char* & buf, int& buf_cap, FILE *stream, off_t* f_pos, int* linelen) {
  //reads a char at a time until \n and/or \r are encountered
  int c=0;
  GDynArray<char> arr(buf, buf_cap);
  off_t fpos=(f_pos!=NULL) ? *f_pos : 0;
  while ((c=getc(stream))!=EOF) {
    if (c=='\n' || c=='\r') {
       if (c=='\r') {
         if ((c=getc(stream))!='\n') ungetc(c,stream);
                                else fpos++;
         }
       fpos++;
       break;
       }
    fpos++;
    arr.Push((char)c);
    } //while i<buf_cap-1
  //if (linelen!=NULL) *linelen=i;
  if (linelen!=NULL) *linelen=arr.Count();
  if (f_pos!=NULL) *f_pos=fpos;
  //if (c==EOF && i==0) return NULL;
  if (c==EOF && arr.Count()==0) return NULL;
  arr.Push('\0');
  buf=arr();
  buf_cap=arr.Capacity();
  return buf;
  }

static std::mt19937_64 rng(20261007);
static int nFail=0;

static std::string content(int n) {
	const char alphabet[]={'a','C','\t',' ','\n','\n','\r','\0',(char)0xFF,'#'};
	std::string s;
	for (int i=0;i<n;i++) {
		if (rng()%400==0) s.append(rng()%3000, 'x'); // a line longer than any starting buffer
		s.push_back(alphabet[rng()%sizeof(alphabet)]);
	}
	return s;
}

// Reads the whole file line by line with both implementations, comparing every result; withPos/withLen pick
// whether the optional outputs are requested, initCap the starting buffer capacity (0 = no buffer).
static void runCase(const std::string& text, bool withPos, bool withLen, int initCap) {
	FILE* fu=tmpfile(); FILE* fc=tmpfile();
	fwrite(text.data(),1,text.size(),fu); fwrite(text.data(),1,text.size(),fc);
	rewind(fu); rewind(fc);
	char* bu=NULL; char* bc=NULL; int capu=initCap, capc=initCap;
	if (initCap) { GMALLOC(bu, initCap); GMALLOC(bc, initCap); }
	off_t posu=0, posc=0; int lenu=-7, lenc=-7;
	for (int line=0;;line++) {
		char* ru=upstreamFgetline(bu, capu, fu, withPos?&posu:NULL, withLen?&lenu:NULL);
		char* rc=fgetline(bc, capc, fc, withPos?&posc:NULL, withLen?&lenc:NULL);
		bool same= (ru==NULL)==(rc==NULL) && posu==posc && lenu==lenc && ftell(fu)==ftell(fc)
			&& (ru==NULL || (capu==capc && !memcmp(ru, rc, (withLen?lenu:(int)strlen(ru))+1)));
		if (!same) {
			if (nFail++<5) printf("  text size %d, line %d: upstream %s len %d pos %lld tell %ld; candidate %s len %d pos %lld tell %ld\n",
				(int)text.size(), line, ru?"line":"NULL", lenu, (long long)posu, ftell(fu), rc?"line":"NULL", lenc, (long long)posc, ftell(fc));
			break;
		}
		if (ru==NULL) break;
	}
	GFREE(bu); GFREE(bc);
	fclose(fu); fclose(fc);
}

static bool testEdgeTexts() {
	nFail=0;
	const char* texts[]={"", "\n", "\r", "\r\n", "\n\r", "\r\r\n", "a", "a\n", "a\r", "a\r\nb", "a\rb\n", "a\n\nb\r\r",
		"\xff", "\xff\n\xff", "x\0y\n"};
	for (const char* t: texts) for (int cap: {0, 1, 64}) for (int o=0;o<4;o++)
		runCase(std::string(t, strlen(t)+(t[0]=='x'?3:0)), o&1, o&2, cap);
	return nFail==0;
}

static bool testRandomTexts() {
	nFail=0;
	const int caps[]={0,1,64,1024};
	for (int r=0;r<3000;r++) runCase(content(rng()%2000), r%2, (r/2)%2, caps[r%4]);
	return nFail==0;
}

int main() {
	int rc=0;
	if (!testEdgeTexts()) { printf("FAIL testEdgeTexts (%d)\n", nFail); rc=1; }
	if (!testRandomTexts()) { printf("FAIL testRandomTexts (%d)\n", nFail); rc=1; }
	return rc;
}
