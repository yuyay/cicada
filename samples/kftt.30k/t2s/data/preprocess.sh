#!/bin/sh

# This is an example to use cabocha for dependency parsing the Japanese side.
# Note that the Japanese side was segmented by mecab (https://code.google.com/p/mecab/).
# Thus, it is a natural choice to use cabocha (https://code.google.com/p/cabocha/).

cicada=../../..
mecab=
cabocha=

if test "$mecab" = ""; then
  echo "where is your mecab?"
  exit 1
fi

if test "$cabocha" = ""; then
  echo "where is your cabocha?"
  exit 1
fi


for data in train dev tune; do

  cat="cat ../../data/$data.ja"
  if test "$data" = train; then
    cat="bzcat ../../data/$data.ja.bz2"
  fi

  # Here, we perform mecab to assign POS
  # cabocha for dependency
  # cicada_fiter_dependency to generate the dependency in hypergraph format
  # cicada to binarize the forest via CYK-binarization
  $cat | \
  gawk '{for (i=1;i<=NF;++i) {printf "%s\t*\n", $i } print "EOS";}' | \
  $mecab -p | \
  $cabocha -f1 -I 1 | \
  $cicada/progs/cicada_filter_dependency \
	--input ../../data/$data.parsed.ja \
	--cabocha \
	--func \
	--forest \
	--head | \
  $cicada/progs/cicada \
	--input-forest \
	--threads 8 \
	--operation binarize:direction=cyk,order=1 \
	--operation output:no-id=true,file=$data.forest.ja.gz
done