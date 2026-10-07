#!/usr/bin/env bash
# Runs one StringTie oracle config and records output hashes in <outdir>/oracle.txt and "wall rss" in <outdir>/time.txt.
# Usage: ST_BAM=... ST_GTF=... [ST_MERGE_GTFS="a.gtf b.gtf"] st_run.sh <stringtie> <outdir> <config>
# Configs: denovo, guided, eB (-e -B ballgown tables), conservative, threads (-p 4), merge.
# Comment lines (^#) are stripped before hashing because they embed the command line. With -p, upstream numbers
# STRG genes/transcripts in bundle completion order, so the threads config hashes sorted lines with STRG ids masked.
set -euo pipefail
ST=$1; OUT=$2; CFG=$3
rm -rf "$OUT"; mkdir -p "$OUT"
case $CFG in
  denovo)       args=(-o "$OUT/out.gtf" -A "$OUT/abund.tab" "$ST_BAM") ;;
  guided)       args=(-G "${ST_GTF:?}" -o "$OUT/out.gtf" -A "$OUT/abund.tab" -C "$OUT/cov_refs.gtf" "$ST_BAM") ;;
  eB)           args=(-e -B -G "${ST_GTF:?}" -o "$OUT/out.gtf" -A "$OUT/abund.tab" "$ST_BAM") ;;
  conservative) args=(--conservative -G "${ST_GTF:?}" -o "$OUT/out.gtf" "$ST_BAM") ;;
  threads)      args=(-p 4 -G "${ST_GTF:?}" -o "$OUT/out.gtf" -A "$OUT/abund.tab" "$ST_BAM") ;;
  merge)        read -r -a gtfs <<< "${ST_MERGE_GTFS:?}"; args=(--merge -G "${ST_GTF:?}" -o "$OUT/out.gtf" "${gtfs[@]}") ;;
  *) echo "unknown config $CFG" >&2; exit 2 ;;
esac
/usr/bin/time -l "$ST" "${args[@]}" > "$OUT/stdout.txt" 2> "$OUT/time.raw"
awk '/real/{w=$1} /maximum resident/{r=$1} END{print w, r}' "$OUT/time.raw" > "$OUT/time.txt"
(cd "$OUT" && for f in $(find . -type f \( -name '*.gtf' -o -name '*.tab' -o -name '*.ctab' \) | sort); do
   if [ "$CFG" = threads ]; then
     h=$(grep -v '^#' "$f" | sed -E 's/STRG\.[0-9]+(\.[0-9]+)?/STRG/g' | LC_ALL=C sort | shasum -a 256 | cut -c1-16)
   else
     h=$(grep -v '^#' "$f" | shasum -a 256 | cut -c1-16)
   fi
   echo "$f $h"
 done) > "$OUT/oracle.txt"
