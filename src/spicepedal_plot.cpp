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

#include "external/CLI11.hpp"
#include "external/httplib.h"

class CSVPlotter
{
private:
    std::string filename;
    std::string separator;
    std::string title;
    std::string output_file;
    std::string output_format;
    
    double x_min;
    double x_max;
    double y_min;
    double y_max;
    bool auto_x_range;
    bool auto_y_range;
    
    bool interactive;
    bool ascii_output;
    
    int width;
    int height;
    
    std::vector<std::vector<double>> data;
    std::vector<std::string> column_names;

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
    
    std::string detectFormatFromFilename(const std::string& filename)
    {
        size_t dot_pos = filename.find_last_of(".");
        if (dot_pos == std::string::npos) return "png";
        
        std::string ext = filename.substr(dot_pos + 1);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        
        if (ext == "html" || ext == "htm") return "html";
        if (ext == "svg") return "svg";
        if (ext == "pdf") return "pdf";
        if (ext == "png") return "png";
        if (ext == "eps") return "eps";
        if (ext == "tex") return "tikz";
        
        return "png"; // default
    }

public:
    CSVPlotter(const std::string& file,
               const std::string& sep,
               const std::string& output,
               const std::string& format,
               double xmin,
               double xmax,
               double ymin,
               double ymax,
               bool inter,
               bool ascii,
               int width,
               int height
               )
        : filename(file),
          separator(sep),
          output_file(output),
          output_format(format),
          x_min(xmin),
          x_max(xmax),
          y_min(ymin),
          y_max(ymax),
          interactive(inter),
          ascii_output(ascii),
          width(width),
          height(height)
    {
        auto_x_range = (x_min == std::numeric_limits<double>::lowest() && 
                        x_max == std::numeric_limits<double>::max());
        auto_y_range = (y_min == std::numeric_limits<double>::lowest() && 
                        y_max == std::numeric_limits<double>::max());
        
        // Auto-detect formato da estensione se non specificato
        if (output_format == "auto") {
            output_format = detectFormatFromFilename(output_file);
            std::cout << "Formato rilevato da estensione: " << output_format << std::endl;
        }
        
        // Auto-detect dimensioni terminale per ASCII
        if (ascii_output && ascii_width == 0) {
            int term_cols, term_rows;
            getTerminalSize(term_cols, term_rows);
            ascii_width = term_cols - 2;
            if (ascii_height == 0) {
                ascii_height = std::min(30, term_rows - 10);
            }
            std::cout << "Dimensioni terminale rilevate: " << term_cols << "x" << term_rows << std::endl;
            std::cout << "Usando dimensioni ASCII: " << ascii_width << "x" << ascii_height << std::endl;
        }
        
        std::cout << std::endl;
    }

    bool loadCSV()
    {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Errore apertura file: " << filename << std::endl;
            return false;
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
                    std::cerr << "Errore conversione valore: " << value << std::endl;
                    return false;
                }
                col_idx++;
            }
        }

        file.close();
        
        std::cout << "CSV caricato con successo" << std::endl;
        std::cout << "   Colonne: " << column_names.size() << std::endl;
        std::cout << "   Righe: " << (data.empty() ? 0 : data[0].size()) << std::endl;
        std::cout << std::endl;
        
        return true;
    }

    std::string generatePlotlyHTML(double xmin, double xmax, double ymin, double ymax,
                                   bool auto_x, bool auto_y)
    {
        std::stringstream html;
        
        html << R"(<!DOCTYPE html>
<html lang="it">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=yes">
    <title>)" << filename << R"(</title>
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

        const std::vector<double>& time = data[0];
        
        for (size_t i = 1; i < data.size(); i++) {
            html << "        traces.push({\n";
            html << "            x: [";
            for (size_t j = 0; j < time.size(); j++) {
                if (j > 0) html << ",";
                html << time[j];
            }
            html << "],\n";
            
            html << "            y: [";
            for (size_t j = 0; j < data[i].size(); j++) {
                if (j > 0) html << ",";
                html << data[i][j];
            }
            html << "],\n";
            
            html << "            mode: 'lines',\n";
            html << "            name: '" << column_names[i] << "',\n";
            html << "            line: { width: 2 }\n";
            html << "        });\n";
        }

        html << R"(
        var layout = {
            title: {
                text: ')" << filename << R"(',
                font: { size: 20 }
            },
            xaxis: {
                title: 'Time [s]',
                gridcolor: '#e0e0e0',
                showgrid: true,
)";
        if (!auto_x) {
            html << "                range: [" << xmin << ", " << xmax << "],\n";
        }
        html << R"(            },
            yaxis: {
                title: 'Value',
                gridcolor: '#e0e0e0',
                showgrid: true,
)";
        if (!auto_y) {
            html << "                range: [" << ymin << ", " << ymax << "],\n";
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
        
        Plotly.newPlot('plot', traces, layout, config);
        
        window.addEventListener('resize', function() {
            Plotly.Plots.resize('plot');
        });
    </script>
</body>
</html>
)";

        return html.str();
    }

    bool plotWithGnuplot()
    {
        if (data.empty() || column_names.empty()) {
            std::cerr << "Nessun dato da plottare" << std::endl;
            return false;
        }

        std::string script_file = "plot_script.gnu";
        std::ofstream script(script_file);
        
        if (!script.is_open()) {
            std::cerr << "Errore creazione script Gnuplot" << std::endl;
            return false;
        }

        // Configura terminale in base alle opzioni
        if (ascii_output) {
            script << "set terminal dumb size " << width << "," << height << "\n";
            std::cout << "Modalità: ASCII Terminal (" << width << "x" << height << ")" << std::endl;
        } else if (interactive) {
            script << "set terminal qt persist\n";
            std::cout << "Modalità: Interattiva (Qt Window)" << std::endl;
            std::cout << "Controlli finestra:" << std::endl;
            std::cout << "   - Click destro + trascina: Zoom" << std::endl;
            std::cout << "   - Shift + trascina: Pan" << std::endl;
            std::cout << "   - Tasto 'a': Autoscale" << std::endl;
            std::cout << "   - Tasto 'r': Reset view" << std::endl;
            std::cout << std::endl;
        } else {
            // Configurazione formato output
            if (output_format == "html") {
                script << "set terminal canvas size " << width << "," << height 
                       << " standalone enhanced mousing jsdir 'https://gnuplot.sourceforge.io/demo_canvas_5.4/'\n";
                script << "set output '" << output_file << "'\n";
                std::cout << "Modalità: HTML Canvas (Interattivo nel browser)" << std::endl;
                std::cout << "Features: zoom, pan, toggle serie, tooltip" << std::endl;
            } else if (output_format == "svg") {
                script << "set terminal svg size " << width << "," << height 
                       << " dynamic enhanced font 'Arial,12'\n";
                script << "set output '" << output_file << "'\n";
                std::cout << "Modalità: SVG (Vettoriale, interattivo)" << std::endl;
            } else if (output_format == "pdf") {
                // PDF usa dimensioni in pollici
                double w_inches = width / 100.0;
                double h_inches = height / 100.0;
                script << "set terminal pdfcairo size " << w_inches << "," << h_inches 
                       << " enhanced font 'Arial,12'\n";
                script << "set output '" << output_file << "'\n";
                std::cout << "Modalità: PDF (Alta qualità)" << std::endl;
            } else if (output_format == "tikz") {
                script << "set terminal tikz standalone size " << (width/100.0) << "," << (height/100.0) << "\n";
                script << "set output '" << output_file << "'\n";
                std::cout << "Modalità: LaTeX/TikZ" << std::endl;
            } else if (output_format == "eps") {
                script << "set terminal postscript eps enhanced color size " 
                       << (width/100.0) << "," << (height/100.0) << "\n";
                script << "set output '" << output_file << "'\n";
                std::cout << "Modalità: EPS (PostScript)" << std::endl;
            } else {
                // PNG default (cairo per alta qualità)
                script << "set terminal pngcairo size " << width << "," << height 
                       << " enhanced font 'Arial,10'\n";
                script << "set output '" << output_file << "'\n";
                std::cout << "Modalità: PNG" << std::endl;
            }
        }
        
        script << "set title '" << filename << " - " << filename << "'\n";
        script << "set xlabel 'Time [s]'\n";
        script << "set ylabel 'Value'\n";
        script << "set grid\n";
        
        if (!ascii_output) {
            script << "set key outside right top\n";
        } else {
            script << "set key below\n";
        }
        
        script << "set datafile separator '" << separator << "'\n";
        
        if (!auto_x_range) {
            script << "set xrange [" << x_min << ":" << x_max << "]\n";
            std::cout << "X Range: [" << x_min << ", " << x_max << "]" << std::endl;
        } else {
            std::cout << "X Range: Auto" << std::endl;
        }
        
        if (!auto_y_range) {
            script << "set yrange [" << y_min << ":" << y_max << "]\n";
            std::cout << "Y Range: [" << y_min << ", " << y_max << "]" << std::endl;
        } else {
            std::cout << "Y Range: Auto" << std::endl;
        }
        std::cout << std::endl;
        
        // Plot comando
        script << "plot ";
        for (size_t i = 1; i < column_names.size(); i++) {
            if (i > 1) script << ", ";
            script << "'" << filename << "' using 1:" << (i + 1) 
                   << " with lines title '" << column_names[i] << "'";
        }
        script << "\n";
        
        if (interactive) {
            script << "pause mouse close\n";
        }
        
        script.close();
        
        std::cout << "Script Gnuplot creato: " << script_file << std::endl;
        
        // Esegui Gnuplot
        std::string command = "gnuplot " + script_file;
        
        if (ascii_output || interactive) {
            int result = system(command.c_str());
            
            if (result != 0) {
                std::cerr << "Errore esecuzione Gnuplot." << std::endl;
                return false;
            }
        } else {
            command += " 2>/dev/null";
            int result = system(command.c_str());
            
            if (result == 0) {
                std::cout << "Grafico salvato con successo: " << output_file << std::endl;
                
                if (output_format == "html") {
                    std::cout << std::endl;
                    std::cout << "Apri " << output_file << " nel browser per vedere il grafico interattivo!" << std::endl;
                    std::cout << "Features disponibili:" << std::endl;
                    std::cout << "   - Mouse wheel: Zoom in/out" << std::endl;
                    std::cout << "   - Click + drag: Pan" << std::endl;
                    std::cout << "   - Click su legenda: Toggle serie on/off" << std::endl;
                    std::cout << "   - Hover: Mostra valori" << std::endl;
                }
                
                std::cout << std::endl;
                
            } else {
                std::cerr << "Errore esecuzione Gnuplot." << std::endl;
                return false;
            }
        }
        
        if (!interactive) {
            remove(script_file.c_str());
        }
        
        return true;
    }
};

int main(int argc, char* argv[])
{
    CLI::App app{"SpicePedal Plot: a probe file plot visualiser"};
   
    std::string filename;
    std::string separator = ";";
    std::string output_file;
    std::string output_format = "auto";
    
    double x_min = std::numeric_limits<double>::lowest();
    double x_max = std::numeric_limits<double>::max();
    double y_min = std::numeric_limits<double>::lowest();
    double y_max = std::numeric_limits<double>::max();
    
    bool interactive = false;
    bool ascii_output = false;
    bool server_mode = false;
    int server_port = 8080;
    
    int width = 0;
    int height = 0;

    app.add_option("-i,--input-file", filename, "Input File")
        ->required()
        ->check(CLI::ExistingFile);
    app.add_option("-s,--separator", separator, "Field Separator")
        ->default_val(separator);
    app.add_option("--xmin", x_min, "Minimum X-axis value");
    app.add_option("--xmax", x_max, "Maximum X-axis value");
    app.add_option("--ymin", y_min, "Minimum Y-axis value");
    app.add_option("--ymax", y_max, "Maximum Y-axis value");
    app.add_option("-o,--output-file", output_file, "Output File");
    app.add_option("--format", output_format, "Output Format: auto, png, html, svg, pdf, eps, tikz")
        ->default_val(output_format)
        ->check(CLI::IsMember({"auto", "png", "html", "svg", "pdf", "eps", "tikz"}));
    app.add_option("--width", width, "Width (pixel fir png/html/svg, cols for ascii)")
        ->default_val(width);
    app.add_option("--height", height, "Height output (pixel for png/html/svg, rows for ascii)")
        ->default_val(height);
    app.add_flag("-w,--interactive", interactive, "Modalità interattiva con finestra Qt (solo Linux)")
        ->default_val(interactive);
    app.add_flag("-s,--server-mode", server_mode, "HTTP Server Mode")
        ->default_val(server_mode);
    app.add_option("-p,--server-port", server_port, "HTTP Server Port")
        ->default_val(server_port)
        ->check(CLI::Range(1024, 65535));
    app.add_flag("-a,--ascii", ascii_output, "Mostra grafico ASCII nel terminale")
        ->default_val(ascii_output);
    app.add_option("--ascii-width", ascii_width, "Larghezza grafico ASCII (0=auto)")
        ->default_val(ascii_width)
        ->check(CLI::Range(0, 500));
    app.add_option("--ascii-height", ascii_height, "Altezza grafico ASCII (0=auto)")
        ->default_val(ascii_height)
        ->check(CLI::Range(0, 100));

    CLI11_PARSE(app, argc, argv);

    if (interactive && ascii_output) {
        std::cerr << "Errore: non puoi usare --interactive e --ascii insieme" << std::endl;
        return 1;
    }
    
    std::cout << "Input Parameters:" << std::endl;
    std::cout << "   Input File: " << filename << std::endl;
    std::cout << "   Separator: " << separator << std::endl;
    
    if (!interactive && !ascii_output) {
        std::cout << "   Output File: " << output_file << std::endl;
        std::cout << "   Formato: " << output_format << std::endl;
        std::cout << "   Dimensioni: " << width << "x" << height << std::endl;
    }
    if (server_mode) {
        std::cout << "   Port: " << server_port << std::endl;
    }
    std::cout << std::endl;

    try {
        CSVPlotter plotter(filename, separator, output_file, output_format,
                          x_min, x_max, y_min, y_max,
                          interactive, ascii_output,
                          ascii_width, ascii_height, width, height);
        
        if (!plotter.loadCSV()) {
            return 1;
        }
        if (server_mode) {
            httplib::Server svr;
            
            svr.Get("/", [&](const httplib::Request& req, httplib::Response& res) {
                std::string html = csv_data.generatePlotlyHTML(x_min, x_max, y_min, y_max, 
                                                       auto_x_range, auto_y_range);
                res.set_content(html, "text/html; charset=utf-8");
            });
            
            std::cout << "Server started on port " << server_port << std::endl;
            std::cout << std::endl;
            
            if (!svr.listen("0.0.0.0", server_port)) {
                std::cerr << "Error starting server on port " << server_port << std::endl;
                return 1;
            }
        } else if (!plotter.plotWithGnuplot()) {
            return 1;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}