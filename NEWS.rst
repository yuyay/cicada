2014-1-12
---------
- Bug fix for max-scope constraint computation for GHKM/Tree
  extractions
- Added feed-forward neural network language model and its recurrent
  version. Training is performed by noise contrastive estimate (NCE).

2013-10-26
----------
 - Bugfix for MPI libraries linking (thanks to the problem reported by
   Hiroshi Umemoto)

2013-10-25
----------
 - Supports OS X 10.9 (with libc++ and libstdc++)

2013-10-24
----------
 - Temporarily removed neural networks (thanks to the problem reported
   by Hiroshi Umemoto)

2013-10-2
--------
 - New: Violation-fixing based online learning
 - BUGFIX: margin-based online learner(s) incorrectly keep all the
   training instances!
 - BUGFIX: two-dimensional vector swap operations are wrong, which
   was the source of core dump in alignment learning with posterior
   probability outputs.

2013-9-20
---------
 - New: OSCAR for grouped regularization.
 - New: k-sampler which shares the interface as kbest.
 - New: regularized dual averaging (RDA) for online learning.
 - BUGFIX: prune-kbest core dump.
 - BUGFIX: check length of morphemes for wordnet
 - Added iterator-based interface for kbest/viterbi/sample
 - Added read/write for ngram counts (TODO: allow reading and dumping)
 - Reworked online learning code to allow alternative regularization
   and learning rate strategies.
 - Operators with weights can assign arbitrary weight by weight parameter.
 - A small bug fix for posterior operation.

2013-8-2
--------
 - INCOMPATIBLE: ngram language model code is completely written, and
   follows the latest "expgram" codebase.
 - INCOMPATIBLE: feature application by
   apply_cube_{prune,grow,grow_coarse} are modified:
   If two candidates share the same score, we prefer
   those closer to the left-upper-corner.
 - INCOMPATIBLE: Use of murmur3 for hash functions. This will not
   break the compatibility with the "indexed, binary" files, but may
   affect the ordering of words, features attributes etc.
 - INCOMPATIBLE: Added significance testing via Fisher's exact test in
   cicada_extract_{pharse,scfg,ghkm,tree}. This affect the count
   format and greatly affect the
   cicada_filter_extract{,phrase,scfg,ghkm} tools.
 - INCOMPATIBLE: added tree-insertion-terminal-penalty feature to
   TreeGrammarFallback (or fallback tree grammar) which captures the
   number of "copied" terminals.
 - Bugfix: strange getline behavior when line is very long.
 - Added KenLM as an ngram language model, in addition to
   ExpGram. Currently, ExpGram is smaller, a little bit slower,
   but better in terms of translation quality, thanks to better
   rest-cost estimation.
 - Added debug messages for "inputs" so that we can identify when
   lattices/forests are loaded.
 - Added cicada_filter_normalize which can perform normalization,
   i.e. NFKC and remove controls etc. with support for colorization of
   Katakana, katakana-marks and symbols.
 - Added cicada_filter_chardet which can detect character in a text
   using the ICU chardet API
 - Added cg_descent as an optimization algorithm.
 - Added attributes for each arc in the lattice data structure.
 - Added Fisher's exact test feature.
 - Faster grammar extraction and scoring by eliminating spurious
   locking
 - Better help messages in python scripts with default values
 - Reduced memory consumption in compose_tree and parse_tree
   especially for plugging phrases.
 - Reduced memory for cicada_extract_{pharse,scfg,ghkm,tree}.
 - Better documentation. Now all the docs are rst-based, not mixed
   with md-based docs.
 - Potential speed up for mpi-based learning algorithm especially when
   exchanging parameters.
 - Potential speed up by revising lockfree implementation.
 - Tweaked ngram cache size.

2013-1-9
--------
 - Bugfix: throw when disk is full + added --temporary option to
   extract_score in order to assingn temporary directory from command
   line.

2013-1-7
--------
 - INCOMPATIBLE: ngram feature computes "exact" score when prefixed by
   <s> during upper bound estimates
 - Faster and less-memory kbest loading in
   learn/oracle/mert/convex_hull

2013-1-4
--------
 - Bugfix: removed spurious "locking" by "not" checking malloced
   resources very frequently.
 - Revised the kbest loading in learn/oracle/mert/convex_hull code.

2012-12-29
----------
 - INCOMPATIBLE: The bleuS metric is completely a sentence-wise
   metric, and document-wise scoring is performed by simple averaging.
 - Bugfix: merge counts when large # of count files are extracted.
 - Bugfix: better parallelization of counts extraction.

2012-12-20
----------
 - New: support L1 regularization for xBLEU.
 - Bugfix: Use of a rather conventional backtracking line search
   strategy, not the strategy of the original LBFGS to avoid
   line-search failure

2012-12-19
----------
 - New: faster MapReduce for mpi-based phrase/scfg/ghkm/tree-based
   extraction.

2012-12-5
---------
 - New: added IBM Model 4 training. Now, Model 1 -> HMM -> Model 4
   training is the default.

2012-11-27
----------
 - Bugfix: kbest-based (mert) learner/oracle computer core dump! The
   reason is the incorrect hash value computation for
   compact-feature-vector.
 - INCOMPATIBLE: The feature-vector is re-implemented by a privately
   implemented compact-hashtable. Thus, the element is not "sorted"
   but randomly inserted in the vector. The compact-hashtable is
   inspired by the dense-hash-map, but differ in that empty/deleted
   items are specified by "template."
 - INCOMPATIBLE: phrase extraction now explicitly enumerate all the
   corners for lexical reordering computation
 - INCOMPATIBLE: lexical weights are now computed by
   cicada_filter_extract_{phrase,scfg,ghkm}, not during extraction
   for flexible feature function computation. Thus, cicada-extract.py
   simply extract rules with counts, then, cicada-index.py to add
   additional features and to perform indexing.
 - INCOMPATIBLE: Use bitvector for features which takes only binary or
   single values.
 - INCOMPATIBLE: revised signature/stemmer interface and caching.
 - New: reduced memory consumption for HMM aligner.
 - New: added more features:# of unaligned words, # of cross for
   terimnals, and generative probabilities (P(pair | root(pair)),
   P(source | root(source)), P(target | root(target)))
 - New: bash-based learning scripts are re-implemented by equivalent,
   better python codes for better readability. Check
   cicada-{learn,mert}{,-moses}.py, not \*.sh equivalents.
 - New: added cutoff threshold for ghkm/tree extraction to prune out
   rare elementary trees in advance.
 - New: added remove-head for removing nodes with '*' as non-temrinal,
   and re-implemented debinarize which shares the same code.
 - New: added codecs which implements various filters for use with
   boost.iostreams (lz4,fastlz,quicklz and optional snappy)
 - New: added rule-wise insertion/deletion features and
   derivation-wise insertion/deletion features.
 - Deprecated: bash based scripts.
 - Deprecated: dependency structure filters:
   cicada_filter_{conll,mst,cabocha} and are merged into
   cicada_filter_dependency.
 - Removed terminal-binarization and re-wrote as
   dependency-binarization.
 - Better parallelization by cicada{,_mpi} with more balanced
   input/output.
 - Better memory allocation: use of power2 heuristics.
 - Completely removed dependence on sparsehashmap/densehashmap!

2012-9-6
--------
 - New: added feature selection by simple kbest merging (for
   cicada_learn_online_{kbest,}_mpi).
 - New: added weight pusing toward root, frontier and left-corner.
 - New: added feature-vector intersection computation for weight
   pusing.
 - New: added sort-topologically operation so that we can verify the
   forest constructed by 3rd parties.
 - New: added # of non-terminal crossing features, singleton features,
   and type-based features.
 - New: added L0 prior for count-based lexicon model induciton
   (cicada_lexicon).

2012-7-18
---------
 - Bugfix: variational decoding and bleu/bleuS computation on a forest :(
 - Bugfix: unique kbest derivations :(
 - INCOMPATIBLE CHANGE by adding loss/reward to evaluation statistics,
   and always use loss() (no negative-Bleu, but 1-Bleu for tuning)
 - INCOMPATIBLE CHANGE by better ITG parsing: use of pialign-style
   parsing with simple outside estimates.
 - Better indexing: prune away unused space for indexed
   (tree-)grammar(s)
 - New: added PYP-pos, an unsupervised pos induction (currently, we
   support training, and no pos assinger exists).
 - New: added PYP-pialign, an unsupervised phrasal ITG aligner!
   (currently, we support training, and no phrasal aligner exists)
 - New: added BleuS, Inv-WER and CDER for MT evaluation.
 - New: added cicada_filter_alignment with visualization mode for the
   ease of word alignment analysis (combined with less -R or lv -c!).
 - New: added PYP-itg, an unsupervised ITG word-aligner!
 - New: added L0-regularization in cicada_lexicon_{hmm, model1}

2012-4-9
--------
 - INCOMPATIBLE CHANGE: renamed cicada_learn_block_mpi to
   cicada_learn_online_kbest_mpi and use the block-wise algorithms for
   cicada_learn_online_mpi
 - Bugfix: Adde brevity penalty for RIBES.
 - Bugfix: Do not read oracles when learning by xBLEU.
 - New: added # of non-terminals in cicada-extract.py with "scfg"
   (currently, we support 0, 1, 2, 3).
 - New: added permute-deterministic which deterministically permute
   hyperedges wrt the category of the head.
 - New: added filter for alignment: Currently, we support inverse,
   permutation.
 - New: added xBLEU to cicada_learn_online_kbest_mpi.
 - New: support the latest sparse hash (moved from google to
   sparsehash) and gperftools (renamed from google-perftools).
 - New: added PYP-LM (highly experimental) and we can use it as our
   feature function!
 - New: added PYP-translit, an unsupervised transliterator model
   (currently, we support training and no real transliterator exists).
 - New: added PYP-segment, an unsupervised word segmenter (currently,
   we support training and no real segmenter exists).

2012-2-15
---------
 - Added MIRA-like optimization for PRO-style learning.
 - Better parse-cky/parse-tree-cky with correct cube-pruning.
 - Smaller memory usage by removing spurious heap allocations.
 - New: xBLEU training at cicada_learn{,_kbest}{,_mpi}.

2012-1-18
---------
 - INCOMPATIBLE CHANGE: revised internal indexing for
   tree-grammars. (You do not have to re-index, but the size is
   slightly smaller)
 - Better ngram feature computation by pre-transforming into word-id.
 - Faster compose-phrase for phrase-based SMT :-)
 - New: added softmax-margin and loss-margin in
   cicaa_learn_kbest{,_mpi} (probably, we will deprecate
   cicada_maxlike{,mpi}).
 - New: added many more learning algorithms in cicda_learn_kbest_mpi:
   pegasos and cutting-plane w/ and w/o line-search optimization.
 - New: added mert-search which performs line-search used in mert.
 - New: added direct-loss cutting plane algorithm (mcp).
 - New: added pa,cw,arow,nherd for block-based optimization.
 - New: added optimized-sgd (osgd) like optimized-pegasos (opegasos)
   in cicada_learn_block_mpi
 - New: added cicada_query_{cky,tree,tree-cky} which query rules (or
   tree-rules) given sentence/lattice or hypergraph, and dump unique
   rules
 - Deprecated cicada-learn-linear.sh which is now integrated in
   cicad-learn.sh

2011-10-29
----------
 - New: Better ngram state handling, inspired by Sorensen and
   Allauzen (2011) and Pauls and Klein (2011).
 - Serious bugfix for ngram access: we may hit ngrams which do not
   exist (very rare, though).
 - Added "project" option to project non-terminal symbols in GHKM
   algorithm.

2011-10-27
----------
 - Better feature application: completely removed "estimates." Now, we
   should encode estiamted score in each hypothesis.

2011-10-26
----------
 - Support posterior matrix dumping in cicada_lexicon_{model1,hmm}.
 - Support MST dependency in cicada_lexicon_{model1,hmm}.
 - Support for aligner script: cicada-alignment.py will generate
   aligner.sh so that you can align arbitrary data, or dump posterior
   matrix.
 - Support alignment combination from posteriors in two directions.
 - Better caching for ngram language model feature: cache only for
   higher order + longer phrases.
 - Warn CKY-style indexing in tree-grammars.

2011-10-17
----------
 - Added softmax-margin to cicada_learn_block.
 - Added pegasos and optimized-pegasos to cicada_learn_block.

2011-10-13
----------
 - New: Support moses training using the cicada tools
   (cicada-learn-moses.sh): learn by LBFGS or liblinear with kbests.
 - New: parse variant of phrasal composition: we do forward lazy graph
   constructin + backward filtering (but very slow at this moment).
 - New: Better online learning by computing oracles in block-wise
   fashion.
 - Updated scripts for tuning: You should revise cicada.config file so
   that decoder's output should be ${file} not directory=${directory}.
 - Serious bugfix: grammar indexing with attributes.
 - Added kbest filter for moses.
 - Added reference format converter to/from moses/cdec/joshua (We do
   not support xml/sgml style refsets found in LDC).

2011-9-20
---------
 - New: pialign derivation to hiero grammar/hypergraph conversion
   filter. We can generate source/target forest, hiero rules, GHKM
   rules in addition to source/target yield and alignment.
 - New: posterior operation to compute "posterior" given particular
   semiring (tropical/logprob/log) and weights.
 - New: remove-unary which remove unaries in forests.
 - New: preliminary support for dependency parsing: arc-standard,
   arc-eager, hybrid and degree2 parsing.
 - New: preliminary support for dependency projection using alignment
   posterior probabilities + source dependency.
 - Serious bugfix: use of zlib_{compressor,decompresso} as a
   workaround for empty data sending/receiving in MPI.
 - Bugfix for alignment by lexicon model. The cause of the bug seem to
   be an initialization issue...?
 - Bugfix for faster cube-pruning (Alg. 2) of
   {tree,string}-to-{tree,string} extractions: we need to start from
   NULL combination.
 - Better composition/parse for string-to-tree by sharing internal
   nodes and terminals.
 - Better composition/parse for tree-to-{string,tree} by sharing
   internal nodes and terminals.
 - Better epsilon/bos-eos/sgml-tag removal w/o recursion.
 - Better left2right and right2left binarizatin by sharing nodes.
 - Better cicada_extract_score{,_mpi} by prohibitting spurious
   mapping.
 - Better cicada_extract_score_mpi by randomizing reduction.
 - Less memory for Viterbi alignment computation by shrinking at some
   intervals.
 - Added max-compose constraint which set the maximum number of
   minimum rule compositions in GHKM.
 - Added sparse/dense option for feature-application to apply only
   sparse/dense features.
 - Added cicada_extract_sort which merge and re-sort counts.
 - Modified "input" option for cicada_extract_score{,_mpi} (and no
   more --counts/--list).

2011-8-11
---------
 - Rework for cicada_lexicon_{model1,hmm}: implement by map/reduce in
   order to reduce memory requriement.
 - Serious bugfix for alignment computation: TODO handle UNK words...

2011-8-10
---------
 - Added ngram OOV feature which greatly improve translation quality
   (and that is found in cdec).
 - Added cicada-{learn,learn-linear,mert,maxlike}.sh to simplify
   tuning.
 - Allow output both of lattice and forest (may potentially be
   extended to output bitext/alignment/spans etc....?).
 - Serious bugfix for feature-vector comparison, which may affect
   epsilon-removal of lattice (this affect experiments after
   "compact-feature-vector").
 - Faster Cube Pruning for GHKM rule extractions.

2011-7-28
---------
 - Implemented Algorihm 2 of Faster Cube Pruning which completely
   eliminates parent book-keeping.
 - Added more human-loop-unrolling in hmm code.
 - Added convex-hull computer, which will answer a question, "If this
   feature scaling were set to ..., your BLEU were ...%."
 - Bugfix for minimum alignment constraint: when checking with more
   non-terminals, it was too-constrained.
 - Differentiated max-span for source/target and min-hole for
   source/target. I'm not sure whether it is worthwhile to completely
   simulate Hiero-rules. At least, I can say that there's small
   difference.

2011-7-12
---------
 - Better caching for sparse-{lexicon,ngram},rule-shape features.
 - Removed human-unrolling (for potential bug?).

2011-7-11
---------
 - INCOMPATIBLE CHANGE: use "EPSILON" instead of "NONE" at many
   places... This will affect cicada-extract.py and
   cicada-alignment.py since they requres NULL word representations.
   "NONE" will be used only for the boundary condition in tree-grammar
   indexing.
 - Added spearse-lexicon and sparse-ngram features to reproduce
   Watanabe et al. (2007).
 - Bugfix for reading features in kbest: if "=" appears in a feature
   name, we cannot parse!

2011-7-7
--------
 - Added lexicon learning by HMM/Model1 + a script to perform
   bidirectional alignment combination.
 - Added alignment constrained learning in HMM/Model1.
 - Added online learning (MIRA/CW/AROW) for kbest-based learner.

2011-6-30
---------
 - Bugfix for kbest oracle for taking unique.
 - Bugfix for oracle computer memorize the best-so-forth results,
   instead of the previously best.
 - Revised cicada_learn{,_kbest}{,_mpi} so that the constant
   hyperparameter is independent of the training data size.
 - Evaluator can take directory input.
 - Evaluator can assign an individual score to each sentence with base
   document.
 - Compact memory consumption in parse-{cky,tree,tree-cky}.

2011-6-27
---------
 - Serious bugfix for parse-{cky,tree,tree-cky} where the features
   from source lattice/forest(s) are completely ignored for pruning.
 - Added format, a formatter/parser derived from ICU's number/date
   format/parse.
 - Added grammar-format, grammar using the ICU's number/date
   formatter/parser as our rule!
 - Added experimental kbest-based learner/mert/oracle computer with
   support for liblinear solver.
 - Support multiple forest loading in cicada_learn_mpi and added an
   option to load previously training parameters.
 - Support margin-based learning in cicada_learn_mpi.

2011-6-6
--------
 - Reimplemented scorer for extracted counts.
   Previously we store all the target side counts in a DB, but now we
   use re-sort based implementation found in moses script, though it
   requires extra storage.
 - Tweaked parameters for grammar-static and tree-grammar-static for large data indexing.

2011-6-1
--------
 - Bugfix for mpi version of extract-score. Instead of pushing into
   stream, wait: We assume that send-buffer is large enough for better
   map-reduce.

2011-5-30
---------
 - Serious bugfix for {compose,parse}-{tree,tree-cky}: internal rules
   are not correctly computed.
 - Serious bugfix for sentence input in cicada and cicada_mpi: by
   default, we will read in sentence-mode, instead of previous
   "lattice" then fallback to "sentence".
 - Added --input-sentence option to explicitly control the behavior.
 - Added --multiple option for cicada_unite_*
 - Added --constrained option for constraining the # of nodes/height
   of minimal rules in GHKM extraction.
 - Added skip-sgml-tag in ngram/bleu/bleu-linear feature and mt evals,
   such as bleu, wer etc. But this will no skip <s> </s>.
 - Less memory consumption in scfg/ghkm/tree extraction by frequently
   checking memory usage (+ slightly slower).
 - Less memory consumption in parse-coarse.
 - Better feature-vector implementation with smaller storage.
 - Faster(?) parse-{cky,tree,tree-cky} by pre-pruning rules if
   exceeding pop limits.
 - Removed "dot" and use separate "dot_product" in dot_product.hpp.
 - Binarize-all now shares binarized nodes in a forest.

2011-5-18
---------
 - Serious bugfix for generate-earley. We will now check the depth of
   all the passive/active edges to be extended.

2011-5-17
---------
- Serious bugfix for simple vector, wrt resize/insert/erase, which
  affect feature-vectors.

2011-5-16
---------
 - Bugfix for spurious memory allocation in compose-cky, parse-cky and
   parse-coarse
 - Bugfix for topological sort after compose-tree and parse-tree
 - Better memory management in compose/parse operations

2011-5-9
--------
 - Added a sample grammar file to support zone/wall found in moses.
 - Added experimental compose-tree-cky and parse-tree-cky for
   string-to-tree translation!
 - Support cky-style indexing in tree-transducer for string-to-tree
   translation.
 - Better global lexicon learning by limiting the source word features
 - More compact representation for feature vectors
 - Bugfix: use base10 -99 for Pr(<s>)

2011-4-22
---------
 - Added push-bos-eos which annotates forests with <s> </s>. Use with
   no-bos-eos options for ngram/variational features.
 - Added prune-edge which prunes forests wrt # of edges for each node.
 - Added parse-tree which is an approximated beam variant of
   compose-tree
 - Randomize hill-climbing for oracle computation.
 - Incompatible change: word-penalty feature do not count <s> </s> as
   a "word"
 - Bugfix: when assigned weights or weights-one, do not perform
   "assign"
 - Bugfix: prune-kbest will not "prune" when we cannot generate
   suffixient # of kbests.

2011-4-18
---------
 - Added coarse-to-fine parsing
 - Added coarse grammar learning
 - Added epsilon-removal for hypergraph
 - Added no-bos-eos option to ngram feature which will not score via
   <s> </s>, assuming <s> and </s> are used in grammar
 - Reworked attribute vector for smaller memory allocation (but slower
   for insert/erase)
 - Use -99 for unigram probability of <s> taken from SRILM

2011-3-28
---------
 - New grammar API: use --grammar and/or --tree-grammar only, and
   removed options, such as --grammar-static etc.
 - Added parser component. Currently, we support tabular-CKY and
   bottom-up agenda-based best-first search with pruning.
 - Added parseval evaluator.
 - Added grammar learner based on latent annotation grammar
 - Better scanner/generator for hypergraph/lattice structures etc. by
   eliminating (potentially slower) symbol-tables.

2011-2-14
---------
 - Better vocabulary management by eliminating temporary buffer.
 - Use boost.spirit for parsing/generating numerics.

2011-1-17
---------
 - Support tree-to-string translation (probably, the code will work
   for tree-to-tree)
 - Added sort-tail operation which sort tails by its order in rule's
   non-terminal index.
 - Added lexicon learning (dice and model1 via
   cicada_lexicon_{dice,model1}) and lexicon model learning from word
   alignment (cicada_lexicon)
 - Added word-cluster learning (cicada_cluster_word)
 - Added filter for forest-charniak and egret (cicada_filter_charniak)
   and CONLL-X (cicada_filter_conll)
 - Added moses compatible phrase/synchronous-CFG rule/GHKM
   rule/tree2tree rule extraction script (cicada-extract.py)
 - Added string-to-tree GHKM extraction by flipping source/target side
   in tree-to-string extraction

2010-12-22
----------
 - Added new API for mt-evaluation score allowing ascii-dumping by
   desciption/encode and recovery by decode.
 - GHKM and Tree extractor first computes terminal span, and removed
   span-forest from cicada_fitler_{penntreebank,cabocha}.

2010-12-20
----------
 - Serious bugfix to ngram-related features with double counting.
 - Serious bugfix to expected ngram collections
 - Added preliminary head-finder
 - Added approximate matcher to cicada_unite_sentence and TER/WER.

2010-12-13
----------
 - Added phrase-extract-like script, extract.py, for easily
   extracting/scoreing phrase/scfg/ghkm.
 - Added alignment tool, cicada_alignment (do we need this?) (TODO:
   add ITG alignment + threading)
 - Added matching API, word matcher using lower-case, stemming and
   wordnet synsets(!) (TODO: use this for TER etc.)
 - Added wordnet API (we use the standard c-interface wrapped by
   thread-safe API)
 - Modified "permute" so that permutation rules are stored in
   attributes and permute-feature can access and perform scoring.

2010-11-28
----------
 - Added "attribute" in each hyperedge which can store key-value pair
   with arbitray value type: 64-bit-int, double float or string.
 - Cache rule allocation in hypergraph for better memory consumption

2010-11-22
----------
 - Support tree transduction (but experimental)
 - Removed bi-rules in hypergraph: removed "yield" in features etc. but
   added in compositional operations.
 - Added phrase/synchronous-rule/GHKM-rule extractor + score accumulator
 - Added tokenizer, and use it for MT evaluators and bleu-related features
 - Added new API for word normalizer (stemmer or cluster) and use it
   for sparce features
 - Added remove-epsilon for lattice
 - Added linguistic stemmer (snowball and LDC's Arabic stemmer)

2010-10-31
----------
 - Support phrase-based composition with length-based distortion
   and lexicalized-reordering (experimental, and only for monotonic
   lattice)

2010-10-23
----------
 - Added ter/wer/per
 - Added cicada_unite_{hypergraph,lattice,sentence} to perform merging
 - Added Earley generator with contextual category
 - Support epsilon in lattice

2010-9-6
--------
 - Added expected-ngram computer
 - Added expected-BLEU feature
 - Added ngram-count-set structure
 - Moved operation/operation_set into global

2010-8-27
---------
 - Added cube-growing with order-based coarse heuristic
 - "Quietly" revised grammar-static structure

2010-8-26
---------
 - Implemented cube-pruning and cube-growing for faster rescoring


