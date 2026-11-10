#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <limits>
#include "core/branch_and_price.hpp"
#include "core/instance.hpp"

struct Params {
    std::string input_file;     // -f
    int depot = 0;              // -s
    int capacity = 0;           // -C
    double time_limit = 0 ;     // -t
    double heuristic_time = 0;  // -h
    std::string output_file;    // -o
    SearchStrategy search = SearchStrategy::DFS;
};

Params parseArgs(int argc, char** argv);

int main(int argc, char** argv) {
    Params p = parseArgs(argc, argv);
    Instance instance;

    try {
        instance = Instance::read(p.input_file, p.depot, p.capacity);
    }
    catch (std::runtime_error){
        throw std::runtime_error("Falha ao ler arquivo");
    }

    // std::cout << "Instância lida: " << instance.vertexCount() << " vértices, "
    //           << instance.conflictsCount() << " conflitos.\n";
    // instance.printInstance();

    // const RouteEvaluation invalid_route = instance.evaluateRoute({1, 2}, 1);
    // std::cout << "Rota (1, 2), dia 1: ";
    // invalid_route.printResult();
    
    Config cfg = {
        p.time_limit,
        p.heuristic_time,
        p.search,
        true,
        true,
        0
    };
    
    // TODO: flags extras para as demais ablações.
    BranchAndPrice bp(instance, cfg);
    Solution sol = bp.solve();

    std::sort(sol.columns.begin(), sol.columns.end(),
              [](const Column& a, const Column& b) { return a.day < b.day; });
    std::vector<int> coverage(instance.vertexCount(), 0);
    int previousDay = 0;
    double compactCost = 0.0;
    for (std::size_t k = 0; k < sol.columns.size(); ++k) {
        auto& column = sol.columns[k];
        if (column.day <= previousDay) throw std::runtime_error("Dias repetidos na solução");
        previousDay = column.day;
        column.day = static_cast<int>(k) + 1;
        const auto evaluation = instance.evaluateRoute(column.route, column.day);
        if (!evaluation.valid) throw std::runtime_error("Rota de saída inválida");
        column.cost = evaluation.total_cost;
        compactCost += column.cost;
        for (int customer : column.route) ++coverage[customer];
    }
    for (int i = 0; i < instance.vertexCount(); ++i) {
        if (coverage[i] != (i == instance.depot ? 0 : 1)) {
            throw std::runtime_error("Cobertura inválida na solução de saída");
        }
    }
    sol.cost = compactCost;
    if (!std::isfinite(bp.lowerBound()) || bp.lowerBound() > sol.cost) {
        throw std::runtime_error("Limite inferior incompatível com a solução");
    }
    std::ofstream output(p.output_file);
    if (!output) throw std::runtime_error("Não foi possível abrir o arquivo de saída");
    output << std::setprecision(std::numeric_limits<double>::max_digits10)
           << sol.cost << '\n' << bp.lowerBound() << '\n' << sol.columns.size() << '\n';
    for (const auto& column : sol.columns) {
        for (std::size_t k = 0; k < column.route.size(); ++k) {
            if (k) output << ' ';
            output << column.route[k];
        }
        output << '\n';
    }
    output.close();
    if (!output) throw std::runtime_error("Erro ao gravar arquivo de saída");

    std::cout << "Custo: " << sol.cost << '\n';

    for (const auto& column : sol.columns) {
        std::cout << "Dia " << column.day << ":";
        for (int customer : column.route) {
            std::cout << ' ' << customer;
        }
        std::cout << " | custo: " << column.cost << '\n';
    }
    // Solution sol = bp.solve();
    
    // TODO: stats em stderr / CSV para o script de experimentos
    std::cout << "B&P: OK\n";
    return 0;
}

Params parseArgs(int argc, char** argv) {
    Params p;
    for (int i = 1; i < argc; i++) {
        std::string cur = argv[i];

        auto next = [&]() -> std::string {
            if (i + 1 >= argc) { std::cerr << "Argumento ausente para " << cur << "\n"; std::exit(1); }
            return argv[++i];
        };

        if      (cur == "-f") p.input_file = next();
        else if (cur == "-s") p.depot = std::stoi(next());
        else if (cur == "-C") p.capacity = std::stoi(next());
        else if (cur == "-t") p.time_limit = std::stod(next());
        else if (cur == "-h") p.heuristic_time = std::stod(next());
        else if (cur == "-o") p.output_file = next();
        else if (cur == "--dfs") p.search = SearchStrategy::DFS;
        else if (cur == "--bestbound") p.search = SearchStrategy::BEST_BOUND;
        else { std::cerr << "Parametro desconhecida: " << cur << "\n"; std::exit(1); }
    }

    if (p.input_file.empty() || p.output_file.empty()) {
        std::cerr << "Uso: " << argv[0]
                  << " -f <inst> -s <deposito> -C <cap> -t <seg> [-h <seg>] -o <saida>\n";
        std::exit(1);
    }
    if (!std::isfinite(p.time_limit) || p.time_limit < 0 ||
        !std::isfinite(p.heuristic_time) || p.heuristic_time < 0 ||
        !std::isfinite(p.time_limit + p.heuristic_time)) {
        throw std::invalid_argument("Os tempos devem ser finitos e não negativos");
    }

    return p;
}
