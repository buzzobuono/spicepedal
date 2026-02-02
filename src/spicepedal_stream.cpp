#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <thread>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <mutex>
#include <chrono>
#include <sndfile.h>
#include <portaudio.h>
#include <samplerate.h>

#include "external/CLI11.hpp"

#include "utils/debug.h"
#include "circuit.h"
#include "solvers/realtime_solver.h"

void setNonBlocking(bool enable) {
    struct termios ttystate;
    tcgetattr(STDIN_FILENO, &ttystate);
    if (enable) {
        ttystate.c_lflag &= ~ICANON;
        ttystate.c_lflag &= ~ECHO;
        fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
    } else {
        ttystate.c_lflag |= ICANON;
        ttystate.c_lflag |= ECHO;
        fcntl(STDIN_FILENO, F_SETFL, 0);
    }
    tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);
}

int kbhit() {
    char ch;
    int nread = read(STDIN_FILENO, &ch, 1);
    if (nread == 1) {
        ungetc(ch, stdin);
        return 1;
    }
    return 0;
}

int getch() {
    return getchar();
}

class SpicePedalStreamingProcessor {
private:
    Circuit circuit;
    std::unique_ptr<RealTimeSolver> solver;
    
    const std::string& netlist_file;
    const std::string& input_file;
    int max_iterations;
    double tolerance;
    double sample_rate;
    double input_gain;
    double output_gain;
    bool clipping;
    
    int buffer_size;

    struct Stats {
        std::atomic<double> cpuExecutionTime{0.0};
        std::atomic<double> bufferDeadline{0.0};
        std::atomic<double> cpuLoadPercentage{0.0};
        std::atomic<double> peakCpuTime{0.0};
    } stats;
    
public:
    SpicePedalStreamingProcessor(const std::string& netlist_file,
                          const std::string& input_file,
                          double input_gain_db,
                          double output_gain_db,
                          bool clipping,
                          int max_iterations,
                          double tolerance,
                          int buffer_size)
        : netlist_file(netlist_file),
          input_file(input_file),
          input_gain(std::pow(10.0, input_gain_db / 20.0)),
          output_gain(std::pow(10.0, output_gain_db / 20.0)),
          clipping(clipping),
          max_iterations(max_iterations),
          tolerance(tolerance),
          buffer_size(buffer_size)
    {
        
    }
    
    bool handleKeyPress() {
        if (circuit.getCtrlParameterIds().empty()) return true;
        if (kbhit()) {
            int c = getch();
            if (c == 'q') return false;
            if (c == 27) {
                if (!kbhit()) return true;
                getch(); 
                if (!kbhit()) return true;
                int c3 = getch();
                float val;
                switch (c3) {
                    case 'A':
                        circuit.incrementCtrlParamValue();
                        break;
                    case 'B':
                        circuit.decrementCtrlParamValue();
                        break;
                    case 'D':
                        circuit.previousCtrlParam();
                        break;
                    case 'C':
                        circuit.nextCtrlParam();
                        break;
                }
            }
        }
        return true;
    }
    
    void printControls() {
        std::cout << "╔══════════════════════════════════════════════════╗" << std::endl;
        std::cout << "║          SpicePedal Controls                     ║" << std::endl;
        std::cout << "╠══════════════════════════════════════════════════╣" << std::endl;
        std::cout << "║  ↑ / ↓ : Adjust Parameter                        ║" << std::endl;
        std::cout << "║  ← / → : Select Parameter                        ║" << std::endl;
        std::cout << "║  q     : Quit                                    ║" << std::endl;
        std::cout << "╚══════════════════════════════════════════════════╝" << std::endl;
    }
    
    bool processAndPlay() {
        if (!circuit.loadNetlist(netlist_file)) {
            throw std::runtime_error("Failed to load netlist");
        }
        
        float last_vout = 0.0f;

        using clock = std::chrono::high_resolution_clock;
        double peakCpuTime = 0.0;
        
        auto lastReport = clock::now();

        SF_INFO sfinfo;
        std::memset(&sfinfo, 0, sizeof(sfinfo));
        
        SNDFILE* infile = sf_open(input_file.c_str(), SFM_READ, &sfinfo);
        if (!infile) {
            std::cerr << "Errore apertura file di input: " << sf_strerror(nullptr) << "\n";
            return false;
        }

        sample_rate = sfinfo.samplerate;
        
        solver = std::make_unique<RealTimeSolver>(circuit, sample_rate, max_iterations, tolerance);

        PaError err = Pa_Initialize();
        if (err != paNoError) {
            std::cerr << "Errore PortAudio: " << Pa_GetErrorText(err) << "\n";
            sf_close(infile);
            return false;
        }
        
        PaStream* stream;
        err = Pa_OpenDefaultStream(&stream, 0, sfinfo.channels, paFloat32, 
                                   sfinfo.samplerate, buffer_size, nullptr, nullptr);
        if (err != paNoError) { 
            Pa_Terminate(); 
            sf_close(infile); 
            return false; 
        }
        Pa_StartStream(stream);

        std::vector<float> buffer(buffer_size * sfinfo.channels);

        solver->initialize();
        
        bool running = true;
        setNonBlocking(true);
        
        printControls();
        
        std::thread inputThread([&]() {
            while (running) {
                running = handleKeyPress();
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });

        sf_count_t readcount;
        while (running) {
            readcount = sf_readf_float(infile, buffer.data(), buffer_size);

            if (readcount == 0) {
                sf_seek(infile, 0, SEEK_SET);
                continue;
            }

            auto processStart = clock::now();

            // OTTIMIZZAZIONE: processa per frame invece che per campione
            size_t numSamples = readcount * sfinfo.channels;
            
            // Logica universale: processa il primo canale di ogni frame e replica su tutti gli altri
            for (size_t frame = 0; frame < static_cast<size_t>(readcount); ++frame) {
                size_t baseIdx = frame * sfinfo.channels;
                float vin = buffer[baseIdx] * input_gain;
                solver->setInputVoltage(vin);
                    
                if (solver->solve()) {
                    last_vout = output_gain * solver->getOutputVoltage();
                }
                last_vout = std::isfinite(last_vout) ? last_vout : 0.0f;
                if (clipping) {
                    last_vout = std::tanh(last_vout);
                }
                
                // Replica last_vout su tutti i canali del frame (1 se mono, 2 se stereo, ecc.)
                for (int ch = 0; ch < sfinfo.channels; ++ch) {
                    buffer[baseIdx + ch] = last_vout;
                }
            }
            
            auto processEnd = std::chrono::high_resolution_clock::now();
            double cpuExecutionTime = std::chrono::duration<double, std::milli>(processEnd - processStart).count();
            double bufferDeadline = (static_cast<double>(readcount) / sample_rate) * 1000.0;
            double cpuLoadPercentage = (cpuExecutionTime / bufferDeadline) * 100.0;
            stats.cpuExecutionTime.store(cpuExecutionTime);
            stats.bufferDeadline.store(bufferDeadline);
            stats.cpuLoadPercentage.store(cpuLoadPercentage);
            if (cpuExecutionTime > stats.peakCpuTime) stats.peakCpuTime.store(stats.cpuExecutionTime);
            auto now = std::chrono::high_resolution_clock::now();
            if (std::chrono::duration<double>(now - lastReport).count() > 1.0) {
                DEBUG_LOG(
                "CPU: " << stats.cpuExecutionTime.load() << " ms,"
                << " Deadline: " << stats.bufferDeadline.load() << " ms,"
                << " Load: " << stats.cpuLoadPercentage.load() << " %,"
                << " Peak: " << stats.peakCpuTime.load() << " ms"
                );
                
                lastReport = now;
            }

            err = Pa_WriteStream(stream, buffer.data(), readcount);
            if (err != paNoError) {
                std::cerr << "Errore Pa_WriteStream: " << Pa_GetErrorText(err) << std::endl;
                break;
            }
        }

        running = false;
        inputThread.join();
        setNonBlocking(false);

        Pa_StopStream(stream);
        Pa_CloseStream(stream);
        Pa_Terminate();
        sf_close(infile);

        std::cout << "Riproduzione terminata" << std::endl;
        return true;
    }
};

int main(int argc, char* argv[]) {
    CLI::App app{"SpicePedal: a realtime simple spice-like simulator for audio"};
    
    std::string input_file;
    std::string netlist_file;
    double input_gain_db = 0.0;
    double output_gain_db = 0.0;
    bool clipping = false;
    int max_iterations;
    double tolerance;
    int buffer_size = 128;
    
    app.add_option("-i,--input", input_file, "Input File")->check(CLI::ExistingFile);
    app.add_option("-c,--circuit", netlist_file, "Netlist File")->check(CLI::ExistingFile)->required();
    app.add_option("--ig,--input-gain", input_gain_db, "Input Gain in dB")->default_val(0.0);
    app.add_option("--og,--output-gain", output_gain_db, "Output Gain in dB")->default_val(0.0);
    app.add_flag("--cl,--clipping", clipping, "Soft Output Clipping")->default_val(clipping);
    app.add_option("-m,--max-iterations", max_iterations, "Max solver's iterations")->default_val(50);
    app.add_option("-t,--tolerance", tolerance, "Solver's tolerance")->default_val(1e-8);
    app.add_option("-b,--buffer-size", buffer_size, "Buffer Size")->check(CLI::Range(32, 131072))->default_val(128);
    
    CLI11_PARSE(app, argc, argv);
    
    try {
        SpicePedalStreamingProcessor processor(netlist_file, input_file, input_gain_db, output_gain_db, clipping, max_iterations, tolerance, buffer_size);
        if (!processor.processAndPlay())
            return 1;

    } catch (const std::exception& e) {
        std::cerr << "Errore: " << e.what() << "\n";
        return 1;
    }

    return 0;
}