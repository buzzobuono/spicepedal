#ifndef CIRCUIT_H
#define CIRCUIT_H

#include <vector>
#include <memory>
#include <string>
#include <map>
#include <fstream>
#include <sstream>
#include <cctype>
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <locale>

#include "utils/param_registry.h"
#include "components/component.h"
#include "components/voltage.h"
#include "components/resistor.h"
#include "components/capacitor.h"
#include "components/diode.h"
#include "components/bjt.h"
#include "components/mosfet.h"
#include "components/opamp.h"
#include "components/inductor.h"
#include "components/potentiometer.h"
#include "components/wire.h"
#include "components/vcvs.h"
#include "components/voltage_behavioral_source.h"
#include "components/subckt/pitch_tracker.h"
#include "components/subckt/fft_pitch_tracker.h"
#include "components/subckt/integrator.h"

#include <Eigen/Dense>
#include <Eigen/LU>

struct ProbeTarget {
    enum class Type { VOLTAGE, CURRENT };
    Type type;
    std::string name;
};

class Circuit {
    
public:
    ParameterRegistry params;
    std::vector<std::unique_ptr<Component>> components;
    int num_nodes;
    int input_node;
    int output_node;
    double warmup_duration = 0;
    std::map<std::string, double> initial_conditions;
    std::vector<std::pair<int, std::string>> pending_params;
    std::map<int, Potentiometer*> param_map;
    std::vector<ProbeTarget> probes;
    std::string probe_file;

    Circuit() : num_nodes(0), output_node(-1) {}
    
    bool loadNetlist(const std::string& filename) {
        std::locale::global(std::locale::classic());
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Cannot open netlist: " << filename << std::endl;
            return false;
        }
        
        std::string line;
        int max_node = 0;
        
        std::cout << "Circuit Creation"<< std::endl;
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '*' || line[0] == '#') continue;
            
            std::istringstream iss(line);
            std::string comp_name;
            iss >> comp_name;
            
            if (comp_name.empty()) continue;
            
            char type = std::toupper(comp_name[0]);
            
            switch(type) {
                case 'R': {
                    int n1, n2;
                    double v;
                    std::string unit;
                    iss >> n1 >> n2 >> v >> unit;
                    v *= parseUnit(unit);
                    components.push_back(std::make_unique<Resistor>(comp_name, n1, n2, v));
                    std::cout << "   Component Resistor name=" << comp_name << " n1=" << n1 << " n2=" << n2 <<" v=" << v << std::endl;
                    max_node = std::max(max_node, std::max(n1, n2));
                    break;
                }
                case 'C': {
                    int n1, n2;
                    double v;
                    std::string unit;
                    iss >> n1 >> n2 >> v >> unit;
                    v *= parseUnit(unit);
                    components.push_back(std::make_unique<Capacitor>(comp_name, n1, n2, v));
                    std::cout << "   Component Capacitor name=" << comp_name << " n1=" << n1 << " n2=" << n2 <<" v=" << v << std::endl;
                    max_node = std::max(max_node, std::max(n1, n2));
                    break;
                }
                case 'L': {
                    int n1, n2;
                    double l;
                    std::string unit;
                    iss >> n1 >> n2 >> l >> unit;
                    l *= parseUnit(unit);
                    components.push_back(std::make_unique<Inductor>(comp_name, n1, n2, l, 100));
                    std::cout << "   Component Inductor name=" << comp_name << " n1=" << n1 << " n2=" << n2 <<" l=" << l << std::endl;
                    max_node = std::max(max_node, std::max(n1, n2));
                    break;
                }
                case 'D': {
                    int n1, n2;
                    std::string model;
                    iss >> n1 >> n2 >> model;
                    double Is, n, Vt;
                    double Cj0;   // Capacità (0 = disabilita)
                    double Vj;
                    double Mj;
                    std::string token;
                    while (iss >> token) {
                        if (token.find("Is=") == 0) Is = std::stod(token.substr(3));
                        else if (token.find("N=") == 0) n = std::stod(token.substr(2));
                        else if (token.find("Vt=") == 0) Vt = std::stod(token.substr(3));
                        else if (token.find("Cj0=") == 0) Cj0 = std::stod(token.substr(4));
                        else if (token.find("Vj=") == 0) Vj = std::stod(token.substr(3));
                        else if (token.find("Mj=") == 0) Mj = std::stod(token.substr(3));
                    }
                    std::cout << "   Component Diode name=" << comp_name << " model=" << model << " n1=" << n1 << " n2=" << n2 <<" Is=" << Is << " N=" << n << " Vt=" << Vt << " Cj0=" << Cj0 << " Vj=" << Vj << " Mj=" << Mj << std::endl;
                    components.push_back(std::make_unique<Diode>(comp_name, n1, n2, Is, n, Vt, Cj0, Vj, Mj));
                    max_node = std::max(max_node, std::max(n1, n2));
                    break;
                }
                case 'Q': {
                    int nc, nb, ne;
                    std::string model;
                    iss >> nc >> nb >> ne >> model;
                    double Is, Bf, Br, Vt;
                    std::string token;
                    while (iss >> token) {
                        if (token.find("Is=") == 0) {
                            Is = std::stod(token.substr(3));
                        } else if (token.find("Bf=") == 0) {
                            Bf = std::stod(token.substr(3));
                        } else if (token.find("Br=") == 0) {
                            Br = std::stod(token.substr(3));
                        } else if (token.find("Vt=") == 0) {
                            Vt = std::stod(token.substr(3));
                        }
                    }
                    std::cout << "   Component Transistor name=" << comp_name << " model=" << model << " nc=" << nc << " nb=" << nb << " ne=" << ne <<" Is=" << Is << " Bf=" << Bf << " Br=" << Br << " Vt=" << Vt << std::endl;
                    components.push_back(std::make_unique<BJT>(
                        comp_name,           // name
                        nc,                  // collector node
                        nb,                  // base node  
                        ne,                  // emitter node
                        Bf,                  // beta (forward beta)
                        Br,                  // beta (reverse beta)
                        Is,                  // saturation current
                        Vt                   // Thermal voltage
                    ));
                    max_node = std::max({max_node, nc, nb, ne});
                    break;
                }
                case 'V': {
                    int n1, n2;
                    std::string model;
                    double value;
                    double rs = 1;
                    iss >> n1 >> n2 >> model >> value;
                    std::string token;
                    while (iss >> token) {
                        if (token.find("Rs=") == 0) {
                            rs = std::stod(token.substr(3));
                        }
                    }
                    auto vs = std::make_unique<VoltageSource>(comp_name, n1, n2, value, rs);
                    std::cout << "   Component VoltageSource name=" << comp_name << " n1=" << n1 << " n2=" << n2 <<" v=" << value <<" Rs=" << rs << std::endl;
                    components.push_back(std::move(vs));
                    max_node = std::max(max_node, std::max(n1, n2));
                    break;
                }
                case 'W': {
                    int n1, n2;
                    iss >> n1 >> n2;
                    auto wire = std::make_unique<Wire>(comp_name, n1, n2);
                    std::cout << "   Component Wire name=" << comp_name << " n1=" << n1 << " n2=" << n2 << std::endl;
                    components.push_back(std::move(wire));
                    max_node = std::max(max_node, std::max(n1, n2));
                    break;
                }
                case 'P': {
                    int n1, n2, nw;
                    double r_total, pos;
                    std::string taper_str, unit;
                    // P1 1 2 3 10k 0.5 LOG
                    iss >> n1 >> n2 >> nw >> r_total >> unit >> pos >> taper_str;
                    r_total *= parseUnit(unit);

                    Potentiometer::TaperType taper = Potentiometer::TaperType::LINEAR;
                    std::transform(taper_str.begin(), taper_str.end(), taper_str.begin(), ::toupper);
                    if (taper_str == "LOG" || taper_str == "A")
                        taper = Potentiometer::TaperType::LOGARITHMIC;
                    else if (taper_str == "LIN" || taper_str == "B")
                        taper = Potentiometer::TaperType::LINEAR;
                    else
                        std::cerr << "   Warning: Potentiometer taper '" << taper_str << "' not recognized, using LINEAR" << std::endl;
                    components.push_back(std::make_unique<Potentiometer>(comp_name, n1, n2, nw, r_total, pos, taper));
                    std::cout << "   Component Potentiometer name=" << comp_name << " n1=" << n1 << " n2=" << n2 << " nw=" << nw << " Rtot=" << r_total << " pos=" << pos << " taper=" << taper_str << std::endl;
                    max_node = std::max({max_node, n1, n2, nw});
                    break;
                }
                case 'O': {
                    int n_out, n_plus, n_minus, n_vcc, n_vee;
                    double r_out, i_max, gain, sr;
                    std::string model;
                    iss >> n_out >> n_plus >> n_minus >> n_vcc >> n_vee >> model;
                    std::string token;
                    while (iss >> token) {
                        if (token.find("Rout=") == 0) {
                            r_out = std::stod(token.substr(5));
                        } else if (token.find("Imax=") == 0) {
                            i_max = std::stod(token.substr(5));
                        } else if (token.find("Gain=") == 0) {
                            gain = std::stod(token.substr(5));
                        } else if (token.find("Sr=") == 0) {
                            sr = std::stod(token.substr(3)) * 1e6;  // Assume input in V/µs
                        }
                    }
                    
                    components.push_back(std::make_unique<OpAmp>(
                       comp_name, n_out, n_plus, n_minus, n_vcc, n_vee,
                       r_out, i_max, gain, sr
                    ));
    
                    std::cout << "   Component OpAmp name=" << comp_name
                    << " model=" << model
                    << " out=" << n_out
                    << " in+=" << n_plus
                    << " in-=" << n_minus
                    << " vcc=" << n_vcc
                    << " vee=" << n_vee
                    << " Rout=" << r_out
                    << " Imax=" << i_max
                    << " Gain=" << gain
                    << " Sr=" << sr
                    << std::endl;
                    
                    max_node = std::max({max_node, n_out, n_plus, n_minus, n_vcc, n_vee});
                    break;
                }
                case 'E': {
                    int n_out_p;
                    int n_out_m;
                    int n_ctrl_p;
                    int n_ctrl_m;  
                    double gain;
                    double v_max;
                    double v_min;
                    double r_out;
                    iss >> n_out_p >> n_out_m >> n_ctrl_p >> n_ctrl_m;
                    std::string token;
                    while (iss >> token) {
                        if (token.find("Rout=") == 0) {
                            r_out = std::stod(token.substr(5));
                        } else if (token.find("Vmax=") == 0) {
                            v_max = std::stod(token.substr(5));
                        } else if (token.find("Vmin=") == 0) {
                            v_min = std::stod(token.substr(5));
                        } else if (token.find("Gain=") == 0) {
                            gain = std::stod(token.substr(5));
                        }
                    }
                    
                    components.push_back(std::make_unique<VCVS>(
                       comp_name, n_out_p, n_out_m, n_ctrl_p, n_ctrl_m,
                       gain, v_max, v_min, r_out
                    ));
    
                    std::cout << "   Component VCVS name=" << comp_name
                    << " n_out_p=" << n_out_p
                    << " n_out_m=" << n_out_m
                    << " n_ctrl_p=" << n_ctrl_p
                    << " n_ctrl_m=" << n_ctrl_m
                    << " Gain=" << gain
                    << " Vmax=" << v_max
                    << " Vmin=" << v_min
                    << " Rout=" << r_out
                    << std::endl;
                    
                    max_node = std::max({max_node, n_out_p, n_out_m, n_ctrl_p, n_ctrl_m});
                    break;
                }
                case 'B': {
                    int n1, n2;
                    std::string expr_part;
                    double rs = 1e-3; // Default robusto
                    
                    // Leggiamo i nodi di uscita
                    iss >> n1 >> n2;
                    
                    // Leggiamo il resto della riga per estrarre l'espressione e Rs
                    std::string remainder;
                    std::getline(iss, remainder);
                    
                    // Cerchiamo se l'utente ha specificato Rs=... alla fine della riga
                    size_t rs_pos = remainder.find("Rs=");
                    std::string expression;
                    
                    if (rs_pos != std::string::npos) {
                        expression = remainder.substr(0, rs_pos);
                        std::string rs_val = remainder.substr(rs_pos + 3);
                        // Rimuoviamo eventuali spazi bianchi
                        rs_val.erase(std::remove_if(rs_val.begin(), rs_val.end(), ::isspace), rs_val.end());
                        if (!rs_val.empty()) rs = std::stod(rs_val);
                    } else {
                        expression = remainder;
                    }
                    
                    // Pulizia dell'espressione (rimuove il prefisso "V=" se presente stile LTspice)
                    size_t v_eq_pos = expression.find("V=");
                    if (v_eq_pos != std::string::npos) {
                        expression = expression.substr(v_eq_pos + 2);
                    }
                    
                    // Rimuoviamo spazi iniziali/finali rimasti
                    expression.erase(0, expression.find_first_not_of(" \t"));
                    expression.erase(expression.find_last_not_of(" \t") + 1);

                    auto b_source = std::make_unique<VoltageBehavioralSource>(comp_name, n1, n2, expression, rs);
                    
                    std::cout << "   Component VoltageBehavioralSource name=" << comp_name 
                              << " n1=" << n1 << " n2=" << n2 
                              << " expr=\"" << expression << "\"" 
                              << " Rs=" << rs << std::endl;
                    
                    b_source->setParams(&(this->params));
                    components.push_back(std::move(b_source));
                    max_node = std::max(max_node, std::max(n1, n2));
                    break;
                }
                case 'X': {
                    int n1, n2;
                    std::string subckt;
                    iss >> n1 >> n2 >> subckt;
                    if (subckt == "PITCH") {
                        double thr = 0.02;
                        double smooth = 0.2;
                        std::string token;
                        while (iss >> token) {
                            if (token.find("thr=") == 0) {
                                thr = std::stod(token.substr(4));
                            } else if (token.find("smooth=") == 0) {
                                smooth = std::stod(token.substr(7));
                            }
                        }
                        std::cout << "   SubCircuit PITCH name=" << comp_name << " n1=" << n1 << " n2=" << n2 << " thr=" << thr << " smooth=" << smooth << std::endl;
                        components.push_back(std::make_unique<PitchTracker>(comp_name, n1, n2, thr, smooth));
                        max_node = std::max(max_node, std::max(n1, n2));
                    } else if (subckt == "FFTPITCH") {
                        int size = 8192;
                        std::string token;
                        while (iss >> token) {
                            if (token.find("size=") == 0) {
                                size = std::stod(token.substr(5));
                            }
                        }
                        std::cout << "   SubCircuit FFTPITCH name=" << comp_name << " n1=" << n1 << " n2=" << n2 << " size=" << size << std::endl;
                        components.push_back(std::make_unique<FFTPitchTracker>(comp_name, n1, n2, size));
                        max_node = std::max(max_node, std::max(n1, n2));
                    } else if (subckt == "INTEGRATOR") {
                        std::cout << "   SubCircuit INTEGRATOR name=" << comp_name << " n1=" << n1 << " n2=" << n2 << std::endl;
                        components.push_back(std::make_unique<Integrator>(comp_name, n1, n2));
                        max_node = std::max(max_node, std::max(n1, n2));
                    }
                    break;
                }
                case '.': {
                    std::string directive = comp_name;
                    if (directive == ".input") {
                        iss >> input_node;
                        std::cout << "   Directive Input Node: " << input_node << std::endl;
                    } else if (directive == ".output") {
                        iss >> output_node;
                        std::cout << "   Directive Output Node: " << output_node << std::endl;
                    } else if (directive == ".probe") {
                        std::string token;
                        std::cout << "   Directive Probe:" << std::endl;
                        iss >> probe_file;
                        std::cout << "      File Name: " << probe_file << std::endl;
                        while (iss >> token) {
                            if (token[0] == 'V' && token[1] == '(') {
                                std::string node_str = token.substr(2, token.size() - 3);
                                probes.push_back({ProbeTarget::Type::VOLTAGE, node_str});
                                std::cout << "      Voltage Node: " << node_str << std::endl;
                            } else if (token[0] == 'I' && token[1] == '(') {
                                std::string comp_str = token.substr(2, token.size() - 3);
                                probes.push_back({ProbeTarget::Type::CURRENT, comp_str});
                                std::cout << "      Current of Component: " << comp_str << std::endl;
                            } else {
                                std::cerr << "      Warning: Unknown probe token " << token << std::endl;
                            }
                        }
                    } else if (directive == ".warmup") {
                        iss >> warmup_duration;
                        std::cout << "   Directive WarmUp Duration: " << warmup_duration << "s" << std::endl;
                    } else if (directive == ".ic") {
                        std::string cap_name;
                        double v0;
                        iss >> cap_name >> v0;
                        initial_conditions[cap_name] = v0;
                        std::cout << "   Directive Initial Condition: " << cap_name << " = " << v0 << " V" << std::endl;
                    } else if (directive == ".pot") {
                        int id;
                        std::string comp_name;
                        iss >> id >> comp_name;
                        pending_params.push_back({id, comp_name});
                        std::cout << "   Directive Pot: id=" << id << " comp=" << comp_name << std::endl;
                    } else if (directive == ".param") {
                        std::string p_name;
                        double p_val;
                        iss >> p_name >> p_val;
                        std::cout << "   Directive Param: name=" << p_name << " val=" << p_val << std::endl;
                        params.set(p_name, p_val);
                    }
                    break;
                }
                default: {
                    throw std::runtime_error("Component type unknown " + std::string(1, type));
                }
            }

        }
        std::cout << std::endl;

        num_nodes = max_node + 1;

        std::cout << "Linked Params"<< std::endl;
        for (auto& [id, comp_name] : pending_params) {
            bool found = false;
            for (auto& comp : components) {
                if (comp->name == comp_name) {
                    auto* pot = dynamic_cast<Potentiometer*>(comp.get());
                    if (!pot)
                        throw std::runtime_error(".pot refers to non-potentiometer: " + comp_name);
                    param_map[id] = pot;
                    found = true;
                    std::cout << "   Link Component " << comp_name << " on Param Id " << id << std::endl;
                    break;
                }
            }
            if (!found)
                throw std::runtime_error(".pot component not found: " + comp_name);
        }
        std::cout << std::endl;
        
        return output_node >= 0;
    }

    bool hasInitialConditions() const {
        return !initial_conditions.empty();
    }
    
    bool hasWarmUp() const {
        return warmup_duration > 0;
    }

    bool hasProbes() const {
        return !probes.empty();
    }

    std::string getProbeFile() {
        return probe_file;
    }
    
    void applyInitialConditions() {
        std::cout << "Initial Conditions apply" << std::endl;
        if (initial_conditions.empty()) {
            std::cout << "   No initial conditions to apply" << std::endl;;
            return;
        }
        
        for (auto& comp : components) {
            if (comp->type == ComponentType::CAPACITOR) {
                auto* cap = dynamic_cast<Capacitor*>(comp.get());
                if (cap && initial_conditions.count(cap->name)) {
                    double v0 = initial_conditions[cap->name];
                    cap->setInitialVoltage(v0);
                    std::cout << "   " << cap->name << " = " << v0 << " V" << std::endl;
                }
            }
        }
        
        std::cout << std::endl;
    }

    std::vector<int> getParameterIds() const {
        std::vector<int> ids;
        ids.reserve(param_map.size());
        for (const auto& [id, pot] : param_map) {
            ids.push_back(id);
        }
        std::sort(ids.begin(), ids.end());
        return ids;
    }

    void setParamValue(int id, double value) {
        auto it = param_map.find(id);
        if (it == param_map.end())
            throw std::runtime_error("Parameter ID not found: " + std::to_string(id));
        it->second->setPosition(value);
    }

    double getParamValue(int id) {
        auto it = param_map.find(id);
        if (it == param_map.end())
            throw std::runtime_error("Parameter ID not found: " + std::to_string(id));
        
        return it->second->getPosition();
    }

    void reset() {
        for (auto& comp : components) {
            comp->reset();
        }
    }

private:
    double parseUnit(const std::string& unit) {
        if (unit.empty()) return 1.0;
        switch(unit[0]) {
            case 'f': return 1e-15;
            case 'p': return 1e-12;
            case 'n': return 1e-9;
            case 'u': return 1e-6;
            case 'm': return 1e-3;
            case 'k': return 1e3;
            case 'M': return 1e6;
            case 'G': return 1e9;
            default: 
                throw std::runtime_error("Unit cannot be determined: " + std::string(1, unit[0]));
            ;
        }
    }
};

#endif