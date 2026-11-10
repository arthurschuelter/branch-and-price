#pragma once
#include <vector>
#include <deque>
#include <optional>
#include <vector>
#include <memory>
#include <chrono>

#include <iterator>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <cmath>
#include <limits>
#include <string>

#include "instance.hpp"
#include "solver_rmp.hpp"

struct Column {
    std::vector<int> route;
    std::vector<bool> has;
    int day;
    double cost;
    // TODO: considere guardar um hash/assinatura para detectar colunas duplicadas
};

enum class PricingStatus {
    Completed,
    Interrupted,
    Inconclusive,
    Error
};

struct PricingResult {
    PricingStatus status;
    std::vector<Column> columns;
    // Índice zero não representa um dia; nullopt indica limite indisponível.
    std::vector<std::optional<double>> dayLowerBounds{};
};

struct RoutingCertificate {
    double integerLowerBound;
    bool lpConverged;
    std::int64_t scale;
};

// Decisão de ramificação de Ryan-Foster
struct BranchDecision {
    int i, j;
    bool same;
};

// Nó da árvore
struct Node {
    int id;
    int depth;
    double lb;
    std::vector<BranchDecision> decisions;
    // TODO: guardar a solução fracionária? duais para warm start? (decida o que precisa)
};

enum class NodeResult {
    TimeLimit,
    RestrictedMasterInfeasible,
    PricingInterrupted,
    PricingInconclusive,
    NoCertification,
    DuplicateColumns,
    Infeasible,
    Converged,
    Error
};

struct Solution {
    double cost = 1e18;
    std::vector<Column> columns;
};

struct Stats {
    long nodes = 0;
    long columns_generated = 0;
    long heuristic_pricing_calls = 0;
    long exact_pricing_calls = 0;
    double time_pricing = 0;
    double time_total = 0;
    double root_lb = 0;
    // TODO: o que mais o rodar_experimentos.py precisa?
};

enum class SearchStrategy { DFS, BEST_BOUND };

struct Config {
    double time_limit;              // -t
    double heuristic_time;          // -h
    SearchStrategy search = SearchStrategy::DFS;
    bool use_heuristic_pricing = true;   // EXP1
    bool use_preprocessing = true;       // EXP2
    int branching_rule = 0;              // EXP3
    // TODO: epsilon, big-M, etc.
};

// ---------------------------------------------------------------
// Classe principal
// ---------------------------------------------------------------
class Instance;
class Pricer;

class BranchAndPrice {
public:
    BranchAndPrice(const Instance& inst, const Config& cfg);
    ~BranchAndPrice();
    Solution solve(std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now());

    double lowerBound() const { return global_lb_; }
    const Stats& stats() const { return stats_; }
    const std::vector<Column>& pool() const { return pool_; }
    
    // --- bebug ---
    void printPool();
    void printOpen();

private:
    // --- árvore ---
    std::optional<Solution> validateInitialRmp(const RMPSolver& rmp, const std::vector<std::size_t>& columnIndices) const;
    void initRoot();                                  
    std::unique_ptr<Node> selectNode();               // DFS ou best-bound
    void pushNode(std::unique_ptr<Node> n);
    void updateGlobalLB();                            // LB global = min lb dos nós abertos

    // --- processamento de um nó ---
    NodeResult processNode(Node& node, std::vector<std::size_t>& columnIndices);
    NodeResult columnGeneration(Node& node, std::vector<std::size_t>& columnIndices);
    bool columnAllowed(const Column& c, const Node& node) const;  // respeita SAME/DIFF?
    std::optional<std::size_t> findPoolColumn(const Column& column) const;
    std::optional<double> diagnosticLowerBound(
        const PricingResult& result,
        const std::vector<double>& customerDuals,
        const std::vector<double>& dayDuals
    ) const;
    std::optional<RoutingCertificate> routingCertificate(
        const Node& node, const std::vector<double>& customerDuals,
        const std::vector<std::size_t>& columnIndices, bool analytical = false,
        std::int64_t scale = 1024, std::int64_t primalScale = 0
    ) const;
    std::optional<std::int64_t> reconstructScale(const std::vector<double>& values,
                                                 double tolerance) const;

    // --- ramificação ---
    bool isIntegral() const;                          // primal racional certificado inteiro?
    bool selectBranchingPair(const std::vector<std::size_t>& columnIndices, int& i, int& j) const;
    std::optional<Solution> assignBlocks(const Node& node, const std::vector<std::size_t>& columnIndices) const;
    void branch(const Node& node, int i, int j);      // cria filhos SAME e DIFFERENT
    

    // --- heurística (-h) ---
    void roundingHeuristic();                         // MIP sobre o pool de colunas

    // --- utilidades ---
    bool timeIsUp() const;
    Solution buildInitialSolution() const;
    double initialRoutingLowerBound() const;
    double reducedCost(
        const Column& column,
        const std::vector<double>& customerDuals,
        const std::vector<double>& dayDuals
    ) const;
    void updateIncumbent(const Solution& s);
    void logNode(const Node& node) const;

    const Instance& inst_;
    Config cfg_;
    Stats stats_;

    std::vector<Column> pool_;
    std::deque<std::unique_ptr<Node>> open_;
    int next_node_id_ = 1;

    Solution incumbent_;
    double global_lb_ = 0;
    std::int64_t certified_scale_ = 1024;

    std::shared_ptr<RMPSolver> rmp_;
    std::shared_ptr<Pricer> pricer_;
    std::chrono::steady_clock::time_point t0_;


};
