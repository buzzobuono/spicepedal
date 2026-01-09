#ifndef PARAM_REGISTRY_H
#define PARAM_REGISTRY_H

#include <map>
#include <string>

class ParameterRegistry {
private:
    std::map<std::string, double> values;
public:
    double* getPtr(const std::string& name) {
        return &values[name];
    }

    void set(const std::string& name, double val) {
        values[name] = val;
    }

    double get(const std::string& name) const {
        auto it = values.find(name);
        return it != values.end() ? it->second : 0.0;
    }
    
    // Metodo per iterare sulla mappa interna
    const std::map<std::string, double>& getAll() const { 
        return values; 
    }
};

#endif
