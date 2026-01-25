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

#include "circuit.h"
#include "solvers/realtime_solver.h"

// =============================================================
// Funzioni per input non bloccante su Linux
// =============================================================
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

// =============================================================
// Classe streaming con controllo live
// =============================================================
class SpicePedalStreamingProcessor {
private:
    Circuit circuit;
    std::unique_ptr<RealTimeSolver> solver;
    double sample_rate;
    double input_gain;
    int source_impedance;
    int buffer_size;
    std::map<int, std::atomic<float>> paramIds;
    int currentParamIndex = 0;

public:
    SpicePedalStreamingProcessor(const std::string& netlist_file,
                          double sample_rate,
                          double input_gain,
                          int source_impedance,
                          int max_iterations,
                          double tolerance,
                          int buffer_size
                          )
        : sample_rate(sample_rate),
          input_gain(input_gain),
          source_impedance(source_impedance),
          buffer_size(buffer_size)
    {
        if (!circuit.loadNetlist(netlist_file)) {
            throw std::runtime_error("Failed to load netlist");
        }

        solver = std::make_unique<RealTimeSolver>(circuit, sample_rate, max_iterations, tolerance);

        initializeParameters();
    }

    void initializeParameters() {
        std::vector<int> ids = circuit.getCtrlParameterIds();
        for (int id : ids) {
            float defaultValue = circuit.getCtrlParamValue(id);
            paramIds[id].store(defaultValue);
        }
    }

    bool handleKeyPress() {
        if (paramIds.empty()) return true;
        if (kbhit()) {
            int c = getch();
            if (c == 'q') { // Quit
                return false;
            } else {
                float value;
                if (c == 27) {
                    if (!kbhit()) return true;  
                    int c2 = getch();
                    if (c2 != '[') return true;

                    if (!kbhit()) return true;
                    int c3 = getch();

                    switch (c3) {
                        case 'A':  // ↑
                            value = std::min(1.0f, paramIds[currentParamIndex].load() + 0.05f);
                            paramIds[currentParamIndex].store(value);
                            circuit.setCtrlParamValue(currentParamIndex, value);
                        break;

                        case 'B':  // ↓
                            value = std::max(0.0f, paramIds[currentParamIndex].load() - 0.05f);
                            paramIds[currentParamIndex].store(value);
                            circuit.setCtrlParamValue(currentParamIndex, value);
                        break;

                        case 'D':  // ←
                            currentParamIndex = (currentParamIndex - 1 + paramIds.size()) % paramIds.size();
                        break;

                        case 'C':  // →
                            currentParamIndex = (currentParamIndex + 1) % paramIds.size();
                        break;
                    }

                    return true; 
                }
            }
        }
        return true;
    }

    void printControls() {
        std::cout << "╔══════════════════════════════════════════════════╗" << std::endl;
        std::cout << "║                 Live Controls                    ║" << std::endl;
        std::cout << "╠══════════════════════════════════════════════════╣" << std::endl;
        std::cout << "║  ↑ : Decrease Current Parameter                  ║" << std::endl;
        std::cout << "║  ↓ : Decrease Current Parameter                  ║" << std::endl;
        std::cout << "║  ← : Previous Parameter                          ║" << std::endl;
        std::cout << "║  → : Next Parameter                              ║" << std::endl;
        std::cout << "║  q : Quit                                        ║" << std::endl;
        std::cout << "╚══════════════════════════════════════════════════╝" << std::endl;
        std::cout << std::endl;
    }

    bool processAndPlay_try2(const std::string& input_file) {
        using clock = std::chrono::high_resolution_clock;
        double worstBufferLatencyMs = 0.0;
        auto lastReportTime = clock::now();
        
        SF_INFO sfinfo;
        std::memset(&sfinfo, 0, sizeof(sfinfo));
        SNDFILE* infile = sf_open(input_file.c_str(), SFM_READ, &sfinfo);
        if (!infile) return false;
        
        double hardware_sr = 44100.0;
        double solver_step_ratio = hardware_sr / sample_rate;
        double accumulator = 0.0;
        float last_vout = 0.0f;
        
        Pa_Initialize();
        PaStream* stream;
        Pa_OpenDefaultStream(&stream, 0, sfinfo.channels, paFloat32, hardware_sr, buffer_size, nullptr, nullptr);
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
        
        while (running) {
            sf_count_t readcount = sf_readf_float(infile, buffer.data(), buffer_size);
            if (readcount == 0) {
                sf_seek(infile, 0, SEEK_SET);
                continue;
            }
            
            auto bufferStart = clock::now();
            
            for (size_t frame = 0; frame < (size_t)readcount; ++frame) {
                size_t idx = frame * sfinfo.channels;
                
                accumulator += 1.0;
                if (accumulator >= solver_step_ratio) {
                    float vin = buffer[idx] * input_gain;
                    solver->setInputVoltage(vin);
                    if (solver->solve()) {
                        last_vout = solver->getOutputVoltage();
                    }
                    last_vout = std::isfinite(last_vout) ? last_vout : 0.0f;
                    accumulator -= solver_step_ratio;
                }
                
                for (int ch = 0; ch < sfinfo.channels; ++ch) {
                    buffer[idx + ch] = last_vout;
                }
            }
            
            auto bufferEnd = clock::now();
            double bufferTimeMs = std::chrono::duration<double, std::milli>(bufferEnd - bufferStart).count();
            
            auto now = clock::now();
            if (std::chrono::duration<double, std::milli>(now - lastReportTime).count() > 1000.0) {
                std::cout << "Compute: " << bufferTimeMs << " ms | Solver SR: " << sample_rate << " Hz | Hardware SR: " << hardware_sr << " Hz" << std::endl;
                lastReportTime = now;
            }
            
            Pa_WriteStream(stream, buffer.data(), readcount);
        }
        
        running = false;
        if (inputThread.joinable()) inputThread.join();
        setNonBlocking(false);
        Pa_StopStream(stream);
        Pa_CloseStream(stream);
        Pa_Terminate();
        sf_close(infile);
        return true;
    }

    bool processAndPlay_try(const std::string& input_file) {
        using clock = std::chrono::high_resolution_clock;
        double worstBufferLatencyMs = 0.0;
        auto lastReportTime = clock::now();

        SF_INFO sfinfo;
        std::memset(&sfinfo, 0, sizeof(sfinfo));

        SNDFILE* infile = sf_open(input_file.c_str(), SFM_READ, &sfinfo);
        if (!infile) {
            std::cerr << "Errore apertura file di input: " << sf_strerror(nullptr) << "\n";
            return false;
        }

        double resampleRatio = sample_rate / static_cast<double>(sfinfo.samplerate);
        bool needResampling = (std::abs(sfinfo.samplerate - sample_rate) > 0.1);

        PaError err = Pa_Initialize();
        if (err != paNoError) {
            std::cerr << "Errore PortAudio: " << Pa_GetErrorText(err) << "\n";
            sf_close(infile);
            return false;
        }
        
        PaStream* stream;
        err = Pa_OpenDefaultStream(&stream, 0, sfinfo.channels, paFloat32, 
                                   sample_rate, buffer_size, nullptr, nullptr);
        if (err != paNoError) { 
            Pa_Terminate(); 
            sf_close(infile); 
            return false; 
        }
        Pa_StartStream(stream);

        std::vector<float> readBuffer(buffer_size * sfinfo.channels);
        std::vector<float> processingBuffer;

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
            readcount = sf_readf_float(infile, readBuffer.data(), buffer_size);

            if (readcount == 0) {
                sf_seek(infile, 0, SEEK_SET);
                continue;
            }

            auto bufferStart = clock::now();

            float* currentDataPtr;
            long framesToProcess;

            if (needResampling) {
                long max_out_frames = static_cast<long>(readcount * resampleRatio) + 2;
                processingBuffer.resize(max_out_frames * sfinfo.channels);

                SRC_DATA src_data;
                src_data.data_in = readBuffer.data();
                src_data.data_out = processingBuffer.data();
                src_data.input_frames = readcount;
                src_data.output_frames = max_out_frames;
                src_data.src_ratio = resampleRatio;

                int src_err = src_simple(&src_data, SRC_SINC_FASTEST, sfinfo.channels);
                if (src_err) {
                    std::cerr << "Resampling error: " << src_strerror(src_err) << "\n";
                    break;
                }
                
                framesToProcess = src_data.output_frames_gen;
                currentDataPtr = processingBuffer.data();
            } else {
                framesToProcess = readcount;
                currentDataPtr = readBuffer.data();
            }

            for (size_t frame = 0; frame < static_cast<size_t>(framesToProcess); ++frame) {
                size_t baseIdx = frame * sfinfo.channels;
                float vin = currentDataPtr[baseIdx] * input_gain;
                float vout = 0.0f;
                
                solver->setInputVoltage(vin);
                if (solver->solve()) {
                    vout = solver->getOutputVoltage();
                }
                
                vout = std::isfinite(vout) ? vout : 0.0f;
                for (int ch = 0; ch < sfinfo.channels; ++ch) {
                    currentDataPtr[baseIdx + ch] = vout;
                }
            }

            auto bufferEnd = clock::now();
            double bufferTimeMs = std::chrono::duration<double, std::milli>(bufferEnd - bufferStart).count();
            double bufferDurationMs = (static_cast<double>(framesToProcess) / sample_rate) * 1000.0;
            double perceivedLatencyMs = bufferTimeMs + bufferDurationMs;

            if (perceivedLatencyMs > worstBufferLatencyMs)
                worstBufferLatencyMs = perceivedLatencyMs;
                
            auto now = clock::now();
            double elapsedMs = std::chrono::duration<double, std::milli>(now - lastReportTime).count();

            if (elapsedMs > 1000.0) {
                std::cout << "BufTime: " << bufferTimeMs << " ms | "
                          << "Latency: " << perceivedLatencyMs << " ms | "
                          << "Worst: " << worstBufferLatencyMs << " ms       " 
                          << std::endl << std::flush;
                lastReportTime = now;
            }

            err = Pa_WriteStream(stream, currentDataPtr, framesToProcess);
            if (err != paNoError) {
                std::cerr << "\nErrore Pa_WriteStream: " << Pa_GetErrorText(err) << "\n";
                break;
            }
        }

        running = false;
        if (inputThread.joinable()) inputThread.join();
        setNonBlocking(false);

        Pa_StopStream(stream);
        Pa_CloseStream(stream);
        Pa_Terminate();
        sf_close(infile);

        std::cout << "\n✓ Riproduzione terminata\n";
        return true;
    }

    bool processAndPlay(const std::string& input_file) {
        double hardware_sr = 44100.0; // O una costante standard
        double solver_step_ratio = hardware_sr / sample_rate;
        double accumulator = 0.0;
        float last_vout = 0.0f;

        using clock = std::chrono::high_resolution_clock;
        double worstBufferLatencyMs = 0.0;
        
        auto lastReportTime = clock::now();

        SF_INFO sfinfo;
        std::memset(&sfinfo, 0, sizeof(sfinfo));

        SNDFILE* infile = sf_open(input_file.c_str(), SFM_READ, &sfinfo);
        if (!infile) {
            std::cerr << "Errore apertura file di input: " << sf_strerror(nullptr) << "\n";
            return false;
        }

        PaError err = Pa_Initialize();
        if (err != paNoError) {
            std::cerr << "Errore PortAudio: " << Pa_GetErrorText(err) << "\n";
            sf_close(infile);
            return false;
        }
        
        PaStream* stream;
        err = Pa_OpenDefaultStream(&stream, 0, sfinfo.channels, paFloat32, 
                                   hardware_sr, buffer_size, nullptr, nullptr);
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

            auto bufferStart = clock::now();

            // OTTIMIZZAZIONE: processa per frame invece che per campione
            size_t numSamples = readcount * sfinfo.channels;
            
            // Logica universale: processa il primo canale di ogni frame e replica su tutti gli altri
            for (size_t frame = 0; frame < static_cast<size_t>(readcount); ++frame) {
                size_t baseIdx = frame * sfinfo.channels;
                accumulator += 1.0;
                if (accumulator >= solver_step_ratio) {
                    float vin = buffer[baseIdx] * input_gain;
                    solver->setInputVoltage(vin);
                    
                    if (solver->solve()) {
                        last_vout = solver->getOutputVoltage();
                    }
                    last_vout = std::isfinite(last_vout) ? last_vout : 0.0f;
                    accumulator -= solver_step_ratio;
                }
                // Replica last_vout su tutti i canali del frame (1 se mono, 2 se stereo, ecc.)
                for (int ch = 0; ch < sfinfo.channels; ++ch) {
                    buffer[baseIdx + ch] = last_vout;
                }
            }
            
            auto bufferEnd = clock::now();
            double bufferTimeMs = std::chrono::duration<double, std::milli>(bufferEnd - bufferStart).count();
            double bufferDurationMs = (static_cast<double>(readcount) / hardware_sr) * 1000.0;
            double perceivedLatencyMs = bufferTimeMs + bufferDurationMs;

            if (perceivedLatencyMs > worstBufferLatencyMs)
                worstBufferLatencyMs = perceivedLatencyMs;
                
            auto now = clock::now();
            double elapsedMs = std::chrono::duration<double, std::milli>(now - lastReportTime).count();

            if (elapsedMs > 1000.0) {
                std::cout << "BufTime: " << bufferTimeMs << " ms | "
                          << "BufDuration: " << bufferDurationMs << " ms | "
                          << "Latency: " << perceivedLatencyMs << " ms | "
                          << "Worst: " << worstBufferLatencyMs << " ms       " 
                          << std::endl << std::flush;
                lastReportTime = now;
            }

            err = Pa_WriteStream(stream, buffer.data(), readcount);
            if (err != paNoError) {
                std::cerr << "\nErrore Pa_WriteStream: " << Pa_GetErrorText(err) << "\n";
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

        std::cout << "\n✓ Riproduzione terminata\n";
        return true;
    }
};

// =============================================================
// MAIN
// =============================================================
int main(int argc, char* argv[]) {
    CLI::App app{"SpicePedal: a realtime simple spice-like simulator for audio"};
    
    std::string input_file;
    std::string netlist_file;
    double input_gain = 1.0;
    int source_impedance = 25000;
    int sample_rate = 44100;
    int max_iterations;
    double tolerance;
    int buffer_size = 128;
    
    app.add_option("-i,--input", input_file, "Input File")
        ->check(CLI::ExistingFile);
    app.add_option("-c,--circuit", netlist_file, "Netlist File")
        ->check(CLI::ExistingFile)
        ->required();
    app.add_option("-I,--source_impedance", source_impedance, "Source Impedance")
        ->check(CLI::Range(0, 30000))
        ->default_val(25000);
    app.add_option("-s,--sample-rate", sample_rate, "Sample Rate");
    app.add_option("-g,--input-gain", input_gain, "Input Gain")
        ->check(CLI::Range(0.0, 5.0))
        ->default_val(1.0);
    app.add_option("-m,--max-iterations", max_iterations, "Max solver's iterations")
        ->default_val(50);
    app.add_option("-t,--tolerance", tolerance, "Solver's tolerance")
        ->default_val(1e-8);
    app.add_option("-b,--buffer-size", buffer_size, "Buffer Size")
        ->check(CLI::Range(32, 131072))
        ->default_val(128);
    
    CLI11_PARSE(app, argc, argv);
    
    try {
        /*SF_INFO sf_info;
        std::memset(&sf_info, 0, sizeof(sf_info));
        SNDFILE* tmp = sf_open(input_file.c_str(), SFM_READ, &sf_info);
        if (!tmp) {
            std::cerr << "Impossibile aprire il file di input: " << input_file << "\n";
            return 1;
        }
        double sample_rate = sf_info.samplerate;
        sf_close(tmp);
        */
        
        SpicePedalStreamingProcessor processor(netlist_file, sample_rate, input_gain, source_impedance, max_iterations, tolerance, buffer_size);
        if (!processor.processAndPlay(input_file))
            return 1;

    } catch (const std::exception& e) {
        std::cerr << "Errore: " << e.what() << "\n";
        return 1;
    }

    return 0;
}