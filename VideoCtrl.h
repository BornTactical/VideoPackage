#ifndef _VideoCtrl_VideoCtrl_h_
#define _VideoCtrl_VideoCtrl_h_
#include <CtrlLib/CtrlLib.h>
#include <GLCtrl/GLCtrl.h>
#include <VideoPackage/VideoPackage.h>
#include "shaders.brc"

class VideoCtrl : public GLCtrl {
    One<MediaDecoder> decoder;

    AudioStream      audio;
    ScalerContext    scaler;
    ResamplerContext resampler;
    PaSampleFormat   sampleFormat;
    
    double         volume             { 1.0f };
    int            droppedAudioFrames { 0 };
    int            droppedVideoFrames { 0 };
    int64          lastVideoTS        { 0 };
    int64          lastAudioTS        { 0 };
    GLuint         texHandle          { 0 };
    Size           texSize            { 1200, 800 };
    bool           isOpen             { false };
    bool           isPlaying          { false };
    bool           hasAudio           { false };
    bool           hasVideo           { false };
    bool           isUpdating         { false };
    int            framesPerBuffer    { 0 };
    int64          fileSize           { 0 };
    int64          filePos            { 0 };
    uint64         videoPos           { 0 };
    uint64         audioPos           { 0 };
    
    ImageBuffer    ib;
    
    void  Initialize() {
        droppedAudioFrames = 0;
        droppedVideoFrames = 0;
        lastVideoTS = 0;
        lastAudioTS = 0;
        texHandle = { 0 };
        texSize = { 1200, 800 };
        ib.Clear();
        isOpen = false;
        isPlaying = false;
        hasAudio = false;
        hasVideo = false;
        isUpdating = false;
        framesPerBuffer = 0;
        fileSize = 0;
        filePos = 0;
        videoPos = 0;
        audioPos = 0;
        volume = 0;
        
        ExecuteGL([&] {
            glGenTextures(1, &texHandle);
            ib.Create(255, 255);
            BufferPainter bp(ib);
            bp.Clear(RGBAZero());
            CreateLogo();
        }, false);
    }
    
    void  AudioOutputReq(void* outBuf, unsigned long fpb);
    void  CompileShader(String program);
    void  EnqueueFrames();
    float GetAspectRatio();
    void  BindTexture();
    void  UpdateTexture(const MediaFrame& frame);
    void  UpdateAudio(MediaFrame& frame);
    void  CreateLogo();
    void  GLPaint() override;
    void  OpenAudio(const CodecContext& cc);

public:
    Event<uint64> WhenVideoPositionChanged;
    
    int64  FileSize()      const { return fileSize; }
    int64  FilePos()       const { return filePos;  }
    uint64 Duration()      const { return decoder->Duration(); }
    uint64 VideoPosition() const { return decoder->HasVideo() ? videoPos : audioPos; }
    uint64 FirstTS()       const { return decoder->FirstTS(); }
    bool   IsOpen()        const { return isOpen; };
    bool   IsPlaying()     const { return isPlaying; }

    void  Volume(double v) { volume = v; }
    void  Render();
    void  Open(String fn);
    void  Play()           { isPlaying = true;  decoder->Play();   }
    void  Pause()          { isPlaying = false; decoder->Pause();  }
    void  Rewind()         { isPlaying = false; decoder->Rewind(); }
    void  Seek(int64 pos)  { decoder->Seek(pos); videoPos = audioPos = pos; }

    VideoCtrl() {
        Ctrl::GlobalBackPaint();
        decoder.Create();
    }
    
    ~VideoCtrl() {
        if(!decoder.IsEmpty()) {
            //Pause();
            //audio.Close();
        }
    }
};

#endif
