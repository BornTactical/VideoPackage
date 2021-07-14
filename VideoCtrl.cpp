#include <VideoCtrl/VideoCtrl.h>
using namespace Upp;

/*void VideoCtrl::CompileShader(String program) {
    const GLint shader = glCreateShader();
}*/

void VideoCtrl::AudioOutputReq(void* outBuf, unsigned long fpb) {
/*  if(audio.IsInterleaved()) {
        auto bytesWanted = fpb * Pa_GetSampleSize(audio.SampleFormat()) * audio.ChannelCount();
        uint8* buf = (uint8*)outBuf;
        int idx=0;

        while(!audioBuf.IsEmpty() && bytesWanted > 0) {
            buf[idx] = audioBuf.PopTail();
            idx++;
            
            bytesWanted--;

        }
        
        if(bytesWanted > 0) {
            memset(buf + idx, 0, bytesWanted);
            return;
        }
    }
    else {
        auto bytesWanted = fpb * Pa_GetSampleSize(audio.SampleFormat());
        
        for(int i = 0; i < audio.ChannelCount(); i++) {
            memset(((char**)outBuf)[i], 0, bytesWanted);
        }
    }*/
}

void VideoCtrl::EnqueueFrames() {
    decoder->Run();
}

void VideoCtrl::UpdateAudio(MediaFrame& f) {
    resampler.ConvertFrame(f);
    auto data     = f.Data();
    auto samples  = f.Samples();
    auto linesize = f.LineSize()[0];
    auto channels = f.Channels();

    if(audio.IsInterleaved()) {
        audio.Put(data[0], samples);
    }
    else {
        audio.Put(data, samples);
    }
    
    audioPos = f.TS();
}

void VideoCtrl::Render() {
    //if(isUpdating) Refresh();
    
    WhenVideoPositionChanged(VideoPosition());
    Sleep(1);
}

void VideoCtrl::OpenAudio(const CodecContext& cc) {
    resampler = ResamplerContext(cc);
    sampleFormat = ToPaSampleFormat(cc.SampleFormat());
    //auto channels   = cc.Channels();
    auto channels   = 2;
    auto sampleRate = cc.SampleRate();
    framesPerBuffer  = cc.FrameSize();
    
    auto size = Pa_GetSampleSize(sampleFormat);
    //int defaultSize = (192000 * 4 / channels) / size;
    int defaultSize = 1024;
    
    if(audio.IsOpen()) audio.Close();
    audio
        .SampleRate(sampleRate)
        .SampleFormat(sampleFormat)
        .ChannelCount(channels)
        .FramesPerBuffer(framesPerBuffer == 0 ? defaultSize : framesPerBuffer);
        
    LOG("Sample rate: "); LOG(sampleRate);
    LOG("Sample format: "); LOG(sampleFormat);
    LOG("Channel Count: "); LOG(channels);
    LOG("Frames per buffer: "); LOG(framesPerBuffer);
    LOG("Single sample size: "); LOG(size);
    
    audio.Open();
}

void VideoCtrl::Open(String fn) {
    Initialize();
    decoder.Create();
    
    decoder->WhenAudioFrame = [&](MediaFrame& frame)       { UpdateAudio(frame);   };
    decoder->WhenVideoFrame = [&](const MediaFrame& frame) { UpdateTexture(frame); };
    decoder->WhenAudioInit  = [&](const CodecContext& ctx) { OpenAudio(ctx); };
    
    if(decoder->Open(fn)) {
        if(decoder->HasVideo()) {
            scaler = pick(ScalerContext(decoder->VideoSize(), decoder->PixelFormat(), ib));
            texSize = ib.GetSize();
        }
        
        isOpen = true;
        decoder->Run();
    }
    
    fileSize = decoder->FileSize();
}

float VideoCtrl::GetAspectRatio() {
    Size sz = GetSize();
    return min((float)sz.cx / (float)texSize.cx, (float)sz.cy / (float)texSize.cy);
}

void VideoCtrl::BindTexture() {
    //texHandleIdx = 1 - texHandleIdx;
    glActiveTexture(texHandle);
    glBindTexture(GL_TEXTURE_2D, texHandle);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

void VideoCtrl::CreateLogo() {
    BindTexture();
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, ib.GetWidth(), ib.GetHeight(), 0, GL_RGB, GL_UNSIGNED_BYTE, ib[0]);
    //texSize = ib.GetSize();
}

void VideoCtrl::UpdateTexture(const MediaFrame& f) {
    scaler.ScaleFrame(f, ib);
    texSize = f.GetSize();
    videoPos = f.TS();
/*
    BufferPainter bp(ib);
    bp.DrawText(0, 0,   Format("Dropped frames:  %d (v) %d (a)", decoder->DroppedFrames(), droppedAudioFrames), StdFont().Height(50), Yellow);
    bp.DrawText(0, 60,  Format("VQ size:         %d", decoder->vq.GetCount()), StdFont().Height(50), Yellow);
    bp.DrawText(0, 120, Format("frame TS:        %d", (int64)f.TS()), StdFont().Height(50), Yellow);
    bp.DrawText(0, 180, Format("timer TS:        %d", (int64)decoder->timer.TS()), StdFont().Height(50), Yellow);
    */
    
    GuiLock __;
    isUpdating = true;
    Refresh();
}

void VideoCtrl::GLPaint()  {
    Size sz = GetSize();
    float ar = GetAspectRatio();
    
    glViewport(0, 0, sz.cx, sz.cy);
    
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, sz.cx, sz.cy, 0, -1, 1);
    
    glScalef(ar, ar, 0);
    float centerX = (float)sz.cx / 2.0f;
    float centerY = (float)sz.cy / 2.0f;
    float topX = centerX - ((float)texSize.cx * ar/2.0);
    float topY = centerY - ((float)texSize.cy * ar/2.0);
    glTranslatef(topX/ar, topY/ar, 0.0f);
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    glEnable(GL_TEXTURE_2D);
    BindTexture();
    
    if(isUpdating) glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ib.GetWidth(), ib.GetHeight(), 0, GL_RGBA, GL_UNSIGNED_BYTE, ib[0]);
    
    glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.0f); glVertex2f(0,           0);
        glTexCoord2f(1.0f, 0.0f); glVertex2f(texSize.cx,  0);
        glTexCoord2f(1.0f, 1.0f); glVertex2f(texSize.cx,  texSize.cy);
        glTexCoord2f(0.0f, 1.0f); glVertex2f(0,           texSize.cy);
    glEnd();
    
    glDisable(GL_TEXTURE_2D);
    isUpdating = false;
    
    GLCtrl::GLPaint();
}


