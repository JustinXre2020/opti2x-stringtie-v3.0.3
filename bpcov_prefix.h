#ifndef BPCOV_PREFIX_H
#define BPCOV_PREFIX_H

#include "GVec.hh"

// Turns the per-base coverage deltas of the three strand arrays (-,.,+) into coverage, clamped at 0, plus a running
// sum of that coverage restarted every bsize bases, so interval coverages can be found fast (count_good_junctions).
// Same float operations in the same order as the upstream loop; the running values stay in registers instead of
// being stored and reloaded through bpcov[s][i] (which -fno-strict-aliasing forced on every step).
inline void bpcovPrefixSums(GVec<float>* bpcov, int bsize) {
	int n=bpcov[1].Count();
	if (n<=0) return;
	float* cov0=&bpcov[0][0];
	float* cov1=&bpcov[1][0];
	float* cov2=&bpcov[2][0];
	float val0=0, val1=0, val2=0;
	for (int start=0; start<n; start+=bsize) {
		int end= n-start>bsize ? start+bsize : n;
		float sum0=0, sum1=0, sum2=0;
		for (int i=start; i<end; i++) {
			float v0=cov0[i]+val0; if (v0<0) v0=0; val0=v0; v0+=sum0; sum0=v0; cov0[i]=v0;
			float v1=cov1[i]+val1; if (v1<0) v1=0; val1=v1; v1+=sum1; sum1=v1; cov1[i]=v1;
			float v2=cov2[i]+val2; if (v2<0) v2=0; val2=v2; v2+=sum2; sum2=v2; cov2[i]=v2;
		}
	}
}

#endif
