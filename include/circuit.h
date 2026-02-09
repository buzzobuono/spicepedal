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
#include <filesystem>

#include "utils/debug.h"
#include "utils/param_registry.h"
#include "components/component.h"
#include "components/voltage_source.h"
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
#include "components/behavioral_voltage_source.h"
#include "components/parameter_evaluator.h"
#include "components/subckt/pitch_tracker.h"
#include "components/subckt/pitch_tracker2.h"
#include "components/subckt/fft_pitch_tracker.h"
#include "components/subckt/integrator.h"

#include <Eigen/Dense>
#include <Eigen/LU>

struct ProbeTarget {
    enum class Type { VOLTAGE, CURRENT };
    Type type;
    std::string name;
};

struct CtrlParam {
    std::string name;
    double min;
    double max;
    double step;
};

class Circuit {
    
public:
    ParameterRegistry params;
    std::vector<std::unique_ptr<Component>> components;
    int num_nodes;
    int input_node;
    int source_impedance;
    int output_node;
    double warmup_duration = 0;
    std::map<std::string, double> initial_conditions;
    std::map<int, CtrlParam> ctrl_params;
    std::vector<ProbeTarget> probes;
    std::string probe_file;
    int currentParam = 0;
    
    Circuit() : num_nodes(0), output_node(-1) {}
    
    bool loadNetlist(const std::string& filename) {
        std::locale::global(std::locale::classic());
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Cannot open netlist: " << filename << std::endl;
            return false;
        }
        std::ostringstream ss;
        ss << file.rdbuf();
        std::string netlistContent = ss.str();

        netlistContent = preprocessNetlist(netlistContent);
        
        #ifdef DEBUG_MODE
            std::ofstream dbg("debug.cir");
            dbg << netlistContent;
            dbg.close();
        #endif
        
        std::istringstream fileStream(netlistContent);
        std::string line;
        int max_node = 0;
        
        std::cout << "Circuit Creation"<< std::endl;
        while (std::getline(fileStream, line)) {
            if (line.empty() || line[0] == '*' || line[0] == '#') continue;
            size_t commentPos = line.find(';');
            if (commentPos != std::string::npos) {
                line.erase(commentPos);
            }
            
            std::istringstream iss(line);
            std::string comp_name;
            iss >> comp_name;
            
            if (comp_name.empty()) continue;
            
            char type = std::toupper(comp_name[0]);
            
            switch(type) {
                case 'R': {
                    int n1, n2;
                    std::string value;
                    iss >> n1 >> n2 >> value;
                    double v = parseNumericValue(value);
                    components.push_back(std::make_unique<Resistor>(comp_name, n1, n2, v));
                    std::cout << "   Component Resistor name=" << comp_name 
                    << " n1=" << n1
                    << " n2=" << n2
                    << " v=" << v
                    << std::endl;
                    max_node = std::max(max_node, std::max(n1, n2));
                    break;
                }
                case 'C': {
                    int n1, n2;
                    std::string value;
                    double v;
                    iss >> n1 >> n2 >> value;
                    v = parseNumericValue(value);
                    components.push_back(std::make_unique<Capacitor>(comp_name, n1, n2, v));
                    std::cout << "   Component Capacitor name=" << comp_name
                    << " n1=" << n1
                    << " n2=" << n2
                    <<" v=" << v
                    << std::endl;
                    max_node = std::max(max_node, std::max(n1, n2));
                    break;
                }
                case 'L': {
                    int n1, n2;
                    std::string value;
                    double v;
                    iss >> n1 >> n2 >> value;
                    v = parseNumericValue(value);
                    std::string attributes;
                    std::getline(iss, attributes);
                    double rs = parseNumericValue(parseAttributeValue(attributes, "Rs", "100"));
                    components.push_back(std::make_unique<Inductor>(comp_name, n1, n2, v, rs));
                    std::cout << "   Component Inductor name=" << comp_name
                    << " n1=" << n1
                    << " n2=" << n2
                    << " v=" << v
                    << " rs=" << rs
                    << std::endl;
                    max_node = std::max(max_node, std::max(n1, n2));
                    break;
                }
                case 'D': {
                    int n1, n2;
                    std::string model;
                    iss >> n1 >> n2 >> model;
                    std::string attributes;
                    std::getline(iss, attributes);
                    double Is = parseNumericValue(parseAttributeValue(attributes, "Is", "1e-14"));
                    double n = parseNumericValue(parseAttributeValue(attributes, "N", "1"));
                    double Vt = parseNumericValue(parseAttributeValue(attributes, "Vt", "0.02585"));
                    double Cj0 = parseNumericValue(parseAttributeValue(attributes, "Cj0", "0"));
                    double Vj = parseNumericValue(parseAttributeValue(attributes, "Vj", "1"));
                    double Mj = parseNumericValue(parseAttributeValue(attributes, "Mj", "0.5"));
                    std::cout << "   Component Diode name=" << comp_name
                    << " model=" << model
                    << " n1=" << n1
                    << " n2=" << n2
                    << " Is=" << Is
                    << " N=" << n
                    << " Vt=" << Vt
                    << " Cj0=" << Cj0
                    << " Vj=" << Vj
                    << " Mj=" << Mj
                    << std::endl;
                    components.push_back(std::make_unique<Diode>(comp_name, n1, n2, Is, n, Vt, Cj0, Vj, Mj));
                    max_node = std::max(max_node, std::max(n1, n2));
                    break;
                }
                case 'Q': {
                    int nc, nb, ne;
                    std::string model;
                    iss >> nc >> nb >> ne >> model;
                    std::string attributes;
                    std::getline(iss, attributes);
                    double Is = parseNumericValue(parseAttributeValue(attributes, "Is", "1e-14"));
                    double Bf = parseNumericValue(parseAttributeValue(attributes, "Bf", "100"));
                    double Br = parseNumericValue(parseAttributeValue(attributes, "Br", "1"));
                    double Vt = parseNumericValue(parseAttributeValue(attributes, "Vt", "0.02585"));
                    std::cout << "   Component Transistor name=" << comp_name
                    << " model=" << model
                    << " nc=" << nc
                    << " nb=" << nb
                    << " ne=" << ne
                    << " Is=" << Is 
                    << " Bf=" << Bf
                    << " Br=" << Br
                    << " Vt=" << Vt
                    << std::endl;
                    components.push_back(std::make_unique<BJT>(comp_name, nc, nb, ne, Bf, Br, Is, Vt));
                    max_node = std::max({max_node, nc, nb, ne});
                    break;
                }
                case 'V': {
                    int n1, n2;
                    std::string model, value;
                    iss >> n1 >> n2 >> model >> value;
                    double v = parseNumericValue(value);
                    std::string attributes;
                    std::getline(iss, attributes);
                    double rs = parseNumericValue(parseAttributeValue(attributes, "Rs", "1"));
                    auto vs = std::make_unique<VoltageSource>(comp_name, n1, n2, v, rs);
                    std::cout << "   Component VoltageSource name=" << comp_name
                    << " n1=" << n1
                    << " n2=" << n2
                    << " v=" << value
                    << " Rs=" << rs
                    << std::endl;
                    components.push_back(std::move(vs));
                    max_node = std::max(max_node, std::max(n1, n2));
                    break;
                }
                case 'W': {
                    int n1, n2;
                    iss >> n1 >> n2;
                    auto wire = std::make_unique<Wire>(comp_name, n1, n2);
                    std::cout << "   Component Wire name=" << comp_name
                    << " n1=" << n1
                    << " n2=" << n2
                    << std::endl;
                    components.push_back(std::move(wire));
                    max_node = std::max(max_node, std::max(n1, n2));
                    break;
                }
                case 'P': {
                    // P1 1 2 3 10k param=volume taper=LOG
                    int n1, n2, nw;
                    std::string value;
                    iss >> n1 >> n2 >> nw >> value;
                    double v = parseNumericValue(value);
                    std::string attributes;
                    std::getline(iss, attributes);
                    std::string taper_str = parseAttributeValue(attributes, "taper", "LIN");
                    Potentiometer::TaperType taper;
                    if (taper_str == "LOG" || taper_str == "A")
                        taper = Potentiometer::TaperType::LOGARITHMIC;
                    else if (taper_str == "LIN" || taper_str == "B")
                        taper = Potentiometer::TaperType::LINEAR;
                    else
                        throw std::runtime_error("Potentiometer taper not recognized:" + taper_str);
                    std::string param = parseAttributeValue(attributes, "param", "");
                    auto pot = std::make_unique<Potentiometer>(comp_name, n1, n2, nw, v, taper, param);
                    std::cout << "   Component Potentiometer name=" << comp_name
                    << " n1=" << n1
                    << " n2=" << n2
                    << " nw=" << nw
                    << " v=" << value
                    << " taper=" << taper_str
                    << " param=" << param
                    << std::endl;
                    pot->setParams(&(this->params));
                    components.push_back(std::move(pot));
                    max_node = std::max({max_node, n1, n2, nw});
                    break;
                }
                case 'O': {
                    int n_out, n_plus, n_minus, n_vcc, n_vee;
                    std::string model;
                    iss >> n_out >> n_plus >> n_minus >> n_vcc >> n_vee >> model;
                    std::string attributes;
                    std::getline(iss, attributes);
                    double r_out = parseNumericValue(parseAttributeValue(attributes, "Rout", "75"));
                    double i_max = parseNumericValue(parseAttributeValue(attributes, "Imax", "20m"));
                    double gain = parseNumericValue(parseAttributeValue(attributes, "Gain", "100k"));
                    double sr = parseNumericValue(parseAttributeValue(attributes, "Sr", "13")); // Assume input in V/µs
                    components.push_back(std::make_unique<OpAmp>(comp_name, n_out, n_plus, n_minus, n_vcc, n_vee, r_out, i_max, gain, sr));
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
                    iss >> n_out_p >> n_out_m >> n_ctrl_p >> n_ctrl_m;
                    std::string attributes;
                    std::getline(iss, attributes);
                    double r_out = parseNumericValue(parseAttributeValue(attributes, "Rout", "75"));
                    double v_max = parseNumericValue(parseAttributeValue(attributes, "Vmax", "15"));
                    double v_min = parseNumericValue(parseAttributeValue(attributes, "Vmin", "-15"));
                    double gain = parseNumericValue(parseAttributeValue(attributes, "Gain", "100k"));
                    components.push_back(std::make_unique<VCVS>(comp_name, n_out_p, n_out_m, n_ctrl_p, n_ctrl_m, gain, v_max, v_min, r_out));    
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
                    iss >> n1 >> n2;
                    std::string attributes;
                    std::getline(iss, attributes);
                    std::string expression = parseAttributeValue(attributes, "V", std::string(""));
                    double rs = parseNumericValue(parseAttributeValue(attributes, "Rs", "1m"));
                    auto b_source = std::make_unique<BehavioralVoltageSource>(comp_name, n1, n2, expression, rs);
                    std::cout << "   Component BehavioralVoltageSource name=" << comp_name 
                    << " n1=" << n1 
                    << " n2=" << n2 
                    << " V=\"" << expression << "\"" 
                    << " Rs=" << rs
                    << std::endl;
                    b_source->setParams(&(this->params));
                    components.push_back(std::move(b_source));
                    max_node = std::max(max_node, std::max(n1, n2));
                    break;
                }
                case 'A': {
                    // A1 param="formula"
                    std::streampos p = iss.tellg();
                    std::string param;
                    getline(iss, param, '=');
                    std::stringstream ss(param);
                    ss >> param;
                    iss.seekg(p);
                    std::string attributes;
                    std::getline(iss, attributes);
                    std::string expression = parseAttributeValue(attributes, param, std::string(""));
                    auto param_eval = std::make_unique<ParameterEvaluator>(comp_name, param, expression);
                    std::cout << "   Component ParameterEvaluator name=" << comp_name 
                    << " param=" << param
                    << " expression=\"" << expression << "\""
                    << std::endl;
                    param_eval->setParams(&(this->params));
                    components.push_back(std::move(param_eval));
                    break;
                }
                case 'X': {
                    int n1, n2;
                    std::string subckt;
                    iss >> n1 >> n2 >> subckt;
                    std::string attributes;
                    std::getline(iss, attributes);
                    if (subckt == "PITCH") {
                        double thr = parseNumericValue(parseAttributeValue(attributes, "thr", "0.02"));
                        double smooth = parseNumericValue(parseAttributeValue(attributes, "smooth", "0.2"));
                        std::cout << "   SubCircuit PITCH name=" << comp_name
                        << " n1=" << n1
                        << " n2=" << n2
                        << " thr=" << thr
                        << " smooth=" << smooth
                        << std::endl;
                        components.push_back(std::make_unique<PitchTracker>(comp_name, n1, n2, thr, smooth));
                    } if (subckt == "PITCH2") {
                        std::string attributes;
                        std::getline(iss, attributes);
                        double thr = parseNumericValue(parseAttributeValue(attributes, "thr", "0.02"));
                        int n_signal = std::stoi(parseAttributeValue(attributes, "nsig", "8"));
                        int n_freq = std::stoi(parseAttributeValue(attributes, "nfreq", "4"));
                        std::cout << "   SubCircuit PITCH2 name=" << comp_name 
                        << " n1=" << n1
                        << " n2=" << n2
                        << " thr=" << thr
                        << " nsig=" << n_signal
                        << " nfreq=" << n_freq
                        << std::endl;
                        components.push_back(std::make_unique<PitchTracker2>(comp_name, n1, n2, thr, n_signal, n_freq));
                        max_node = std::max(max_node, std::max(n1, n2));
                    } else if (subckt == "FFTPITCH") {
                        int size = std::stoi(parseAttributeValue(attributes, "size", "8192"));
                        std::cout << "   SubCircuit FFTPITCH name=" << comp_name
                        << " n1=" << n1
                        << " n2=" << n2
                        << " size=" << size
                        << std::endl;
                        components.push_back(std::make_unique<FFTPitchTracker>(comp_name, n1, n2, size));
                    } else if (subckt == "INTEGRATOR") {
                        std::cout << "   SubCircuit INTEGRATOR name=" << comp_name
                        << " n1=" << n1
                        << " n2=" << n2
                        << std::endl;
                        components.push_back(std::make_unique<Integrator>(comp_name, n1, n2));
                    }
                    max_node = std::max(max_node, std::max(n1, n2));
                    break;
                }
                case '.': {
                    std::string directive = comp_name;
                    if (directive == ".input") {
                        iss >> input_node;
                        std::string attributes;
                        std::getline(iss, attributes);
                        source_impedance = parseNumericValue(parseAttributeValue(attributes, "Z", "15k"));
                        std::cout << "   Directive Input Node: " << input_node
                        << " Z=" << source_impedance
                        << std::endl;
                    } else if (directive == ".output") {
                        iss >> output_node;
                        std::cout << "   Directive Output Node: " << output_node
                        << std::endl;
                    } else if (directive == ".probe") {
                        std::string token;
                        std::cout << "   Directive Probe:" << std::endl;
                        std::filesystem::path filepath(filename);
                        probe_file = filepath.replace_extension(".csv").filename().string();
                        
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
                                throw std::runtime_error("Unknown probe token: " + token);
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
                    } else if (directive == ".ctrl") {
                        int id;
                        std::string param;
                        double min, max, step;
                        iss >> id >> param >> min >> max >> step;
                        ctrl_params[id] = {param, min, max, step};
                        std::cout << "   Directive Ctrl id=" << id
                        << " param=" << param <<
                        " min=" << min <<
                        " max=" << max <<
                        " step=" << step <<
                        std::endl;
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

    std::vector<int> getCtrlParameterIds() const {
        std::vector<int> ids;
        ids.reserve(ctrl_params.size());
        for (auto const& [id, param] : ctrl_params) {
            ids.push_back(id);
        }
        return ids;
    }

    double getCtrlParamValue(int id) const {
        auto it = ctrl_params.find(id);
        const auto& param = it->second;
        return params.get(param.name);
    }

    double setCtrlParamValue(int id, double value) {
        auto it = ctrl_params.find(id);
        const auto& param = it->second;
        double actualValue = std::clamp(value, param.min, param.max);
        params.set(param.name, actualValue);
        return actualValue;
    }
    
    void incrementCtrlParamValue() {
        auto it = ctrl_params.find(currentParam);
        const auto& param = it->second;
        double actualValue = std::clamp(params.get(param.name) + param.step, param.min, param.max);
        params.set(param.name, actualValue);
        std::cout << "Param '" << param.name << "': " << actualValue << std::endl;
    }
    
    void decrementCtrlParamValue() {
        auto it = ctrl_params.find(currentParam);
        const auto& param = it->second;
        double actualValue = std::clamp(params.get(param.name) - param.step, param.min, param.max);
        params.set(param.name, actualValue);
        std::cout << "Param '" << param.name << "': " << actualValue << std::endl;
    }
    
    void nextCtrlParam() {
        currentParam = (currentParam + 1) % ctrl_params.size();
        auto it = ctrl_params.find(currentParam);
        const auto& param = it->second;
        std::cout << "Param '" << param.name << "' selected" << std::endl;
    }
    
    void previousCtrlParam() {
        currentParam = (currentParam - 1 + ctrl_params.size()) % ctrl_params.size();
        auto it = ctrl_params.find(currentParam);
        const auto& param = it->second;
        std::cout << "Param '" << param.name << "' selected" << std::endl;
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
    
    double parseNumericValue(const std::string& valStr) {
        if (valStr.empty()) return 0.0;
        size_t firstChar = valStr.find_first_not_of("0123456789e.-+");
        if (firstChar == std::string::npos) {
            return std::stod(valStr);
        }
        std::string numPart = valStr.substr(0, firstChar);
        std::string unitPart = valStr.substr(firstChar);
        return std::stod(numPart) * parseUnit(unitPart);
    }
    
    std::string parseAttributeValue(const std::string& line, const std::string& key, const std::string& defaultValue) {
        const auto pos = line.find(key + "=");
        if (pos == std::string::npos) return defaultValue;
        size_t i = pos + key.size() + 1;
        while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i]))) ++i;
        size_t end;
        std::string val;
        if (i < line.size() && line[i] == '"') {
            end = line.find('"', ++i);
            if (end == std::string::npos) return defaultValue;
            val = line.substr(i, end - i);
        } else {
            end = line.find_first_of(" \t\r\n", i);
            val = line.substr(i, (end == std::string::npos) ? std::string::npos : end - i);
        }
        if (val.empty()) return defaultValue;
        return val;
    }

    std::string preprocessNetlist(const std::string &netlistText) {
        std::vector<std::string> netlistLines;
        std::istringstream file1(netlistText);
        std::string line;
        while (std::getline(file1, line)) {
            std::istringstream iss(line);
            std::string firstWord;
            iss >> firstWord;
            if (firstWord == ".include") {
                std::string includePath;
                iss >> includePath;
                std::ifstream incFile(includePath);
                if (!incFile.is_open()) {
                    throw std::runtime_error("Cannot open include file: " + includePath);
                }
                std::string incLine;
                while (std::getline(incFile, incLine)) {
                    netlistLines.push_back(incLine);
                }
                continue;
            }
            netlistLines.push_back(line);
        }

        std::unordered_map<std::string, std::string> modelAttributes;
        for (auto &l : netlistLines) {
            std::istringstream iss(l);
            std::string firstWord;
            iss >> firstWord;
            if (firstWord == ".model") {
                std::string modelName, type;
                iss >> modelName >> type;
                std::string attributes;
                std::getline(iss, attributes);
                modelAttributes[modelName] = attributes;
            }
        }

        std::ostringstream output;
        for (auto l : netlistLines) {
            std::istringstream iss(l);
            std::string firstWord;
            iss >> firstWord;
            if (firstWord.empty()) {
                output << l << std::endl;
                continue;
            }
            char lineType = firstWord[0];
            if (lineType != 'D' && lineType != 'Q' && lineType != 'O') {
                output << l << std::endl;
                continue;
            }
            for (const auto& [modelName, attrs] : modelAttributes) {
                std::regex re("\\b" + modelName + "\\b");
                l = std::regex_replace(l, re, modelName + attrs);
            }
            output << l << std::endl;
        }
        return output.str();
    }
    
};

#endif