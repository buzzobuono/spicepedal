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
#include <atomic>
#include <map>
#include <chrono>
#include <signal.h>
#include <sndfile.h>
#include <portaudio.h>

#include "external/CLI11.hpp"

#include "utils/debug.h"
#include "circuit.h"
#include "solvers/realtime_solver.h"

std::atomic<bool> global_running{true};

void signal_handler(int sig) {
    global_running.store(false);
}

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

    SNDFILE* infile = nullptr;
    SF_INFO sfinfo;

    std::vector<float> input_buf;

    double sample_rate;
    double input_gain;
    double output_gain;
    bool clipping;

    int buffer_size;

    float last_vout = 0.0f;

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
        : input_gain(std::pow(10.0, input_gain_db / 20.0)),
          output_gain(std::pow(10.0, output_gain_db / 20.0)),
          clipping(clipping),
          buffer_size(buffer_size)
    {
        std::memset(&sfinfo, 0, sizeof(sfinfo));
        infile = sf_open(input_file.c_str(), SFM_READ, &sfinfo);
        if (!infile) throw std::runtime_error("Could not open WAV file");

        if (!circuit.loadNetlist(netlist_file)) {
            throw std::runtime_error("Failed to load netlist");
        }

        sample_rate = sfinfo.samplerate;

        solver = std::make_unique<RealTimeSolver>(
            circuit, sample_rate, max_iterations, tolerance
        );
        solver->initialize();

        input_buf.resize(buffer_size * sfinfo.channels);
    }

    ~SpicePedalStreamingProcessor() {
        if (infile) sf_close(infile);
    }

    bool process(PaStream* stream, std::vector<float>& pa_buffer) {
        auto processStart = std::chrono::high_resolution_clock::now();

        const unsigned long nframes = buffer_size;

        sf_count_t readcount = sf_readf_float(
            infile,
            input_buf.data(),
            nframes
        );

        if (readcount == 0) {
            sf_seek(infile, 0, SEEK_SET);
            readcount = sf_readf_float(infile, input_buf.data(), nframes);
            if (readcount == 0) return false;
        }

        for (unsigned long i = 0; i < nframes; i++) {
            float vin = 0.0f;
            if (i < (unsigned long)readcount) {
                vin = input_buf[i * sfinfo.channels] * input_gain;
            }
            solver->setInputVoltage(vin);

            if (solver->solve()) {
                last_vout = output_gain * solver->getOutputVoltage();
            }

            if (!std::isfinite(last_vout)) last_vout = 0.0f;
            if (clipping) last_vout = std::tanh(last_vout);

            for (int ch = 0; ch < sfinfo.channels; ++ch)
                pa_buffer[i * sfinfo.channels + ch] = last_vout;
        }

        PaError err = Pa_WriteStream(stream, pa_buffer.data(), nframes);
        if (err != paNoError) return false;

        auto processEnd = std::chrono::high_resolution_clock::now();
        double cpuExecutionTime = std::chrono::duration<double, std::milli>(processEnd - processStart).count();
        double bufferDeadline = (static_cast<double>(nframes) / sample_rate) * 1000.0;
        double cpuLoadPercentage = (cpuExecutionTime / bufferDeadline) * 100.0;
        stats.cpuExecutionTime.store(cpuExecutionTime);
        stats.bufferDeadline.store(bufferDeadline);
        stats.cpuLoadPercentage.store(cpuLoadPercentage);
        if (cpuExecutionTime > stats.peakCpuTime.load()) stats.peakCpuTime.store(stats.cpuExecutionTime);
        return 0;
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

    void start() {
        PaError err = Pa_Initialize();
        if (err != paNoError) {
            std::cerr << "Errore PortAudio: " << Pa_GetErrorText(err) << "\n";
            sf_close(infile);
            return;
        }

        PaStream* stream;
        err = Pa_OpenDefaultStream(
            &stream,
            0,
            sfinfo.channels,
            paFloat32,
            sfinfo.samplerate,
            buffer_size,
            nullptr,
            nullptr
        );

        if (err != paNoError) {
            Pa_Terminate();
            sf_close(infile);
            return;
        }

        Pa_StartStream(stream);

        std::vector<float> buffer(buffer_size * sfinfo.channels);

        setNonBlocking(true);
        printControls();

        auto lastReport = std::chrono::high_resolution_clock::now();

        while (global_running.load()) {
            if (!handleKeyPress()) global_running.store(false);
            if (!process(stream, buffer)) {
                global_running.store(false);
                break;
            }

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
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        setNonBlocking(false);

        Pa_StopStream(stream);
        Pa_CloseStream(stream);
        Pa_Terminate();
        sf_close(infile);

        std::cout << "Riproduzione terminata\n";
    }

};

int main(int argc, char* argv[]) {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    CLI::App app{"SpicePedal: a realtime simple spice-like simulator for audio"};
    
    std::string input_file, netlist_file;
    double input_gain_db = 0.0, output_gain_db = 0.0, tolerance = 1e-8;
    int max_iterations = 50;
    bool clipping = false;
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
        processor.start();
    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
