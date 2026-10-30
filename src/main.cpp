#include <cstdlib>
#include <iostream>
#include <unistd.h>
#include "core/branch_and_price.hpp"
// #include "instance.hpp"

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
    std::cout << "Parsing: OK\n";


    Config cfg{};
    std::string file = p.input_file;
    std::string out = p.output_file;
    int depot = p.depot;
    int capacity = p.capacity;
    cfg.time_limit = p.time_limit;
    cfg.heuristic_time = p.heuristic_time;

    // TODO: parsing: -f -s -C -t -h -o   (cuidado: -h conflita com ajuda no getopt)
    // TODO: flags extras p/ ablações (--no-heur-pricing, --dfs, --bestbound, ...)

    // TODO: Instance inst = Instance::read(file, depot, capacity);

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

        if (cur == "-f" || cur == "-s" || cur == "-C" || cur == "-t" ||
            cur == "-h" || cur == "-o") {
            if (i + 1 >= argc) {
                std::cerr << "Argumento ausente para " << cur << "\n";
                std::exit(1);
            }

            const std::string param_value = argv[++i];
            if      (cur == "-f") p.input_file = param_value;
            else if (cur == "-s") p.depot = std::stoi(param_value);
            else if (cur == "-C") p.capacity = std::stoi(param_value);
            else if (cur == "-t") p.time_limit = std::stod(param_value);
            else if (cur == "-h") p.heuristic_time = std::stod(param_value);
            else if (cur == "-o") p.output_file = param_value;
        } else {
            std::cerr << "Opção desconhecida: " << cur << "\n";
            std::exit(1);
        }
    }

    if (p.input_file.empty() || p.output_file.empty()) {
        std::cerr << "Uso: " << argv[0]
                  << " -f <inst> -s <deposito> -C <cap> -t <seg> [-h <seg>] -o <saida>\n";
        std::exit(1);
    }

    return p;
}
