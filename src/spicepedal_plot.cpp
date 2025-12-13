#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>
#include <cstdlib>
#include <limits>
#include <unistd.h>
#include <climits>
#include <sys/ioctl.h>
#include <fftw3.h>
#include <cmath>
#include <vector>
#include <complex>

#include "external/CLI11.hpp"
#include "external/httplib.h"

struct PlotData
{
    std::string title;
    std::string filename;
    std::string separator;
    std::vector<std::vector<double>> data;
    std::vector<std::string> column_names;
    std::string type;
    
    PlotData() = default;
    
    PlotData(std::string title, std::string filename, std::string separator, const std::vector<std::vector<double>>& data, const std::vector<std::string>& column_names, std::string type)
        : title(title),
          filename(filename),
          separator(separator),
          data(data),
          column_names(column_names),
          type(type) {
            
        }
};

class CSVPlotter
{
private:
    std::string output_file;
    std::string output_format;
    
    double x_min;
    double x_max;
    double y_min;
    double y_max;
    bool auto_x;
    bool auto_y;
    
    int width;
    int height;
    
    void getTerminalSize(int& cols, int& rows)
    {
        struct winsize w;
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
            cols = w.ws_col;
            rows = w.ws_row;
        } else {
            cols = 80;
            rows = 24;
        }
    }
    
public:
    CSVPlotter(const std::string& output_file,
               const std::string& output_format,
               double x_min,
               double x_max,
               double y_min,
               double y_max,
               int width,
               int height
               )
        : output_file(output_file),
          output_format(output_format),
          x_min(x_min),
          x_max(x_max),
          y_min(y_min),
          y_max(y_max),
          width(width),
          height(height)
    {
        auto_x = (x_min == std::numeric_limits<double>::lowest() && x_max == std::numeric_limits<double>::max());
        auto_y = (y_min == std::numeric_limits<double>::lowest() && y_max == std::numeric_limits<double>::max());

    }

    std::unique_ptr<PlotData> loadCSV(std::string filename, std::string separator)
    {
        std::vector<std::vector<double>> data;
        std::vector<std::string> column_names;
  
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Errore apertura file: " << filename << std::endl;
            return nullptr;
        }

        std::string line;
        
        if (std::getline(file, line)) {
            std::stringstream ss(line);
            std::string column;
            
            while (std::getline(ss, column, separator[0])) {
                column.erase(std::remove_if(column.begin(), column.end(), ::isspace), column.end());
                column_names.push_back(column);
            }
            
            data.resize(column_names.size());
        }

        while (std::getline(file, line)) {
            std::stringstream ss(line);
            std::string value;
            size_t col_idx = 0;
            
            while (std::getline(ss, value, separator[0]) && col_idx < data.size()) {
                try {
                    data[col_idx].push_back(std::stod(value));
                } catch (const std::exception& e) {
                    std::cerr << "Error: value conversion " << value << std::endl;
                    return nullptr;
                }
                col_idx++;
            }
        }

        file.close();
        
        std::cout << "CSV caricato con successo" << std::endl;
        std::cout << "   Colonne: " << column_names.size() << std::endl;
        std::cout << "   Righe: " << (data.empty() ? 0 : data[0].size()) << std::endl;
        std::cout << std::endl;
        
        return std::make_unique<PlotData>(filename, filename, separator, data, column_names, "lin");
    }

    std::unique_ptr<PlotData> computeFrequencyResponse(const PlotData& plotData)
    {
        size_t input_col = 1;
        size_t output_col = 2;
        if (plotData.data.empty() || plotData.data.size() <= std::max(input_col, output_col)) {
            std::cerr << "Invalid input/output column indices" << std::endl;
            return nullptr;
        }
        if (plotData.data.size() != 3) {
            std::cerr << "Error: computeFrequencyResponse requires exactly 3 columns (time, input, output), "
                  << "but found " << plotData.data.size() << " columns." << std::endl;
            std::cerr << "This function compares two signals to compute H(f) = Output(f) / Input(f)." << std::endl;
            return nullptr;
        }
        size_t N = plotData.data[input_col].size();
        if (N < 2) return nullptr;
    
        // Calcola FFT di input e output
        size_t out_N = N / 2 + 1;
    
        double* in_signal = fftw_alloc_real(N);
        double* out_signal = fftw_alloc_real(N);
        fftw_complex* fft_in = fftw_alloc_complex(out_N);
        fftw_complex* fft_out = fftw_alloc_complex(out_N);
    
        fftw_plan plan_in = fftw_plan_dft_r2c_1d(N, in_signal, fft_in, FFTW_ESTIMATE);
        fftw_plan plan_out = fftw_plan_dft_r2c_1d(N, out_signal, fft_out, FFTW_ESTIMATE);
        
        // Copia dati
        for (size_t i = 0; i < N; ++i) {
            in_signal[i] = plotData.data[input_col][i];
            out_signal[i] = plotData.data[output_col][i];
        }
        
        // Esegui FFT
        fftw_execute(plan_in);
        fftw_execute(plan_out);
        
        // Calcola frequenze
        double dt = plotData.data[0][1] - plotData.data[0][0];
        double fs = 1.0 / dt;
        
        // Prepara output
        std::vector<std::vector<double>> response(3); // freq, magnitude_dB, phase
        response[0].reserve(out_N);
        response[1].reserve(out_N);
        response[2].reserve(out_N);
        
        for (size_t i = 0; i < out_N; ++i) {
            double freq = i * fs / N;
            
            // Complessi
            std::complex<double> H_in(fft_in[i][0], fft_in[i][1]);
            std::complex<double> H_out(fft_out[i][0], fft_out[i][1]);
            
            // Funzione di trasferimento H(f) = Out(f) / In(f)
            std::complex<double> H = H_out / (H_in + 1e-20); // evita divisione per zero
            
            double magnitude = std::abs(H);
            double magnitude_dB = 20.0 * std::log10(magnitude + 1e-20);
            double phase_deg = std::arg(H) * 180.0 / M_PI;
            
            response[0].push_back(freq);
            response[1].push_back(magnitude_dB);
            response[2].push_back(phase_deg);
        }
        
        // Cleanup
        fftw_destroy_plan(plan_in);
        fftw_destroy_plan(plan_out);
        fftw_free(in_signal);
        fftw_free(out_signal);
        fftw_free(fft_in);
        fftw_free(fft_out);
        
        std::vector<std::string> column_names(3);
        column_names[0] = "Frequency (Hz)";
        column_names[1] = "Magnitude (dB)";
        column_names[2] = "Phase (deg)";
        
        return std::make_unique<PlotData>(plotData.title + " (Frequency Response Analysis)", plotData.filename, plotData.separator, response, column_names, "log");
    }
    
    std::unique_ptr<PlotData> convertInFrequencyDomain(const PlotData& plotData)
    {
        if (plotData.data.empty() || plotData.data[0].size() < 2) {
            std::cerr << "Error: insufficient data to perform FFT analysis" << std::endl;
            return nullptr;
        }

        size_t num_cols = plotData.data.size();
        size_t N = plotData.data[0].size();
        
        // 1. Calcolo frequenza di campionamento (Fs)
        // Assumiamo passo costante come richiesto
        double dt = plotData.data[0][1] - plotData.data[0][0];
        if (dt <= 0) dt = 1e-9; // Prevenzione divisione per zero
        double fs = 1.0 / dt;

        // FFTW Real-to-Complex produce N/2 + 1 output (spettro simmetrico)
        size_t out_N = N / 2 + 1;

        // Struttura dati di output (stesso formato di 'data')
        std::vector<std::vector<double>> fft_data(num_cols);

        // 2. Preparazione risorse FFTW
        // Allocazione buffer (riutilizziamo lo stesso buffer per tutte le colonne per risparmiare RAM)
        double* in = fftw_alloc_real(N);
        fftw_complex* out = fftw_alloc_complex(out_N);

        // Creiamo il plan una volta sola (FFTW_ESTIMATE è veloce da inizializzare)
        fftw_plan plan = fftw_plan_dft_r2c_1d(N, in, out, FFTW_ESTIMATE);

        // 3. Generazione Asse Frequenze (Indice 0)
        fft_data[0].reserve(out_N);
        for (size_t i = 0; i < out_N; ++i) {
            double freq = i * fs / N;
            fft_data[0].push_back(freq);
        }

        // 4. Elaborazione Segnali (Indici 1...N)
        for (size_t col = 1; col < num_cols; ++col) {
            fft_data[col].reserve(out_N);

            // Copia i dati dal vettore std::vector al buffer C di input
            // (FFTW distrugge l'input durante l'esecuzione con certi flag, quindi meglio ricaricarlo)
            for (size_t i = 0; i < N; ++i) {
                in[i] = plotData.data[col][i];
            }

            // Esegui la FFT
            fftw_execute(plan);

            // Calcola Magnitudo e normalizza
            for (size_t i = 0; i < out_N; ++i) {
                double real = out[i][0];
                double imag = out[i][1];
                
                // Modulo del numero complesso
                double mag = std::sqrt(real * real + imag * imag);
                
                // Normalizzazione matematica standard per segnali discreti
                mag = mag / N;

                // Moltiplichiamo per 2 per recuperare l'energia delle frequenze negative
                // (che abbiamo scartato usando r2c), tranne per la componente DC (i=0)
                if (i > 0) {
                    mag *= 2.0;
                }

                fft_data[col].push_back(mag);
            }
        }
        
        fftw_destroy_plan(plan);
        fftw_free(in);
        fftw_free(out);
        
        std::vector<std::string> column_names(num_cols);
        column_names[0] = "Frequency (Hz)";
        for (size_t col = 1; col < num_cols; ++col) {
            if (col < plotData.column_names.size() && !plotData.column_names[col].empty()) {
                column_names[col] = "FFT " + plotData.column_names[col];
            } else {
                column_names[col] = "FFT Col" + std::to_string(col);
            }
        }
        
        return std::make_unique<PlotData>(plotData.title + " (Fast Fourier Transform)", plotData.filename, plotData.separator, fft_data, column_names, "log");
    }
    
    std::string generatePlotlyHTML(const PlotData& plotData)
    {
        std::stringstream html;
        
        html << R"(<!DOCTYPE html>
<html lang="it">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=yes">
    <title>)" << plotData.title << R"(</title>
    <script src="https://cdn.plot.ly/plotly-2.27.0.min.js"></script>
    <style>
        body {
            margin: 0;
            padding: 10px;
            font-family: Arial, sans-serif;
            background: #f5f5f5;
        }
        #plot {
            width: 100%;
            height: calc(100vh - 20px);
            background: white;
            border-radius: 8px;
            box-shadow: 0 2px 8px rgba(0,0,0,0.1);
        }
    </style>
</head>
<body>
    <div id="plot"></div>
    <script>
        var traces = [];
)";
        const std::vector<double>& time = plotData.data[0];
        
        for (size_t i = 1; i < plotData.data.size(); i++) {
            html << "        traces.push({\n";
            html << "            x: [";
            for (size_t j = 0; j < time.size(); j++) {
                if (j > 0) html << ",";
                html << time[j];
            }
            html << "],\n";
            
            html << "            y: [";
            for (size_t j = 0; j < plotData.data[i].size(); j++) {
                if (j > 0) html << ",";
                html << plotData.data[i][j];
            }
            html << "],\n";
            
            html << "            mode: 'lines',\n";
            html << "            name: '" << plotData.column_names[i] << "',\n";
            html << "            line: { width: 2 }\n";
            html << "        });\n";
        }

        html << R"(
        var layout = {
            title: {
                text: ')" << plotData.title << R"(',
                font: { size: 20 }
            },
            xaxis: {
                title: ')" << plotData.column_names[0] << R"(',
                gridcolor: '#e0e0e0',
                showgrid: true,
                type: ')" << plotData.type << "',\n";
        if (!auto_x) {
            html << "                range: [" << x_min << ", " << x_max << "],\n";
        }
        html << R"(            },
            yaxis: {
                title: 'Value',
                gridcolor: '#e0e0e0',
                showgrid: true,
)";
        if (!auto_y) {
            html << "                range: [" << y_min << ", " << y_max << "],\n";
        }
        html << R"(            },
            hovermode: 'closest',
            showlegend: true,
            legend: {
                x: 1.02,
                y: 1,
                xanchor: 'left',
                bgcolor: 'rgba(255,255,255,0.8)',
                bordercolor: '#ddd',
                borderwidth: 1
            },
            margin: { l: 60, r: 150, t: 60, b: 60 },
            plot_bgcolor: 'white',
            paper_bgcolor: '#f5f5f5'
        };
        )";
        html << R"(
        var config = {
            responsive: true,
            displayModeBar: true,
            modeBarButtonsToRemove: ['select2d', 'lasso2d'],
            displaylogo: false,
            toImageButtonOptions: {
                format: 'png',
                filename: 'plot',
                height: 1080,
                width: 1920,
                scale: 2
            },
            scrollZoom: true
        };
        )";
        html << R"(
        Plotly.newPlot('plot', traces, layout, config);
        
        window.addEventListener('resize', function() {
            Plotly.Plots.resize('plot');
        }
        );
    </script>
</body>
</html>
)";

        return html.str();
    }

    bool plotWithGnuplot(const PlotData& plotData)
    {
        if (plotData.data.empty() || plotData.column_names.empty()) {
            std::cerr << "Error: no data to plot" << std::endl;
            return false;
        }

        std::string script_file = "plot_script.gnu";
        std::ofstream script(script_file);
        
        if (!script.is_open()) {
            std::cerr << "Error: cannot create Gnuplot script" << std::endl;
            return false;
        }

        if (output_format == "html") {
            script << "set terminal canvas size " << width << "," << height << " standalone enhanced mousing jsdir 'https://gnuplot.sourceforge.io/demo_canvas_5.4/'\n";
            script << "set output '" << output_file << "'" << std::endl;
        } else if (output_format == "svg") {
            script << "set terminal svg size " << width << "," << height << " dynamic enhanced font 'Arial,12'\n";
            script << "set output '" << output_file << "'" << std::endl;
        } else if (output_format == "pdf") {
            // PDF usa dimensioni in pollici
            double w_inches = width / 100.0;
            double h_inches = height / 100.0;
            script << "set terminal pdfcairo size " << w_inches << "," << h_inches << " enhanced font 'Arial,12'\n";
            script << "set output '" << output_file << "'\n";
        } else if (output_format == "tikz") {
            script << "set terminal tikz standalone size " << (width/100.0) << "," << (height/100.0) << "\n";
            script << "set output '" << output_file << "'\n";
        } else if (output_format == "eps") {
            script << "set terminal postscript eps enhanced color size " << (width/100.0) << "," << (height/100.0) << "\n";
            script << "set output '" << output_file << "'\n";
        } else if (output_format == "ascii") {
            int term_cols, term_rows;
            getTerminalSize(term_cols, term_rows);
            width = term_cols - 2;
            if (height == 0) {
                height = std::min(30, term_rows - 10);
            }
            script << "set terminal dumb size " << width << "," << height << "\n";
        } else {
            script << "set terminal pngcairo size " << width << "," << height << " enhanced font 'Arial,10'\n";
            script << "set output '" << output_file << "'\n";
        }
        
        script << "set title '" << plotData.title << "'\n";
        script << "set xlabel 'Time [s]'\n";
        script << "set ylabel 'Value'\n";
        script << "set grid\n";
        
        if (output_format != "ascii") {
            script << "set key outside right top\n";
        } else {
            script << "set key below\n";
        }
        
        script << "set datafile separator '" << plotData.separator << "'\n";
        
        if (!auto_x) {
            script << "set xrange [" << x_min << ":" << x_max << "]\n";
            std::cout << "X Range: [" << x_min << ", " << x_max << "]" << std::endl;
        } else {
            std::cout << "X Range: Auto" << std::endl;
        }
        if (!auto_y) {
            script << "set yrange [" << y_min << ":" << y_max << "]\n";
            std::cout << "Y Range: [" << y_min << ", " << y_max << "]" << std::endl;
        } else {
            std::cout << "Y Range: Auto" << std::endl;
        }
        std::cout << std::endl;
        
        // Plot comando
        script << "plot ";
        for (size_t i = 1; i < plotData.column_names.size(); i++) {
            if (i > 1) script << ", ";
            script << "'" << plotData.filename << "' using 1:" << (i + 1) 
                   << " with lines title '" << plotData.column_names[i] << "'";
        }
        script << "\n";
        
        script.close();
        
        std::string command = "gnuplot " + script_file;
        
        if (output_format == "ascii") {
            int result = system(command.c_str());
            
            if (result != 0) {
                std::cerr << "Error: executing Gnuplot" << std::endl;
                return false;
            }
        } else {
            command += " 2>/dev/null";
            int result = system(command.c_str());
            
            if (result == 0) {
                std::cout << "Plot saved: " << output_file << std::endl;
                std::cout << std::endl;
            } else {
                std::cerr << "Error: executing Gnuplot" << std::endl;
                return false;
            }
        }
        
        return true;
    }
    
    std::string get_file_extension(const std::string& filename) {
        std::filesystem::path p(filename);
        std::string ext = p.extension().string();
        if (!ext.empty() && ext[0] == '.') {
            ext = ext.substr(1);
        }
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        return ext;
    }
    
};

int main(int argc, char* argv[])
{
    CLI::App app{"SpicePedal Plot: .probe file visualizer"};
   
    std::string filename;
    std::string separator = ";";
    std::string output_file;
    std::string output_format;
    
    double x_min = std::numeric_limits<double>::lowest();
    double x_max = std::numeric_limits<double>::max();
    double y_min = std::numeric_limits<double>::lowest();
    double y_max = std::numeric_limits<double>::max();
    
    bool server_mode = false;
    int server_port = 8080;
    
    int width = 800;
    int height = 600;

    bool fft = false;
    bool fra = false;
    
    app.add_option("-i,--input-file", filename, "Input File")
        ->required()
        ->check(CLI::ExistingFile);
    app.add_option("-s,--separator", separator, "Field Separator")
        ->default_val(separator);
    
    app.add_option("--xmin", x_min, "Minimum X-axis value");
    app.add_option("--xmax", x_max, "Maximum X-axis value");
    app.add_option("--ymin", y_min, "Minimum Y-axis value");
    app.add_option("--ymax", y_max, "Maximum Y-axis value");
    
    auto* output_file_opt = app.add_option("-o,--output-file", output_file, "Output File");
    auto* output_format_opt = app.add_option("-f,--format", output_format, "Output Format: png, html, svg, pdf, eps, tex, ascii")
        ->check(CLI::IsMember({"png", "html", "svg", "pdf", "eps", "tex", "ascii"}));
    output_format_opt->needs(output_file_opt);
    output_file_opt->needs(output_format_opt);
    
    app.add_option("--width", width, "Width")
        ->default_val(width);
    app.add_option("--height", height, "Height")
        ->default_val(height);
    
    auto* server_mode_opt = app.add_flag("-d,--server-mode", server_mode, "HTTP Server Mode")
        ->default_val(server_mode);
    
    auto* server_port_opt = app.add_option("-p,--server-port", server_port, "HTTP Server Port")
        ->default_val(server_port)
        ->check(CLI::Range(1024, 65535));

    server_port_opt->needs(server_mode_opt);
    
    auto* fft_opt = app.add_flag("--fft", fft, "Fast Fourier Transform")
        ->default_val(fft);
    auto* fra_opt = app.add_flag("--fra", fra, "Frequency Response Analysis (1=in, 2=out)")
        ->default_val(fra);
    fft_opt->excludes(fra_opt);
    fra_opt->excludes(fft_opt);
    
    CLI11_PARSE(app, argc, argv);
    
    std::cout << "Input Parameters:" << std::endl;
    std::cout << "   Input File: " << filename << std::endl;
    std::cout << "   Separator: " << separator << std::endl;
    std::cout << "   Output File: " << output_file << std::endl;
    std::cout << "   Formato: " << output_format << std::endl;
    std::cout << "   Dimensioni: " << width << "x" << height << std::endl;
    std::cout << "   Port: " << server_port << std::endl;
    std::cout << std::endl;

    try {
        CSVPlotter plotter(output_file, output_format,
                          x_min, x_max, y_min, y_max,
                          width, height
                          );
        
        std::unique_ptr<PlotData> plotData = plotter.loadCSV(filename, separator);
        if (!plotData) {
            return 1;
        }
        if (fft) {
            plotData = plotter.convertInFrequencyDomain(*plotData);
        } else if (fra) {
            plotData = plotter.computeFrequencyResponse(*plotData);
        }
        if (!plotData) {
            return 1;
        }
        if (server_mode) {
            httplib::Server svr;
            
            svr.Get("/", [&](const httplib::Request& req, httplib::Response& res) {
                std::string html = plotter.generatePlotlyHTML(*plotData);
                res.set_content(html, "text/html; charset=utf-8");
            });
            
            std::cout << "Server started on port " << server_port << std::endl;
            std::cout << std::endl;
            
            if (!svr.listen("0.0.0.0", server_port)) {
                std::cerr << "Error starting server on port " << server_port << std::endl;
                return 1;
            }
        } else if (!plotter.plotWithGnuplot(*plotData)) {
            return 1;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}