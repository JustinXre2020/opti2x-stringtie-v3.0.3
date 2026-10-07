#!/usr/bin/env bash
# End-to-end oracle: the candidate StringTie must produce byte-identical outputs (assembled GTF, gene abundances,
# reference coverage, ballgown tables, merged GTF) to upstream StringTie v3.0.3 for every config.
# Usage: ST_BAM=... ST_GTF=... [ST_MERGE_GTFS=...] st_oracle_test.sh <candidate> <upstream> <workdir> [config ...]
# Default configs: denovo guided eB conservative threads (merge runs only when ST_MERGE_GTFS is set).
# Upstream results are cached in <workdir>/upstream-<config>.
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
CAND=$1; UPS=$2; WORK=$3
shift 3
CONFIGS=("$@")
if [ ${#CONFIGS[@]} -eq 0 ]; then
  CONFIGS=(denovo guided eB conservative threads)
  [ -z "${ST_MERGE_GTFS:-}" ] || CONFIGS+=(merge)
fi
mkdir -p "$WORK"

rc=0
for cfg in "${CONFIGS[@]}"; do
  ref=$WORK/upstream-$cfg
  key="$(cat "$UPS" "$HERE/st_run.sh" | shasum -a 256 | cut -c1-16) $ST_BAM ${ST_GTF:-} ${ST_MERGE_GTFS:-}"
  if [ ! -f "$ref/oracle.txt" ] || [ "$(cat "$ref/cache_key" 2>/dev/null)" != "$key" ]; then
    if ! "$HERE/st_run.sh" "$UPS" "$ref" "$cfg"; then
      echo "FAIL $cfg: upstream run failed, see $ref/time.raw"; rc=1; continue
    fi
    echo "$key" > "$ref/cache_key"
  fi
  cand=$WORK/candidate-$cfg
  if ! "$HERE/st_run.sh" "$CAND" "$cand" "$cfg"; then
    echo "FAIL $cfg: candidate run failed, see $cand/time.raw"; rc=1; continue
  fi
  if diff "$ref/oracle.txt" "$cand/oracle.txt" > "$cand/oracle.diff"; then
    echo "PASS $cfg (upstream $(cut -d' ' -f1 "$ref/time.txt")s, candidate $(cut -d' ' -f1 "$cand/time.txt")s)"
  else
    echo "FAIL $cfg"; sed 's/^/    /' "$cand/oracle.diff"; rc=1
  fi
done
exit $rc
