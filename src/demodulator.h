#pragma once

#include <thread>
#include <complex>
#include <dsp/agc.h>
#include <dsp/fir_filter.h>
#include <dsp/fir_gen.h>
#include <dsp/costas_loop.h>
#include <dsp/clock_recovery_mm.h>
#include <dsp/complex_to_real.h>
#include "processing/repack_bits_byte.h"
#include "processing/pipe.h"

class SVISSRDemodulator
{
private:
    // Settings
    const int samplerate_m;
    const bool enableThreading_m;

    // DSP Blocks
    libdsp::AgcCC agc;
    libdsp::FIRFilterCCF rrcFilter;
    libdsp::CostasLoop costasLoop;
    libdsp::ClockRecoveryMMCC clockRecoveryMM;
    libdsp::ComplexToReal complexToReal;
    RepackBitsByte bitRepacker;

    // Buffers to work with
    std::complex<float> *agc_buffer,
        *filter_buffer,
        *costas_buffer,
        *recovery_buffer;
    std::complex<float> *agc_buffer2,
        *filter_buffer2,
        *costas_buffer2,
        *recovery_buffer2,
        *recovery_buffer3;
    float *recovery_real_buffer;
    uint8_t *recovery_bit_buffer;
    uint8_t *recovery_byte_buffer;

    // Piping stuff if we need it
    pipe_t *sdr_pipe;
    pipe_producer_t *sdr_pipe_producer;
    pipe_consumer_t *sdr_pipe_consumer;

    pipe_t *agc_pipe;
    pipe_producer_t *agc_pipe_producer;
    pipe_consumer_t *agc_pipe_consumer;

    pipe_t *rrc_pipe;
    pipe_producer_t *rrc_pipe_producer;
    pipe_consumer_t *rrc_pipe_consumer;

    pipe_t *costas_pipe;
    pipe_producer_t *costas_pipe_producer;
    pipe_consumer_t *costas_pipe_consumer;

    pipe_t *recovery_pipe;
    pipe_producer_t *recovery_pipe_producer;
    pipe_consumer_t *recovery_pipe_consumer;

    // Threading
    std::thread agcThread, rrcThread, costasThread, recoveryThread;

    // Thread bools
    bool agcRun, rrcRun, costasRun, recoveryRun;
    bool stopped;

private:
    void agcThreadFunc();
    void rrcThreadFunc();
    void costasThreadFunc();
    void recoveryThreadFunc();

public:
    SVISSRDemodulator(int samplerate, bool enableThreading = false);
    ~SVISSRDemodulator();
    int workSingleThread(std::complex<float> *in_samples, int length, uint8_t *out_samples);
    void pushMultiThread(std::complex<float> *in_samples, int length);
    int pullMultiThread(uint8_t *out_samples);
    void stop();
};