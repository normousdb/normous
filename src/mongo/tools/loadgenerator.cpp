/** @file loadgenerator.cpp */

/**
 *    Copyright (C) 2012 10gen Inc.
 *
 *    This program is free software: you can redistribute it and/or  modify
 *    it under the terms of the GNU Affero General Public License, version 3,
 *    as published by the Free Software Foundation.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU Affero General Public License for more details.
 *
 *    You should have received a copy of the GNU Affero General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 *  LoadGenerator drives a certain number (# threads) of simultaneous findOne queries into a
 *  specified number of databases  as quickly as it can at a mongo instance,
 *  continuously, for some number of seconds.
 *
 *  The document of interest is selected by picking up a random document
 *  from total number of documents.
 *
 */

/*
 * For internal reference:
 * Each document generated by the docgenerator.cpp is 176 bytes long
 * Number of documents per instance size :
 * small (500 MB) : 2978905 docs spread over 5 dbs (each db is 100 MB)  Docs Per DB :  595781
 * medium (5 GB) : 30504030 docs spread over 5 dbs (each db is 1 GB)         Docs Per DB :  6100806
 * large (25 GB) : 152520145 docs evenly spread over 5 dbs (each db is 5 GB)  Docs Per DB :  30504029
 * vlarge (100 GB) : 621172954 docs evenly spread over 10 dbs (each db is 10 GB)  Docs Per DB : 61008058
 *
 *
 */

#include <string>

#include <boost/scoped_ptr.hpp>
#include <boost/program_options.hpp>

#include "mongo/client/dbclientinterface.h"
#include "mongo/scripting/bench.h"
#include "mongo/util/assert_util.h"
#include "mongo/util/mongoutils/str.h"
#include "mongo/util/md5.hpp"

using namespace std;

using namespace mongo;

namespace po = boost::program_options;

namespace {

struct LoadGeneratorOptions {

    LoadGeneratorOptions() :
        hostname("localhost"),
        instanceSize( "large" ),
        numdbs( 5 ),
        resultDB( "" ),
        numOps( 60000 ),
        durationSeconds( 60 ),
        parallelThreads( 32 ),
        trials( 5 ),
        docsPerDB( 0 )
     { }

    string hostname;
    string instanceSize;
    int numdbs;
    string resultDB;
    int numOps;
    double durationSeconds;
    int parallelThreads;
    int trials;
    unsigned long long docsPerDB;
};

LoadGeneratorOptions globalLoadGenOption;

double randomBetweenRange(const int& min, const int& max) {
    return rand() % (max - min) + min;
}

mongo::DBClientBase *getDBConnection() {
    string errmsg;
    mongo::ConnectionString connectionString = mongo::ConnectionString::parse(
            globalLoadGenOption.hostname, errmsg );
    mongo::fassert( 16182, connectionString.isValid() );
    mongo::DBClientBase *connection = connectionString.connect( errmsg );
    mongo::fassert( 16183, connection != NULL );
    return connection;
}

void dropNS(const string ns) {
    boost::scoped_ptr<mongo::DBClientBase> connection( getDBConnection() );
    connection->dropCollection( ns );
}

void writeToNS(const string ns, const mongo::BSONObj bs) {
    boost::scoped_ptr<mongo::DBClientBase> connection( getDBConnection() );
    connection->insert( ns, bs );
    mongo::fassert( 16184, connection->getLastError().empty() );
}

// find the number of documents in a namespace
void numDocsInNS(const string ns) {
    boost::scoped_ptr<mongo::DBClientBase> connection( getDBConnection() );
    globalLoadGenOption.docsPerDB =  connection->count( ns );
}


mongo::BSONArray generateFindOneOps() {

    mongo::BSONArrayBuilder queryOps;

    /*
     *  query a namespace and find the number of docs in that ns. All benchmark namespaces should have
     *  the same number of docs.
     */
    string queryNS = mongoutils::str::stream() << globalLoadGenOption.instanceSize
                                               << "DB0.sampledata";
    numDocsInNS( queryNS );

    /*
     * Now fill the queryOps array. The findOne query operations will be evenly distributed
     * across all databases. Thus it tries to find a random document from db1, then db2,
     * and so on and so forth.
     */
    for (int i = 0; i < globalLoadGenOption.numOps; ++i) {

        queryNS = mongoutils::str::stream() << globalLoadGenOption.instanceSize
                                            << "DB"
                                            << i % globalLoadGenOption.numdbs
                                            << ".sampledata";

        // select a random document among all the documents
        unsigned long long centerQueryKey =
                ( randomBetweenRange(0, 100 ) *  globalLoadGenOption.docsPerDB ) / 100;

        // cast to long long from unsigned long long as BSON didn't have the overloaded method
        mongo::BSONObj query =
                BSON( "counterUp" << static_cast<long long>( floor(centerQueryKey) ) );

        queryOps.append( BSON( "ns" << queryNS <<
                                "op" << "findOne" <<
                                "query" << query) );
    }

    return queryOps.arr();
}


mongo::BenchRunConfig *createBenchRunConfig() {

    return mongo::BenchRunConfig::createFromBson(
            BSON ( "ops" << generateFindOneOps()
                         << "parallel" << globalLoadGenOption.parallelThreads
                         << "seconds" << globalLoadGenOption.durationSeconds
                          << "host"<< globalLoadGenOption.hostname ) );
}

void runTest() {

    stringstream oss;

    for (int i = 0; i<globalLoadGenOption.trials; ++i) {

        mongo::BenchRunner runner( createBenchRunConfig() );
        runner.start();
        mongo::sleepmillis( 1000 * globalLoadGenOption.durationSeconds );
        runner.stop();
        mongo::BenchRunStats stats;
        runner.populateStats( &stats );

        long long numEvents =
                static_cast<long long>( stats.findOneCounter.getNumEvents() );
        long long totalTimeMicros =
                static_cast<long long>( stats.findOneCounter.getTotalTimeMicros() );

        long long latencyMicros = 0.0;

        if (numEvents)
            latencyMicros = totalTimeMicros / numEvents;

        mongo::BSONObjBuilder buf;
        buf.append( "numEvents", numEvents );
        buf.append( "totalTimeMicros", totalTimeMicros );
        buf.append( "insertLatencyMicros", latencyMicros );

        for (map<string, long long>::iterator it = stats.opcounters.begin(); it != stats.opcounters.end(); ++it) {
            buf.append( (*it).first + "ThroughputPerSec",
                        (*it).second/globalLoadGenOption.durationSeconds );
        }

        /*
         *   if the user did not pass a resultdb cmdline parameter then we won't write to the db.
         *   this is useful in cases where we just want to drive a constant load from
         *   a client and are not really interested in the statistics from it and
         *   so don't really care to save the stats to a db.
         */

        if( !globalLoadGenOption.resultDB.empty() ) {
            string resultNS = mongoutils::str::stream() <<
                    globalLoadGenOption.resultDB << ".trial" << i;
            dropNS( resultNS );
            writeToNS( resultNS, buf.obj() );
        }
        oss << latencyMicros << "    "
            << stats.opcounters["query"] / globalLoadGenOption.durationSeconds << "    ";

    }
    cout << oss.str() << endl;
}


int parseCmdLineOptions(int argc, char **argv) {

    try {

        po::options_description general_options("General options");

        general_options.add_options()
        ("help", "produce help message")
        ("hostname,H", po::value<string>() , "ip address of the host where mongod is running " )
        ("instanceSize,I", po::value<string>(), "DB type (small/medium/large/vlarge) " )
        ("numdbs", po::value<int>(), " number of databases in this instance" )
        ("trials", po::value<int>(), "number of trials")
        ("durationSeconds,D", po::value<double>(), "how long should each trial run")
        ("parallelThreads,P",po::value<int>(), "number of threads")
        ("numOps", po::value<int>(), "number of ops per thread")
        ("resultDB", po::value<string>(), "DB name where you would like to save the results. "
                "If this parameter is empty results will not be written to a db")
        ;

        po::variables_map params;
        po::store(po::parse_command_line(argc, argv, general_options), params);
        po::notify(params);

        /*
         * Parse the values if supplied by the user. No data sanity check is performed
         * here so meaningless values (for eg. passing --numdbs 0) can crash the program.
         * TODO: Perform data sanity check
         */

        if(params.count("help")) {
            cout << general_options << "\n";
            return 0;
        }
        if (params.count("hostname")) {
            globalLoadGenOption.hostname = params["hostname"].as<string>();
        }
        if (params.count("instanceSize")) {
            globalLoadGenOption.instanceSize = params["instanceSize"].as<string>();
        }
        if (params.count("numdbs")) {
            globalLoadGenOption.numdbs = params["numdbs"].as<int>();
        }
        if (params.count("trials")) {
            globalLoadGenOption.trials = params["trials"].as<int>();
        }
        if (params.count("durationSeconds")) {
            globalLoadGenOption.durationSeconds = params["durationSeconds"].as<double>();
        }
        if (params.count("parallelThreads")) {
            globalLoadGenOption.parallelThreads = params["parallelThreads"].as<int>();
        }
        if (params.count("numOps")) {
            globalLoadGenOption.numOps = params["numOps"].as<int>();
        }
        if (params.count("resultDB")) {
           globalLoadGenOption.resultDB = params["resultDB"].as<string>();
       }
    }
    catch(exception& e) {
        cerr << "error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}

} // namespace


int main(int argc, char **argv) {

    if( parseCmdLineOptions(argc, argv) )
        return 1;

    runTest();
    return 0;
}

