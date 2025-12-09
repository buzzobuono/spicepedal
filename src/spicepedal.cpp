#include <iostream>
#include <fstream>
#include <cmath>
#include <vector>
#include <string>
#include <sndfile.h>

#include "external/CLI11.hpp"

#include "circuit.h"
#include "solvers/dc_solver.h"
#include "solvers/zin_solver.h"
#include "solvers/zout_solver.h"
#include "solvers/transient_solver.h"
#include "tran_circuit_solver.h"

class SpicePedalProcessor
{
private:
    Circuit circuit;
    std::string analysis_type;
    double sample_rate;
    int input_frequency;
    double input_duration;
    double input_amplitude;
    bool frequency_sweep;
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
                     bool frequency_sweep,
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
          frequency_sweep(frequency_sweep),
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
        if (analysis_type == "DC") {
            std::cout << "DC Analysis" << std::endl;
            std::unique_ptr<DCSolver> dc_solver = std::make_unique<DCSolver>(circuit, max_iterations, tolerance);
            
            dc_solver->initialize();
            
            dc_solver->setInputVoltage(input_amplitude);
            
            auto start = std::chrono::high_resolution_clock::now();
            if (!dc_solver->solve()) {
                std::cerr << "   ERROR: DC Analysis not convergent after " << max_iterations << " iterations" << std::endl;
            }
            auto end = std::chrono::high_resolution_clock::now();
            
            dc_solver->printDCOperatingPoint();

            std::cout << "Process Statistics:" << std::endl;
            std::cout << "  Solver's Execution Time: " << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() << " us" << std::endl;
            std::cout << "  Solver's Failure Percentage: " << dc_solver->getFailurePercentage() << " %" << std::endl;
            std::cout << "  Solver's Total Samples: " << dc_solver->getTotalSamples() << std::endl;
            std::cout << "  Solver's Total Iterations: " << dc_solver->getTotalIterations() << std::endl;
            std::cout << "  Solver's Mean Iterations: " << dc_solver->getMeanIterations() << std::endl;
            std::cout << std::endl;

            return true;
        } else if (analysis_type == "ZIN") {
            std::cout << "ZIn Analysis" << std::endl;
            std::unique_ptr<ZInSolver> zin_solver = std::make_unique<ZInSolver>(circuit, sample_rate, source_impedance, input_amplitude, input_frequency, input_duration, max_iterations, tolerance);
            
            zin_solver->initialize();
            
            auto start = std::chrono::high_resolution_clock::now();
            if (!zin_solver->solve()) {
                std::cerr << "   ERROR: Zin Analysis not convergent after " << max_iterations << " iterations" << std::endl;
            }
            auto end = std::chrono::high_resolution_clock::now();
            
            zin_solver->printInputImpedance();

            std::cout << "Process Statistics:" << std::endl;
            std::cout << "  Solver's Execution Time: " << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() << " us" << std::endl;
            std::cout << "  Solver's Failure Percentage: " << zin_solver->getFailurePercentage() << " %" << std::endl;
            std::cout << "  Solver's Total Samples: " << zin_solver->getTotalSamples() << std::endl;
            std::cout << "  Solver's Total Iterations: " << zin_solver->getTotalIterations() << std::endl;
            std::cout << "  Solver's Mean Iterations: " << zin_solver->getMeanIterations() << std::endl;
            std::cout << std::endl;
            return true;
        } else if (analysis_type == "ZOUT") {
            std::cout << "ZOut Analysis" << std::endl;
            std::unique_ptr<ZOutSolver> zout_solver = std::make_unique<ZOutSolver>(circuit, sample_rate, source_impedance, input_amplitude, input_frequency, input_duration, max_iterations, tolerance);
            
            zout_solver->initialize();
            
            auto start = std::chrono::high_resolution_clock::now();
            if (!zout_solver->solve()) {
                std::cerr << "   ERROR: Zout Analysis not convergent after " << max_iterations << " iterations" << std::endl;
            }
            auto end = std::chrono::high_resolution_clock::now();
            
            zout_solver->printOutputImpedance();

            std::cout << "Process Statistics:" << std::endl;
            std::cout << "  Solver's Execution Time: " << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() << " us" << std::endl;
            std::cout << "  Solver's Failure Percentage: " << zout_solver->getFailurePercentage() << " %" << std::endl;
            std::cout << "  Solver's Total Samples: " << zout_solver->getTotalSamples() << std::endl;
            std::cout << "  Solver's Total Iterations: " << zout_solver->getTotalIterations() << std::endl;
            std::cout << "  Solver's Mean Iterations: " << zout_solver->getMeanIterations() << std::endl;
            std::cout << std::endl;
            return true;
        } else if (analysis_type == "TRAN" || analysis_type == "TEST") {
            std::vector<double> signalIn;
            if (!input_file.empty()) {
                std::cout << "Circuit input: File" << std::endl;
                // Open Input WAV
                SF_INFO sfInfo;
                sfInfo.format = 0;
                SNDFILE* file = sf_open(input_file.c_str(), SFM_READ, &sfInfo);
                
                if (!file) {
                    std::cerr << "Errore apertura WAV: " << sf_strerror(file) << std::endl;
                    return false;
                }
                
                std::cout << "Input File Format" << std::endl;
                printFileFormat(input_file);
                
                if (sample_rate) sample_rate = sfInfo.samplerate;
                
                // Leggi tutti i sample
                std::vector<double> buffer(sfInfo.frames * sfInfo.channels);
                sf_count_t numFrames = sf_readf_double(file, buffer.data(), sfInfo.frames);
                sf_close(file);
                
                if (numFrames != sfInfo.frames) {
                    std::cerr << "Errore lettura sample" << std::endl;
                    return false;
                }
                
                // Estrai canale sinistro
                signalIn.resize(numFrames, 0.0f);
                for (sf_count_t i = 0; i < numFrames; i++) {
                    signalIn[i] = buffer[i * sfInfo.channels];
                }

                // Calcola e rimuovi DC offset 
                for (double s : signalIn) mean += s;
                mean /= signalIn.size();

                for (double& s : signalIn) s -= mean;

                // Calcola maxNormalized e Normalizza in Volt
                for (double s : signalIn) {
                    maxNormalized = std::max(maxNormalized, std::abs(s));
                }
                
                if (maxNormalized > 1e-10) {
                    scale = input_amplitude / maxNormalized;
                }
            } else if (frequency_sweep) {
                size_t total_samples = static_cast<size_t>(sample_rate * input_duration);
                signalIn.resize(total_samples, 0.0f);
                int f_start = 1;
                int f_end = sample_rate / 2.0;
                if (true) {
                    // Log Sweep
                    double log_f_start = std::log(f_start);
                    double log_f_end = std::log(f_end);
                    double k = (log_f_end - log_f_start) / input_duration;
                    
                    for (size_t i = 0; i < total_samples; ++i) {
                        double t = i / sample_rate;
                        // Fase per sweep logaritmico: integrale di f(t) = f_start * exp(k*t)
                        double phase = 2.0 * M_PI * f_start * (std::exp(k * t) - 1.0) / k;
                        signalIn[i] = input_amplitude * std::sin(phase);
                    }
                    std::cout << "Logarithmic sweep: " << f_start << " Hz -> " << f_end << " Hz" << std::endl;
                } else {
                    // Lin Sweep
                    double k = (f_end - f_start) / input_duration;
                    for (size_t i = 0; i < total_samples; ++i) {
                        double t = i / sample_rate;
                        // Fase per sweep lineare: integrale di f(t) = f_start + k*t
                        double phase = 2.0 * M_PI * (f_start * t + 0.5 * k * t * t);
                        signalIn[i] = input_amplitude * std::sin(phase);
                    }
                    std::cout << "Linear sweep: " << f_start << " Hz -> " << f_end << " Hz" << std::endl;
                }
                
                // DC offset
                mean = 0;
                 // maxNormalized
                maxNormalized = input_amplitude;

            } else if (input_frequency > 0) {
                std::cout << "Circuit input: Sinusoid" << std::endl;
                size_t total_samples = static_cast<size_t>(sample_rate * input_duration);
                signalIn.resize(total_samples, 0.0f);
                for (size_t i = 0; i < total_samples; ++i) {
                    double t = i / sample_rate;
                    signalIn[i] = input_amplitude * std::sin(2.0 * M_PI * input_frequency * t);
                }
                // DC offset
                mean = 0;
                // maxNormalized
                maxNormalized = input_amplitude;

            } else {
                std::cout << "Circuit input: DC" << std::endl;
                size_t total_samples = static_cast<size_t>(sample_rate * input_duration);
                signalIn.resize(total_samples, 0.0f);
                for (size_t i = 0; i < total_samples; ++i) {
                    signalIn[i] = input_amplitude;
                }
                // Calcola DC offset
                mean = input_amplitude;
                 // maxNormalized
                maxNormalized = input_amplitude;
            }
            std::cout << std::endl;
            
            for (double& s : signalIn) s *= scale;
            
            std::unique_ptr<TranCircuitSolver> tran_solver = std::make_unique<TranCircuitSolver>(circuit, sample_rate, source_impedance, max_iterations, tolerance);

            std::unique_ptr<TransientSolver> transient_solver = std::make_unique<TransientSolver>(circuit, sample_rate, source_impedance, max_iterations, tolerance);

            if (analysis_type == "TRAN") {
                if (!bypass) {
                    tran_solver->initialize();
                    std::cout << "Circuit initialized with this Operating Point" << std::endl;
                    tran_solver->printDCOperatingPoint();
                }
            } else if (analysis_type == "TEST") {
                if (!bypass) {
                    transient_solver->initialize();
                    std::cout << "Circuit initialized with this Operating Point" << std::endl;
                    transient_solver->printDCOperatingPoint();
                }
            }
            
            std::vector<double> signalOut(signalIn.size());
            
            double peak_in = 0.0, peak_out = 0.0;
            double rms_in = 0.0, rms_out = 0.0;
            
            auto start = std::chrono::high_resolution_clock::now();
    
            for (size_t i = 0; i < signalIn.size(); i++) {
                if (analysis_type == "TRAN") {
                    if (!bypass) {
                        signalOut[i] = 0;
                        tran_solver->setInputVoltage(signalIn[i]);
                        if (tran_solver->solve()) {
                            signalOut[i] = tran_solver->getOutputVoltage();
                        }
                    } else {
                        signalOut[i] = signalIn[i];
                    }
                 
                } else if (analysis_type == "TEST") {
                    if (!bypass) {
                        signalOut[i] = 0;
                        transient_solver->setInputVoltage(signalIn[i]);
                        if (transient_solver->solve()) {
                            signalOut[i] = transient_solver->getOutputVoltage();
                        }
                    } else {
                        signalOut[i] = signalIn[i];
                    }
                    
                }
                    
                
                // Update statistics
                peak_in = std::max(peak_in, std::abs(signalIn[i]));
                peak_out = std::max(peak_out, std::abs(signalOut[i]));
                rms_in += signalIn[i] * signalIn[i];
                rms_out += signalOut[i] * signalOut[i];
            }
            
            auto end = std::chrono::high_resolution_clock::now();
            
            rms_in = std::sqrt(rms_in / signalIn.size());
            rms_out = std::sqrt(rms_out / signalIn.size());
            
            double outputPeak = 0.0;
            for (double v : signalOut) {
                outputPeak = std::max(outputPeak, std::abs(v));
            }
            if (analysis_type == "TRAN") {
                std::cout << "Simulation ended with this Operating Point" << std::endl;
            tran_solver->printDCOperatingPoint();
            
            // Print statistics
            std::cout << "Audio Statistics:" << std::endl;
            std::cout << "  Mean Input Signal " << mean << std::endl;
            std::cout << "  Max Normalized " << maxNormalized << " V, Scale Factor " << scale << std::endl;
            std::cout << "  Input Peak: " << peak_in << " V, " << 20 * std::log10(peak_in) << " dBFS, RMS: " << 20 * std::log10(rms_in) << " dBFS" << std::endl;
            std::cout << "  Output Peak: " << peak_out << " V, " << 20 * std::log10(peak_out) << " dBFS, RMS: " << 20 * std::log10(rms_out) << " dBFS" << std::endl;
            std::cout << "  Circuit gain: " << 20 * std::log10(rms_out / rms_in) << " dB" << std::endl;
            std::cout << std::endl;
            
            std::cout << "Process Statistics:" << std::endl;
            std::cout << "  Solver's Execution Time: " << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() << " us" << std::endl;
            std::cout << "  Solver's Failure Percentage: " << tran_solver->getFailurePercentage() << " %" << std::endl;
            std::cout << "  Solver's Total Samples: " << tran_solver->getTotalSamples() << std::endl;
            std::cout << "  Solver's Total Iterations: " << tran_solver->getTotalIterations() << std::endl;
            std::cout << "  Solver's Mean Iterations: " << tran_solver->getMeanIterations() << std::endl;
            std::cout << std::endl;
                
            } else if (analysis_type == "TEST") {
                
                std::cout << "Simulation ended with this Operating Point" << std::endl;
            transient_solver->printDCOperatingPoint();
            
            // Print statistics
            std::cout << "Audio Statistics:" << std::endl;
            std::cout << "  Mean Input Signal " << mean << std::endl;
            std::cout << "  Max Normalized " << maxNormalized << " V, Scale Factor " << scale << std::endl;
            std::cout << "  Input Peak: " << peak_in << " V, " << 20 * std::log10(peak_in) << " dBFS, RMS: " << 20 * std::log10(rms_in) << " dBFS" << std::endl;
            std::cout << "  Output Peak: " << peak_out << " V, " << 20 * std::log10(peak_out) << " dBFS, RMS: " << 20 * std::log10(rms_out) << " dBFS" << std::endl;
            std::cout << "  Circuit gain: " << 20 * std::log10(rms_out / rms_in) << " dB" << std::endl;
            std::cout << std::endl;
            
            std::cout << "Process Statistics:" << std::endl;
            std::cout << "  Solver's Execution Time: " << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() << " us" << std::endl;
            std::cout << "  Solver's Failure Percentage: " << transient_solver->getFailurePercentage() << " %" << std::endl;
            std::cout << "  Solver's Total Samples: " << transient_solver->getTotalSamples() << std::endl;
            std::cout << "  Solver's Total Iterations: " << transient_solver->getTotalIterations() << std::endl;
            std::cout << "  Solver's Mean Iterations: " << transient_solver->getMeanIterations() << std::endl;
            std::cout << std::endl;
            }
            
            
            if (!output_file.empty()) {
                writeWav(signalOut, output_file, sample_rate);
            }
            return true;
        } else {
            std::cerr << "Analysis Type not valid: " << analysis_type << std::endl;
            return false;
        }
    }

    bool writeWav(std::vector<double> signalOut,
              const std::string& output_file,
              int sample_rate,
              int bitDepth = 24) {
        
        // Configura WAV
        SF_INFO sfInfo;
        sfInfo.samplerate = sample_rate;
        sfInfo.channels = 1;
        
        switch (bitDepth) {
            case 16: sfInfo.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16; break;
            case 24: sfInfo.format = SF_FORMAT_WAV | SF_FORMAT_PCM_24; break;
            case 32: sfInfo.format = SF_FORMAT_WAV | SF_FORMAT_FLOAT; break;
            default:
                std::cerr << "Bit depth non supportato" << std::endl;
                return false;
        }
        
        if (!sf_format_check(&sfInfo)) {
            std::cerr << "Formato WAV invalido" << std::endl;
            return false;
        }
        
        // Scrivi WAV
        SNDFILE* file = sf_open(output_file.c_str(), SFM_WRITE, &sfInfo);
        if (!file) {
            std::cerr << "Errore apertura WAV: " << sf_strerror(file) << std::endl;
            return false;
        }
        
        sf_count_t written = sf_writef_double(file, signalOut.data(), signalOut.size());
        sf_close(file);
        
        if (written != (sf_count_t)signalOut.size()) {
            std::cerr << "Errore scrittura WAV" << std::endl;
            return false;
        }
        
        std::cout << "Output File Format" << std::endl;
        std::cout << "   File Name: " << output_file << std::endl;
        std::cout << "   Duration: " << (float)signalOut.size() / sample_rate << "s" << std::endl;
        
        printFileFormat(output_file);
        
        return true;
    }

    void printFileFormat(const std::string &file) {
        SF_INFO sfInfo;
        sfInfo.format = 0;
        SNDFILE* sound_file = sf_open(file.c_str(), SFM_READ, &sfInfo);
        sf_close(sound_file);
        std::cout << "   Channels: " << sfInfo.channels << std::endl;
        std::cout << "   Sample Rate: " << sfInfo.samplerate << "Hz" << std::endl;
        std::cout << "   Frames: " << sfInfo.frames << std::endl;
        std::cout << "   Numeric Format (bitmask): 0x" << std::hex << sfInfo.format << std::dec << std::endl;

        // Decodifica del formato audio (opzionale)
        int major_format = sfInfo.format & SF_FORMAT_TYPEMASK;
        int subtype = sfInfo.format & SF_FORMAT_SUBMASK;

        std::cout << "   Container Format: ";
        switch (major_format) {
            case SF_FORMAT_WAV:      std::cout << "WAV"; break;
            case SF_FORMAT_AIFF:     std::cout << "AIFF"; break;
            case SF_FORMAT_FLAC:     std::cout << "FLAC"; break;
            default:                 std::cout << "Other (" << std::hex << major_format << std::dec << ")"; break;
        }
        std::cout << std::endl;

        std::cout << "   Subtype (PCM, float, etc.): ";
        switch (subtype) {
            case SF_FORMAT_PCM_16:   std::cout << "PCM 16-bit"; break;
            case SF_FORMAT_PCM_24:   std::cout << "PCM 24-bit"; break;
            case SF_FORMAT_PCM_32:   std::cout << "PCM 32-bit"; break;
            case SF_FORMAT_FLOAT:    std::cout << "Float"; break;
            case SF_FORMAT_DOUBLE:   std::cout << "Double"; break;
            default:                 std::cout << "Other (" << std::hex << subtype << std::dec << ")"; break;
        }
        std::cout << std::endl;

        std::cout << std::endl;
    }

};

int main(int argc, char *argv[]) {
    CLI::App app { "SpicePedal: a realtime simple spice-like simulator for audio" };
    
    std::string analysis_type = "TRAN";
    std::string input_file;
    int input_frequency = 0;
    double input_duration = 2.0;
    double input_amplitude = 0.15;
    bool frequency_sweep = false;
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
    app.add_flag("-F,--frequency-sweep", frequency_sweep, "Frequency Sweep")->default_val(frequency_sweep);
    app.add_option("-I,--source_impedance", source_impedance, "Source Impedance")->check(CLI::Range(0, 30000))->default_val(source_impedance);
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
    std::cout << "   Frequency Sweep: " << (frequency_sweep ? "True" : "False") << std::endl;
    std::cout << "   Sample Rate: " << sample_rate << "Hz" << std::endl;
    std::cout << "   Output File: " << output_file << std::endl;
    std::cout << "   Circuit File: " << netlist_file << std::endl;
    std::cout << "   Bypass Circuit: " << (bypass ? "True" : "False") << std::endl;
    std::cout << "   Max Iterations: " << max_iterations << std::endl;
    std::cout << "   Tolerance: " << tolerance << std::endl;
    std::cout << std::endl;

    try {
        SpicePedalProcessor processor(analysis_type, netlist_file, sample_rate, input_frequency, input_duration, input_amplitude, frequency_sweep, source_impedance, bypass, max_iterations, tolerance);
        if (!processor.process(input_file, output_file)) {
            return 1;
        }
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
    
}
