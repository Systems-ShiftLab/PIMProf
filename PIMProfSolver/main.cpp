//===- main.cpp - The main entrance of PIMProfSolver tool ---------------------------*- C++ -*-===//
//
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//
#include <vector>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <unistd.h>

#include <Util.h>
#include <CostSolver.h>

using namespace PIMProf;

/* ===================================================================== */
/* main */
/* ===================================================================== */

CostSolver _cost_solver;
CommandLineParser _command_line_parser;

int main(int argc, char *argv[])
{
    _command_line_parser.initialize(argc, argv);

    _cost_solver.initialize(&_command_line_parser);
    std::ofstream ofs(_command_line_parser.outputfile());
    _cost_solver.PrintSolution(ofs);

    // std::ofstream ofs("decision.out");
    // DECISION decision = _cost_solver.PrintSolution(ofs);
    // ofs.close();

    // ofs.open("BBLReuseCost.dot", std::ofstream::out);
    // pkg._bbl_data_reuse.print(
    //     ofs,
    //     pkg._bbl_data_reuse.getRoot());
    // ofs.close();

    // ofs.open("bblcdf.out", std::ofstream::out);
    // // TODO: Need bug fix, cause Pin out of memory error
    // self->_cost_solver.PrintAnalytics(ofs);
    // self->_instruction_latency.WriteConfig("testconfig.ini");
    // ofs.close();

    // ofs.open(pkg._command_line_parser.statsfile().c_str(), std::ofstream::out);
    // self->_storage.WriteStats(ofs);
    // ofs << "Number of times entering ROI: " << pkg._enter_roi_cnt << std::endl;
    // ofs << "Number of times exiting ROI: " << pkg._exit_roi_cnt << std::endl;
    // self->_cost_solver.PrintAnalytics(ofs);
    // ofs.close();
    return 0;
}