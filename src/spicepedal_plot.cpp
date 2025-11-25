#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>
#include <cstdlib>
#include <limits>

#include "external/CLI11.hpp"

class CSVPlotter
{
private:
    std::string filename;
    std::string separator;
    std::string title;
    std::string output_file;
    
    double x_min;
    double x_max;
    double y_min;
    double y_max;
    bool auto_x_range;
    bool auto_y_range;
    
    std::vector<std::vector<double>> data;
    std::vector<std::string> column_names;

public:
    CSVPlotter(const std::string& file,
               const std::string& sep,
               const std::string& plot_title,
               const std::string& output,
               double xmin,
               double xmax,
               double ymin,
               double ymax)
        : filename(file),
          separator(sep),
          title(plot_title),
          output_file(output),
          x_min(xmin),
          x_max(xmax),
          y_min(ymin),
          y_max(ymax)
    {
        auto_x_range = (x_min == std::numeric_limits<double>::lowest() && 
                        x_max == std::numeric_limits<double>::max());
        auto_y_range = (y_min == std::numeric_limits<double>::lowest() && 
                        y_max == std::numeric_limits<double>::max());
    }

    bool loadCSV()
    {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Errore apertura file: " << filename << std::endl;
            return false;
        }

        std::string line;
        
        // Leggi header
        if (std::getline(file, line)) {
            std::stringstream ss(line);
            std::string column;
            
            while (std::getline(ss, column, separator[0])) {
                // Rimuovi spazi bianchi
                column.erase(std::remove_if(column.begin(), column.end(), ::isspace), column.end());
                column_names.push_back(column);
            }
            
            data.resize(column_names.size());
        }

        // Leggi dati
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

        // Crea script Gnuplot
        std::string script_file = "plot_script.gnu";
        std::ofstream script(script_file);
        
        if (!script.is_open()) {
            std::cerr << "Errore creazione script Gnuplot" << std::endl;
            return false;
        }

        // Determina il formato di output dall'estensione
        std::string extension = output_file.substr(output_file.find_last_of(".") + 1);
        
        // Configura output
        script << "set terminal png size 1000,600 enhanced font 'Arial,10'\n";
        script << "set output '" << output_file << "'\n";
        script << "set title '" << title << " - " << filename << "'\n";
        script << "set xlabel 'Time [s]'\n";
        script << "set ylabel 'Value'\n";
        script << "set grid\n";
        script << "set key outside right top\n";
        script << "set datafile separator '" << separator << "'\n";
        
        // Imposta range X
        if (!auto_x_range) {
            script << "set xrange [" << x_min << ":" << x_max << "]\n";
            std::cout << "X Range: [" << x_min << ", " << x_max << "]" << std::endl;
        } else {
            std::cout << "X Range: Auto" << std::endl;
        }
        
        // Imposta range Y
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
        
        script.close();
        
        std::cout << "Script Gnuplot creato: " << script_file << std::endl;
        
        // Esegui Gnuplot
        std::string command = "gnuplot " + script_file;
        int result = system(command.c_str());
        
        if (result == 0) {
            std::cout << "Grafico salvato con successo: " << output_file << std::endl;
            std::cout << std::endl;
            
            // Rimuovi script temporaneo
            remove(script_file.c_str());
            return true;
        } else {
            std::cerr << "Errore esecuzione Gnuplot. Assicurati che sia installato." << std::endl;
            std::cerr << "Su Ubuntu/Debian: sudo apt-get install gnuplot" << std::endl;
            std::cerr << "Su macOS: brew install gnuplot" << std::endl;
            return false;
        }
    }
};

int main(int argc, char* argv[])
{
    CLI::App app{"Plot dei segnali da un file CSV di probe"};

    std::string filename;
    std::string separator = ";";
    std::string title = "Probe Signals";
    std::string output_file = "output.png";
    
    double x_min = std::numeric_limits<double>::lowest();
    double x_max = std::numeric_limits<double>::max();
    double y_min = std::numeric_limits<double>::lowest();
    double y_max = std::numeric_limits<double>::max();

    app.add_option("file", filename, "Nome del file CSV da leggere (es: probe.csv)")
        ->required()
        ->check(CLI::ExistingFile);
    
    app.add_option("--sep", separator, "Separatore di campo")
        ->default_val(separator);
    
    app.add_option("--title", title, "Titolo del grafico")
        ->default_val(title);
    
    app.add_option("--save", output_file, "File di output per il grafico (es: output.png)")
        ->default_val(output_file);
    
    app.add_option("--xmin", x_min, "Valore minimo asse X");
    app.add_option("--xmax", x_max, "Valore massimo asse X");
    app.add_option("--ymin", y_min, "Valore minimo asse Y");
    app.add_option("--ymax", y_max, "Valore massimo asse Y");

    CLI11_PARSE(app, argc, argv);

    std::cout << "Parametri Input" << std::endl;
    std::cout << "   File CSV: " << filename << std::endl;
    std::cout << "   Separatore: " << separator << std::endl;
    std::cout << "   Titolo: " << title << std::endl;
    std::cout << "   Output File: " << output_file << std::endl;
    
    if (x_min != std::numeric_limits<double>::lowest() || 
        x_max != std::numeric_limits<double>::max()) {
        std::cout << "   X Range: [" << x_min << ", " << x_max << "]" << std::endl;
    } else {
        std::cout << "   X Range: Auto" << std::endl;
    }
    
    if (y_min != std::numeric_limits<double>::lowest() || 
        y_max != std::numeric_limits<double>::max()) {
        std::cout << "   Y Range: [" << y_min << ", " << y_max << "]" << std::endl;
    } else {
        std::cout << "   Y Range: Auto" << std::endl;
    }
    
    std::cout << std::endl;

    try {
        CSVPlotter plotter(filename, separator, title, output_file, 
                          x_min, x_max, y_min, y_max);
        
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