#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>
#include <cstring>
#include <thread>
#include <atomic>
#include <csignal>

// Socket includes
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "external/CLI11.hpp"

// Server globale per gestire il segnale CTRL+C
std::atomic<bool> server_running(true);

void signalHandler(int signum) {
    std::cout << "\n\nRicevuto segnale di interruzione. Chiusura server..." << std::endl;
    server_running = false;
}

class CSVData
{
public:
    std::vector<std::vector<double>> data;
    std::vector<std::string> column_names;
    std::string filename;
    std::string title;
    
    bool loadCSV(const std::string& file, const std::string& separator)
    {
        filename = file;
        std::ifstream infile(file);
        if (!infile.is_open()) {
            std::cerr << "Errore apertura file: " << file << std::endl;
            return false;
        }

        std::string line;
        
        // Leggi header
        if (std::getline(infile, line)) {
            std::stringstream ss(line);
            std::string column;
            
            while (std::getline(ss, column, separator[0])) {
                column.erase(std::remove_if(column.begin(), column.end(), ::isspace), column.end());
                column_names.push_back(column);
            }
            
            data.resize(column_names.size());
        }

        // Leggi dati
        while (std::getline(infile, line)) {
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

        infile.close();
        
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
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=yes">
    <title>)" << title << R"(</title>
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
            height: calc(100vh - 100px);
            background: white;
            border-radius: 8px;
            box-shadow: 0 2px 8px rgba(0,0,0,0.1);
        }
        .header {
            text-align: center;
            padding: 10px;
            background: white;
            margin-bottom: 10px;
            border-radius: 8px;
            box-shadow: 0 2px 8px rgba(0,0,0,0.1);
        }
        h1 {
            margin: 0;
            color: #333;
            font-size: 24px;
        }
        .info {
            color: #666;
            font-size: 14px;
            margin-top: 5px;
        }
        .controls {
            text-align: center;
            padding: 10px;
            background: white;
            margin-top: 10px;
            border-radius: 8px;
            box-shadow: 0 2px 8px rgba(0,0,0,0.1);
        }
        button {
            padding: 10px 20px;
            margin: 5px;
            border: none;
            border-radius: 4px;
            background: #4CAF50;
            color: white;
            cursor: pointer;
            font-size: 14px;
        }
        button:hover {
            background: #45a049;
        }
        button:active {
            transform: scale(0.98);
        }
    </style>
</head>
<body>
    <div class="header">
        <h1>)" << title << R"(</h1>
        <div class="info">File: )" << filename << R"( | Colonne: )" << column_names.size() 
             << R"( | Punti: )" << (data.empty() ? 0 : data[0].size()) << R"(</div>
    </div>
    
    <div id="plot"></div>
    
    <div class="controls">
        <button onclick='resetZoom()'>Reset Zoom</button>
        <button onclick='autoScale()'>Auto Scale</button>
        <button onclick='toggleGrid()'>Toggle Grid</button>
        <button onclick='downloadPNG()'>Download PNG</button>
    </div>

    <script>
        // Dati
        var traces = [];
)";

        // Genera i dati per ogni traccia
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
        // Layout
        var layout = {
            title: {
                text: ')" << title << R"(',
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

        // Config per renderlo ottimo su mobile
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
            scrollZoom: true  // Abilita zoom con scroll/pinch su mobile
        };

        // Crea il plot
        Plotly.newPlot('plot', traces, layout, config);

        // Funzioni controllo
        var gridVisible = true;
        
        function resetZoom() {
            Plotly.relayout('plot', {
                'xaxis.autorange': true,
                'yaxis.autorange': true
            });
        }

        function autoScale() {
            Plotly.relayout('plot', {
                'xaxis.autorange': true,
                'yaxis.autorange': true
            });
        }

        function toggleGrid() {
            gridVisible = !gridVisible;
            Plotly.relayout('plot', {
                'xaxis.showgrid': gridVisible,
                'yaxis.showgrid': gridVisible
            });
        }

        function downloadPNG() {
            Plotly.downloadImage('plot', {
                format: 'png',
                width: 1920,
                height: 1080,
                filename: 'plot'
            });
        }

        // Rendi il plot responsive al resize della finestra
        window.addEventListener('resize', function() {
            Plotly.Plots.resize('plot');
        });

        // Supporto touch per mobile
        var plotDiv = document.getElementById('plot');
        
        // Abilita gesture per zoom/pan su mobile
        plotDiv.on('plotly_relayout', function(eventdata) {
            console.log('Zoom/Pan event:', eventdata);
        });
    </script>
</body>
</html>
)";

        return html.str();
    }
};

class SimpleHTTPServer
{
private:
    int server_fd;
    int port;
    CSVData& csv_data;
    double x_min, x_max, y_min, y_max;
    bool auto_x_range, auto_y_range;

public:
    SimpleHTTPServer(int p, CSVData& data, 
                    double xmin, double xmax, double ymin, double ymax,
                    bool auto_x, bool auto_y)
        : port(p), csv_data(data),
          x_min(xmin), x_max(xmax), y_min(ymin), y_max(ymax),
          auto_x_range(auto_x), auto_y_range(auto_y)
    {
        server_fd = -1;
    }

    bool start()
    {
        struct sockaddr_in address;
        int opt = 1;

        // Crea socket
        if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
            std::cerr << "Errore creazione socket" << std::endl;
            return false;
        }

        // Permetti riuso indirizzo
        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
            std::cerr << "Errore setsockopt" << std::endl;
            return false;
        }

        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(port);

        // Bind
        if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
            std::cerr << "Errore bind sulla porta " << port << std::endl;
            std::cerr << "La porta potrebbe essere già in uso. Prova un'altra porta con --port" << std::endl;
            return false;
        }

        // Listen
        if (listen(server_fd, 3) < 0) {
            std::cerr << "Errore listen" << std::endl;
            return false;
        }

        std::cout << "Server HTTP avviato con successo!" << std::endl;
        std::cout << "Apri il browser a uno di questi indirizzi:" << std::endl;
        std::cout << "   http://localhost:" << port << std::endl;
        std::cout << "   http://127.0.0.1:" << port << std::endl;
        
        // Prova a ottenere l'IP locale
        printLocalIPs();
        
        std::cout << "\nPremi CTRL+C per terminare il server" << std::endl;
        std::cout << std::string(60, '=') << std::endl;
        std::cout << std::endl;

        return true;
    }

    void run()
    {
        struct sockaddr_in address;
        int addrlen = sizeof(address);

        while (server_running) {
            int new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
            
            if (new_socket < 0) {
                if (server_running) {
                    std::cerr << "Errore accept" << std::endl;
                }
                continue;
            }

            handleRequest(new_socket);
            close(new_socket);
        }

        close(server_fd);
    }

private:
    void handleRequest(int socket)
    {
        char buffer[4096] = {0};
        read(socket, buffer, 4096);

        std::string request(buffer);
        
        // Log richiesta
        std::string first_line = request.substr(0, request.find("\r\n"));
        std::cout << "[" << getCurrentTime() << "] " << first_line << std::endl;

        // Genera HTML
        std::string html = csv_data.generatePlotlyHTML(x_min, x_max, y_min, y_max, 
                                                       auto_x_range, auto_y_range);

        // Prepara risposta HTTP
        std::stringstream response;
        response << "HTTP/1.1 200 OK\r\n";
        response << "Content-Type: text/html; charset=UTF-8\r\n";
        response << "Content-Length: " << html.length() << "\r\n";
        response << "Connection: close\r\n";
        response << "Cache-Control: no-cache\r\n";
        response << "\r\n";
        response << html;

        std::string response_str = response.str();
        send(socket, response_str.c_str(), response_str.length(), 0);
    }

    std::string getCurrentTime()
    {
        time_t now = time(0);
        struct tm tstruct;
        char buf[80];
        tstruct = *localtime(&now);
        strftime(buf, sizeof(buf), "%H:%M:%S", &tstruct);
        return buf;
    }

    void printLocalIPs()
    {
        // Metodo semplice: suggerisci di controllare manualmente
        std::cout << "\nPer accedere da altri dispositivi sulla stessa rete:" << std::endl;
        std::cout << "1. Trova il tuo IP locale:" << std::endl;
        std::cout << "   Linux/Mac: ifconfig | grep 'inet ' | grep -v 127.0.0.1" << std::endl;
        std::cout << "   Termux: ifconfig wlan0 | grep 'inet '" << std::endl;
        std::cout << "2. Usa http://TUO_IP:" << port << std::endl;
        std::cout << std::endl;
    }
};

int main(int argc, char* argv[])
{
    CLI::App app{"CSV Plot Server - Server HTTP per visualizzazione interattiva di dati CSV"};

    std::string filename;
    std::string separator = ";";
    std::string title = "Probe Signals";
    int port = 8080;
    
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
    
    app.add_option("-p,--port", port, "Porta del server HTTP")
        ->default_val(port)
        ->check(CLI::Range(1024, 65535));
    
    app.add_option("--xmin", x_min, "Valore minimo asse X");
    app.add_option("--xmax", x_max, "Valore massimo asse X");
    app.add_option("--ymin", y_min, "Valore minimo asse Y");
    app.add_option("--ymax", y_max, "Valore massimo asse Y");

    CLI11_PARSE(app, argc, argv);

    bool auto_x_range = (x_min == std::numeric_limits<double>::lowest() && 
                         x_max == std::numeric_limits<double>::max());
    bool auto_y_range = (y_min == std::numeric_limits<double>::lowest() && 
                         y_max == std::numeric_limits<double>::max());

    std::cout << std::string(60, '=') << std::endl;
    std::cout << "CSV PLOT SERVER" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    std::cout << std::endl;

    std::cout << "Parametri:" << std::endl;
    std::cout << "   File CSV: " << filename << std::endl;
    std::cout << "   Separatore: " << separator << std::endl;
    std::cout << "   Titolo: " << title << std::endl;
    std::cout << "   Porta: " << port << std::endl;
    
    if (!auto_x_range) {
        std::cout << "   X Range: [" << x_min << ", " << x_max << "]" << std::endl;
    } else {
        std::cout << "   X Range: Auto" << std::endl;
    }
    
    if (!auto_y_range) {
        std::cout << "   Y Range: [" << y_min << ", " << y_max << "]" << std::endl;
    } else {
        std::cout << "   Y Range: Auto" << std::endl;
    }
    
    std::cout << std::endl;

    // Carica dati CSV
    CSVData csv_data;
    csv_data.title = title;
    
    if (!csv_data.loadCSV(filename, separator)) {
        return 1;
    }

    // Installa handler per CTRL+C
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    // Avvia server
    SimpleHTTPServer server(port, csv_data, x_min, x_max, y_min, y_max, 
                           auto_x_range, auto_y_range);
    
    if (!server.start()) {
        return 1;
    }

    server.run();

    std::cout << "\nServer terminato correttamente." << std::endl;
    
    return 0;
}