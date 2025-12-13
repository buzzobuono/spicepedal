#include <lv2/core/lv2.h>
#include <cmath>
#include <cstring>
#include <iostream>
#include <atomic>
#include <lv2/worker/worker.h> 

#include "circuit.h"
#include "solvers/realtime_solver.h"

#define PLUGIN_URI "http://github.com/buzzobuono/spicepedal"

typedef enum {
    PORT_INPUT = 0,
    PORT_OUTPUT = 1,
    PORT_GAIN = 2,
    PORT_BYPASS = 3,
    PORT_CIRCUIT_SELECT = 4
} PortIndex;

typedef struct {

    const float* input;
    float* output;
    const float* gain;
    const float* bypass;
    const float* circuit_select;
    
    Circuit* circuit;
    RealTimeSolver* solver;

    double sample_rate;
    
    std::atomic<float> last_gain;
    std::atomic<bool> last_bypass;
    bool initialized;
    
    uint64_t total_samples;
    uint32_t non_convergence_count;
    
    std::atomic<int> current_circuit_id;
    std::atomic<int> requested_circuit_id;

    const LV2_Worker_Interface* worker;
    LV2_Worker_Schedule* schedule;
    std::string bundle_path;

} CircuitPlugin;

static const char* circuit_id_to_path(int id) {
    switch (id) {
        case 0: return "/circuits/fuzzes/bazz-fuss.cir";
        case 1: return "/circuits/booster/booster.cir";
        case 2: return "/circuits/fuzzes/wolly-mammoth.cir";
        case 3: return "/circuits/filters/lowpass-rc.cir";
        case 4: return "/circuits/filters/highpass-rc.cir";
        default: return nullptr;
    }
}

static LV2_Worker_Status worker_work(
    LV2_Handle instance,
    LV2_Worker_Respond_Function respond,
    LV2_Worker_Respond_Handle handle,
    uint32_t size,
    const void* data);

static LV2_Worker_Status worker_end(
    LV2_Handle instance,
    uint32_t size,
    const void* data);

static LV2_Worker_Status worker_destroy(LV2_Handle instance);

static LV2_Handle instantiate(
    const LV2_Descriptor* descriptor,
    double sample_rate,
    const char* bundle_path,
    const LV2_Feature* const* features)
{


    CircuitPlugin* plugin = new CircuitPlugin();
    plugin->input = nullptr;
    plugin->output = nullptr;
    plugin->gain = nullptr;
    plugin->bypass = nullptr;
    plugin->circuit_select = nullptr;
    
    plugin->worker = nullptr;
    plugin->schedule = nullptr;
    plugin->bundle_path = std::string(bundle_path);

    plugin->sample_rate = sample_rate;
    plugin->initialized = false;
    plugin->total_samples = 0;
    plugin->current_circuit_id.store(-1);
    plugin->requested_circuit_id.store(-1);
    
    int initial_id = 0;
    const char* initial_path_suffix = circuit_id_to_path(initial_id);

    plugin->circuit = new Circuit();
    std::string netlist_path = plugin->bundle_path + initial_path_suffix;
    
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
            25000,
            15,
            1e-6
        );
        
        plugin->solver->initialize();

        plugin->current_circuit_id.store(initial_id);
        plugin->requested_circuit_id.store(initial_id);
        
        std::cerr << "SpicePedal Loaded Successfully @ " 
                  << sample_rate << " Hz" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "SpicePedal ERROR: " << e.what() << std::endl;
        delete plugin->circuit;
        plugin->circuit = nullptr;
        plugin->solver = nullptr;
    }
    
    for (int i = 0; features[i]; ++i) {
        if (!strcmp(features[i]->URI, LV2_WORKER__schedule)) {
            plugin->schedule = (LV2_Worker_Schedule*)features[i]->data;
        }
    }
    
    if (!plugin->schedule) {
        std::cerr << "SpicePedal WARNING: LV2 Worker extension not supported by host. Runtime circuit change disabled." << std::endl;
    }
    
    return (LV2_Handle)plugin;
}

static void connect_port(
    LV2_Handle instance,
    uint32_t port,
    void* data)
{
    CircuitPlugin* plugin = (CircuitPlugin*)instance;
    
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
        case PORT_CIRCUIT_SELECT:
            plugin->circuit_select = (const float*)data;
            break;
    }
}

static void activate(LV2_Handle instance)
{
    CircuitPlugin* plugin = (CircuitPlugin*)instance;
    
    if (plugin->solver && !plugin->initialized) {
        try {
            plugin->solver->initialize();
            plugin->initialized = true;
            std::cerr << "[SpicePedal LV2] Circuit initialized" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[SpicePedal LV2] Initialization error: " << e.what() << std::endl;
        }
    }
}

static void run(LV2_Handle instance, uint32_t n_samples)
{
    CircuitPlugin* plugin = (CircuitPlugin*)instance;
    
    const float* input = plugin->input;
    float* output = plugin->output;

    if (plugin->circuit_select && plugin->schedule) {
        int new_id = (int)std::round(*(plugin->circuit_select)); 
        
        if (new_id != plugin->current_circuit_id.load() && 
            new_id != plugin->requested_circuit_id.load()) 
        {
            const char* path = circuit_id_to_path(new_id);
            if (path) {
                plugin->requested_circuit_id.store(new_id);
                plugin->schedule->schedule_work(plugin->schedule->handle, sizeof(int), &new_id);
            } else {
                std::cerr << "[SpicePedal LV2] Invalid circuit ID requested: " << new_id << std::endl;
            }
        }
    }
    
    float gain = *(plugin->gain);
    bool bypass = (*(plugin->bypass) > 0.5f);
    
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
            plugin->non_convergence_count++;
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
    CircuitPlugin* plugin = (CircuitPlugin*)instance;
}

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

static LV2_Worker_Status worker_work(
    LV2_Handle instance,
    LV2_Worker_Respond_Function respond,
    LV2_Worker_Respond_Handle handle,
    uint32_t size,
    const void* data)
{
    CircuitPlugin* plugin = (CircuitPlugin*)instance;
    const int new_circuit_id = *(const int*)data;
    const char* path_suffix = circuit_id_to_path(new_circuit_id);

    if (!path_suffix) {
        int failed_id = -1;
        respond(handle, sizeof(int), &failed_id);
        return LV2_WORKER_SUCCESS;
    }

    if (plugin->solver) delete plugin->solver;
    if (plugin->circuit) delete plugin->circuit;
    plugin->solver = nullptr;
    plugin->circuit = nullptr;
    plugin->initialized = false;
    
    std::string full_path = plugin->bundle_path + path_suffix;
    
    plugin->circuit = new Circuit();
    
    if (plugin->circuit->loadNetlist(full_path)) {
        try {
            plugin->solver = new RealTimeSolver(
                *plugin->circuit,
                plugin->sample_rate,
                25000, 15, 1e-6
            );
            
            respond(handle, sizeof(int), &new_circuit_id);
            return LV2_WORKER_SUCCESS;
            
        } catch (const std::exception& e) {
            std::cerr << "SpicePedal Worker ERROR: " << e.what() << std::endl;
        }
    } else {
        std::cerr << "SpicePedal Worker ERROR: Failed to load netlist: " << full_path << std::endl;
    }
    
    if (plugin->solver) delete plugin->solver;
    if (plugin->circuit) delete plugin->circuit;
    plugin->solver = nullptr;
    plugin->circuit = nullptr;
    
    int failed_id = -1;
    respond(handle, sizeof(int), &failed_id);
    
    return LV2_WORKER_SUCCESS;
}

static LV2_Worker_Status worker_end(
    LV2_Handle instance,
    uint32_t size,
    const void* data)
{
    CircuitPlugin* plugin = (CircuitPlugin*)instance;
    const int finished_circuit_id = *(const int*)data;

    if (finished_circuit_id >= 0 && plugin->solver) {
        
        try {
            plugin->solver->initialize();
            plugin->initialized = true;
            
            plugin->current_circuit_id.store(finished_circuit_id);
            plugin->requested_circuit_id.store(finished_circuit_id);

            std::cerr << "[SpicePedal LV2] Circuit change successful: ID " 
                      << finished_circuit_id << std::endl;
                      
        } catch (const std::exception& e) {
            std::cerr << "[SpicePedal LV2] Final initialization error after worker: " 
                      << e.what() << std::endl;
            delete plugin->solver;
            delete plugin->circuit;
            plugin->solver = nullptr;
            plugin->circuit = nullptr;
            plugin->current_circuit_id.store(-1);
            plugin->requested_circuit_id.store(-1);
        }
    } else {
         plugin->requested_circuit_id.store(plugin->current_circuit_id.load()); 
         std::cerr << "[SpicePedal LV2] Circuit change failed in Worker." << std::endl;
    }

    return LV2_WORKER_SUCCESS;
}

static LV2_Worker_Status worker_destroy(LV2_Handle instance)
{
    return LV2_WORKER_SUCCESS;
}

static const LV2_Worker_Interface worker_iface = {
    worker_work,
    worker_end,
    worker_destroy
};

static const void* extension_data(const char* uri)
{
    if (!strcmp(uri, LV2_WORKER__interface)) {
        return &worker_iface;
    }
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
