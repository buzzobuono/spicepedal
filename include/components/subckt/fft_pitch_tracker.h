//#include <fftw3.h>
#include <vector>
#include <cmath>
#include <algorithm>

class FFTPitchTracker : public Component {
private:
    int n_in, n_out;
    int buffer_size;
    std::vector<double> buffer;
    int buffer_ptr = 0;
    
    //double *fft_in;
    //fftw_complex *fft_out;
    //fftw_plan plan;

    double current_freq = 0;
    double sample_rate = 44100.0;

public:
    FFTPitchTracker(const std::string& name, int in, int out, int size = 8192) 
        : n_in(in), n_out(out), buffer_size(size) {
        this->name = name;
        nodes = {n_in, n_out};
        type = ComponentType::SUBCIRCUIT;
        buffer.resize(buffer_size, 0.0);
        
        //fft_in = (double*) fftw_malloc(sizeof(double) * buffer_size);
        //fft_out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * (buffer_size / 2 + 1));
        //plan = fftw_plan_dft_r2c_1d(buffer_size, fft_in, fft_out, FFTW_ESTIMATE);
    }

    ~FFTPitchTracker() {
        //fftw_destroy_plan(plan);
        //fftw_free(fft_in);
        //fftw_free(fft_out);
    }

    void updateHistory(const Eigen::VectorXd& V, double dt) override {
        sample_rate = 1.0 / dt;
        buffer[buffer_ptr++] = V(n_in);

        if (buffer_ptr >= buffer_size) {
            analyzeFrequency();
            buffer_ptr = 0; 
        }
    }

    void analyzeFrequency() {
        int tau_max = buffer_size / 2;
        std::vector<double> nsdf(tau_max, 0.0);
        
        // 1. Calcolo della NSDF (Simil-Autocorrelazione Normalizzata)
        for (int tau = 0; tau < tau_max; tau++) {
            double acf = 0;
            double mdf = 0;
            for (int i = 0; i < tau_max; i++) {
                double a = buffer[i];
                double b = buffer[i + tau];
                acf += a * b;          // Autocorrelazione
                mdf += a * a + b * b;  // Fattore di normalizzazione
            }
            nsdf[tau] = 2.0 * acf / mdf;
        }
        
        // 2. Ricerca dei Picchi (Key Peaks)
        // Cerchiamo il primo picco che superi una certa soglia (es. 0.9 per segnali puri)
        int best_tau = 0;
        double max_pos = 0;
        double threshold = 0.8; 

        for (int tau = 1; tau < tau_max - 1; tau++) {
            if (nsdf[tau] > threshold) {
                // Cerchiamo il massimo locale (picco)
                if (nsdf[tau] > nsdf[tau-1] && nsdf[tau] > nsdf[tau+1]) {
                    // Trovato! Usiamo l'interpolazione parabolica per tau
                    double val_prev = nsdf[tau-1];
                    double val_curr = nsdf[tau];
                    double val_next = nsdf[tau+1];
                    
                    double offset = 0.5 * (val_prev - val_next) / (val_prev - 2.0 * val_curr + val_next);
                    max_pos = (double)tau + offset;
                    break; // Il primo picco sopra soglia è la FONDAMENTALE
                }
            }
        }
        
        // 3. Conversione in frequenza
        if (max_pos > 0) {
            current_freq = sample_rate / max_pos;
        }
    }

    void stamp(Eigen::MatrixXd& G, Eigen::VectorXd& I, const Eigen::VectorXd& V, double dt) override {
        double g_out = 1e6; // Aumentiamo la forza del Vnodo
        if (n_out != 0) {
            G(n_out, n_out) += g_out;
            I(n_out) += current_freq * g_out; 
        }
    }
};
