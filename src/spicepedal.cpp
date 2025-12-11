#include <iostream>
#include <fstream>
#include <cmath>
#include <vector>
#include <string>
#include <sndfile.h>

#include "external/CLI11.hpp"

#include "circuit.h"
#include "solvers/newton_raphson_solver.h"
#include "solvers/dc_solver.h"
#include "solvers/zin_solver.h"
#include "solvers/zout_solver.h"
#include "solvers/transient_solver.h"
#include "signals/signal_generator.h"
#include "signals/file_input_generator.h"
#include "signals/sinusoid_generator.h"
#include "signals/logarithmic_frequency_sweep_generator.h"
#include "signals/linear_frequency_sweep_generator.h"
#include "signals/dc_generator.h"
#include "utils/wav_helper.h"

class SpicePedalProcessor
{
private:
    Circuit circuit;
    std::string analysis_type;
    double sample_rate;
    int input_frequency;
    double input_duration;
    double input_amplitude;
    bool frequency_sweep_log;
    bool frequency_sweep_lin;
    int source_impedance;
    bool bypass;
    int max_iterations;
    double tolerance;

public:
    SpicePedalProcessor(std::string analysis_type,
                     const std::string &netlist_file,
                     double sample_rate,
                     int input_frequency,
                     double input_duration,
                     double input_amplitude,
                     bool frequency_sweep_log,
                     bool frequency_sweep_lin,
                     int source_impedance,
                     bool bypass,
                     int max_iterations,
                     double tolerance
                    )
        : analysis_type(analysis_type),
          sample_rate(sample_rate),
          input_frequency(input_frequency),
          input_duration(input_duration),
          input_amplitude(input_amplitude),
          frequency_sweep_log(frequency_sweep_log),
          frequency_sweep_lin(frequency_sweep_lin),
          source_impedance(source_impedance),
          bypass(bypass),
          max_iterations(max_iterations),
          tolerance(tolerance)
    {
        if (!circuit.loadNetlist(netlist_file)) {
            throw std::runtime_error("Failed to load netlist");
        }

    }
    
    bool process(const std::string &input_file, const std::string &output_file)
    {
        double mean = 0.0f;
        double maxNormalized = 0.0;
        double scale = 1;
        std::unique_ptr<NewtonRaphsonSolver> solver;
        if (analysis_type != "TRAN") {
            if (analysis_type == "DC") {
                solver = std::make_unique<DCSolver>(circuit, max_iterations, tolerance);
            } else if (analysis_type == "ZIN") {
                solver = std::make_unique<ZInSolver>(circuit, sample_rate, source_impedance, input_amplitude, input_frequency, input_duration, max_iterations, tolerance);
            } else if (analysis_type == "ZOUT") {
                solver = std::make_unique<ZOutSolver>(circuit, sample_rate, source_impedance, input_amplitude, input_frequency, input_duration, max_iterations, tolerance);
            }
            solver->initialize();
            if (!solver->solve()) {
                std::cerr << "   ERROR: Solver not convergent after " << max_iterations << " iterations" << std::endl;
                return 1;
            }
        } else if (analysis_type == "TRAN") {
            std::unique_ptr<SignalGenerator> signal_generator;
            std::vector<double> signalIn;
            
            if (!input_file.empty()) {
                signal_generator = std::make_unique<FileInputGenerator>(input_file, input_amplitude);
            } else if (frequency_sweep_log) {
                signal_generator = std::make_unique<LogarithmicFrequencySweepGenerator>(sample_rate, input_duration, input_amplitude);
            } else if (frequency_sweep_lin) {
                signal_generator = std::make_unique<LinearFrequencySweepGenerator>(sample_rate, input_duration, input_amplitude);
            } else if (input_frequency > 0) {
                signal_generator = std::make_unique<SinusoidGenerator>(sample_rate, input_frequency, input_duration, input_amplitude);
            } else {
                signal_generator = std::make_unique<DCGenerator>(sample_rate, input_duration, input_amplitude);
            }
            
            signal_generator->printInfo();
            signalIn = signal_generator->generate();
            sample_rate = signal_generator->getSampleRate();
            mean = signal_generator->getMean();
            maxNormalized = signal_generator->getMaxNormalized();
            scale = signal_generator->getScaleFactor();
            
            solver = std::make_unique<TransientSolver>(circuit, sample_rate, source_impedance, max_iterations, tolerance);
            
            if (!bypass) {
                solver->initialize();
                std::cout << "Circuit initialized with this Operating Point" << std::endl;
                solver->printDCOperatingPoints();
            }
            
            std::vector<double> signalOut(signalIn.size());
            
            double peak_in = 0.0, peak_out = 0.0;
            double rms_in = 0.0, rms_out = 0.0;
            
            for (size_t i = 0; i < signalIn.size(); i++) {
                if (!bypass) {
                    signalOut[i] = 0;
                    solver->setInputVoltage(signalIn[i]);
                    if (solver->solve()) {
                        signalOut[i] = solver->getOutputVoltage();
                    }
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
             
            std::cout << "Simulation ended with this Operating Point" << std::endl;
            solver->printDCOperatingPoints();
            
            std::cout << "Signal Statistics" << std::endl;
            std::cout << "  Mean Input Signal " << mean << std::endl;
            std::cout << "  Max Normalized " << maxNormalized << " V, Scale Factor " << scale << std::endl;
            std::cout << "  Input Peak: " << peak_in << " V, " << 20 * std::log10(peak_in) << " dBFS, RMS: " << 20 * std::log10(rms_in) << " dBFS" << std::endl;
            std::cout << "  Output Peak: " << peak_out << " V, " << 20 * std::log10(peak_out) << " dBFS, RMS: " << 20 * std::log10(rms_out) << " dBFS" << std::endl;
            std::cout << "  Circuit gain: " << 20 * std::log10(rms_out / rms_in) << " dB" << std::endl;
            std::cout << std::endl;

            if (!output_file.empty()) {
                WavHelper wav_helper;
                wav_helper.write(signalOut, output_file, sample_rate);
            }
        }
        solver->printResult();
        solver->printProcessStatistics();
        return 0;
    }
    
};

int main(int argc, char *argv[]) {
    CLI::App app { "SpicePedal: a realtime simple spice-like simulator for audio" };
    
    std::string analysis_type = "TRAN";
    std::string input_file;
    int input_frequency = 0;
    double input_duration = 2.0;
    double input_amplitude = 0.15;
    bool frequency_sweep_log= false;
    bool frequency_sweep_lin = false;
    int source_impedance = 25000;
    int sample_rate = 44100;
    std::string output_file;
    std::string netlist_file;
    bool bypass = false;
    int max_iterations = 20;
    double tolerance = 1e-6;
    
    app.add_option("-a,--analysis-type", analysis_type, "Analysis Type")->check(CLI::IsMember({"TRAN", "DC", "ZIN", "ZOUT", "TEST"}))->default_val(analysis_type);
    
    app.add_option("-i,--input-file", input_file, "Input File")->check(CLI::ExistingFile);
    app.add_option("-f,--input-frequency", input_frequency, "Input Frequency");
    app.add_option("-d,--input-duration", input_duration, "Input Duration")->default_val(input_duration);
    app.add_option("-v,--input-amplitude", input_amplitude, "Input Amplitude")->check(CLI::Range(-5.0, 5.0))->default_val(input_amplitude);
    app.add_flag("-F,--fslog,--frequency-sweep-log", frequency_sweep_log, "Frequency Sweep Logarithmic")->default_val(frequency_sweep_log);
    app.add_flag("-L,--fslin,--frequency-sweep-lin", frequency_sweep_lin, "Frequency Sweep Linear")->default_val(frequency_sweep_lin);
    app.add_option("-I,--source-impedance", source_impedance, "Source Impedance")->check(CLI::Range(0, 30000))->default_val(source_impedance);
    app.add_option("-s,--sample-rate", sample_rate, "Sample Rate");
    
    app.add_option("-o,--output-file", output_file, "Output File");
    
    app.add_option("-c,--circuit", netlist_file, "Netlist File")->check(CLI::ExistingFile)->required();
    app.add_flag("-b,--bypass", bypass, "Bypass Circuit")->default_val(bypass);
    
    app.add_option("-m,--max-iterations", max_iterations, "Max Solver's Iterations")->default_val(max_iterations);
    app.add_option("-t,--tolerance", tolerance, "Solver's Tolerance")->default_val(tolerance);
    
    CLI11_PARSE(app, argc, argv);

    std::cout << "Input Parameters" << std::endl;
    std::cout << "   Analysis Type: " << analysis_type << std::endl;
    std::cout << "   Input File: " << input_file << std::endl;
    std::cout << "   Input Frequency: " << input_frequency << "Hz" << std::endl;
    std::cout << "   Input Duration: " << input_duration << "s" << std::endl;
    std::cout << "   Input Amplitude: " << input_amplitude << "V" << std::endl;
    std::cout << "   Source Impedance: " << source_impedance << "Ω" << std::endl;
    std::cout << "   Frequency Sweep Logarithmic: " << (frequency_sweep_log ? "True" : "False") << std::endl;
    std::cout << "   Frequency Sweep Linear: " << (frequency_sweep_log ? "True" : "False") << std::endl;
    std::cout << "   Sample Rate: " << sample_rate << "Hz" << std::endl;
    std::cout << "   Output File: " << output_file << std::endl;
    std::cout << "   Circuit File: " << netlist_file << std::endl;
    std::cout << "   Bypass Circuit: " << (bypass ? "True" : "False") << std::endl;
    std::cout << "   Max Iterations: " << max_iterations << std::endl;
    std::cout << "   Tolerance: " << tolerance << std::endl;
    std::cout << std::endl;

    try {
        SpicePedalProcessor processor(analysis_type, netlist_file, sample_rate, input_frequency, input_duration, input_amplitude, frequency_sweep_log, frequency_sweep_lin, source_impedance, bypass, max_iterations, tolerance);
        if (!processor.process(input_file, output_file)) {
            return 1;
        }
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
    
}
