
//
// refset format:
// 0 |||  reference translatin for source sentence 0
// 0 |||  another reference
// 1 |||  reference translation for source sentence 1
//

#include <iostream>
#include <vector>
#include <deque>
#include <string>
#include <stdexcept>
#include <numeric>
#include <algorithm>

#include "cicada/sentence.hpp"
#include "cicada/lattice.hpp"
#include "cicada/hypergraph.hpp"
#include "cicada/inside_outside.hpp"

#include "cicada/feature_function.hpp"
#include "cicada/weight_vector.hpp"
#include "cicada/semiring.hpp"
#include "cicada/viterbi.hpp"

#include "cicada/apply.hpp"
#include "cicada/model.hpp"

#include "cicada/feature/bleu.hpp"
#include "cicada/feature/bleu_linear.hpp"
#include "cicada/parameter.hpp"

#include "cicada/eval.hpp"

#include "utils/program_options.hpp"
#include "utils/compress_stream.hpp"
#include "utils/resource.hpp"
#include "utils/lockfree_list_queue.hpp"
#include "utils/bithack.hpp"
#include "utils/space_separator.hpp"

#include <boost/tokenizer.hpp>
#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/lexical_cast.hpp>

#include <boost/thread.hpp>

#include "lbfgs.h"

#include "utils/base64.hpp"
#include "utils/mpi.hpp"
#include "utils/mpi_device.hpp"
#include "utils/mpi_device_bcast.hpp"
#include "utils/mpi_stream.hpp"
#include "utils/mpi_stream_simple.hpp"

typedef boost::filesystem::path path_type;
typedef std::vector<path_type, std::allocator<path_type> > path_set_type;

typedef cicada::Symbol   symbol_type;
typedef cicada::Vocab    vocab_type;
typedef cicada::Sentence sentence_type;

typedef cicada::HyperGraph hypergraph_type;
typedef cicada::Rule       rule_type;

typedef cicada::Model model_type;

typedef hypergraph_type::feature_set_type    feature_set_type;
typedef cicada::WeightVector<double>   weight_set_type;
typedef feature_set_type::feature_type feature_type;

typedef cicada::FeatureFunction feature_function_type;
typedef feature_function_type::feature_function_ptr_type feature_function_ptr_type;

typedef std::vector<hypergraph_type, std::allocator<hypergraph_type> > hypergraph_set_type;


typedef std::vector<feature_function_ptr_type, std::allocator<feature_function_ptr_type> > feature_function_ptr_set_type;

typedef cicada::SentenceVector sentence_set_type;
typedef std::vector<sentence_set_type, std::allocator<sentence_set_type> > sentence_document_type;

typedef cicada::eval::Scorer         scorer_type;
typedef cicada::eval::ScorerDocument scorer_document_type;

typedef scorer_type::score_ptr_type  score_ptr_type;
typedef std::vector<score_ptr_type, std::allocator<score_ptr_type> > score_ptr_set_type;


path_set_type tstset_files;
path_set_type refset_files;
path_type     output_file = "-";

std::string scorer_name = "bleu:order=4,exact=true";

int iteration = 10;
int cube_size = 200;
int debug = 0;

void read_tstset(const path_set_type& files,
		 hypergraph_set_type& graphs,
		 const sentence_document_type& sentences,
		 feature_function_ptr_set_type& features);
void read_refset(const path_set_type& file,
		 scorer_document_type& scorers,
		 sentence_document_type& sentences);
void compute_oracles(const hypergraph_set_type& graphs,
		     const feature_function_ptr_set_type& features,
		     const scorer_document_type& scorers,
		     sentence_set_type& sentences);

void bcast_sentences(sentence_set_type& sentences);
void bcast_weights(const int rank, weight_set_type& weights);
void options(int argc, char** argv);

enum {
  weights_tag = 1000,
  sentence_tag,
  gradients_tag,
  notify_tag,
  termination_tag,
};

inline
int loop_sleep(bool found, int non_found_iter)
{
  if (! found) {
    boost::thread::yield();
    ++ non_found_iter;
  } else
    non_found_iter = 0;
    
  if (non_found_iter >= 64) {
    struct timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 2000001; // above 2ms
    nanosleep(&tm, NULL);
    
    non_found_iter = 0;
  }
  return non_found_iter;
}

int main(int argc, char ** argv)
{
  utils::mpi_world mpi_world(argc, argv);
  
  const int mpi_rank = MPI::COMM_WORLD.Get_rank();
  const int mpi_size = MPI::COMM_WORLD.Get_size();
  
  try {
    options(argc, argv);
    
    // read test set
    if (tstset_files.empty())
      throw std::runtime_error("no test set?");
    
    // read reference set
    scorer_document_type   scorers(scorer_name);
    sentence_document_type sentences;
    
    read_refset(refset_files, scorers, sentences);
    
    if (mpi_rank == 0 && debug)
      std::cerr << "# of references: " << sentences.size() << std::endl;
    
    if (mpi_rank == 0 && debug)
      std::cerr << "reading hypergraphs" << std::endl;
    
    hypergraph_set_type       graphs(sentences.size());
    feature_function_ptr_set_type features(sentences.size());
    
    read_tstset(tstset_files, graphs, sentences, features);

    if (mpi_rank == 0 && debug)
      std::cerr << "# of features: " << feature_type::allocated() << std::endl;
    
    sentence_set_type oracles(sentences.size());
    compute_oracles(graphs, features, scorers, oracles);
    
    if (mpi_rank == 0) {
      utils::compress_ostream os(output_file, 1024 * 1024);
      for (size_t id = 0; id != oracles.size(); ++ id)
	if (! oracles[id].empty())
	  os << id << " ||| " << oracles[id] << '\n';
    }
  }
  catch (const std::exception& err) {
    std::cerr << "error: " << err.what() << std::endl;
    MPI::COMM_WORLD.Abort(1);
    return 1;
  }
  return 0;
}

struct TaskOracle
{
  TaskOracle(const hypergraph_set_type&           __graphs,
	     const feature_function_ptr_set_type& __features,
	     const scorer_document_type&          __scorers,
	     score_ptr_set_type&                  __scores,
	     sentence_set_type&                   __sentences)
    : graphs(__graphs),
      features(__features),
      scorers(__scorers),
      scores(__scores),
      sentences(__sentences)
  {
    score_optimum.reset();
    
    score_ptr_set_type::const_iterator siter_end = scores.end();
    for (score_ptr_set_type::const_iterator siter = scores.begin(); siter != siter_end; ++ siter) 
      if (*siter) {
	if (! score_optimum)
	  score_optimum = (*siter)->clone();
	else
	  *score_optimum += *(*siter);
      } 
  }
  
  
  struct bleu_function
  {
    typedef hypergraph_type::feature_set_type feature_set_type;
    
    typedef cicada::semiring::Logprob<double> value_type;

    bleu_function(const weight_set_type::feature_type& __name, const double& __factor)
      : name(__name), factor(__factor) {}

    weight_set_type::feature_type name;
    double factor;
    
    template <typename Edge>
    value_type operator()(const Edge& edge) const
    {
      return cicada::semiring::traits<value_type>::log(edge.features[name] * factor);
    }
    
  };
  
  struct kbest_traversal
  {
    typedef sentence_type value_type;
    
    template <typename Edge, typename Iterator>
    void operator()(const Edge& edge, value_type& yield, Iterator first, Iterator last) const
    {
      // extract target-yield, features
      
      yield.clear();
    
      rule_type::symbol_set_type::const_iterator titer_end = edge.rule->rhs.end();
      for (rule_type::symbol_set_type::const_iterator titer = edge.rule->rhs.begin(); titer != titer_end; ++ titer)
	if (titer->is_non_terminal()) {
	  const int pos = titer->non_terminal_index() - 1;
	  yield.insert(yield.end(), (first + pos)->begin(), (first + pos)->end());
	} else if (*titer != vocab_type::EPSILON)
	  yield.push_back(*titer);
    }
  };

  struct weight_bleu_function
  {
    typedef cicada::semiring::Logprob<double> value_type;
    
    weight_bleu_function(const weight_set_type::feature_type& __name, const double& __factor)
      : name(__name), factor(__factor) {}
    
    weight_set_type::feature_type name;
    double factor;
    
    value_type operator()(const feature_set_type& x) const
    {
      return cicada::semiring::traits<value_type>::log(x[name] * factor);
    }
  };
  
  void operator()()
  {
    // we will try maximize    
    const bool error_metric = scorers.error_metric();
    const double score_factor = (error_metric ? - 1.0 : 1.0);
    
    weight_set_type::feature_type feature_bleu;
    for (size_t i = 0; i != features.size(); ++ i)
      if (features[i]) {
	feature_bleu = features[i]->feature_name();
	break;
      }
    
    double objective_optimum = (score_optimum
				? score_optimum->score().first * score_factor
				: - std::numeric_limits<double>::infinity());

    hypergraph_type graph_oracle;
    
    for (size_t id = 0; id != graphs.size(); ++ id) {
      if (! graphs[id].is_valid()) continue;
      
      score_ptr_type score_curr;
      if (score_optimum)
	score_curr = score_optimum->clone();
      
      if (scores[id])
	*score_curr -= *scores[id];
      
      cicada::feature::Bleu*       __bleu = dynamic_cast<cicada::feature::Bleu*>(features[id].get());
      cicada::feature::BleuLinear* __bleu_linear = dynamic_cast<cicada::feature::BleuLinear*>(features[id].get());
      
      if (__bleu)
	__bleu->assign(score_curr);
      else
	__bleu_linear->assign(score_curr);
      
      model_type model;
      model.push_back(features[id]);
      
      cicada::apply_cube_prune(model, graphs[id], graph_oracle, weight_bleu_function(feature_bleu, score_factor), cube_size);
      
      cicada::semiring::Logprob<double> weight;
      sentence_type sentence;
      cicada::viterbi(graph_oracle, sentence, weight, kbest_traversal(), bleu_function(feature_bleu, score_factor));
      
      score_ptr_type score_sample = scorers[id]->score(sentence);
      if (score_curr)
	*score_curr += *score_sample;
      else
	score_curr = score_sample;
      
      const double objective = score_curr->score().first * score_factor;
      
      if (objective > objective_optimum || ! scores[id]) {
	score_optimum = score_curr;
	objective_optimum = objective;
	sentences[id] = sentence;
	scores[id] = score_sample;
      }
    }
  }
  
  score_ptr_type score_optimum;
  
  const hypergraph_set_type&           graphs;
  const feature_function_ptr_set_type& features;
  const scorer_document_type&          scorers;
  score_ptr_set_type&                  scores;
  sentence_set_type&                   sentences;
};


void compute_oracles(const hypergraph_set_type& graphs,
		     const feature_function_ptr_set_type& features,
		     const scorer_document_type& scorers,
		     sentence_set_type& sentences)
{
  typedef TaskOracle task_type;

  const int mpi_rank = MPI::COMM_WORLD.Get_rank();
  const int mpi_size = MPI::COMM_WORLD.Get_size();
  
  score_ptr_set_type scores(graphs.size());
  
  score_ptr_type score_optimum;
  double objective_optimum = - std::numeric_limits<double>::infinity();
  
  const bool error_metric = scorers.error_metric();
  const double score_factor = (error_metric ? - 1.0 : 1.0);
  
  for (int iter = 0; iter < iteration; ++ iter) {
    if (debug && mpi_rank == 0)
      std::cerr << "iteration: " << (iter + 1) << std::endl;
    
    task_type(graphs, features, scorers, scores, sentences)();

    bcast_sentences(sentences);
    
    score_optimum.reset();
    for (size_t id = 0; id != sentences.size(); ++ id)
      if (! sentences[id].empty()) {
	scores[id] = scorers[id]->score(sentences[id]);
	
	if (! score_optimum)
	  score_optimum = scores[id]->clone();
	else
	  *score_optimum += *scores[id];
      }
    
    const double objective = score_optimum->score().first * score_factor;
    if (mpi_rank == 0 && debug)
      std::cerr << "oracle score: " << objective << std::endl;

    int terminate = (objective <= objective_optimum);
    
    MPI::COMM_WORLD.Bcast(&terminate, 1, MPI::INT, 0);
    
    if (terminate) break;
    
    objective_optimum = objective;
  }
  
  for (size_t id = 0; id != graphs.size(); ++ id)
    if (features[id]) {
      if (! scores[id])
	throw std::runtime_error("no scores?");
      
      score_ptr_type score_curr = score_optimum->clone();
      *score_curr -= *scores[id];
      
      cicada::feature::Bleu*       __bleu = dynamic_cast<cicada::feature::Bleu*>(features[id].get());
      cicada::feature::BleuLinear* __bleu_linear = dynamic_cast<cicada::feature::BleuLinear*>(features[id].get());
      
      if (__bleu)
	__bleu->assign(score_curr);
      else
	__bleu_linear->assign(score_curr);
    }

}


void read_tstset(const path_set_type& files,
		 hypergraph_set_type& graphs,
		 const sentence_document_type& sentences,
		 feature_function_ptr_set_type& features)
{
  const int mpi_rank = MPI::COMM_WORLD.Get_rank();
  const int mpi_size = MPI::COMM_WORLD.Get_size();

  path_set_type::const_iterator titer_end = tstset_files.end();
  for (path_set_type::const_iterator titer = tstset_files.begin(); titer != titer_end; ++ titer) {
    
    if (mpi_rank == 0 && debug)
      std::cerr << "file: " << *titer << std::endl;
      
    if (boost::filesystem::is_directory(*titer)) {

      for (int i = mpi_rank; /**/; i += mpi_size) {
	const path_type path = (*titer) / (boost::lexical_cast<std::string>(i) + ".gz");

	if (! boost::filesystem::exists(path)) break;
	
	utils::compress_istream is(path, 1024 * 1024);
	
	int id;
	std::string sep;
	hypergraph_type hypergraph;
            
	if (is >> id >> sep >> hypergraph) {
	  if (sep != "|||")
	    throw std::runtime_error("format error?: " + path.file_string());
	
	  if (id >= static_cast<int>(graphs.size()))
	    throw std::runtime_error("tstset size exceeds refset size?" + boost::lexical_cast<std::string>(id) + ": " + path.file_string());
	  
	  if (id % mpi_size != mpi_rank)
	    throw std::runtime_error("difference it?");
	  
	  graphs[id].unite(hypergraph);
	} else
	  throw std::runtime_error("format error?: " + path.file_string());
      }
    } else {
      utils::compress_istream is(*titer, 1024 * 1024);
      
      int id;
      std::string sep;
      hypergraph_type hypergraph;
            
      while (is >> id >> sep >> hypergraph) {
	
	if (sep != "|||")
	  throw std::runtime_error("format error?: " + titer->file_string());
	
	if (id >= static_cast<int>(graphs.size()))
	  throw std::runtime_error("tstset size exceeds refset size?" + boost::lexical_cast<std::string>(id) + ": " + titer->file_string());
	
	if (id % mpi_size == mpi_rank)
	  graphs[id].unite(hypergraph);
      }
    }
  }

  if (debug && mpi_rank == 0)
    std::cerr << "assign BLEU scorer" << std::endl;
    
  for (size_t id = 0; id != graphs.size(); ++ id) 
    if (static_cast<int>(id % mpi_size) == mpi_rank) {
      if (graphs[id].goal == hypergraph_type::invalid)
	std::cerr << "invalid graph at: " << id << std::endl;
      else {
	features[id] = feature_function_type::create(scorer_name);
	
	cicada::feature::Bleu*       __bleu = dynamic_cast<cicada::feature::Bleu*>(features[id].get());
	cicada::feature::BleuLinear* __bleu_linear = dynamic_cast<cicada::feature::BleuLinear*>(features[id].get());
	
	if (! __bleu && ! __bleu_linear)
	  throw std::runtime_error("invalid bleu feature function...");

	static const cicada::Lattice       __lattice;
	static const cicada::SpanVector    __spans;
	static const cicada::NGramCountSet __ngram_counts;
	
	if (__bleu)
	  __bleu->assign(id, graphs[id], __lattice, __spans, sentences[id], __ngram_counts);
	else
	  __bleu_linear->assign(id, graphs[id], __lattice, __spans, sentences[id], __ngram_counts);
      }
    }
  
  // collect weights...
  for (int rank = 0; rank < mpi_size; ++ rank) {
    weight_set_type weights;
    weights.allocate();
    
    for (feature_type::id_type id = 0; id != feature_type::allocated(); ++ id)
      if (! feature_type(id).empty())
	weights[feature_type(id)] = 1.0;
    
    bcast_weights(rank, weights);
  }
}

void read_refset(const path_set_type& files,
		 scorer_document_type& scorers,
		 sentence_document_type& sentences)
{
  typedef boost::tokenizer<utils::space_separator> tokenizer_type;

  if (files.empty())
    throw std::runtime_error("no reference files?");
  
  sentences.clear();

  for (path_set_type::const_iterator fiter = files.begin(); fiter != files.end(); ++ fiter) {
    
    if (! boost::filesystem::exists(*fiter) && *fiter != "-")
      throw std::runtime_error("no reference file: " + fiter->file_string());

    utils::compress_istream is(*fiter, 1024 * 1024);
    
    std::string line;
    
    while (std::getline(is, line)) {
      tokenizer_type tokenizer(line);
    
      tokenizer_type::iterator iter = tokenizer.begin();
      if (iter == tokenizer.end()) continue;
    
      const int id = boost::lexical_cast<int>(*iter);
      ++ iter;
    
      if (iter == tokenizer.end()) continue;
      if (*iter != "|||") continue;
      ++ iter;
      
      if (id >= static_cast<int>(scorers.size()))
	scorers.resize(id + 1);
      
      if (id >= static_cast<int>(sentences.size()))
	sentences.resize(id + 1);
      
      if (! scorers[id])
	scorers[id] = scorers.create();
      
      sentences[id].push_back(sentence_type(iter, tokenizer.end()));
      scorers[id]->insert(sentences[id].back());
    }
  }  
}

void bcast_sentences(sentence_set_type& sentences)
{
  const int mpi_rank = MPI::COMM_WORLD.Get_rank();
  const int mpi_size = MPI::COMM_WORLD.Get_size();
  
  for (int rank = 0; rank < mpi_size; ++ rank) {
    if (rank == mpi_rank) {
      boost::iostreams::filtering_ostream os;
      os.push(boost::iostreams::gzip_compressor());
      os.push(utils::mpi_device_bcast_sink(rank, 4096));
      
      for (size_t id = 0; id != sentences.size(); ++ id)
	if (static_cast<int>(id % mpi_size) == mpi_rank)
	  os << id << " ||| " << sentences[id] << '\n';
      
    } else {
      boost::iostreams::filtering_istream is;
      is.push(boost::iostreams::gzip_decompressor());
      is.push(utils::mpi_device_bcast_source(rank, 4096));
      
      int id;
      std::string sep;
      sentence_type sentence;
      
      while (is >> id >> sep >> sentence) {
	if (sep != "|||")
	  throw std::runtime_error("invalid sentence format");
	
	sentences[id] = sentence;
      }
    }
  }
}


void bcast_weights(const int rank, weight_set_type& weights)
{
  typedef std::vector<char, std::allocator<char> > buffer_type;

  const int mpi_rank = MPI::COMM_WORLD.Get_rank();
  const int mpi_size = MPI::COMM_WORLD.Get_size();
  
  if (mpi_rank == rank) {
    boost::iostreams::filtering_ostream os;
    os.push(utils::mpi_device_bcast_sink(rank, 1024));
    
    static const weight_set_type::feature_type __empty;
    
    weight_set_type::const_iterator witer_begin = weights.begin();
    weight_set_type::const_iterator witer_end = weights.end();
    
    for (weight_set_type::const_iterator witer = witer_begin; witer != witer_end; ++ witer)
      if (*witer != 0.0) {
	const weight_set_type::feature_type feature(witer - witer_begin);
	if (feature != __empty)
	  os << feature << ' ' << utils::encode_base64(*witer) << '\n';
      }
  } else {
    weights.clear();
    weights.allocate();
    
    boost::iostreams::filtering_istream is;
    is.push(utils::mpi_device_bcast_source(rank, 1024));
    
    std::string feature;
    std::string value;
    
    while ((is >> feature) && (is >> value))
      weights[feature] = utils::decode_base64<double>(value);
  }
}


void options(int argc, char** argv)
{
  namespace po = boost::program_options;

  po::options_description opts_config("configuration options");
  
  opts_config.add_options()
    ("tstset",  po::value<path_set_type>(&tstset_files)->multitoken(), "test set file(s) (in hypergraph format)")
    ("refset",  po::value<path_set_type>(&refset_files)->multitoken(), "reference set file(s)")
    
    ("output", po::value<path_type>(&output_file)->default_value(output_file), "output file")
    
    ("scorer",      po::value<std::string>(&scorer_name)->default_value(scorer_name), "error metric")
    
    ("iteration", po::value<int>(&iteration), "# of hill-climbing iteration")
    ("cube-size", po::value<int>(&cube_size), "cube-pruning size")
    ;
  
  po::options_description opts_command("command line options");
  opts_command.add_options()
    ("debug", po::value<int>(&debug)->implicit_value(1), "debug level")
    ("help", "help message");
  
  po::options_description desc_config;
  po::options_description desc_command;
  
  desc_config.add(opts_config);
  desc_command.add(opts_config).add(opts_command);
  
  po::variables_map variables;

  po::store(po::parse_command_line(argc, argv, desc_command, po::command_line_style::unix_style & (~po::command_line_style::allow_guessing)), variables);
  
  po::notify(variables);

  if (variables.count("help")) {
    std::cout << argv[0] << " [options]\n"
	      << desc_command << std::endl;
    exit(0);
  }
}
