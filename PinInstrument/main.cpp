//===- main.cpp - The main entrance of PinInstrument tool ---------------------------*- C++ -*-===//
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

#include <PinUtil.h>
#include <CostSolver.h>

using namespace PIMProf;

/* ===================================================================== */
/* main */
/* ===================================================================== */

CostPackage _cost_package;
CostSolver _cost_solver;

int main(int argc, char *argv[])
{
    _cost_solver.initialize(&_cost_package, _cost_package._config_reader);

    std::ofstream ofs("decision.out");
    CostSolver::DECISION decision = _cost_solver.PrintSolution(ofs);
    ofs.close();

    // ofs.open("BBLReuseCost.dot", std::ofstream::out);
    // pkg._data_reuse.print(
    //     ofs,
    //     pkg._data_reuse.getRoot());
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