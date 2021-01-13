//===- Util.cpp - Utils for instrumentation ------------*- C++ -*-===//
//
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//

#include "Util.h"
#include <getopt.h>

using namespace PIMProf;

void Usage()
{
    infomsg("Usage: ./Solver.exe <mode> <mode_specific_params>");
    infomsg("Select mode from: mpki, para, reuse");
    infomsg("Mode specific parameters:");
    infomsg("mpki mode: ./Solver.exe mpki <cpu_stats_file> <pim_stats_file> <output_file>");
    infomsg("para mode: ./Solver.exe para <cpu_stats_file> <pim_stats_file> <output_file>");
    infomsg("reuse mode: ./Solver.exe reuse <cpu_stats_file> <pim_stats_file> <reuse_file> <output_file>");
    exit(0);
}

void CommandLineParser::initialize(int argc, char *argv[])
{
    auto parser = [&](const char* const short_opt, const option long_opt[]) {
        while (true) {
            const auto opt = getopt_long(argc, argv, short_opt, long_opt, nullptr);
            if (opt == -1) break;

            switch (opt)
            {
            case 'c':
                _cpustatsfile = std::string(optarg); std::cout << "c " << _cpustatsfile << std::endl; break;
            case 'p':
                _pimstatsfile = std::string(optarg); std::cout << "p " << _pimstatsfile << std::endl; break;
            case 'r':
                _reusefile = std::string(optarg); std::cout << "r " << _reusefile << std::endl; break;
            case 'o':
                _outputfile = std::string(optarg); std::cout << "o " << _outputfile << std::endl; break;
            case 'h': // -h or --help
            case '?': // Unrecognized option
            default:
                Usage();
            }
        }
    };

    
    if (argc <= 1) { Usage(); }
    std::string _mode_string(argv[1]);
    optind++;
    if (_mode_string == "mpki") {
        _mode = Mode::MPKI;
        const char* const short_opt = "c:p:r:o:h";
        const option long_opt[] = {
            {"cpu", required_argument, nullptr, 'c'},
            {"pim", required_argument, nullptr, 'p'},
            {"reuse", required_argument, nullptr, 'r'},
            {"output", required_argument, nullptr, 'o'},
            {"help", no_argument, nullptr, 'h'},
            {nullptr, no_argument, nullptr, 0}
        };
        parser(short_opt, long_opt);
        if (_cpustatsfile == "" || _pimstatsfile == "" || _outputfile == "") {
            Usage();
        }
    }
    else if (_mode_string == "para") {
        _mode = Mode::PARA;
        assert(0);
    }
    else if (_mode_string == "reuse") {
        _mode = Mode::REUSE;
        const char* const short_opt = "c:p:r:o:h";
        const option long_opt[] = {
            {"cpu", required_argument, nullptr, 'c'},
            {"pim", required_argument, nullptr, 'p'},
            {"reuse", required_argument, nullptr, 'r'},
            {"output", required_argument, nullptr, 'o'},
            {"help", no_argument, nullptr, 'h'},
            {nullptr, no_argument, nullptr, 0}
        };
        parser(short_opt, long_opt);
        if (_cpustatsfile == "" || _pimstatsfile == "" || _reusefile == "" || _outputfile == "") {
            Usage();
        }
    }
    else if (_mode_string == "debug") {
        _mode = Mode::DEBUG;
        const char* const short_opt = "c:p:r:o:h";
        const option long_opt[] = {
            {"cpu", required_argument, nullptr, 'c'},
            {"pim", required_argument, nullptr, 'p'},
            {"reuse", required_argument, nullptr, 'r'},
            {"output", required_argument, nullptr, 'o'},
            {"help", no_argument, nullptr, 'h'},
            {nullptr, no_argument, nullptr, 0}
        };
        parser(short_opt, long_opt);
        if (_cpustatsfile == "" || _pimstatsfile == "" || _reusefile == "" || _outputfile == "") {
            Usage();
        }
    }
    else {
        Usage();
    }
}

void PIMProf::PrintInstruction(std::ostream *out, uint64_t insAddr, std::string insDis, uint32_t simd_len) {
    *out << std::hex << insAddr << std::dec << ", " << insDis << " " << simd_len << std::endl;
    // *out << insDis << std::endl;
}

void PIMProf::PrintInfo(std::ostream *out, std::string info) {
    *out << info << std::endl;
}