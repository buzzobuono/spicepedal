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
#include "signals/pulse_generator.h"
#include "utils/wav_helper.h"

class SpicePedalProcessor
{
private:
    Circuit circuit;
    std::string analysis_type;
    double sample_rate;
    std::string input_file;
    int input_frequency;
    double input_duration;
    double input_amplitude;
    double input_gain_db;
    double output_gain_db;
    bool frequency_sweep_log;
    bool frequency_sweep_lin;
    bool input_pulse;
    bool bypass;
    bool clipping;
    int max_iterations;
    double tolerance;

    std::string output_file;
    
public:
    SpicePedalProcessor(std::string analysis_type,
                     const std::string &netlist_file,
                     double sample_rate,
                     std::string input_file,
                     int input_frequency,
                     double input_duration,
                     double input_amplitude,
                     double input_gain_db,
                     double output_gain_db,
                     bool frequency_sweep_log,
                     bool frequency_sweep_lin,
                     bool input_pulse,
                     bool bypass,
                     bool clipping,
                     int max_iterations,
                     double tolerance,
                     std::string output_file
                    )
        : analysis_type(analysis_type),
          sample_rate(sample_rate),
          input_file(input_file),
          input_frequency(input_frequency),
          input_duration(input_duration),
          input_amplitude(input_amplitude),
          input_gain_db(input_gain_db),
          output_gain_db(output_gain_db),
          frequency_sweep_log(frequency_sweep_log),
          frequency_sweep_lin(frequency_sweep_lin),
          input_pulse(input_pulse),
          bypass(bypass),
          clipping(clipping),
          max_iterations(max_iterations),
          tolerance(tolerance),
          output_file(output_file)
    {
        if (!circuit.loadNetlist(netlist_file)) {
            throw std::runtime_error("Failed to load netlist");
        }

    }
    
    bool process()
    {
        double mean = 0.0f;
        double maxNormalized = 0.0;
        double scale = 1;
        std::unique_ptr<NewtonRaphsonSolver> solver;
        
        if (analysis_type == "DC") {
            solver = std::make_unique<DCSolver>(circuit, max_iterations, tolerance);
        } else if (analysis_type == "ZIN") {
            solver = std::make_unique<ZInSolver>(circuit, sample_rate, input_amplitude, input_frequency, input_duration, max_iterations, tolerance);
        } else if (analysis_type == "ZOUT") {
            solver = std::make_unique<ZOutSolver>(circuit, sample_rate, input_amplitude, input_frequency, input_duration, max_iterations, tolerance);
        } else if (analysis_type == "TRAN") {
            std::unique_ptr<SignalGenerator> signal_generator = getSignalGenerator();
            solver = std::make_unique<TransientSolver>(circuit, sample_rate, std::move(signal_generator), std::pow(10.0, input_gain_db / 20.0), std::pow(10.0, output_gain_db / 20.0), output_file, bypass, clipping, max_iterations, tolerance);
        }
            
        solver->initialize();
        if (!solver->solve()) {
            std::cerr << "   ERROR: Solver not convergent after " << max_iterations << " iterations" << std::endl;
            return 1;
        }
        
        solver->printResult();
        solver->printProcessStatistics();
        return 0;
    }
    
    std::unique_ptr<SignalGenerator> getSignalGenerator() {
        std::unique_ptr<SignalGenerator> signal_generator;
        std::vector<double> signalIn;
            
        if (!input_file.empty()) {
            signal_generator = std::make_unique<FileInputGenerator>(sample_rate, input_file, input_amplitude);
        } else if (frequency_sweep_log) {
            signal_generator = std::make_unique<LogarithmicFrequencySweepGenerator>(sample_rate, input_duration, input_amplitude);
        } else if (frequency_sweep_lin) {
            signal_generator = std::make_unique<LinearFrequencySweepGenerator>(sample_rate, input_duration, input_amplitude);
        } else if (input_pulse){
            signal_generator = std::make_unique<PulseGenerator>(sample_rate, input_duration, 0, input_amplitude, input_duration/3, 0, 0, input_duration/10, 1.0/input_frequency);
        } else if (input_frequency > 0) {
            signal_generator = std::make_unique<SinusoidGenerator>(sample_rate, input_frequency, input_duration, input_amplitude);
        } else {
            signal_generator = std::make_unique<DCGenerator>(sample_rate, input_duration, input_amplitude);
        }
        return signal_generator;
    }
    
};

int main(int argc, char *argv[]) {
    CLI::App app { "SpicePedal: a realtime simple spice-like simulator for audio" };
    
    std::string analysis_type = "TRAN";
    std::string input_file;
    int input_frequency = 0;
    double input_duration = 2.0;
    double input_amplitude = 0.15;
    double input_gain_db = 0.0;
    double output_gain_db = 0.0;
    bool frequency_sweep_log= false;
    bool frequency_sweep_lin = false;
    bool input_pulse = false;
    int sample_rate = 44100;
    std::string output_file;
    std::string netlist_file;
    bool bypass = false;
    bool clipping = false;
    int max_iterations = 20;
    double tolerance = 1e-6;
    
    app.add_option("-a,--analysis-type", analysis_type, "Analysis Type")->check(CLI::IsMember({"TRAN", "DC", "ZIN", "ZOUT", "TEST"}))->default_val(analysis_type);
    
    app.add_option("-i,--input-file", input_file, "Input File")->check(CLI::ExistingFile);
    app.add_option("-f,--input-frequency", input_frequency, "Input Frequency");
    app.add_option("-d,--input-duration", input_duration, "Input Duration")->default_val(input_duration);
    app.add_option("-v,--input-amplitude", input_amplitude, "Input Amplitude")->check(CLI::Range(-5.0, 5.0))->default_val(input_amplitude);
    app.add_option("--ig,--input-gain", input_gain_db, "Input Gain in dB")->default_val(0.0);
    app.add_option("--og,--output-gain", output_gain_db, "Output Gain in dB")->default_val(0.0);
    app.add_flag("-F,--fslog,--frequency-sweep-log", frequency_sweep_log, "Frequency Sweep Logarithmic")->default_val(frequency_sweep_log);
    app.add_flag("-L,--fslin,--frequency-sweep-lin", frequency_sweep_lin, "Frequency Sweep Linear")->default_val(frequency_sweep_lin);
    app.add_flag("-p,--input-pulse", input_pulse, "Input Pulse")->default_val(input_pulse);
    app.add_option("-s,--sample-rate", sample_rate, "Sample Rate");
    
    app.add_option("-o,--output-file", output_file, "Output File");
    
    app.add_option("-c,--circuit", netlist_file, "Netlist File")->check(CLI::ExistingFile)->required();
    app.add_flag("-b,--bypass", bypass, "Bypass Circuit")->default_val(bypass);
    app.add_flag("--cl,--clipping", clipping, "Soft Output Clipping")->default_val(clipping);
    
    app.add_option("-m,--max-iterations", max_iterations, "Max Solver's Iterations")->default_val(max_iterations);
    app.add_option("-t,--tolerance", tolerance, "Solver's Tolerance")->default_val(tolerance);
    
    CLI11_PARSE(app, argc, argv);

    std::cout << "Input Parameters" << std::endl;
    std::cout << "   Analysis Type: " << analysis_type << std::endl;
    std::cout << "   Input File: " << input_file << std::endl;
    std::cout << "   Input Frequency: " << input_frequency << "Hz" << std::endl;
    std::cout << "   Input Duration: " << input_duration << "s" << std::endl;
    std::cout << "   Input Amplitude: " << input_amplitude << "V" << std::endl;
    std::cout << "   Input Gain: " << input_gain_db << "dB" << std::endl;
    std::cout << "   Output Gain: " << output_gain_db << "dB" << std::endl;
    std::cout << "   Frequency Sweep Logarithmic: " << (frequency_sweep_log ? "True" : "False") << std::endl;
    std::cout << "   Frequency Sweep Linear: " << (frequency_sweep_log ? "True" : "False") << std::endl;
    std::cout << "   Input Pulse: " << (input_pulse ? "True" : "False") << std::endl;
    std::cout << "   Sample Rate: " << sample_rate << "Hz" << std::endl;
    std::cout << "   Output File: " << output_file << std::endl;
    std::cout << "   Circuit File: " << netlist_file << std::endl;
    std::cout << "   Bypass Circuit: " << (bypass ? "True" : "False") << std::endl;
    std::cout << "   Max Iterations: " << max_iterations << std::endl;
    std::cout << "   Tolerance: " << tolerance << std::endl;
    std::cout << std::endl;

    try {
        SpicePedalProcessor processor(analysis_type, netlist_file, sample_rate, input_file, input_frequency, input_duration, input_amplitude, input_gain_db, output_gain_db, frequency_sweep_log, frequency_sweep_lin, input_pulse, bypass, clipping, max_iterations, tolerance, output_file);
        if (!processor.process()) {
            return 1;
        }
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
    
}
