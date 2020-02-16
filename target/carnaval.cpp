#include <time.h>
#include <cstdlib>
#include <stdexcept>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <random>
#include <boost/program_options.hpp>

#include "../src/cell.h"
#include "../src/util.h"
#include "../src/bitmap_image.hpp"

using namespace std;
namespace po = boost::program_options;

int main (int argc, char** argv) {

  try {

    // Declare the supported options.
    po::options_description opts("Options");
    opts.add_options()
      ("help,h", "display this help message")
      ("xsize,x", po::value<int>()->default_value(64), "size of board in X dimension")
      ("ysize,y", po::value<int>()->default_value(64), "size of board in Y dimension")
      ("zsize,z", po::value<int>()->default_value(1), "size of board in Z dimension")
      ("init,i",  po::value<string>(), "specify initial sequence")
      ("rnd,r",  po::value<int>(), "seed random number generator")
      ("total-moves,t",  po::value<long>()->default_value(0), "total number of moves")
      ("unit-moves,u",  po::value<long>()->default_value(0), "number of moves per unit")
      ("folds,f",  "periodically log fold string, energy, radius of gyration, and centroid")
      ("monochrome,m",  "no ANSI color codes in logging, please")
      ("period,p", po::value<long>()->default_value(1000), "logging period")
      ("temp,T",  po::value<double>(), "specify temperature")
      ("load,l", po::value<string>(), "load board state from file")
      ("save,s", po::value<string>(), "save board state to file")
      ("post,P", po::value<string>(), "save base-pairing posterior probabilities to JSON file")
      ("bitmap,b", po::value<string>(), "save base-pairing probabilities to bitmap image file")
      ;

    po::variables_map vm;
    po::parsed_options parsed = po::command_line_parser(argc,argv).options(opts).allow_unregistered().run();
    po::store (parsed, vm);
    po::notify(vm);    
      
    // parse args
    if (vm.count("help")) {
      cout << opts << endl;
      return 1;
    }

    time_t timer;
    time (&timer);
    const int seed = vm.count("rnd") ? vm.at("rnd").as<int>() : timer;
    mt19937 mt (seed);
    cerr << "Random seed is " << seed << endl;

    Board board;
    if (vm.count("load")) {
      ifstream infile (vm.at("load").as<string>());
      if (!infile)
	throw runtime_error ("Can't load board file");
      json j;
      infile >> j;
      board = Board::fromJson (j);
    } else {
      board = Board (vm["xsize"].as<int>(),
		     vm["ysize"].as<int>(),
		     vm["zsize"].as<int>());
    }

    if (vm.count("init"))
      board.addSeq (vm.at("init").as<string>());

    if (vm.count("temp"))
      board.params.temp = vm.at("temp").as<double>();

    const long moves = vm.at("total-moves").as<long>() + board.unit.size() * vm.at("unit-moves").as<long>();
    const bool logFolds = vm.count("folds");
    const long logPeriod = vm.at("period").as<long>();
    const bool logColors = !vm.count("monochrome");
    long succeeded = 0, samples = 0;
    map<Board::IndexPair,long> pairCount;
    for (long move = 0; move < moves; ++move) {
      if (board.tryMove (mt))
	++succeeded;
      if (move % logPeriod == 0) {
	if (logFolds)
	  cout << "Move " << move
	       << " (" << fixed << setprecision(1) << (100. * move / moves) << "%) "
	       << (logColors ? board.coloredFoldString() : board.foldString())
	       << " " << setw(5) << board.foldEnergy()
	       << " " << setw(5) << board.unitRadiusOfGyration()
	       << " (" << to_string_join(board.unitCentroid()) << ")"
	       << endl;
	for (const auto& ij: board.indexPairs())
	  ++pairCount[ij];
	++samples;
      }
    }

    if (moves)
      cerr << "Tried " << moves << " moves, " << succeeded << " succeeded" << endl;

    if (vm.count("bitmap")) {
      bitmap_image image (board.unit.size(), board.unit.size());
      for (const auto& ij_n: pairCount) {
	const int level = 255 * ij_n.second / samples;
	image.set_pixel (ij_n.first.first, ij_n.first.second, level, level, level);
      }
      image.save_image (vm.at("bitmap").as<string>().c_str());
    }

    if (vm.count("post")) {
      json js;
      js["samples"] = samples;
      js["sequence"] = board.sequence();
      for (const auto& ij_n: pairCount) {
	const string i = to_string(ij_n.first.first), j = to_string(ij_n.first.second);
	//	js["count"][i][j] = ij_n.second;
	js["prob"][i][j] = ((double) ij_n.second) / samples;
      }
      ofstream outfile (vm.at("post").as<string>());
      if (!outfile)
	throw runtime_error ("Can't save basepair probabilities file");
      outfile << js << endl;
    }

    if (vm.count("save")) {
      json j = board.toJson();
      ofstream outfile (vm.at("save").as<string>());
      if (!outfile)
	throw runtime_error ("Can't save board file");
      outfile << j << endl;
    } else if (!logFolds)
      cout << board.toJson() << endl;

  } catch (const exception& e) {
    cerr << e.what() << endl;
    return EXIT_FAILURE;
  }
  
  return EXIT_SUCCESS;
}
