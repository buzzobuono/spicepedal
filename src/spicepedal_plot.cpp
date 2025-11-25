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
    bool open_after;
    
    int ascii_width;
    int ascii_height;
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
               const std::string& plot_title,
               const std::string& output,
               const std::string& format,
               double xmin,
               double xmax,
               double ymin,
               double ymax,
               bool inter,
               bool ascii,
               bool open_file,
               int awidth,
               int aheight,
               int w,
               int h)
        : filename(file),
          separator(sep),
          title(plot_title),
          output_file(output),
          output_format(format),
          x_min(xmin),
          x_max(xmax),
          y_min(ymin),
          y_max(ymax),
          interactive(inter),
          ascii_output(ascii),
          open_after(open_file),
          ascii_width(awidth),
          ascii_height(aheight),
          width(w),
          height(h)
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
            script << "set terminal dumb size " << ascii_width << "," << ascii_height << "\n";
            std::cout << "Modalità: ASCII Terminal (" << ascii_width << "x" << ascii_height << ")" << std::endl;
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
        
        script << "set title '" << title << " - " << filename << "'\n";
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
                
                if (open_after) {
                    openFile(output_file);
                }
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

private:
    bool openFile(const std::string& filepath)
    {
        std::cout << "Apertura file..." << std::endl;
        
        char absolute_path[PATH_MAX];
        if (!realpath(filepath.c_str(), absolute_path)) {
            std::cerr << "Errore: impossibile ottenere percorso assoluto" << std::endl;
            return false;
        }
        
        std::cout << "   Percorso: " << absolute_path << std::endl;
        
        std::ifstream test(absolute_path);
        if (!test.good()) {
            std::cerr << "Errore: file non accessibile" << std::endl;
            return false;
        }
        test.close();
        
        // xdg-open (Linux)
        std::string cmd = "xdg-open \"" + std::string(absolute_path) + "\" 2>/dev/null &";
        if (system(cmd.c_str()) == 0) {
            std::cout << "   File aperto con xdg-open" << std::endl;
            return true;
        }
        
        // termux-open (Termux)
        cmd = "termux-open \"" + std::string(absolute_path) + "\" 2>/dev/null";
        if (system(cmd.c_str()) == 0) {
            std::cout << "   File aperto con termux-open" << std::endl;
            return true;
        }
        
        // Copia in Downloads (Termux)
        std::string filename_only = filepath.substr(filepath.find_last_of("/") + 1);
        std::string downloads_path = "/storage/emulated/0/Download/" + filename_only;
        
        cmd = "cp \"" + std::string(absolute_path) + "\" \"" + downloads_path + "\" 2>/dev/null";
        if (system(cmd.c_str()) == 0) {
            std::cout << "   File copiato in: " << downloads_path << std::endl;
            
            cmd = "termux-open \"" + downloads_path + "\" 2>/dev/null";
            if (system(cmd.c_str()) == 0) {
                std::cout << "   File aperto da Downloads" << std::endl;
                return true;
            }
        }
        
        // termux-share
        cmd = "termux-share \"" + std::string(absolute_path) + "\" 2>/dev/null";
        if (system(cmd.c_str()) == 0) {
            std::cout << "   File condiviso con termux-share" << std::endl;
            return true;
        }
        
        std::cout << "   Impossibile aprire automaticamente" << std::endl;
        std::cout << "   Apri manualmente il file: " << absolute_path << std::endl;
        
        return false;
    }
};

int main(int argc, char* argv[])
{
    CLI::App app{"Plot dei segnali da un file CSV di probe"};

    std::string filename;
    std::string separator = ";";
    std::string title = "Probe Signals";
    std::string output_file = "output.png";
    std::string output_format = "auto";
    
    double x_min = std::numeric_limits<double>::lowest();
    double x_max = std::numeric_limits<double>::max();
    double y_min = std::numeric_limits<double>::lowest();
    double y_max = std::numeric_limits<double>::max();
    
    bool interactive = false;
    bool ascii_output = false;
    bool open_after = false;
    
    int ascii_width = 0;
    int ascii_height = 0;
    int width = 1000;
    int height = 600;

    app.add_option("file", filename, "Nome del file CSV da leggere (es: probe.csv)")
        ->required()
        ->check(CLI::ExistingFile);
    
    app.add_option("--sep", separator, "Separatore di campo")
        ->default_val(separator);
    
    app.add_option("--title", title, "Titolo del grafico")
        ->default_val(title);
    
    app.add_option("--save", output_file, "File di output per il grafico")
        ->default_val(output_file);
    
    app.add_option("--format", output_format, "Formato output: auto, png, html, svg, pdf, eps, tikz")
        ->default_val(output_format)
        ->check(CLI::IsMember({"auto", "png", "html", "svg", "pdf", "eps", "tikz"}));
    
    app.add_option("--width", width, "Larghezza output (pixel per png/html/svg)")
        ->default_val(width);
    
    app.add_option("--height", height, "Altezza output (pixel per png/html/svg)")
        ->default_val(height);
    
    app.add_option("--xmin", x_min, "Valore minimo asse X");
    app.add_option("--xmax", x_max, "Valore massimo asse X");
    app.add_option("--ymin", y_min, "Valore minimo asse Y");
    app.add_option("--ymax", y_max, "Valore massimo asse Y");
    
    app.add_flag("-w,--interactive", interactive, "Modalità interattiva con finestra Qt (solo Linux)")
        ->default_val(interactive);
    
    app.add_flag("-a,--ascii", ascii_output, "Mostra grafico ASCII nel terminale")
        ->default_val(ascii_output);
    
    app.add_option("--ascii-width", ascii_width, "Larghezza grafico ASCII (0=auto)")
        ->default_val(ascii_width)
        ->check(CLI::Range(0, 500));
    
    app.add_option("--ascii-height", ascii_height, "Altezza grafico ASCII (0=auto)")
        ->default_val(ascii_height)
        ->check(CLI::Range(0, 100));
    
    app.add_flag("-o,--open", open_after, "Apri automaticamente il file generato")
        ->default_val(open_after);

    CLI11_PARSE(app, argc, argv);

    if (interactive && ascii_output) {
        std::cerr << "Errore: non puoi usare --interactive e --ascii insieme" << std::endl;
        return 1;
    }
    
    if ((interactive || ascii_output) && open_after) {
        std::cerr << "Warning: --open ignorato in modalità interattiva/ascii" << std::endl;
        open_after = false;
    }

    std::cout << "Parametri Input" << std::endl;
    std::cout << "   File CSV: " << filename << std::endl;
    std::cout << "   Separatore: " << separator << std::endl;
    std::cout << "   Titolo: " << title << std::endl;
    
    if (!interactive && !ascii_output) {
        std::cout << "   Output File: " << output_file << std::endl;
        std::cout << "   Formato: " << output_format << std::endl;
        std::cout << "   Dimensioni: " << width << "x" << height << std::endl;
        std::cout << "   Apri automaticamente: " << (open_after ? "Sì" : "No") << std::endl;
    }
    
    std::cout << std::endl;

    try {
        CSVPlotter plotter(filename, separator, title, output_file, output_format,
                          x_min, x_max, y_min, y_max,
                          interactive, ascii_output, open_after,
                          ascii_width, ascii_height, width, height);
        
        if (!plotter.loadCSV()) {
            return 1;
        }
        
        if (!plotter.plotWithGnuplot()) {
            return 1;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}