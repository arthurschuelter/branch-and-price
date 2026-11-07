#pragma once
#include <vector>
#include <deque>
#include <cmath>
#include <limits>
#include <memory>
#include <chrono>
#include <string>

#include "instance.hpp"
#include "solver_rmp.hpp"

struct Column {
    std::vector<int> route;   // ordem de visita (sem o depósito) -> útil p/ saída
    std::vector<bool> has;    // has[i] = true se cliente i está na rota (consulta O(1))
    int day;                  // dia d
    double cost;              // c_{R,d} = rho(R) + pi(R,d)
    // TODO: considere guardar um hash/assinatura para detectar colunas duplicadas
};

// Decisão de ramificação de Ryan-Foster
struct BranchDecision {
    int i, j;       // par de clientes (i < j)
    bool same;      // true = SAME (mesma rota), false = DIFFERENT
};

// Nó da árvore
struct Node {
    int id;
    int depth;
    double lb;                              // limitante inferior do nó (valor do PMR)
    std::vector<BranchDecision> decisions;  // caminho da raiz até aqui (todas as decisões)
    // TODO: guardar a solução fracionária? duais para warm start? (decida o que precisa)
};

struct Solution {
    double cost = 1e18;                 // Upper Bound
    std::vector<Column> columns;        // rotas escolhidas
};

struct Stats {
    long nodes = 0;
    long columns_generated = 0;
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
class Instance;     // instance.hpp
class Pricer;       // pricing.hpp     (heurístico + exato)

class BranchAndPrice {
public:
    BranchAndPrice(const Instance& inst, const Config& cfg);
    ~BranchAndPrice();
    Solution solve();                       // laço principal da árvore

    double lowerBound() const { return global_lb_; }
    const Stats& stats() const { return stats_; }
    const std::vector<Column>& pool() const { return pool_; }
    
    // --- bebug ---
    void printPool();
    void printOpen();

private:
    // --- árvore ---
    void initRoot();                                  // cria nó raiz + colunas iniciais
    std::unique_ptr<Node> selectNode();               // DFS ou best-bound
    void pushNode(std::unique_ptr<Node> n);
    void updateGlobalLB();                            // LB global = min lb dos nós abertos

    // --- processamento de um nó ---
    // Retorna false se nó inviável / podado
    bool processNode(Node& node);
    bool columnGeneration(Node& node);                // loop: RMP -> pricing -> add cols
    bool columnAllowed(const Column& c, const Node& node) const;  // respeita SAME/DIFF?

    // --- ramificação ---
    bool isIntegral(double eps = 1e-6) const;         // x_{R,d} todo inteiro?
    bool selectBranchingPair(int& i, int& j) const;   // f_ij mais perto de 0.5 (EXP3)
    void branch(const Node& node, int i, int j);      // cria filhos SAME e DIFFERENT

    // --- heurística (-h) ---
    void roundingHeuristic();                         // MIP sobre o pool de colunas

    // --- utilidades ---
    bool timeIsUp() const;
    void updateIncumbent(const Solution& s);
    void logNode(const Node& node) const;             // stderr: ajuda no exemplo à mão


    const Instance& inst_;
    Config cfg_;
    Stats stats_;

    std::vector<Column> pool_;          // pool GLOBAL de colunas (filtra-se por nó)
    std::deque<std::unique_ptr<Node>> open_;   // nós abertos (TODO: trocar por priority_queue p/ best-bound)

    Solution incumbent_;                // Upper Bound
    double global_lb_ = 0;

    std::shared_ptr<RMPSolver> rmp_;
    std::shared_ptr<Pricer> pricer_;
    std::chrono::steady_clock::time_point t0_;
};
