#include "demodulator.h"
#include "processing/differentialencoding.h"

SVISSRDemodulator::SVISSRDemodulator(int samplerate, bool enableThreading) : samplerate_m(samplerate),
                                                                         enableThreading_m(enableThreading),
                                                                         agc(libdsp::AgcCC(0.0010e-3f, 1.0f, 1.0f, 65536)),
                                                                         rrcFilter(libdsp::FIRFilterCCF(1, libdsp::firgen::root_raised_cosine(1, samplerate, 660e3, 0.5f, 31))),
                                                                         costasLoop(libdsp::CostasLoop(0.02f, 2)),
                                                                         clockRecoveryMM(libdsp::ClockRecoveryMMCC((float)samplerate / 660e3f, pow(8.7e-3, 2) / 4.0, 0.5f, 8.7e-3, 0.000005f))
{
    agc_buffer = new std::complex<float>[8192];
    filter_buffer = new std::complex<float>[8192];
    costas_buffer = new std::complex<float>[8192];
    recovery_buffer = new std::complex<float>[8192];
    recovery_real_buffer = new float[8192];
    recovery_bit_buffer = new uint8_t[8192];
    recovery_byte_buffer = new uint8_t[8192];

    if (enableThreading_m)
    {
        agc_buffer2 = new std::complex<float>[8192];
        filter_buffer2 = new std::complex<float>[8192];
        costas_buffer2 = new std::complex<float>[8192];
        recovery_buffer2 = new std::complex<float>[8192];
        recovery_buffer3 = new std::complex<float>[8192];

        sdr_pipe = pipe_new(sizeof(std::complex<float>), 8192 * 10);
        sdr_pipe_producer = pipe_producer_new(sdr_pipe);
        sdr_pipe_consumer = pipe_consumer_new(sdr_pipe);

        agc_pipe = pipe_new(sizeof(std::complex<float>), 8192 * 10);
        agc_pipe_producer = pipe_producer_new(agc_pipe);
        agc_pipe_consumer = pipe_consumer_new(agc_pipe);

        rrc_pipe = pipe_new(sizeof(std::complex<float>), 8192 * 10);
        rrc_pipe_producer = pipe_producer_new(rrc_pipe);
        rrc_pipe_consumer = pipe_consumer_new(rrc_pipe);

        costas_pipe = pipe_new(sizeof(std::complex<float>), 8192 * 10);
        costas_pipe_producer = pipe_producer_new(costas_pipe);
        costas_pipe_consumer = pipe_consumer_new(costas_pipe);

        recovery_pipe = pipe_new(sizeof(std::complex<float>), 8192 * 10);
        recovery_pipe_producer = pipe_producer_new(recovery_pipe);
        recovery_pipe_consumer = pipe_consumer_new(recovery_pipe);

        agcRun = true;
        rrcRun = true;
        costasRun = true;
        recoveryRun = true;

        agcThread = std::thread(&SVISSRDemodulator::agcThreadFunc, this);
        rrcThread = std::thread(&SVISSRDemodulator::rrcThreadFunc, this);
        costasThread = std::thread(&SVISSRDemodulator::costasThreadFunc, this);
        recoveryThread = std::thread(&SVISSRDemodulator::recoveryThreadFunc, this);
    }

    stopped = false;
}

void SVISSRDemodulator::stop()
{
    if (enableThreading_m)
    {
        // Exit all threads... Without causing a race condition!
        agcRun = false;

        pipe_producer_free(sdr_pipe_producer);
        pipe_consumer_free(sdr_pipe_consumer);
        pipe_free(sdr_pipe);

        if (agcThread.joinable())
            agcThread.join();

        rrcRun = false;

        pipe_producer_free(agc_pipe_producer);
        pipe_consumer_free(agc_pipe_consumer);
        pipe_free(agc_pipe);

        if (rrcThread.joinable())
            rrcThread.join();

        costasRun = false;

        pipe_producer_free(rrc_pipe_producer);
        pipe_consumer_free(rrc_pipe_consumer);
        pipe_free(rrc_pipe);

        if (costasThread.joinable())
            costasThread.join();

        recoveryRun = false;

        pipe_producer_free(costas_pipe_producer);
        pipe_consumer_free(costas_pipe_consumer);
        pipe_free(costas_pipe);

        if (recoveryThread.joinable())
            recoveryThread.join();

        pipe_producer_free(recovery_pipe_producer);
        pipe_consumer_free(recovery_pipe_consumer);
        pipe_free(recovery_pipe);
    }

    stopped = true;
}

SVISSRDemodulator::~SVISSRDemodulator()
{
    if (!stopped)
        stop();

    delete[] agc_buffer;
    delete[] filter_buffer;
    delete[] costas_buffer;
    delete[] recovery_buffer;
    delete[] recovery_real_buffer;
    delete[] recovery_bit_buffer;
    delete[] recovery_byte_buffer;

    if (enableThreading_m)
    {
        delete[] agc_buffer2;
        delete[] filter_buffer2;
        delete[] costas_buffer2;
        delete[] recovery_buffer2;
        delete[] recovery_buffer3;
    }
}

void volk_32f_binary_slicer_8i_generic(int8_t *cVector, const float *aVector, unsigned int num_points)
{
    int8_t *cPtr = cVector;
    const float *aPtr = aVector;
    unsigned int number = 0;

    for (number = 0; number < num_points; number++)
    {
        if (*aPtr++ >= 0)
        {
            *cPtr++ = 1;
        }
        else
        {
            *cPtr++ = 0;
        }
    }
}

void SVISSRDemodulator::pushMultiThread(std::complex<float> *in_samples, int length)
{
    pipe_push(sdr_pipe_producer, in_samples, length);
}

int SVISSRDemodulator::pullMultiThread(uint8_t *out_samples)
{
    int recovery_count = pipe_pop(recovery_pipe_consumer, recovery_buffer3, 1024);

    // Convert to real (BPSK)
    complexToReal.work(recovery_buffer3, recovery_count, recovery_real_buffer);

    // Slice into bits
    volk_32f_binary_slicer_8i_generic((int8_t *)recovery_bit_buffer, recovery_real_buffer, recovery_count);

    // Pack into bytes
    int out = bitRepacker.work(recovery_bit_buffer, recovery_count, out_samples);

    // Differential decoding
    nrzmDecode(out_samples, out);

    return out;
}

int SVISSRDemodulator::workSingleThread(std::complex<float> *in_samples, int length, uint8_t *out_samples)
{
    // Run AGC
    int agc_count = agc.work(in_samples, length, agc_buffer);

    // Run RRC
    int rrc_count = rrcFilter.work(agc_buffer, agc_count, filter_buffer);

    // Run Costas
    int costas_count = costasLoop.work(filter_buffer, rrc_count, costas_buffer);

    // Run clock recovery
    int recovery_count = clockRecoveryMM.work(costas_buffer, costas_count, recovery_buffer);

    // Convert to real (BPSK)
    complexToReal.work(recovery_buffer, recovery_count, recovery_real_buffer);

    // Slice into bits
    volk_32f_binary_slicer_8i_generic((int8_t *)recovery_bit_buffer, recovery_real_buffer, recovery_count);

    // Pack into bytes
    int out = bitRepacker.work(recovery_bit_buffer, recovery_count, out_samples);

    // Differential decoding
    nrzmDecode(out_samples, out);

    return out;
}

void SVISSRDemodulator::agcThreadFunc()
{
    while (agcRun)
    {
        int num = pipe_pop(sdr_pipe_consumer, agc_buffer, 8192);
        agc.work(agc_buffer, num, agc_buffer2);
        pipe_push(agc_pipe_producer, agc_buffer2, num);
    }
}

void SVISSRDemodulator::rrcThreadFunc()
{
    while (rrcRun)
    {
        int num = pipe_pop(agc_pipe_consumer, filter_buffer, 8192);
        int rrc_samples = rrcFilter.work(filter_buffer, num, filter_buffer2);
        pipe_push(rrc_pipe_producer, filter_buffer2, rrc_samples);
    }
}

void SVISSRDemodulator::costasThreadFunc()
{
    while (costasRun)
    {
        int num = pipe_pop(rrc_pipe_consumer, costas_buffer, 8192);
        int costas_samples = costasLoop.work(costas_buffer, num, costas_buffer2);
        pipe_push(costas_pipe_producer, costas_buffer2, costas_samples);
    }
}

void SVISSRDemodulator::recoveryThreadFunc()
{
    while (recoveryRun)
    {
        int num = pipe_pop(costas_pipe_consumer, recovery_buffer, 8192);
        int recovery_samples = clockRecoveryMM.work(recovery_buffer, num, recovery_buffer2);
        pipe_push(recovery_pipe_producer, recovery_buffer2, recovery_samples);
    }
}