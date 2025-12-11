#ifndef SIGNAL_GENERATOR_H
#define SIGNAL_GENERATOR_H

#include <vector>
#include <string>

class SignalGenerator {

    public:
    
    virtual ~SignalGenerator() = default;
    
    virtual std::vector<double> generate() = 0;
    
    virtual double getScaleFactor() const { return 1.0; }
    
    virtual double getMean() const { return 0.0; }
    
    virtual double getMaxNormalized() const { return 0.0; }
    
    virtual void printInfo() const = 0;
    
};

#endif
