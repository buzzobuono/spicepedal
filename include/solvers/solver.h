#ifndef SOLVER_H
#define SOLVER_H

#include <string>
#include <vector>

class Solver {
    
    protected:
    
    uint64_t sample_count = 0;
    uint64_t failed_count = 0;
    uint64_t iteration_count = 0;
    
    void initCounters() {
        sample_count = 0;
        failed_count = 0;
        iteration_count = 0;
    };
    
    public:
    
    virtual ~Solver() = default;
    
    virtual bool initialize() = 0;
    
    virtual bool solve() = 0;
    
    virtual bool reset() = 0;
    
    double getFailurePercentage() {
        return (sample_count > 0) ? (100.0 * failed_count / sample_count) : 0.0;
    }
    
    double getTotalIterations() {
        return iteration_count;
    }
    
    double getTotalSamples() {
        return sample_count;
    }
    
    double getMeanIterations() {
        return (sample_count > 0) ? (1.0 * iteration_count / sample_count) : 0.0;
    }
    
};

#endif
