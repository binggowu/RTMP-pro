#include <iostream>
#include <vector>
#include <memory>
#include "audioresample.h"
#include "aacencoder.h"
#include "dlog.h"

using namespace std;

int testAacEncoder(const char *pcmFileName, const char* aacFileName)
{
    FILE *pcmFp = NULL;
    FILE *aacFp = NULL;

    pcmFp = fopen(pcmFileName, "rb");
    if (!pcmFp)
    {
        LogInfo("Open File:%s LogError.\n", pcmFileName);
        return -1;
    }

    aacFp = fopen(aacFileName, "wb");
    if (!aacFp)
    {
        LogInfo("Open File:%s LogError.\n", aacFileName);
        return -1;
    }

    {
      auto frame = shared_ptr<AVFrame>(av_frame_alloc(), [](AVFrame *p) {
              cout << "~AVFrame "  << endl;
              if (p) av_frame_free(&p);});
      }

    Properties properties;
    AACEncoder *aacEncoder = NULL;
    properties.SetProperty("samplerate", 96000);
    aacEncoder = new AACEncoder();
    int nb_frame_size = 4096;
    int nb_frame_samples = aacEncoder->GetFrameSampleSize();
    uint8_t *in_pcm_buf = new uint8_t[4096];
    int out_aac_buf_size = 8096;
    int out_aac_get_size = 8096;
    uint8_t *out_aac_buf = new uint8_t[out_aac_buf_size];

    LQF::AudioResampleParams aParams;
    aParams.logtag = "[audio-resample]";
    aParams.src_sample_fmt = AV_SAMPLE_FMT_S16;
    aParams.dst_sample_fmt = AV_SAMPLE_FMT_FLTP;
    aParams.src_sample_rate = 48000;
    aParams.dst_sample_rate = aacEncoder->get_sample_rate();
    aParams.src_channel_layout = AV_CH_LAYOUT_STEREO;
    aParams.dst_channel_layout = AV_CH_LAYOUT_STEREO;
    aParams.logtag = "audio-resample-test";
    LQF::AudioResampler *audio_resampler = new LQF::AudioResampler();
    audio_resampler->InitResampler(aParams);

    while (1)
    {
        size_t read_bytes =  fread(in_pcm_buf, 1, nb_frame_size, pcmFp);
        if(read_bytes != nb_frame_size)
        {
            LogInfo("read finish\n");
            break;
        }
        auto ret = audio_resampler->SendResampleFrame(in_pcm_buf, read_bytes);
        if(ret <0)
        {   cout << "~SendResampleFrame failed "  << endl;
            continue;
        }
        vector<shared_ptr<AVFrame>> resampledFrames;
        ret = audio_resampler->ReceiveResampledFrame(resampledFrames, nb_frame_samples);
//        auto frame = audio_resampler->getOneFrame(nb_frame_samples);
        if(ret !=0)
        {
            LogInfo("ReceiveResampledFrame ret:%d\n",ret);
        }
        for(int i = 0; i < resampledFrames.size(); i++)
        {
            out_aac_get_size = aacEncoder->Encode(resampledFrames[i].get(), out_aac_buf, out_aac_buf_size);
            if(out_aac_get_size > 0)
            {
                uint8_t adts_header[7];
                aacEncoder->GetAdtsHeader(adts_header, out_aac_get_size);
                fwrite(adts_header, 1, 7, aacFp);
                fwrite(out_aac_buf, 1, out_aac_get_size, aacFp);
            }
        }

    }
    if(in_pcm_buf)
        delete [] in_pcm_buf;
    if(out_aac_buf)
        delete [] out_aac_buf;
    if(aacEncoder)
        delete aacEncoder;
    if(aacFp)
        fclose(aacFp);
    if(pcmFp)
        fclose(pcmFp);
}
