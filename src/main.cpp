#include <cstdlib>
#include <iostream>
#include "core/branch_and_price.hpp"
#include "core/instance.hpp"

struct Params {
    std::string input_file;     // -f
    int depot = 0;              // -s
    int capacity = 0;           // -C
    double time_limit = 0 ;     // -t
    double heuristic_time = 0;       // -h
    std::string output_file;    // -o
};

Params parseArgs(int argc, char** argv);

int main(int argc, char** argv) {
    Params p = parseArgs(argc, argv);

    // TODO: implementar o parser de Instance a partir de p.input_file,
    // validando vertices, pesos, coeficientes de prioridade e pares de conflito.
    Instance instance = Instance::read(p.input_file, p.depot, p.capacity);
    
    // TODO: flags extras p/ ablações (--no-heur-pricing, --dfs, --bestbound, ...)
    // TODO: Instance inst = Instance::read(p.input_file, p.depot, p.capacity);
    // TODO: BranchAndPrice bp(inst, cfg);
    // TODO: Solution sol = bp.solve();

    // TODO: escrever sol + bp.lowerBound() em 'out' no formato do enunciado
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
        else { std::cerr << "Parametro desconhecida: " << cur << "\n"; std::exit(1); }
    }

    if (p.input_file.empty() || p.output_file.empty()) {
        std::cerr << "Uso: " << argv[0]
                  << " -f <inst> -s <deposito> -C <cap> -t <seg> [-h <seg>] -o <saida>\n";
        std::exit(1);
    }

    return p;
}
