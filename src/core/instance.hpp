#pragma once

#include <string>
#include <utility>
#include <vector>
#include <fstream>
#include <stdexcept>
#include <string>
#include <iostream>
#include <sstream>

struct Vertex {
    int x = 0;
    int y = 0;
    int weight = 0;
    int priority = 0;
};

class Instance {
public:
    static Instance read(const std::string& path, int depot, int capacity);
    int vertexCount() const { return static_cast<int>(vertices.size()); }
    int conflictsCount() const { return static_cast<int>(conflicts.size()); }
    bool areInConflict(int i, int j) const;
    void printInstance();

    int depot = 0;
    int capacity = 0;
    std::vector<Vertex> vertices;
    std::vector<std::pair<int, int>> conflicts;
};