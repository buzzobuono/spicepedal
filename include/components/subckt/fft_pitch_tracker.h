#include <fftw3.h>
#include <vector>
#include <cmath>
#include <algorithm>

class FFTPitchTracker : public Component {
private:
    int n_in, n_out;
    int buffer_size;
    std::vector<double> buffer;
    int buffer_ptr = 0;
    
    double *fft_in;
    fftw_complex *fft_out;
    fftw_plan plan;

    double current_freq = 0;
    double sample_rate = 44100.0;

public:
    FFTPitchTracker(const std::string& name, int in, int out, int size = 8192) 
        : n_in(in), n_out(out), buffer_size(size) {
        this->name = name;
        nodes = {n_in, n_out};
        buffer.resize(buffer_size, 0.0);
        
        fft_in = (double*) fftw_malloc(sizeof(double) * buffer_size);
        fft_out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * (buffer_size / 2 + 1));
        plan = fftw_plan_dft_r2c_1d(buffer_size, fft_in, fft_out, FFTW_ESTIMATE);
    }

    ~FFTPitchTracker() {
        fftw_destroy_plan(plan);
        fftw_free(fft_in);
        fftw_free(fft_out);
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
    
    void analyzeFrequency__() {
        // 1. Windowing (Hann) per ridurre il leakage spettrale
        for (int i = 0; i < buffer_size; i++) {
            double window = 0.5 * (1.0 - cos(2.0 * M_PI * i / (buffer_size - 1)));
            fft_in[i] = buffer[i] * window;
        }

        fftw_execute(plan);

        // 2. Calcolo Magnitudo (Spettro Reale)
        int num_bins = buffer_size / 2 + 1;
        std::vector<double> magnitude(num_bins);
        for (int i = 0; i < num_bins; i++) {
            magnitude[i] = sqrt(fft_out[i][0] * fft_out[i][0] + fft_out[i][1] * fft_out[i][1]);
        }

        // 3. HPS (Harmonic Product Spectrum) - Per beccare la fondamentale
        // Moltiplichiamo lo spettro per le sue versioni downsamplate
        std::vector<double> hps = magnitude;
        int num_harmonics = 3; // Controlliamo fino alla 3° armonica
        for (int j = 2; j <= num_harmonics; j++) {
            for (int i = 0; i < num_bins / j; i++) {
                hps[i] *= magnitude[i * j];
            }
        }

        // 4. Ricerca del picco nel range utile (30Hz - 1000Hz)
        double max_val = 0;
        int max_bin = 0;
        int min_bin = (30.0 * buffer_size) / sample_rate;
        int max_search_bin = (1000.0 * buffer_size) / sample_rate;

        for (int i = std::max(1, min_bin); i < std::min(num_bins, max_search_bin); i++) {
            if (hps[i] > max_val) {
                max_val = hps[i];
                max_bin = i;
            }
        }

        // 5. Interpolazione Parabolica per precisione millimetrica (Vnodo stabile)
        if (max_bin > 1 && max_bin < num_bins - 1 && magnitude[max_bin] > 0.001) {
            double y1 = magnitude[max_bin - 1];
            double y2 = magnitude[max_bin];
            double y3 = magnitude[max_bin + 1];
            
            // Calcola lo spostamento frazionario del bin
            double delta = 0.5 * (y1 - y3) / (y1 - 2 * y2 + y3);
            current_freq = (max_bin + delta) * sample_rate / buffer_size;
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
