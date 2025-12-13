#ifndef WAV_HELPER_H
#define WAV_HELPER_H

#include <vector>
#include <string>
#include <iostream>
#include <sndfile.h>

struct WavData {
    std::vector<double> samples;
    int sample_rate;
};

class WavHelper {
public:
    
    WavHelper() = default;

    // read only the first channel
    WavData read(const std::string& input_file)
    {
        SF_INFO sfInfo{};
        sfInfo.format = 0;
        SNDFILE* file = sf_open(input_file.c_str(), SFM_READ, &sfInfo);

        if (!file) {
            throw std::runtime_error(
                "Errore apertura WAV: " + std::string(sf_strerror(nullptr))
            );
        }

        // Buffer per tutti i sample multicanale
        std::vector<double> buffer(sfInfo.frames * sfInfo.channels);
        sf_count_t numFrames = sf_readf_double(file, buffer.data(), sfInfo.frames);
        sf_close(file);

        if (numFrames != sfInfo.frames) {
            throw std::runtime_error("Errore lettura sample: frames mismatch");
        }

        // Estrai canale sinistro o unico canale
        std::vector<double> samples(numFrames);
        for (sf_count_t i = 0; i < numFrames; i++) {
            samples[i] = buffer[i * sfInfo.channels];
        }

        return WavData{
            .samples = std::move(samples),
            .sample_rate = sfInfo.samplerate
        };

    }

    bool write(std::vector<double> samples,
              const std::string& output_file,
              int sample_rate,
              int bitDepth = 24) {
        
        // Configura WAV
        SF_INFO sfInfo;
        sfInfo.samplerate = sample_rate;
        sfInfo.channels = 1;
        
        switch (bitDepth) {
            case 16: sfInfo.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16; break;
            case 24: sfInfo.format = SF_FORMAT_WAV | SF_FORMAT_PCM_24; break;
            case 32: sfInfo.format = SF_FORMAT_WAV | SF_FORMAT_FLOAT; break;
            default:
                std::cerr << "Bit depth non supportato" << std::endl;
                return false;
        }
        
        if (!sf_format_check(&sfInfo)) {
            std::cerr << "Formato WAV invalido" << std::endl;
            return false;
        }
        
        // Scrivi WAV
        SNDFILE* file = sf_open(output_file.c_str(), SFM_WRITE, &sfInfo);
        if (!file) {
            std::cerr << "Errore apertura WAV: " << sf_strerror(file) << std::endl;
            return false;
        }
        
        sf_count_t written = sf_writef_double(file, samples.data(), samples.size());
        sf_close(file);
        
        if (written != (sf_count_t)samples.size()) {
            std::cerr << "Errore scrittura WAV" << std::endl;
            return false;
        }
        
        std::cout << "Output File Format" << std::endl;
        std::cout << "   File Name: " << output_file << std::endl;
        std::cout << "   Duration: " << (float)samples.size() / sample_rate << "s" << std::endl;
        std::cout << std::endl;
        
        return true;
    }
    
};


#endif
