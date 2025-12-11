// lv2_plugin.cpp - SpicePedal LV2 Plugin
#include <lv2/core/lv2.h>
#include <cmath>
#include <cstring>
#include <iostream>
#include <atomic>

#include "circuit.h"
#include "solvers/realtime_solver.h"

#define PLUGIN_URI "http://github.com/buzzobuono/spicepedal"

// Port indices
typedef enum {
    PORT_INPUT = 0,
    PORT_OUTPUT = 1,
    PORT_GAIN = 2,
    PORT_BYPASS = 3
} PortIndex;

typedef struct {

    const double* input;
    double* output;
    
    const double* gain;
    const double* bypass;
    
    Circuit* circuit;
    RealTimeSolver* solver;

    double sample_rate;
    
    std::atomic<float> last_gain;
    std::atomic<bool> last_bypass;
    bool initialized;
    
    uint64_t total_samples;
    uint32_t non_convergence_count;
    
} CircuitPlugin;

static LV2_Handle instantiate(
    const LV2_Descriptor* descriptor,
    double sample_rate,
    const char* bundle_path,
    const LV2_Feature* const* features)
{
    CircuitPlugin* plugin = new CircuitPlugin();
    
    plugin->sample_rate = sample_rate;
    plugin->initialized = false;
    plugin->total_samples = 0;
    plugin->non_convergence_count = 0;
    plugin->last_gain.store(1.0f);
    plugin->last_bypass.store(false);
    
    plugin->circuit = new Circuit();
    std::string netlist_path = std::string(bundle_path) + "/circuits/fuzzes/bazz-fuss.cir";
    
    if (!plugin->circuit->loadNetlist(netlist_path)) {
        std::cerr << "SpicePedal ERROR: Failed to load netlist: " << std::endl;

        plugin->circuit = nullptr;
        plugin->solver = nullptr;
        return plugin;
    }
    
    try {
        plugin->solver = new RealTimeSolver(
            *plugin->circuit,
            sample_rate,
            25000,      // input impedance (25kΩ typical for guitar)
            15,         // max_iterations (reduced for real-time)
            1e-6       // tolerance (relaxed for speed)
        );
        
        plugin->solver->initialize();

        std::cerr << "SpicePedal Loaded Successfully @ " 
                  << sample_rate << " Hz" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "SpicePedal ERROR: " << e.what() << std::endl;
        delete plugin->circuit;
        plugin->circuit = nullptr;
        plugin->solver = nullptr;
    }
    
    return (LV2_Handle)plugin;
}

// ============================================
// Connect Port - Host tells us where buffers are
// ============================================
static void connect_port(
    LV2_Handle instance,
    uint32_t port,
    void* data)
{
    CircuitPlugin* plugin = (CircuitPlugin*)instance;
    
    switch ((PortIndex)port) {
        case PORT_INPUT:
            plugin->input = (const double*)data;
            break;
        case PORT_OUTPUT:
            plugin->output = (double*)data;
            break;
        case PORT_GAIN:
            plugin->gain = (const double*)data;
            break;
        case PORT_BYPASS:
            plugin->bypass = (const double*)data;
            break;
    }
}

// ============================================
// Activate - Called when host starts processing
// ============================================
static void activate(LV2_Handle instance)
{
    CircuitPlugin* plugin = (CircuitPlugin*)instance;
    
    if (plugin->solver && !plugin->initialized) {
        // Initialize circuit (warmup)
        try {
            plugin->solver->initialize();
            plugin->initialized = true;
            std::cerr << "[SpicePedal LV2] Circuit initialized" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[SpicePedal LV2] Initialization error: " 
                      << e.what() << std::endl;
        }
    }
}

static void run(LV2_Handle instance, uint32_t n_samples)
{
    CircuitPlugin* plugin = (CircuitPlugin*)instance;
    
    const double* input = plugin->input;
    double* output = plugin->output;
    
    // Read control parameters
    double gain = *(plugin->gain);
    bool bypass = (*(plugin->bypass) > 0.5f);
    
    // Bypass mode - simple passthrough
    if (bypass || !plugin->solver) {
        // Just copy input to output
        std::memcpy(output, input, n_samples * sizeof(double));
        return;
    }
    
    // Process audio through SpicePedal
    for (uint32_t i = 0; i < n_samples; ++i) {
        // Input: scale from [-1, 1] to voltage range
        // Assuming your solver expects [-1, 1] volt range
        double vin = static_cast<double>(input[i]) * gain;
        
        plugin->solver->setInputVoltage(vin);

        bool converged = plugin->solver->solve();
        
        double vout = 0.0f;
        if (converged) {
            vout = static_cast<float>(plugin->solver->getOutputVoltage());
        } else {
            // Non-convergence: use last good value or zero
            plugin->non_convergence_count++;
            vout = (i > 0) ? output[i-1] : 0.0f;
        }
        
        // Safety: clip output to prevent DAC damage
        if (std::isnan(vout) || std::isinf(vout)) {
            vout = 0.0f;
        }
        
        // Soft clip to [-1, 1] range
        if (vout > 1.0f) vout = 1.0f;
        if (vout < -1.0f) vout = -1.0f;
        
        output[i] = vout;
        plugin->total_samples++;
    }
    
    // Debug: print stats every 10 seconds
    static uint64_t last_report = 0;
    if (plugin->total_samples - last_report > plugin->sample_rate * 10) {
        if (plugin->non_convergence_count > 0) {
            std::cerr << "[SpicePedal LV2] Non-convergences: " 
                      << plugin->non_convergence_count 
                      << " / " << plugin->total_samples 
                      << " (" << (100.0 * plugin->non_convergence_count / plugin->total_samples) 
                      << "%)" << std::endl;
        }
        last_report = plugin->total_samples;
    }
}

// ============================================
// Deactivate - Called when host stops processing
// ============================================
static void deactivate(LV2_Handle instance)
{
    CircuitPlugin* plugin = (CircuitPlugin*)instance;
    
    std::cerr << "[SpicePedal LV2] Deactivated. Total samples: " 
              << plugin->total_samples << std::endl;
    
    if (plugin->non_convergence_count > 0) {
        std::cerr << "[SpicePedal LV2] Total non-convergences: " 
                  << plugin->non_convergence_count << std::endl;
    }
}

// ============================================
// Cleanup - Free all resources
// ============================================
static void cleanup(LV2_Handle instance)
{
    CircuitPlugin* plugin = (CircuitPlugin*)instance;
    
    if (plugin->solver) {
        delete plugin->solver;
    }
    if (plugin->circuit) {
        delete plugin->circuit;
    }
    
    delete plugin;
}

// ============================================
// Extension Data - For advanced features
// ============================================
static const void* extension_data(const char* uri)
{
    return nullptr;  // No extensions for now
}

// ============================================
// Descriptor - Plugin metadata
// ============================================
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

// ============================================
// Entry point - LV2 discovery
// ============================================
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