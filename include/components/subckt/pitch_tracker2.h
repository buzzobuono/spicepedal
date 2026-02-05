#ifndef PITCH_TRACKER_2_H
#define PITCH_TRACKER_2_H

#include "components/component.h"
#include <Eigen/Dense>
#include <vector>

class PitchTracker2 : public Component {
private:
    int n_in, n_out;
    double threshold;
    double last_cross_t;
    double current_freq;
    int last_state;

    std::vector<double> signal_window;
    int signal_n;
    int signal_idx;
    double signal_sum;

    std::vector<double> freq_window;
    int freq_n;
    int freq_idx;
    double freq_sum;
    double smoothed_freq;

public:
    PitchTracker2(const std::string& name, int in, int out, double thr = 0.02, int n_signal = 8, int n_freq = 4)
        : n_in(in), n_out(out), threshold(thr), signal_n(n_signal), freq_n(n_freq)
    {
        this->name = name;
        nodes = {n_in, n_out};
        type = ComponentType::SUBCIRCUIT;
        last_cross_t = 0.0;
        current_freq = 0.0;
        smoothed_freq = 0.0;
        last_state = 0;

        signal_idx = 0;
        signal_sum = 0.0;
        signal_window.assign(signal_n, 0.0);

        freq_idx = 0;
        freq_sum = 0.0;
        freq_window.assign(freq_n, 0.0);
    }

    // void stamp(Eigen::MatrixXd& G, Eigen::VectorXd& I, const Eigen::VectorXd& V, double dt) override {
    void stamp(Eigen::Ref<Eigen::MatrixXd> G, Eigen::Ref<Eigen::VectorXd> I, const Eigen::Ref<const Eigen::VectorXd>& V, double dt) override {
        double g_out = 1000.0;
        if (n_out != 0) {
            G(n_out, n_out) += g_out;
            I(n_out) += smoothed_freq * g_out;
        }
    }
    
    // void updateHistory(const Eigen::VectorXd& V, double dt) override {
    void updateHistory(const Eigen::Ref<const Eigen::VectorXd>& V, double dt) override {
        static double internal_time = 0;
        internal_time += dt;

        double v_raw = V(n_in);
        signal_sum -= signal_window[signal_idx];
        signal_window[signal_idx] = v_raw;
        signal_sum += signal_window[signal_idx];
        signal_idx = (signal_idx + 1) % signal_n;
        
        double v_smooth = signal_sum / signal_n;

        if (v_smooth > threshold && last_state <= 0) {
            double period = internal_time - last_cross_t;
            if (period > 0.0005) {
                current_freq = 1.0 / period;

                freq_sum -= freq_window[freq_idx];
                freq_window[freq_idx] = current_freq;
                freq_sum += freq_window[freq_idx];
                freq_idx = (freq_idx + 1) % freq_n;
                
                smoothed_freq = freq_sum / freq_n;
            }
            last_cross_t = internal_time;
            last_state = 1;
        } else if (v_smooth < -threshold) {
            last_state = -1;
        }
    }
};

#endif
