#include <lv2/core/lv2.h>
#include <cmath>
#include <cstring>
#include <iostream>
#include <atomic>

#include "circuit.h"
#include "solvers/realtime_solver.h"

#ifndef PLUGIN_URI
#define PLUGIN_URI "http://github.com/buzzobuono/spicepedal#default"
#endif

typedef enum {
    PORT_INPUT = 0,
    PORT_OUTPUT = 1,
    PORT_GAIN = 2,
    PORT_BYPASS = 3,
    PORT_PARAM0 = 4,
    PORT_PARAM1 = 5,
    PORT_PARAM2 = 6,
    PORT_PARAM3 = 7
} PortIndex;

typedef struct {
    const float* input;
    float* output;
    const float* gain;
    const float* bypass;
    const float* param0;
    const float* param1;
    const float* param2;
    const float* param3;
    Circuit* circuit;
    RealTimeSolver* solver;
    double sample_rate;
    bool initialized;
    std::string bundle_path;
} SpicePedalPlugin;

static LV2_Handle instantiate(
    const LV2_Descriptor* descriptor,
    double sample_rate,
    const char* bundle_path,
    const LV2_Feature* const* features)
{

    SpicePedalPlugin* plugin = new SpicePedalPlugin();
    plugin->input = nullptr;
    plugin->output = nullptr;
    plugin->gain = nullptr;
    plugin->bypass = nullptr;
    plugin->param0 = nullptr;
    plugin->param1 = nullptr;
    plugin->param2 = nullptr;
    plugin->param3 = nullptr;
    plugin->bundle_path = std::string(bundle_path);

    plugin->sample_rate = sample_rate;
    plugin->initialized = false;
    
    plugin->circuit = new Circuit();
    std::string netlist_path = plugin->bundle_path + "/circuit.cir";
    
    if (!plugin->circuit->loadNetlist(netlist_path)) {
        std::cerr << "SpicePedal ERROR: Failed to load initial netlist: " << netlist_path << std::endl;
        plugin->circuit = nullptr;
        plugin->solver = nullptr;
        return plugin;
    }
    
    try {
        plugin->solver = new RealTimeSolver(
            *plugin->circuit,
            sample_rate,
            25000,
            15,
            1e-6
        );
    } catch (const std::exception& e) {
        std::cerr << "SpicePedal ERROR: " << e.what() << std::endl;
        delete plugin->circuit;
        plugin->circuit = nullptr;
        plugin->solver = nullptr;
    }
    
    return (LV2_Handle)plugin;
}

static void set_param_values(SpicePedalPlugin* plugin) {
    std::vector<int> ids = plugin->circuit->getCtrlParameterIds();
    for (int id : ids) {
        if (id == 0) {
            plugin->circuit->setCtrlParamValue(id, static_cast<double>(*plugin->param0));
        }
        if (id == 1) {
            plugin->circuit->setCtrlParamValue(id, static_cast<double>(*plugin->param1));
        }
        if (id == 2) {
            plugin->circuit->setCtrlParamValue(id, static_cast<double>(*plugin->param2));
        }
        if (id == 3) {
            plugin->circuit->setCtrlParamValue(id, static_cast<double>(*plugin->param3));
        }
    }
}

static void connect_port(
    LV2_Handle instance,
    uint32_t port,
    void* data)
{
    SpicePedalPlugin* plugin = (SpicePedalPlugin*)instance;
    
    switch ((PortIndex)port) {
        case PORT_INPUT:
            plugin->input = (const float*)data;
            break;
        case PORT_OUTPUT:
            plugin->output = (float*)data;
            break;
        case PORT_GAIN:
            plugin->gain = (const float*)data;
            break;
        case PORT_BYPASS:
            plugin->bypass = (const float*)data;
            break;
        case PORT_PARAM0:
            plugin->param0 = (const float*)data;
            break;
        case PORT_PARAM1:
            plugin->param1 = (const float*)data;
            break;
        case PORT_PARAM2:
            plugin->param2 = (const float*)data;
            break;
        case PORT_PARAM3:
            plugin->param3 = (const float*)data;
            break;
        
    }
}

static void activate(LV2_Handle instance)
{
    SpicePedalPlugin* plugin = (SpicePedalPlugin*)instance;
    if (plugin->solver && !plugin->initialized) {
        plugin->solver->initialize();
        plugin->initialized = true;
    }
}

static void run(LV2_Handle instance, uint32_t n_samples)
{
    SpicePedalPlugin* plugin = (SpicePedalPlugin*)instance;
    
    const float* input = plugin->input;
    float* output = plugin->output;
    
    float gain = *(plugin->gain);
    bool bypass = (*(plugin->bypass) > 0.5f);
    
    set_param_values(plugin);
    
    if (bypass || !plugin->solver) {
        if (!plugin->solver) {
            std::memset(output, 0, n_samples * sizeof(float));
            return; 
        }
        std::memcpy(output, input, n_samples * sizeof(float));
        return;
    }
    
    for (uint32_t i = 0; i < n_samples; ++i) {
        float vin = static_cast<float>(input[i]) * gain;
        
        plugin->solver->setInputVoltage(vin);
        
        bool converged = plugin->solver->solve();
        
        float vout = 0.0f;
        if (converged) {
            vout = static_cast<float>(plugin->solver->getOutputVoltage());
        } else {
            vout = (i > 0) ? output[i-1] : 0.0f;
        }
        
        if (std::isnan(vout) || std::isinf(vout)) {
            vout = 0.0f;
        }
        
        if (vout > 1.0f) vout = 1.0f;
        if (vout < -1.0f) vout = -1.0f;
        
        output[i] = vout;
    }
}

static void deactivate(LV2_Handle instance)
{
    
}

static void cleanup(LV2_Handle instance)
{
    SpicePedalPlugin* plugin = (SpicePedalPlugin*)instance;
    
    if (plugin->solver) {
        delete plugin->solver;
    }
    if (plugin->circuit) {
        delete plugin->circuit;
    }
    
    delete plugin;
}

static const void* extension_data(const char* uri)
{
    return nullptr;
}

static const LV2_Descriptor descriptor = {
    PLUGIN_URI,
    instantiate,
    connect_port,
    activate,
    run,
    deactivate,
    cleanup,
    extension_data
};

LV2_SYMBOL_EXPORT
const LV2_Descriptor* lv2_descriptor(uint32_t index)
{
    switch (index) {
        case 0:
            return &descriptor;
        default:
            return nullptr;
    }
}
