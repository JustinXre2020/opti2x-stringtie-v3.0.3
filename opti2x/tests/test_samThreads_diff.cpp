// GSamReader::setThreads must turn on threaded BGZF decompression, and the threaded reader must return exactly the
// records (core fields and data bytes) the single-threaded reader returns, across many BGZF blocks.
#include "GSam.h"
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <unistd.h>
#include <vector>

static std::string bamPath;

// Writes a coordinate-sorted BAM of nrec random paired/spliced records (several MB uncompressed, many BGZF blocks).
static void writeBam(int nrec) {
	std::mt19937_64 rng(20261007);
	samFile* out=sam_open(bamPath.c_str(), "wb");
	const char* htxt="@HD\tVN:1.6\tSO:coordinate\n@SQ\tSN:chr1\tLN:50000000\n@SQ\tSN:chr2\tLN:50000000\n";
	sam_hdr_t* h=sam_hdr_parse(strlen(htxt), htxt);
	if (!out || !h || sam_hdr_write(out, h)<0) { printf("cannot write %s\n", bamPath.c_str()); exit(2); }
	bam1_t* b=bam_init1();
	kstring_t line={0,0,NULL};
	int pos=1;
	for (int i=0;i<nrec;i++) {
		if (i==nrec/2) pos=1;
		pos+=rng()%40;
		std::string seq, qual;
		for (int j=0;j<100;j++) { seq+="ACGT"[rng()%4]; qual+=(char)('!'+rng()%40); }
		const char* cigar= rng()%3 ? "100M" : "40M500N60M";
		line.l=0;
		ksprintf(&line, "r%d\t%d\t%s\t%d\t60\t%s\t=\t%d\t300\t%s\t%s\tNH:i:%d\tXS:A:%c",
			i, rng()%2 ? 99 : 163, i<nrec/2 ? "chr1" : "chr2", pos, cigar, pos+200, seq.c_str(), qual.c_str(),
			1+(int)(rng()%3), rng()%2 ? '+' : '-');
		if (sam_parse1(&line, h, b)<0 || sam_write1(out, h, b)<0) { printf("cannot write record %d\n", i); exit(2); }
	}
	free(line.s);
	bam_destroy1(b);
	sam_hdr_destroy(h);
	sam_close(out);
}

static int nFail=0;

static bool sameRecord(GSamRecord& a, GSamRecord& c) {
	bam1_t* x=a.get_b(); bam1_t* y=c.get_b();
	return !memcmp(&x->core, &y->core, sizeof(bam1_core_t)) && x->l_data==y->l_data
		&& !memcmp(x->data, y->data, x->l_data);
}

static bool testSetThreadsEnablesThreadedReading() {
	nFail=0;
	for (int n: {1, 3}) {
		GSamReader r(bamPath.c_str());
		if (!r.setThreads(n)) { if (nFail++<5) printf("  setThreads(%d) returned false\n", n); }
	}
	return nFail==0;
}

static bool testThreadedRecordsMatch() {
	nFail=0;
	for (int n: {1, 3}) {
		GSamReader plain(bamPath.c_str()), threaded(bamPath.c_str());
		threaded.setThreads(n);
		GSamRecord a, c;
		long nrec=0;
		while (true) {
			bool ga=plain.next(a), gc=threaded.next(c);
			if (ga!=gc || (ga && !sameRecord(a, c))) {
				if (nFail++<5) printf("  threads=%d: record %ld differs (plain %d, threaded %d)\n", n, nrec, ga, gc);
				break;
			}
			if (!ga) break;
			nrec++;
		}
		if (nrec==0 && nFail++<5) printf("  threads=%d: no records read\n", n);
	}
	return nFail==0;
}

int main() {
	const char* dir=getenv("TMPDIR");
	std::string base= dir ? std::string(dir)+"/st_samthreads_XXXXXX" : std::string("/tmp/st_samthreads_XXXXXX");
	std::vector<char> path(base.begin(), base.end()); path.push_back(0);
	int fd=mkstemp(path.data());
	if (fd<0) { printf("mkstemp failed\n"); return 2; }
	close(fd);
	bamPath=std::string(path.data())+".bam";
	unlink(path.data());
	writeBam(60000);
	int rc=0;
	if (!testSetThreadsEnablesThreadedReading()) { printf("FAIL testSetThreadsEnablesThreadedReading (%d)\n", nFail); rc=1; }
	if (!testThreadedRecordsMatch()) { printf("FAIL testThreadedRecordsMatch (%d)\n", nFail); rc=1; }
	unlink(bamPath.c_str());
	return rc;
}
