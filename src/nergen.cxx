/*
  Copyright (c) 2015 - 2020
  CLST Radboud University

  This file is part of toad

  toad is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 3 of the License, or
  (at your option) any later version.

  toad is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.


  For questions and suggestions, see:
      https://github.com/LanguageMachines/toad/issues
  or send mail to:
      lamasoftware (at ) science.ru.nl
*/

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <exception>
#include "ticcutils/StringOps.h"
#include "ticcutils/CommandLine.h"
#include "ticcutils/FileUtils.h"
#include "ticcutils/Configuration.h"
#include "mbt/MbtAPI.h"
#include "libfolia/folia.h"
#include "ucto/tokenize.h"
#include "unicode/ustream.h"
#include "unicode/unistr.h"
#include "frog/ner_tagger_mod.h"
#include "config.h"

using namespace std;

TiCC::LogStream mylog(cerr);

static NERTagger myNer(&mylog);

string EOS_MARK = "\n";

static TiCC::Configuration default_config; // sane defaults
static TiCC::Configuration use_config;     // the config we gonna use

void set_default_config(){
  default_config.setatt( "baseName", "nergen", "NER" );
  default_config.setatt( "settings", "froggen.settings", "tagger" );
  default_config.setatt( "p", "ddwdwfWawaa", "NER" );
  default_config.setatt( "P", "chnppddwdwFawawasss", "NER" );
  default_config.setatt( "n", "10", "NER" );
  default_config.setatt( "M", "1000", "NER" );
  default_config.setatt( "%", "5", "NER" );
  default_config.setatt( "timblOpts",
		    "+vS -G -FColumns K: -a4 U: -a4 -mM -k19 -dID",
		    "NER" );
  default_config.setatt( "set", "http://ilk.uvt.nl/folia/sets/frog-ner-nl", "NER" );
  default_config.setatt( "max_ner_size", "15", "NER" );

}

class setting_error: public std::runtime_error {
public:
  setting_error( const string& key, const string& mod ):
    std::runtime_error( "missing key: '" + key + "' for module: '" + mod + "'" )
  {};
};

void usage( const string& name ){
  cerr << name << " [-c configfile] [-O outputdir] [-g gazetteerfile] inputfile"
       << endl;
  cerr << name << " will convert a 'traditionally' IOB tagged corpus into\n"
       << " a MBT datafile enriched with both POS tag and gazetteer information\n"
       << endl << " After that, a MBT tagger will be trained on that file"
       << endl;
  cerr << "-c 'configfile'\t An existing configfile that will be enriched\n"
       << "\t\t with additional NER specific information." << endl;
  cerr << "-O 'outputdir'\t The directoy where all the outputfiles are stored\n"
       << "\t\t highly recommended to use, because a lot of files are created\n"
       << "\t\t and your working directory will get cluttered." << endl;
  cerr << "-g 'gazetteer'\t a file describing the gazetteer info in the\n"
       << "\t\t format 'ner-cat1<tab>file1'" << endl
       << "\t\t        '...' " << endl
       << "\t\t        'ner-catn<tab> filen'" << endl
       << "\t\t were every file-1 .. file-N is a list of space separated names"
       << endl;
  cerr << "--override\t override O NER tags with those derived from the gazeteers," << endl
       << "\t\t so ONLY when there is NO CONFLICT" << endl;
  cerr << "--bootstrap\t override ALL NER tags with those derived from the gazeteers." << endl
       << "\t\t UNCONDITIONALLY. Creates a new trainfile for nergen, and stops then. " << endl;
  cerr << "--running When using --bootstrap, you can specify this, to signal an input file" << endl
       << "\t\t with 'running text'. A simple file with one sentence per line." << endl
       << "\t\t Otherwise a 2 column tagged file is assumed ." << endl;
}


bool fill_gazet( const string& name ){
  string file = TiCC::basename( name );
  string dir = TiCC::dirname( name );
  return myNer.read_gazets( file, dir );
}

void spit_out( ostream& os,
	       const vector<Tagger::TagResult>& tagv,
	       const vector<string>& orig_ner_file_tags,
	       bool override,
	       bool bootstrap ){
  vector<string> words;
  vector<string> tags;
  for( const auto& tr : tagv ){
    words.push_back( tr.word() );
    tags.push_back( tr.assigned_tag() );
  }

  vector<string> gazet_tags = myNer.create_ner_list( words );
  vector<string> ner_file_tags = orig_ner_file_tags;
  if ( override ){
    vector<tc_pair> orig_ners;
    for ( const auto& it : orig_ner_file_tags ){
      orig_ners.push_back( make_pair( it, 1.0 ) );
    }
    vector<tc_pair> gazet_ners;
    for ( const auto& it : gazet_tags ){
      gazet_ners.push_back( make_pair( it, 1.0 ) );
    }
    myNer.merge_override( orig_ners, gazet_ners, bootstrap, tags );
  }
  if ( bootstrap ){
    for ( size_t i=0; i < words.size(); ++i ){
      string line = words[i] + "\t";
      line += ner_file_tags[i];
      os << line << endl;
    }
  }
  else {
    string prevP = "_";
    string prevN = "_";
    for ( size_t i=0; i < words.size(); ++i ){
      string line = words[i] + "\t" + prevP + "\t" + tags[i] + "\t";
      prevP = tags[i];
      if ( i < words.size() - 1 ){
	line += tags[i+1] + "\t";
      }
      else {
	line += "_\t";
      }
      line += prevN + "\t" + gazet_tags[i] + "\t";
      prevN = gazet_tags[i];
      if ( i < words.size() - 1 ){
	line += gazet_tags[i+1] + "\t";
      }
      else {
	line += "_\t";
      }
      line += ner_file_tags[i];
      os << line << endl;
    }
  }
  if ( EOS_MARK == "\n" ){
    // avoid spurious newlines!
    os << endl;
  }
  else {
    os << EOS_MARK << endl;
  }
}

string to_tag( const string& label ){
  vector<string> parts = TiCC::split_at( label, "+" );
  if ( parts.size() > 1 ){
    // undecided
    return "O";
  }
  else {
    return parts[0];
  }
}

void boot_out( ostream& os,
	       const vector<string>& words ){
  vector<string> gazet_tags = myNer.create_ner_list( words );
  string prev_tag;
  for ( size_t i=0; i < words.size(); ++i ){
    string line = words[i] + "\t";
    string tag = to_tag( gazet_tags[i] );
    if ( tag != "O" ){
      if ( tag == prev_tag ) {
	line += "I-";
      }
      else {
	line += "B-";
      }
      prev_tag = tag;
    }
    line += tag;
    os << line << endl;
  }
  if ( EOS_MARK == "\n" ){
    // avoid spurious newlines!
    os << endl;
  }
  else {
    os << EOS_MARK << endl;
  }
}

void create_train_file( MbtAPI *tagger,
			const string& inpname,
			const string& outname,
			bool override ){
  ofstream os( outname );
  ifstream is( inpname );
  string line;
  string blob;
  vector<string> ner_file_tags; // store the tags as specified in the input
  size_t HeartBeat=0;
  while ( getline( is, line ) ){
    if ( line == "<utt>" ){
      EOS_MARK = "<utt>";
      line.clear();
    }
    if ( line.empty() ) {
      if ( !blob.empty() ){
	vector<Tagger::TagResult> tagv = tagger->TagLine( blob );
	spit_out( os, tagv, ner_file_tags, override, false );
	if ( ++HeartBeat % 8000 == 0 ) {
	  cout << endl;
	}
	if ( HeartBeat % 100 == 0 ) {
	  cout << ".";
	  cout.flush();
	}
	blob.clear();
	ner_file_tags.clear();
      }
      continue;
    }
    vector<string> parts;
    if ( TiCC::split( line, parts) != 2 ){
      cerr << "DOOD: " << line << endl;
      exit(EXIT_FAILURE);
    }
    blob += parts[0] + "\n";
    ner_file_tags.push_back( parts[1] );
  }
  if ( !blob.empty() ){
    vector<Tagger::TagResult> tagv = tagger->TagLine( blob );
    spit_out( os, tagv, ner_file_tags, override, false );
  }
}

void create_boot_file( const string& inpname,
		       const string& outname,
		       bool running=false ){
  ofstream os( outname );
  ifstream is( inpname );
  string line;
  string blob;
  size_t HeartBeat=0;
  while ( getline( is, line ) ){
    if ( line == "<utt>" ){
      EOS_MARK = "<utt>";
      line.clear();
    }
    if ( line.empty() ) {
      if ( !blob.empty() ){
	vector<string> words = TiCC::split( blob );
	boot_out( os, words );
	if ( ++HeartBeat % 8000 == 0 ) {
	  cout << endl;
	}
	if ( HeartBeat % 100 == 0 ) {
	  cout << ".";
	  cout.flush();
	}
	blob.clear();
      }
      continue;
    }
    if ( running ){
      vector<string> words = TiCC::split( line );
      boot_out( os, words );
    }
    else {
      vector<string> parts;
      if ( TiCC::split( line, parts) == 2 ){
	blob += parts[0] + " ";
      }
      else {
	cerr << "DOOD: " << line << endl;
	exit(EXIT_FAILURE);
      }
    }
  }
  if ( !blob.empty() ){
    vector<string> words = TiCC::split( blob );
    boot_out( os, words );
  }
}

int main(int argc, char * const argv[] ) {
  TiCC::CL_Options opts("b:O:c:hVg:X","gazeteer:,help,version,override,bootstrap,running");
  try {
    opts.parse_args( argc, argv );
  }
  catch ( const exception& e ){
    cerr << e.what() << endl;
    exit(EXIT_FAILURE);
  }
  string outputdir;
  string configfile;
  string base_name;
  string gazetteer_name;
  bool override = false;
  bool bootstrap = false;
  bool running = false;
  if ( opts.extract( 'h' ) || opts.extract( "help" ) ){
    usage( opts.prog_name() );
    exit( EXIT_SUCCESS );
  }

  if ( opts.extract( 'V' ) || opts.extract( "version" ) ){
    cerr << "VERSION: " << VERSION << endl;
    exit( EXIT_SUCCESS );
  }
  set_default_config();
  if ( opts.extract( 'c', configfile ) ){
    if ( !use_config.fill( configfile ) ) {
      cerr << "unable to open:" << configfile << endl;
      exit( EXIT_FAILURE );
    }
    cout << "using configuration: " << configfile << endl;
  }
  bool keepX = opts.extract( 'X' );
  opts.extract( 'O', outputdir );
  if ( !outputdir.empty() ){
    if ( outputdir[outputdir.length()-1] != '/' )
      outputdir += "/";
    if ( !TiCC::isDir( outputdir ) && !TiCC::createPath( outputdir ) ){
      cerr << "output dir not usable: " << outputdir << endl;
      exit(EXIT_FAILURE);
    }
  }
  else if ( !configfile.empty() ){
    outputdir = TiCC::dirname( configfile );
  }
  if ( opts.extract( 'b', base_name ) ){
    use_config.setatt( "baseName", base_name, "NER" );
  }
  cerr << "cfdir=" << use_config.configDir() << endl;
  cerr << "default cfdir=" << default_config.configDir() << endl;
  use_config.merge( default_config ); // to be sure to have all we need
  cerr << "na merge cfdir=" << use_config.configDir() << endl;
  if ( opts.extract( 'g', gazetteer_name )
       || opts.extract( "gazeteer", gazetteer_name ) ){
  }
  else {
    gazetteer_name = use_config.lookUp( "known_ners", "NER" );
  }
  gazetteer_name = TiCC::realpath( gazetteer_name );
  if ( gazetteer_name.empty() ){
    cerr << "WARNING: missing gazetteer option (-g). " << endl;
    cerr << "Are u sure ?" << endl;
  }
  if ( !fill_gazet( gazetteer_name ) ){
    exit( EXIT_FAILURE );
  }
  override = opts.extract( "override" );
  bootstrap = opts.extract( "bootstrap" );
  running = opts.extract( "running" );
  if ( running && !bootstrap ){
    cerr << "option --running only allowed for --bootstrap" << endl;
    exit(EXIT_FAILURE);
  }
  // get all required options from the merged config
  // normally these are all there now, so no exceptions then

  string ner_set_name = use_config.lookUp( "set", "NER" );
  if ( ner_set_name.empty() ){
    throw setting_error( "set", "NER" );
  }
  string p_pat = use_config.lookUp( "p", "NER" );
  if ( p_pat.empty() ){
    throw setting_error( "p", "NER" );
  }
  string P_pat = use_config.lookUp( "P", "NER" );
  if ( P_pat.empty() ){
    throw setting_error( "P", "NER" );
  }
  string timblopts = use_config.lookUp( "timblOpts", "NER" );
  if ( timblopts.empty() ){
    throw setting_error( "timblOpts", "NER" );
  }
  string M_opt = use_config.lookUp( "M", "NER" );
  if ( M_opt.empty() ){
    throw setting_error( "M", "NER" );
  }
  string n_opt = use_config.lookUp( "n", "NER" );
  if ( n_opt.empty() ){
    throw setting_error( "n", "NER" );
  }
  string perc_opt = use_config.lookUp( "%", "NER" );
  if ( perc_opt.empty() ){
    throw setting_error( "%", "NER" );
  }
  base_name = use_config.lookUp( "baseName", "NER" );
  if ( base_name.empty() ){
    throw setting_error( "baseName", "NER" );
  }
  vector<string> names = opts.getMassOpts();
  if ( names.size() == 0 ){
    cerr << "missing inputfile" << endl;
    usage( opts.prog_name() );
    exit(EXIT_FAILURE);
  }
  else if ( names.size() > 1 ){
    cerr << "only 1 inputfile is allowed" << endl;
    exit(EXIT_FAILURE);
  }
  string inpname = names[0];
  string outname = outputdir + base_name;
  if ( bootstrap ){
    outname += ".boosted";
    create_boot_file( inpname, outname, running );
    cout << endl << "Created a new bootstrapped nergen data file: " << outname << endl;
    return EXIT_SUCCESS;
  }
  string mbt_setting = use_config.lookUp( "settings", "tagger" );
  if ( mbt_setting.empty() ){
    throw setting_error( "settings", "tagger" );
  }
  string use_dir = use_config.configDir();
  if ( use_dir.empty() ){
    mbt_setting = "-s " + outputdir + mbt_setting + " -vcf" ;
  }
  else {
    mbt_setting = "-s " + use_dir + mbt_setting + " -vcf" ;
  }
  MbtAPI *PosTagger = new MbtAPI( mbt_setting, mylog );
  if ( !PosTagger->isInit() ){
    cerr << "unable to initialize a POS tagger using:" << mbt_setting << endl;
    exit( EXIT_FAILURE );
  }
  outname += ".data";
  string settings_name = outputdir + base_name + ".settings";
  cout << "Start enriching: " << inpname << " with POS tags"
       << " (every dot represents 100 tagged sentences)" << endl;
  create_train_file( PosTagger, inpname, outname, override );
  cout << endl << "Created a trainingfile: " << outname << endl;
  string taggercommand = "-E " + outname
    + " -s " + settings_name
    + " -p " + p_pat + " -P " + P_pat
    + " -O\""+ timblopts + "\""
    + " -M " + M_opt
    + " -n " + n_opt
    + " -% " + perc_opt;
  if ( EOS_MARK != "<utt>" ){
    taggercommand += " -eEL";
  }
  if ( keepX ){
    taggercommand += " -X";
  }
  taggercommand += " -DLogSilent"; // shut up
  cout << "start tagger: " << taggercommand << endl;
  cout << "this may take several minutes, depending on the corpus size."
       << endl;
  MbtAPI::GenerateTagger( taggercommand );
  cout << "finished tagger" << endl;
  // create a new configfile, based on the use_config
  // first clear unwanted stuff
  use_config.clearatt( "baseName", "NER" );
  use_config.clearatt( "p", "NER" );
  use_config.clearatt( "P", "NER" );
  use_config.clearatt( "timblOpts", "NER" );
  use_config.clearatt( "M", "NER" );
  use_config.clearatt( "n", "NER" );
  use_config.clearatt( "%", "NER" );

  TiCC::Configuration output_config = use_config;

  string setting_name = TiCC::realpath(outputdir) + "/" + base_name + ".settings";
  output_config.setatt( "settings", setting_name, "NER" );
  output_config.setatt( "known_ners", gazetteer_name, "NER" );
  output_config.setatt( "version", "2.0", "NER" );

  string cfg_out;
  if ( configfile.empty() ){
    cfg_out = outputdir + "frog-nergen.cfg.template";
  }
  else {
    configfile = TiCC::basename( configfile );
    const auto ppos = configfile.find( "." );
    if ( ppos == string::npos ){
      cfg_out = outputdir + configfile + "-nergen.cfg.template";
    }
    else {
      cfg_out = outputdir + configfile.substr(0,ppos)
	+ "-nergen" + configfile.substr( ppos );
    }
  }
  output_config.create_configfile( cfg_out );
  cout << "stored a frog configfile template: " << cfg_out << endl;
  return EXIT_SUCCESS;
}
