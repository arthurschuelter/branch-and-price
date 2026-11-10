#pragma once
#include <memory>

#include "gurobi_c++.h"
#include "instance.hpp"

struct Column;

// PMR: modelo que escolhe entre as rotas já disponíveis.
// Comece pelo passo 1. Continue no solver_rmp.cpp.
class RMPSolver {
public:
    explicit RMPSolver(const Instance& inst);

    void reset();
    void discard() noexcept;
    void addColumn(const Column& column);
    int solveLP(double remainingSeconds);
    
    double objective() const;
    std::vector<double> values() const;
    std::vector<double> customerDuals() const;
    std::vector<double> dayDuals() const;

private:
    const Instance& inst_;
    GRBEnv env_;
    std::unique_ptr<GRBModel> model_;
    std::vector<GRBVar> vars_;
    std::vector<GRBConstr> coverage_;
    std::vector<GRBConstr> days_;
};
