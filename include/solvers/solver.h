#ifndef SOLVER_H
#define SOLVER_H

#include <string>
#include <vector>
#include <chrono>

class Solver {

protected:

    uint64_t sample_count = 0;
    uint64_t failed_count = 0;
    uint64_t iteration_count = 0;
    uint64_t execution_time = 0;

    void initCounters() {
        sample_count = 0;
        failed_count = 0;
        iteration_count = 0;
        execution_time = 0;
    }
    
    virtual bool solveImpl() = 0;

public:

    virtual ~Solver() = default;

    virtual bool initialize() = 0;
    virtual bool reset() = 0;

    bool solve() {
        auto start = std::chrono::steady_clock::now();
        bool ok = solveImpl();
        auto end = std::chrono::steady_clock::now();
        execution_time += std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        return ok;
    }

    double getFailurePercentage() const {
        return (sample_count > 0) ? (100.0 * failed_count / sample_count) : 0.0;
    }

    uint64_t getTotalIterations() const {
        return iteration_count;
    }

    uint64_t getTotalSamples() const {
        return sample_count;
    }

    double getMeanIterations() {
        return (sample_count > 0) ? (1.0 * iteration_count / sample_count) : 0.0;
    }
    
    uint64_t getExecutionTime() const {
        return execution_time;
    }

    virtual void printProcessStatistics() {
        std::cout << "Process Statistics:" << std::endl;
        std::cout << "  Solver's Execution Time: " << getExecutionTime() << " us" << std::endl;
        std::cout << "  Solver's Failure Percentage: " << getFailurePercentage() << " %" << std::endl;
        std::cout << "  Solver's Total Samples: " << getTotalSamples() << std::endl;
        std::cout << "  Solver's Total Iterations: " << getTotalIterations() << std::endl;
        std::cout << "  Solver's Mean Iterations: " << getMeanIterations() << std::endl;
        std::cout << std::endl;
    }
    
};

#endif
