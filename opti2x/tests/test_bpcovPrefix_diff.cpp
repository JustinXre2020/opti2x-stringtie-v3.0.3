// Differential test: bpcovPrefixSums must leave exactly the bytes the coverage prefix-sum loop of upstream StringTie
// v3.0.3 count_good_junctions leaves, for realistic deltas and for every float special value.
#include "bpcov_prefix.h"
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <random>

#define BSIZE 10000 // as in rlink.cpp

// Verbatim from upstream rlink.cpp count_good_junctions (bpcov deltas -> coverage + per-block running sums).
static void upstreamPrefixSums(GVec<float>* bpcov) {
	// this code enssures that I can find interval coverages very fast
	int m=int((bpcov[1].Count()-1)/BSIZE);
	//fprintf(stderr,"m=%d refstart=%d refend=%d bpcount=%d\n",m,refstart,refend,bpcov[1].Count());
	int k=0;
	float prev_val[3]={0,0,0};
	float prev_sum[3]={0,0,0};
	while(k<m) {
		int end=(k+1)*BSIZE;


		//fprintf(stderr,"end=%d\n",end);
		for(int i=k*BSIZE;i<end;i++) {

			for(int s=0;s<3;s++) {
				//fprintf(stderr,"(1)bpcov[%d][%d]=%f ",s,i,bpcov[s][i]);
				bpcov[s][i]+=prev_val[s];
				if(bpcov[s][i]<0) bpcov[s][i]=0;
				prev_val[s]=bpcov[s][i];
				//fprintf(stderr,"(2)bpcov[%d][%d]=%f ",s,i,bpcov[s][i]);
				bpcov[s][i]+=prev_sum[s];
				prev_sum[s]=bpcov[s][i];
				//fprintf(stderr,"(2)bpcov[%d][%d]=%f ",s,i,bpcov[s][i]);
				//fprintf(stderr,"bpPos[%d][%d]: prev_val=%f prev_sum=%f\n",s,i,prev_val[s],prev_sum[s]);
			}
		}
		for(int s=0;s<3;s++) prev_sum[s]=0;
		k++;
	}

	for(int i=k*BSIZE;i<bpcov[1].Count();i++)
		for(int s=0;s<3;s++) {
			bpcov[s][i]+=prev_val[s];
			if(bpcov[s][i]<0) bpcov[s][i]=0;
			prev_val[s]=bpcov[s][i];
			bpcov[s][i]+=prev_sum[s];
			prev_sum[s]=bpcov[s][i];
			//fprintf(stderr,"bpPos[%d][%d]: cov=%f sumbpcov=%f\n",s,i,prev_val[s],prev_sum[s]);
		}
}

static std::mt19937_64 rng(20261007);

static float special() {
	const float v[]={std::numeric_limits<float>::quiet_NaN(), -std::numeric_limits<float>::quiet_NaN(),
		std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), -0.0f, 0.0f,
		FLT_MIN/8, -FLT_MIN/8, FLT_MAX, -FLT_MAX, FLT_EPSILON, -1e30f};
	return v[rng()%(sizeof(v)/sizeof(v[0]))];
}

// mode 0: read-coverage deltas (+count at read start, -count past its end, counts like 1/nh or junction-scaled);
// mode 1: arbitrary float bit patterns; mode 2: deltas with float specials sprinkled in.
static float delta(int mode) {
	if (mode==1) { uint32_t b=(uint32_t)rng(); float f; memcpy(&f,&b,4); return f; }
	if (mode==2 && rng()%50==0) return special();
	if (rng()%4) return 0;
	float c= rng()%3 ? 1.0f/(1+rng()%10) : (float)(rng()%1000)/7;
	return rng()%2 ? c : -c;
}

static int nFail=0, nCase=0;

static void runCase(int n, int mode) {
	GVec<float> ref[3], cand[3];
	for (int s=0;s<3;s++) {
		ref[s].Resize(n);
		for (int i=0;i<n;i++) ref[s][i]=delta(mode);
		cand[s]=ref[s];
	}
	upstreamPrefixSums(ref);
	bpcovPrefixSums(cand, BSIZE);
	nCase++;
	for (int s=0;s<3;s++) {
		if (cand[s].Count()!=n || memcmp(&ref[s][0], &cand[s][0], n*sizeof(float))) {
			int i=0;
			while (i<n && !memcmp(&ref[s][i], &cand[s][i], sizeof(float))) i++;
			if (nFail++<5) printf("  n=%d mode=%d strand=%d first diff at %d: upstream %g, candidate %g\n",
				n, mode, s, i, ref[s][i], cand[s][i]);
			return;
		}
	}
}

static bool testBlockBoundarySizes() {
	nFail=0;
	const int sizes[]={1, 2, 3, BSIZE-1, BSIZE, BSIZE+1, 2*BSIZE-1, 2*BSIZE, 2*BSIZE+1, 3*BSIZE+7};
	for (int n: sizes) for (int mode=0; mode<3; mode++) runCase(n, mode);
	return nFail==0;
}

static bool testRandomSizes() {
	nFail=0;
	for (int r=0; r<300; r++) runCase(1+rng()%(5*BSIZE), r%3);
	return nFail==0;
}

int main() {
	int rc=0;
	if (!testBlockBoundarySizes()) { printf("FAIL testBlockBoundarySizes (%d)\n", nFail); rc=1; }
	if (!testRandomSizes()) { printf("FAIL testRandomSizes (%d)\n", nFail); rc=1; }
	fprintf(stderr, "%d cases\n", nCase);
	return rc;
}
