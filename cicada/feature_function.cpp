//
//  Copyright(C) 2010-2013 Taro Watanabe <taro.watanabe@nict.go.jp>
//

#include "feature_function.hpp"

#include "parameter.hpp"

#include "feature/alignment.hpp"
#include "feature/antecedent.hpp"
#include "feature/bleu.hpp"
#include "feature/bleus.hpp"
#include "feature/bleu_expected.hpp"
#include "feature/bleu_linear.hpp"
#include "feature/bleu_multi.hpp"
#include "feature/dependency.hpp"
#include "feature/depeval.hpp"
#include "feature/deletion.hpp"
#include "feature/distortion.hpp"
#include "feature/frontier_bigram.hpp"
#include "feature/frontier_lexicon.hpp"
#include "feature/frontier_pair.hpp"
#include "feature/frontier_shape.hpp"
#include "feature/global_lexicon.hpp"
#include "feature/insertion.hpp"
#include "feature/kenlm.hpp"
#include "feature/lexicalized_reordering.hpp"
#include "feature/lexicon.hpp"
#include "feature/neighbours.hpp"
#include "feature/ngram.hpp"
#include "feature/ngram_nn.hpp"
#include "feature/ngram_pyp.hpp"
#include "feature/ngram_rnn.hpp"
#include "feature/ngram_tree.hpp"
#include "feature/parent.hpp"
#include "feature/penalty.hpp"
#include "feature/permute.hpp"
#include "feature/rule_shape.hpp"
#include "feature/sgml_tag.hpp"
#include "feature/span.hpp"
#include "feature/sparse_lexicon.hpp"
#include "feature/sparse_ngram.hpp"
#include "feature/variational.hpp"
#include "feature/vocabulary.hpp"

#include "utils/piece.hpp"

namespace cicada
{
  
  const char* FeatureFunction::lists()
  {
    static const char* desc = "\
antecedent: antecedent feature\n\
\tcluster=[word class file]\n\
\tstemmer=[stemmer spec]\n\
\talignment=[true|false] alignment forest mode\n\
\tsource-root=[true|false] source root mode (for tree composition)\n\
\tname=feature-name-prefix(default: antecedent)\n\
bleu: BLEU\n\
\torder=<order>\n\
\texact=[true|false] clipped ngram computation\n\
\tskip-sgml-tag=[true|false] skip sgml tags\n\
\ttokenizer=[tokenizer spec]\n\
\tname=feature-name(default: bleu)\n\
\trefset=reference set file\n\
bleus: BLEUS\n\
\torder=<order>\n\
\texact=[true|false] clipped ngram computation\n\
\tskip-sgml-tag=[true|false] skip sgml tags\n\
\ttokenizer=[tokenizer spec]\n\
\tname=feature-name(default: bleus)\n\
\trefset=reference set file\n\
bleu-expected: expected-BLEU\n\
\torder=<order>\n\
bleu-linear: linear corpus-BLEU\n\
\torder=<order>\n\
\tprecision=<default 0.8>\n\
\tratio=<default 0.6>\n\
\ttokenizer=[tokenizer spec]\n\
\tname=feature-name(default: bleu-linear)\n\
\trefset=reference set file\n\
\tskip-sgml-tag=[true|false] skip sgml tags\n\
bleu-multi: multiple BLEU\n\
\torder=<order>\n\
\texact=[true|false] clipped ngram computation\n\
\tskip-sgml-tag=[true|false] skip sgml tags\n\
\ttokenizer=[tokenizer spec]\n\
\tname=feature-name (default: bleu-multi:integer)\n\
\tsize=# of BLEU features\n\
deletion: deletion feature\n\
\tfile=lexicon model file\n\
\tpopulate=[true|false] \"populate\" by pre-fetching\n\
\tskip-sgml-tag=[true|false] skip sgml tags\n\
\tunique-source=[true|source] unique source labels\n\
\tname=feature-name (default: deletion)\n\
\tthreshold=threshold (default: 0.5)\n\
dependency: dependency feature\n\
\torder=[order] (default: 2)\n\
depeval: dependency evaluation feature\n\
\tskip-sgml-tag=[true|false] skip sgml tags\n\
\ttokenizer=[tokenizer spec]\n\
distortion: phrase-based distortion\n\
frontier-bigram: sparse frontier source side bigram\n\
\tsource=[true|false] source side bigram (this is a default)\n\
\ttarget=[true|false] target side bigram (you can specify both)\n\
\tskip-sgml-tag=[true|false] skip sgml tags\n\
\tname=feature-name-prefix (default: frontier-bigram)\n\
frontier-lexicon: sparse lexicon feature from frontiers\n\
\tcluster-source=[word class file] word-class for source side\n\
\tcluster-target=[word class file] word-class for target side\n\
\tstemmer-source=[stemmer spec] stemming for source side\n\
\tstemmer-target=[stemmer spec] stemming for target side\n\
\tskip-sgml-tag=[true|false] skip sgml tags\n\
\tname=feature-name-prefix (default: frontier-lexicon)\n\
frontier-pair: sparse frontier pair features\n\
\tskip-sgml-tag=[true|false] skip sgml tags\n\
\tname=feature-name-prefix (default: frontier-pair)\n\
frontier-shape: sparse frontier shape features\n\
\tskip-sgml-tag=[true|false] skip sgml tags\n\
\tname=feature-name-prefix (default: frontier-shape)\n\
global-lexicon: global lexicon feature\n\
\tfile=global lexicon file\n\
insertion: insertion feature\n\
\tfile=lexicon model file\n\
\tpopulate=[true|false] \"populate\" by pre-fetching\n\
\tskip-sgml-tag=[true|false] skip sgml tags\n\
\tunique-source=[true|source] unique source labels\n\
\tname=feature-name (default: insertion)\n\
\tthreshold=threshold (default: 0.5)\n\
kenlm: ngram LM from kenlm\n\
\tfile=<file>\n\
\tpopulate=[true|false] \"populate\" by pre-fetching\n\
\tcluster=<word class>\n\
\tname=feature-name(default: ngram)\n\
\tno-bos-eos=[true|false] do not add bos/eos\n\
\tskip-sgml-tag=[true|false] skip sgml tags\n\
\tcoarse-file=<file>   ngram for coarrse heuristic\n\
\tcoarse-populate=[true|false] \"populate\" by pre-fetching\n\
\tcoarse-cluster=<word class> word class for coarse heuristics\n\
lexicalized-reordering: lexicalized reordering for phrase-based\n\
\tbidirectional=[true|false]\n\
\tmonotonicity=[true|false]\n\
\tfeature=attribute name mapping\n\
lexicon: lexicon model feature based on P(target-sentence | source-sentence)\n\
\tfile=lexicon model file\n\
\tpopulate=[true|false] \"populate\" by pre-fetching\n\
\tskip-sgml-tag=[true|false] skip sgml tags\n\
\tunique-source=[true|source] unique source labels\n\
\tname=feature-name-prefix (default: lexicon)\n\
neighbours: neighbour words feature\n\
\tcluster=[word class file]\n\
\tstemmer=[stemmer spec]\n\
\talignment=[true|false] alignment forest mode\n\
\tsource-root=[true|false] source root mode (for tree composition)\n\
\tname=feature-name-prefix(default: neighbours)\n\
ngram: ngram language model\n\
\tfile=<file>\n\
\tpopulate=[true|false] \"populate\" by pre-fetching\n\
\tcluster=<word class>\n\
\tname=feature-name(default: ngram)\n\
\tapproximate=[true|false] approximated upper-bound estimates\n\
\tno-bos-eos=[true|false] do not add bos/eos\n\
\tsplit-estimate=[true|false] split estimated ngram score\n\
\tskip-sgml-tag=[true|false] skip sgml tags\n\
\tcoarse-file=<file>   ngram for coarrse heuristic\n\
\tcoarse-populate=[true|false] \"populate\" by pre-fetching\n\
\tcoarse-cluster=<word class> word class for coarse heuristics\n\
\tcoarse-approximate=[true|false] approximated upper-bound estimates\n\
ngram-nn: neural network ngram language model\n\
\tfile=<file>\n\
\tpopulate=[true|false] \"populate\" by pre-fetching\n\
\torder=<order>\n\
\tname=feature-name(default: ngram-nn)\n\
\tno-bos-eos=[true|false] do not add bos/eos\n\
\tskip-sgml-tag=[true|false] skip sgml tags\n\
\tsplit-estimate=[true|false] split estimated ngram score\n\
\tcoarse-order=<order> ngram order for coarse heuristic\n\
\tcoarse-file=<file>   ngram for coarrse heuristic\n\
\tcoarse-populate=[true|false] \"populate\" by pre-fetching\n\
ngram-pyp: Pitman-Yor Process ngram language model\n\
\tfile=<file>\n\
\tpopulate=[true|false] \"populate\" by pre-fetching\n\
\torder=<order>\n\
\tname=feature-name(default: ngram-pyp)\n\
\tno-bos-eos=[true|false] do not add bos/eos\n\
\tskip-sgml-tag=[true|false] skip sgml tags\n\
\tsplit-estimate=[true|false] split estimated ngram score\n\
\tcoarse-order=<order> ngram order for coarse heuristic\n\
\tcoarse-file=<file>   ngram for coarrse heuristic\n\
\tcoarse-populate=[true|false] \"populate\" by pre-fetching\n\
ngram-rnn: limited memory recurrent neural network ngram language model\n\
\tfile=<file>\n\
\tpopulate=[true|false] \"populate\" by pre-fetching\n\
\torder=<order>\n\
\tname=feature-name(default: ngram-rnn)\n\
\tno-bos-eos=[true|false] do not add bos/eos\n\
\tskip-sgml-tag=[true|false] skip sgml tags\n\
\tsplit-estimate=[true|false] split estimated ngram score\n\
\tcoarse-order=<order> ngram order for coarse heuristic\n\
\tcoarse-file=<file>   ngram for coarrse heuristic\n\
\tcoarse-populate=[true|false] \"populate\" by pre-fetching\n\
ngram-tree: ngram tree feature\n\
\tcluster=[word class file]\n\
\tstemmer=[stemmer spec]\n\
\talignment=[true|false] alignment forest mode\n\
\tsource-root=[true|false] source root mode (for tree composition)\n\
\tname=feature-name-prefix(default: ngram-tree)\n\
parent: parent feature\n\
\tcluster=[word class file]\n\
\tstemmer=[stemmer spec]\n\
\texclude-terminal=[true|false] exclude terminal symbol\n\
\talignment=[true|false] alignment forest mode\n\
\tsource-root=[true|false] source root mode (for tree composition)\n\
\tname=feature-name-prefix(default: parent)\n\
permute: permutation feature\n\
\tweights=weight file for collapsed feature\n\
\tcollapse=[true|false] collapsed feature\n\
sgml-tag: sgml-tag feature\n\
span: lexical span feature\n\
variational: variational feature for variational decoding\n\
\torder=<order>\n\
\tno-bos-eos=[true|false] do not add bos/eos\n\
sparse-lexicon: sparse lexicon feature\n\
\tcluster-source=[word class file] word-class for source side\n\
\tcluster-target=[word class file] word-class for target side\n\
\tstemmer-source=[stemmer spec] stemming for source side\n\
\tstemmer-target=[stemmer spec] stemming for target side\n\
\tskip-sgml-tag=[true|false] skip sgml tags\n\
\tunique-source=[true|source] unique source labels\n\
\tname=feature-name-prefix (default: sparse-lexicon)\n\
\tlexicon=lexicon file\n\
\tlexicon-prefix=prefix lexicon file\n\
\tlexicon-suffix=suffix lexicon file\n\
\tpair=[true|false]   use of simple source/target pair (default: true)\n\
\tprefix=[true|false] use of source prefix\n\
\tsuffix=[true|false] use of source suffix\n\
\tfertility=[true|false] lexical fertility feature\n\
sparse-ngram: sparse ngram feature\n\
\torder=<order>\n\
\tno-bos-eos=[true|false] do not add bos/eos\n\
\tskip-sgml-tag=[true|false] skip sgml tags\n\
\tname=feature-name-prefix (default: sparse-ngram)\n\
\tcluster=[word class file] word-class for ngram\n\
\tstemmer=[stemmer spec] stemming for ngram\n\
word-penalty: word penalty feature\n\
rule-penalty: rule penalty feature\n\
arity-penalty: rule arity penalty feature\n\
glue-tree-penalty: glue tree penalty feature\n\
non-latin-penalty: non-latin word penalty feature\n\
internal-node-penalty: internal node penalty feature\n\
rule-shape: rule shape feature\n\
vocabulary: vocabulary feature\n\
\tfile=[vocabulary file]\n\
\toov=[true|false] oov penalty mode\n\
\tname=feature-name (default: vocabulary)\n\
relative-position: relative alignment feature\n\
\tcluster=[word class file]\n\
\tstemmer=[stemmer spec]\n\
path: path features\n\
\tcluster-source=[word class file]\n\
\tcluster-target=[word class file]\n\
\tstemmer-source=[stemmer spec]\n\
\tstemmer-target=[stemmer spec]\n\
null-path: path involving epsilon\n\
fertility-local: local fertility feature\n\
\tcluster=[word class file]\n\
\tstemmer=[stemmer spec]\n\
target-bigram: target bigram feature\n\
\tcluster=[word class file]\n\
\tstemmer=[stemmer spec]\n\
word-pair: word pair feature\n\
\tcluster-source=[word class file]\n\
\tcluster-target=[word class file]\n\
\tstemmer-source=[stemmer spec]\n\
\tstemmer-target=[stemmer spec]\n\
";
    return desc;
  }

  FeatureFunction::feature_function_ptr_type FeatureFunction::create(const utils::piece& parameter)
  {
    typedef cicada::Parameter parameter_type;
    
    const parameter_type param(parameter);
    const utils::ipiece param_name(param.name());
    
    if (param_name == "ngram")
      return feature_function_ptr_type(new feature::NGram(parameter));
    else if (param_name == "kenlm")
      return feature::KenLMFactory().create(parameter);
    else if (param_name == "ngram-nn")
      return feature_function_ptr_type(new feature::NGramNN(parameter));
    else if (param_name == "ngram-rnn")
      return feature_function_ptr_type(new feature::NGramRNN(parameter));
    else if (param_name == "ngram-pyp")
      return feature_function_ptr_type(new feature::NGramPYP(parameter));
    else if (param_name == "neighbours" || param_name == "neighbors")
      return feature_function_ptr_type(new feature::Neighbours(parameter));
    else if (param_name == "ngram-tree")
      return feature_function_ptr_type(new feature::NGramTree(parameter));
    else if (param_name == "antecedent")
      return feature_function_ptr_type(new feature::Antecedent(parameter));
    else if (param_name == "parent")
      return feature_function_ptr_type(new feature::Parent(parameter));
    else if (param_name == "permute")
      return feature_function_ptr_type(new feature::Permute(parameter));
    else if (param_name == "bleu")
      return feature_function_ptr_type(new feature::Bleu(parameter));
    else if (param_name == "bleus")
      return feature_function_ptr_type(new feature::BleuS(parameter));
    else if (param_name == "bleu-expected")
      return feature_function_ptr_type(new feature::BleuExpected(parameter));
    else if (param_name == "bleu-linear")
      return feature_function_ptr_type(new feature::BleuLinear(parameter));
    else if (param_name == "bleu-multi" || param_name == "bleu-multiple")
      return feature_function_ptr_type(new feature::BleuMulti(parameter));
    else if (param_name == "distortion")
      return feature_function_ptr_type(new feature::Distortion(parameter));
    else if (param_name == "deletion")
      return feature_function_ptr_type(new feature::Deletion(parameter));
    else if (param_name == "dependency")
      return feature_function_ptr_type(new feature::Dependency(parameter));
    else if (param_name == "depeval")
      return feature_function_ptr_type(new feature::Depeval(parameter));
    else if (param_name == "frontier-bigram")
      return feature_function_ptr_type(new feature::FrontierBigram(parameter));
    else if (param_name == "frontier-lexicon")
      return feature_function_ptr_type(new feature::FrontierLexicon(parameter));
    else if (param_name == "frontier-pair")
      return feature_function_ptr_type(new feature::FrontierPair(parameter));
    else if (param_name == "frontier-shape")
      return feature_function_ptr_type(new feature::FrontierShape(parameter));
    else if (param_name == "global-lexicon")
      return feature_function_ptr_type(new feature::GlobalLexicon(parameter));
    else if (param_name == "insertion")
      return feature_function_ptr_type(new feature::Insertion(parameter));
    else if (param_name == "lexicalized-reordering"
	     || param_name == "lexicalized-reorder"
	     || param_name == "lexical-reordering"
	     || param_name == "lexical-reorder")
      return feature_function_ptr_type(new feature::LexicalizedReordering(parameter));
    else if (param_name == "lexicon")
      return feature_function_ptr_type(new feature::Lexicon(parameter));
    else if (param_name == "sgml-tag")
      return feature_function_ptr_type(new feature::SGMLTag(parameter));
    else if (param_name == "span")
      return feature_function_ptr_type(new feature::Span(parameter));
    else if (param_name == "variational")
      return feature_function_ptr_type(new feature::Variational(parameter));
    else if (param_name == "word-penalty")
      return feature_function_ptr_type(new feature::WordPenalty());
    else if (param_name == "rule-penalty")
      return feature_function_ptr_type(new feature::RulePenalty());
    else if (param_name == "arity-penalty")
      return feature_function_ptr_type(new feature::ArityPenalty());
    else if (param_name == "glue-tree-penalty")
      return feature_function_ptr_type(new feature::GlueTreePenalty());
    else if (param_name == "non-latin-penalty")
      return feature_function_ptr_type(new feature::NonLatinPenalty());
    else if (param_name == "internal-node-penalty")
      return feature_function_ptr_type(new feature::InternalNodePenalty());
    else if (param_name == "relative-position")
      return feature_function_ptr_type(new feature::RelativePosition(parameter));
    else if (param_name == "null-path")
      return feature_function_ptr_type(new feature::NullPath(parameter));
    else if (param_name == "path")
      return feature_function_ptr_type(new feature::Path(parameter));
    else if (param_name == "fertility-local")
      return feature_function_ptr_type(new feature::FertilityLocal(parameter));
    else if (param_name == "target-bigram")
      return feature_function_ptr_type(new feature::TargetBigram(parameter));
    else if (param_name == "word-pair")
      return feature_function_ptr_type(new feature::WordPair(parameter));
    else if (param_name == "rule-shape")
      return feature_function_ptr_type(new feature::RuleShape(parameter));
    else if (param_name == "sparse-lexicon")
      return feature_function_ptr_type(new feature::SparseLexicon(parameter));
    else if (param_name == "sparse-ngram")
      return feature_function_ptr_type(new feature::SparseNGram(parameter));
    else
      throw std::runtime_error("unknown feature: " + parameter);
    
  }
  
};
