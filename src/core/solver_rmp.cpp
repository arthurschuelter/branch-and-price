#include "solver_rmp.hpp"
#include "branch_and_price.hpp"
#include <vector>


RMPSolver::RMPSolver(const Instance& inst) : inst_(inst) { reset(); }

void RMPSolver::reset() {
    this->vars_.clear();
    this->coverage_.clear();
    this->days_.clear();
    
    this->model_ = std::make_unique<GRBModel>(env_);
    this->model_->set(GRB_IntAttr_ModelSense, GRB_MINIMIZE);
    
    this->coverage_.resize(inst_.vertexCount());
    this->days_.resize(inst_.vertexCount());

    for (int v = 0; v < this->inst_.vertexCount(); ++v) {
        if (v == this->inst_.depot) { continue; }

        this->coverage_[v] = model_->addConstr(GRBLinExpr(0) == 1);
    }


    for (int d = 1; d < this->inst_.vertexCount(); ++d) {
        this->days_[d] = this->model_->addConstr(GRBLinExpr(0) <= 1);
    }

    this->model_->update();
}

void RMPSolver::addColumn(const Column& column) {
    GRBColumn coefficients;

    for (int v = 0; v < this->inst_.vertexCount(); ++v) {
        if (v == this->inst_.depot) { continue; }

        if (column.has[v]) {
            coefficients.addTerm(1.0, this->coverage_[v]);
        }
    }

    coefficients.addTerm(1.0, this->days_[column.day]);

    this->vars_.push_back(
        this->model_->addVar(
            0.0, GRB_INFINITY, column.cost, GRB_CONTINUOUS, coefficients
        )
    );
}

// Quem chama deve verificar se:
//   - GRB_OPTIMAL: LP ótimo, pode consultar os métodos do passo 6.
//   - GRB_INFEASIBLE: PMR inviável.
//   - GRB_TIME_LIMIT: parou por tempo, não prova inviabilidade nem otimalidade.
int RMPSolver::solveLP(double remainingSeconds) { 
    if (remainingSeconds <= 0) { return GRB_TIME_LIMIT; }
    
    this->model_->set(GRB_DoubleParam_TimeLimit, remainingSeconds);
    this->model_->optimize();

    return this->model_->get(GRB_IntAttr_Status); 
}

double RMPSolver::objective() const { return this->model_->get(GRB_DoubleAttr_ObjVal); }

std::vector<double> RMPSolver::values() const {
    std::vector<double> values = std::vector<double>();

    for (const auto& var : this->vars_) {
        values.push_back(var.get(GRB_DoubleAttr_X));
    }
    
    return values;
}

std::vector<double> RMPSolver::customerDuals() const {
    std::vector<double> duals = std::vector<double>(this->inst_.vertexCount(), 0);

    for (int v = 0; v < this->inst_.vertexCount(); ++v) {
        if (v == this->inst_.depot) { continue; }
        duals[v] = this->coverage_[v].get(GRB_DoubleAttr_Pi);
    }
    return duals;
}

std::vector<double> RMPSolver::dayDuals() const {
    std::vector<double> duals = std::vector<double>(this->inst_.vertexCount(), 0);
    
    for (int d = 1; d < this->inst_.vertexCount(); ++d) {
        duals[d] = days_[d].get(GRB_DoubleAttr_Pi);
    }
    
    return duals;
}