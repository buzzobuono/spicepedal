#ifndef TRAN_SOLVER_H
#define TRAN_SOLVER_H

#include <iostream>
#include <fstream>
#include <iomanip>
#include <memory>
#include <map>

#include "solvers/newton_raphson_solver.h"
#include "circuit.h"
#include "signals/signal_generator.h"
#include "utils/wav_helper.h"

class TransientSolver : public NewtonRaphsonSolver {
    
    private:
    
    std::ofstream logFile;
    bool probe_enabled = false;
    
    std::vector<double> signalIn;
    std::unique_ptr<SignalGenerator> signal_generator;
    std::string output_file;
    bool bypass;
    
    double mean = 0.0f;
    double maxNormalized = 0.0;
    double scale = 1;
    double peak_in = 0.0;
    double peak_out = 0.0;
    double rms_in = 0.0;
    double rms_out = 0.0;
    
    public:
    
    TransientSolver(Circuit& circuit, double sample_rate, std::unique_ptr<SignalGenerator> signal_generator, int source_impedance, std::string output_file, bool bypass, int max_iterations, double tolerance)
        : NewtonRaphsonSolver(circuit, sample_rate, source_impedance, max_iterations, tolerance),
          signal_generator(std::move(signal_generator)),
          bypass(bypass),
          output_file(output_file)
    {
        this->signal_generator->printInfo();
        signalIn = this->signal_generator->generate();
    }
    
    ~TransientSolver() override {
        if (circuit.hasProbes()) {
            closeProbeFile();
        }
    }
    
    bool initialize() override {
        if (bypass) return 0;
        
        NewtonRaphsonSolver::initialize(); 
        
        if (circuit.hasInitialConditions()) {
            circuit.applyInitialConditions();
        }
        
        if (circuit.hasWarmUp()) {
            warmUp(circuit.warmup_duration);
        }
        
        if (circuit.hasProbes()) {
            openProbeFile();
        }
        
        std::cout << "Circuit initialized with this Operating Point" << std::endl;
        this->printDCOperatingPoints();
        
        return true;
    }

    bool solveImpl() override {
        sample_rate = signal_generator->getSampleRate();
        mean = signal_generator->getMean();
        maxNormalized = signal_generator->getMaxNormalized();
        scale = signal_generator->getScaleFactor();
        
        std::vector<double> signalOut(signalIn.size());
        
        for (size_t i = 0; i < signalIn.size(); i++) {
            if (!bypass) {
                signalOut[i] = 0;
                this->setInputVoltage(signalIn[i]);
                if (runNewtonRaphson(dt)) {
                    signalOut[i] = this->getOutputVoltage();
                }
                logProbes(); 
            } else {
                signalOut[i] = signalIn[i];
            }
                
            // Update statistics
            peak_in = std::max(peak_in, std::abs(signalIn[i]));
            peak_out = std::max(peak_out, std::abs(signalOut[i]));
            rms_in += signalIn[i] * signalIn[i];
            rms_out += signalOut[i] * signalOut[i];
        }
            
        rms_in = std::sqrt(rms_in / signalIn.size());
        rms_out = std::sqrt(rms_out / signalIn.size());
            
        double outputPeak = 0.0;
        for (double v : signalOut) {
            outputPeak = std::max(outputPeak, std::abs(v));
        }
        
        if (!output_file.empty()) {
            WavHelper wav_helper;
            wav_helper.write(signalOut, output_file, sample_rate);
        }
        
        std::cout << "Simulation ended with this Operating Point" << std::endl;
        this->printDCOperatingPoints();
        
        return true;
    }
    
    void openProbeFile() {
        std::string filename = circuit.getProbeFile();
        logFile.open(filename);
        if (!logFile.is_open()) {
            throw std::runtime_error("Cannot open probe file: " + filename);
        }
        
        probe_enabled = true;
        
        logFile << "time";
        for (auto& p : circuit.probes) {
            if (p.type == ProbeTarget::Type::VOLTAGE) {
                logFile << ";V(" << p.name << ")";
            } else if (p.type == ProbeTarget::Type::CURRENT) {
                logFile << ";I(" << p.name << ")";
            }
        }
        logFile << "\n";
        
        std::cout << "Probe file opened: " << filename << std::endl;
    }
    
    void logProbes() {
        if (!probe_enabled) return;
        double time = sample_count * dt;
        logFile << std::fixed << std::setprecision(9) << time;
        
        for (auto& p : circuit.probes) {
            if (p.type == ProbeTarget::Type::VOLTAGE) {
                if (p.name == "input") {
                    logFile << ";" << input_voltage;    
                } else {
                    int node = std::stoi(p.name);
                    if (node < circuit.num_nodes) {
                        logFile << ";" << V(node);
                    } else {
                        logFile << ";NaN";
                    }
                }
            } else if (p.type == ProbeTarget::Type::CURRENT) {
                double current = 0.0;
                bool found = false;
                for (auto& comp : circuit.components) {
                    if (comp->name == p.name) {
                        current = comp->getCurrent();
                        found = true;
                        break;
                    }
                }
                if (found) {
                    logFile << ";" << current;
                } else {
                    logFile << ";NaN";
                }
            }
        }
        
        logFile << "\n";
    }
    
    void closeProbeFile() {
        if (probe_enabled && logFile.is_open()) {
            logFile.close();
            probe_enabled = false;
            std::cout << "Probe file closed." << std::endl;
        }
    }
    
    void printResult() override {
        std::cout << "Signal Statistics" << std::endl;
        std::cout << "  Mean Input Signal " << mean << std::endl;
        std::cout << "  Max Normalized " << maxNormalized << " V, Scale Factor " << scale << std::endl;
        std::cout << "  Input Peak: " << peak_in << " V, " << 20 * std::log10(peak_in) << " dBFS, RMS: " << 20 * std::log10(rms_in) << " dBFS" << std::endl;
        std::cout << "  Output Peak: " << peak_out << " V, " << 20 * std::log10(peak_out) << " dBFS, RMS: " << 20 * std::log10(rms_out) << " dBFS" << std::endl;
        std::cout << "  Circuit gain: " << 20 * std::log10(rms_out / rms_in) << " dB" << std::endl;
        std::cout << std::endl;
    }
    
};

#endif

