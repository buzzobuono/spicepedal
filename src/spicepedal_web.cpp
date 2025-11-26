#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>
#include <limits>

#include "external/CLI11.hpp"
#include "external/httplib.h"

class CSVData
{
public:
    std::vector<std::vector<double>> data;
    std::vector<std::string> column_names;
    std::string filename;
    
    bool loadCSV(const std::string& file, const std::string& separator)
    {
        filename = file;
        std::ifstream infile(file);
        if (!infile.is_open()) {
            std::cerr << "Error opening file: " << file << std::endl;
            return false;
        }

        std::string line;
        
        if (std::getline(infile, line)) {
            std::stringstream ss(line);
            std::string column;
            
            while (std::getline(ss, column, separator[0])) {
                column.erase(std::remove_if(column.begin(), column.end(), ::isspace), column.end());
                column_names.push_back(column);
            }
            
            data.resize(column_names.size());
        }
        
        while (std::getline(infile, line)) {
            std::stringstream ss(line);
            std::string value;
            size_t col_idx = 0;
            
            while (std::getline(ss, value, separator[0]) && col_idx < data.size()) {
                try {
                    data[col_idx].push_back(std::stod(value));
                } catch (const std::exception& e) {
                    std::cerr << "Error value conversion: " << value << std::endl;
                    return false;
                }
                col_idx++;
            }
        }

        infile.close();
        
        std::cout << "CSV file loaded" << std::endl;
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
};

int main(int argc, char* argv[])
{
    CLI::App app{"SpicePedal Plot: an interactive probe file plot visualiser via http server"};
    
    std::string filename;
    std::string separator = ";";
    int port = 8080;
    
    double x_min = std::numeric_limits<double>::lowest();
    double x_max = std::numeric_limits<double>::max();
    double y_min = std::numeric_limits<double>::lowest();
    double y_max = std::numeric_limits<double>::max();
    
    app.add_option("-i,--input-file", filename, "Input File")
        ->required()
        ->check(CLI::ExistingFile);
    
    app.add_option("-s,--separator", separator, "Field Separator")
        ->default_val(separator);
    
    app.add_option("-p,--port", port, "HTTP Server Port")
        ->default_val(port)
        ->check(CLI::Range(1024, 65535));
    
    app.add_option("--xmin", x_min, "Minimum X-axis value");
    app.add_option("--xmax", x_max, "Maximum X-axis value");
    app.add_option("--ymin", y_min, "Minimum Y-axis value");
    app.add_option("--ymax", y_max, "Maximum Y-axis value");
    
    CLI11_PARSE(app, argc, argv);

    bool auto_x_range = (x_min == std::numeric_limits<double>::lowest() && 
                         x_max == std::numeric_limits<double>::max());
    bool auto_y_range = (y_min == std::numeric_limits<double>::lowest() && 
                         y_max == std::numeric_limits<double>::max());
    
    std::cout << "Input Parameters:" << std::endl;
    std::cout << "   Input File: " << filename << std::endl;
    std::cout << "   Separator: " << separator << std::endl;
    std::cout << "   Port: " << port << std::endl;
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
    
    CSVData csv_data;
    
    if (!csv_data.loadCSV(filename, separator)) {
        return 1;
    }
    
    httplib::Server svr;
    
    svr.Get("/", [&](const httplib::Request& req, httplib::Response& res) {
        std::string html = csv_data.generatePlotlyHTML(x_min, x_max, y_min, y_max, 
                                                       auto_x_range, auto_y_range);
        res.set_content(html, "text/html; charset=utf-8");
    });
    
    if (!svr.listen("0.0.0.0", port)) {
        std::cerr << "Error starting server on port " << port << std::endl;
        return 1;
    }

    return 0;
}