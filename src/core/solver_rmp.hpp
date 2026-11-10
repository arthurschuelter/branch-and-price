#pragma once
#include <memory>
#include <functional>

#include "gurobi_c++.h"
#include "instance.hpp"

struct Column;
struct RmpTimeLimit {};

// PMR: modelo que escolhe entre as rotas já disponíveis.
// Comece pelo passo 1. Continue no solver_rmp.cpp.
class RMPSolver {
public:
    explicit RMPSolver(const Instance& inst);

    bool reset(const std::function<bool()>& expired);
    void discard() noexcept;
    void addColumn(const Column& column);
    int solveLP(double remainingSeconds);
    
    double objective() const;
    std::vector<double> values(const std::function<bool()>& expired = {}) const;
    std::vector<double> customerDuals(const std::function<bool()>& expired = {}) const;
    std::vector<double> dayDuals(const std::function<bool()>& expired = {}) const;

private:
    const Instance& inst_;
    GRBEnv env_;
    std::unique_ptr<GRBModel> model_;
    std::vector<GRBVar> vars_;
    std::vector<GRBConstr> coverage_;
    std::vector<GRBConstr> days_;
};
