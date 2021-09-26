#include <VideoPackage/VideoPackage.h>

namespace Upp {
    
Packet::Packet() {
    packet = av_packet_alloc();
    if(!packet) throw Exc("Failed to allocate memory for AVPacket!");
}

Packet::~Packet() {
    av_packet_unref(packet);
    av_packet_free(&packet);
}

void Packet::Drop() {
    av_packet_unref(packet);
}

MediaFrame::MediaFrame(AVFrame* framea) {
    frame = framea;
}

MediaFrame::MediaFrame() {
    frame = av_frame_alloc();
    if(!frame) throw Exc("Failed to allocate memory for AVFrame!");
}

void MediaFrame::Drop() {
    av_frame_unref(frame);
}

MediaFrame::~MediaFrame() {
    if(frame != nullptr) {
        //av_frame_unref(frame);
        av_frame_free(&frame);
    }
}

MediaFrame::MediaFrame(MediaFrame&& other) {
    streamIndex = other.streamIndex;
    frame = other.frame;
    other.frame = nullptr;
    ts = other.ts;
}

MediaFrame& MediaFrame::operator=(MediaFrame&& other) {
    streamIndex = other.streamIndex;
    frame = other.frame;
    other.frame = nullptr;
    ts = other.ts;
    return *this;
}

MediaFrame::MediaFrame(const MediaFrame& f, int deep) :
    frame(f.frame),
    streamIndex(f.streamIndex),
    ts(f.ts) {
        
    }

String MediaFrame::ToString() const {
    String out;
    out << "Timestamp:   " << Pts()
        << " Width:       " << Width()
        << " Height:      " << Height()
        << " StreamIdx:   " << StreamIndex()
        << " Samples:     " << Samples()
        << " Sample rate: " << SampleRate()
        << " Byte size:   " << ByteSize();
    return out;
}

int CodecContext::Decode(Packet& packet, MediaFrame& frame) {
    int response = avcodec_send_packet(ctx, packet.packet);
    if(response < 0) throw Exc("Error sending packet for decoding!");
    
    response = avcodec_receive_frame(ctx, frame.frame);
    if(response == AVERROR_EOF || response == AVERROR(EAGAIN)) return response;
    else if(response == AVERROR_INVALIDDATA) throw Exc("Codec failed to open!");
    else if(response < 0) throw Exc("Decoding error!");

    return response;
}

int CodecContext::SendPacket(Packet& packet) {
    return avcodec_send_packet(ctx, packet.packet);
}

int CodecContext::ReceiveFrame(MediaFrame& frame) {
    return avcodec_receive_frame(ctx, frame.frame);
}

void CodecContext::Drain() {
    int response = avcodec_send_packet(ctx, nullptr);
}

    String CodecContext::ToString() {
    return Format("Number: %d\nRate:%d:%d\nTime base:%d:%d",
        FrameNumber(),
        FrameRate().num, FrameRate().den,
        TimeBase().num, TimeBase().den
    );
}

bool CodecContext::IsValid() {
    return ctx != nullptr;
}

void CodecContext::Flush() {
    avcodec_flush_buffers(ctx);
}

CodecContext::CodecContext(MediaStream& stream) {
    codec = avcodec_find_decoder(stream.CodecId());
    if(!codec) throw Exc("Unsupported codec!");
    
    ctx = avcodec_alloc_context3(codec);
    if(!ctx) throw Exc("Failed to allocated memory for AVCodecContext");
    if(avcodec_parameters_to_context(ctx, stream.Parms()) < 0)
        throw Exc("Failed to copy codec parameters to codec context");
    if(avcodec_open2(ctx, codec, nullptr) < 0)
        throw Exc("Failed to open codec through avcodec_open2");
}

CodecContext::~CodecContext() {
    avcodec_close(ctx);
    avcodec_free_context(&ctx);
}

    void IOContext::Initialize() {
    buf.Clear();
    if(ctx != nullptr) avio_context_free(&ctx);
    fileName = "";
    
    buf.Alloc(4096 + AVPROBE_PADDING_SIZE, 0);
    ctx = avio_alloc_context(buf, bufferSize, 0, (void*)this, IORead, 0, IOSeek);
}

int IOContext::Open(String fn) {
    Initialize();
    fileName = fn;
    return FileStream::Open(fn, FileStream::READ);
}

IOContext::~IOContext() {
    if(IsOpen()) Close();
    
    if(ctx != nullptr) {
        avio_context_free(&ctx);
    }
}

AVFormatContext* FormatContext::CreateContext(String file) {
    ctx = avformat_alloc_context();
    if(!ctx) throw Exc("Error creating avformat_alloc_context!");
    if(avformat_open_input(&ctx, file, nullptr, nullptr) != 0) throw Exc("Error opening file!");
    if(avformat_find_stream_info(ctx, nullptr) < 0) throw Exc("Error getting stream info!");

    return ctx;
}

void FormatContext::CreateCodecParms() {
    for(int i = 0; i < ctx->nb_streams; i++) {
        streams.Create(MediaStream(ctx->streams[i]));
    }
}

void FormatContext::Initialize() {
    if(ioCtx.IsOpen()) {
        ioCtx.Close();
    }
    
    streams.Clear();
    
    if(ctx != nullptr) {
        avformat_close_input(&ctx);
        avformat_free_context(ctx);
    }
    
    CreateContext();
}

MediaStream& FormatContext::operator[](int idx) {
    return streams[idx];
}

int FormatContext::FindBestVideoStream() {
    return av_find_best_stream(ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
}

int FormatContext::FindBestAudioStream() {
    return av_find_best_stream(ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
}

int FormatContext::ReadPacket(Packet& packet) {
    int response = av_read_frame(ctx, packet.packet);
    while(response > 0) response = av_read_frame(ctx, packet.packet);

    if(response >= 0)           return response;
    if(response == AVERROR_EOF) return response;
    if(response < 0)            throw Exc("Error reading frame!");
    return response;
}

void FormatContext::Seek(int64_t ts) {
    auto bestVideo = FindBestVideoStream();
    auto bestAudio = FindBestAudioStream();
    
    if(bestVideo >= 0) {
        int ret = av_seek_frame(ctx, bestVideo, ts, AVSEEK_FLAG_BACKWARD);
    }
    else {
        avformat_seek_file(ctx, bestAudio, ts, ts, ts, AVSEEK_FLAG_BACKWARD);
    }

    avformat_flush(ctx);
}

void FormatContext::SeekBegin() {
    auto bestVideo = FindBestVideoStream();
    auto bestAudio = FindBestAudioStream();
    
    if(bestVideo >= 0) {
        avformat_seek_file(ctx, bestVideo, 0, 0, streams[bestVideo].Duration(), 0);
    }
    else {
        avformat_seek_file(ctx, bestAudio, 0, 0, streams[bestAudio].Duration(), 0);
    }

    avformat_flush(ctx);
}

void FormatContext::CreateContext() {
    ctx = avformat_alloc_context();
    if(!ctx) throw Exc("Error creating avformat_alloc_context!");

}

void FormatContext::DestroyContext() {
    avformat_close_input(&ctx);
    avformat_free_context(ctx);
}

void FormatContext::Close() {
    avformat_close_input(&ctx);
}

bool FormatContext::IsOpen() {
    return ioCtx.IsOpen();
}

void FormatContext::Open(String filename) {
    Initialize();
    
    ioCtx.Open(filename);
    ctx->pb = ioCtx.ctx;
    ctx->flags |= AVFMT_FLAG_CUSTOM_IO;
    ioCtx.Seek(0);
    size_t len = ioCtx.Get64(ioCtx.buf, 4096);
    
    if(len == 0) return;
    ioCtx.Seek(0);
    
    AVProbeData probeData { "", ioCtx.buf, 4096, "" };
    ctx->iformat = av_probe_input_format(&probeData, 1);
    if(avformat_open_input(&ctx, "", nullptr, nullptr) != 0) throw Exc("Error opening file!");
    if(avformat_find_stream_info(ctx, nullptr) < 0) throw Exc("Error getting stream info!");
    CreateCodecParms();
}

FormatContext::~FormatContext() {
    if(ctx != nullptr) {
        avformat_close_input(&ctx);
        avformat_free_context(ctx);
    }
}

const void ScalerContext::ScaleFrame(const MediaFrame& frame, const ImageBuffer& buf) {
    int rgb32_stride[1] = { 4 * frame.Width() };
    uint8_t *rgb32[1] { (uint8_t*)buf.Begin() };
    sws_scale(ctx, frame.Data(), frame.LineSize(), 0, frame.Height(), rgb32, rgb32_stride);
}

ScalerContext::ScalerContext(Size src, enum AVPixelFormat srcFmt, ImageBuffer& buf) {
    buf.Create(src.cx, src.cy);
    ctx = sws_getCachedContext(ctx, src.cx, src.cy, srcFmt, src.cx, src.cy, AV_PIX_FMT_BGR32,
             SWS_BICUBIC, nullptr, nullptr, nullptr);
}

ScalerContext::ScalerContext(ScalerContext&& other) {
    ctx = other.ctx;
    other.ctx = nullptr;
}

ScalerContext& ScalerContext::operator=(ScalerContext&& other) {
    ctx = other.ctx;
    other.ctx = nullptr;
    return *this;
}

ScalerContext::~ScalerContext() {
    if(ctx != nullptr) sws_freeContext(ctx);
}

ResamplerContext::ResamplerContext(const CodecContext& cc) {
    ctx = swr_alloc_set_opts(
        nullptr,
        AV_CH_LAYOUT_STEREO,
        cc.SampleFormat(),
        cc.SampleRate(),
        cc.ChannelLayout(),
        cc.SampleFormat(),
        cc.SampleRate(),
        0,
        nullptr);
        
    swr_init(ctx);
}

void ResamplerContext::ConvertFrame(MediaFrame& frame) {
    MediaFrame temp;
    temp.frame->channel_layout = AV_CH_LAYOUT_STEREO;
    temp.frame->sample_rate    = frame.SampleRate();
    temp.frame->format         = frame.SampleFormat();
    temp.ts = frame.TS();
    
    swr_convert_frame(ctx, temp.frame, frame.frame);
    
    av_frame_free(&(frame.frame));
    frame = pick(temp);
}

ResamplerContext::ResamplerContext(ResamplerContext&& other) {
    ctx = other.ctx;
    other.ctx = nullptr;
}

ResamplerContext& ResamplerContext::operator=(ResamplerContext&& other) {
    ctx = other.ctx;
    other.ctx = nullptr;
    return *this;
}

ResamplerContext::~ResamplerContext() {
    if(ctx != nullptr) swr_free(&ctx);
}
    
void MediaDecoder::StopThreads() {
    isStopped = true;
    
    t.BeginShutdownThreads();
    while(t.GetCount()) { LocalLoop::ProcessEvents(); }
    vq.Clear();
    aq.Clear();
    
    t.EndShutdownThreads();
}

void MediaDecoder::Seek(int64_t ts) {
    auto wasStopped = isStopped;
    isStopped = true;
    StopThreads();
    avformat_flush(fc.ctx);
    
    for(auto& cc : codecs) {
        cc.Flush();
    }
    
    ts = av_rescale_q(ts, AVRational{1, 1000}, AV_TIME_BASE_Q);
    
    av_seek_frame(fc.ctx, -1, ts, AVSEEK_FLAG_BACKWARD);

    Run();
    lastTS = firstTimestamp;
    timer.Reset();
    
    if(!wasStopped) isStopped = false;
}

void MediaDecoder::Play() {
    timer.Reset();
    timer.Offset(lastTS);
    isStopped = false;
}

void MediaDecoder::Pause() {
    isStopped = true;
}

void MediaDecoder::Rewind() {
    Seek(0);
}

bool MediaDecoder::IsOpen() {
    return fc.IsOpen();
}

int MediaDecoder::Open(String fn) {
    fc.Open(fn);
    
    if(!fc.IsOpen()) return false;
    
    AVRational timebase;
    
    bestVideo = fc.FindBestVideoStream();
    if(bestVideo >= 0) {
        codecs.Create<CodecContext>(bestVideo, fc.streams[bestVideo]);
        MediaStream& s = fc.streams[bestVideo];
        videoSize = Size(s.Width(), s.Height());
        pixFmt    = s.PixelFormat();
        timebase  = s.TimeBase();
    }
    
    bestAudio = fc.FindBestAudioStream();
    if(bestAudio >= 0) {
        codecs.Create<CodecContext>(bestAudio, fc.streams[bestAudio]);
        WhenAudioInit(codecs.Get(bestAudio));
        if(bestVideo < 0) {
            MediaStream& s = fc.streams[bestAudio];
            timebase = s.TimeBase();
        }
    }
    return fc.IsOpen();
}

void MediaDecoder::Close() {
    fc.Close();
}

void MediaDecoder::DoDispatch() {
    dropnext:
    if(vq.GetCount() > 0) {
        {
            videoMutex.EnterRead();
            MediaFrame& vf = vq.Tail();
            
            if(vf.TS() <= timer.TS()) {
                if(timer.TS() - vf.TS() > 60) {
                    droppedFrames++;
                    vq.DropTail();
                    goto dropnext;
                }
                else {
                    WhenVideoFrame(vf);
                    lastTS = vf.TS();
                    vq.DropTail();
                }
            }
            videoMutex.LeaveRead();
        }
    }
}

void MediaDecoder::DoDispatch2() {
    if(aq.GetCount() > 0) {
        audioMutex.EnterRead();
        MediaFrame& af = aq.Tail();
        WhenAudioFrame(af);
        aq.DropTail();
        audioMutex.LeaveRead();
    }
}

void MediaDecoder::DoDecode() {
    while(vq.GetCount() + aq.GetCount() < 32) {
        auto result = fc.ReadPacket(packet);
        if(result < 0) break;
        
        int idx = codecs.Find(packet.StreamIndex());
        if(idx < 0) {
            packet.Drop();
            continue;
        }
        
        CodecContext& cc = codecs[idx];
        if(result == AVERROR_EOF) cc.Drain();
        
        MediaFrame f;
        result = cc.SendPacket(packet);
        
        int cresult = cc.ReceiveFrame(f);
        if(cresult == AVERROR(EAGAIN)) {
            f.Drop();
            packet.Drop();
            continue;
        }
        else if(cresult == AVERROR_EOF) cc.Flush();

        auto tb = fc.streams[packet.StreamIndex()].TimeBase();
        //f.ts = av_rescale_q(f.BestEffortTS(), tb, AV_TIME_BASE_Q);
        f.ts = av_rescale_q(f.BestEffortTS(), tb, AVRational{1, 1000});
        f.streamIndex = packet.StreamIndex();

        if(packet.StreamIndex() == bestVideo) {
            videoMutex.EnterWrite();
            vq.AddHead(pick(f));
            videoMutex.LeaveWrite();
        }
        else if(packet.StreamIndex() == bestAudio) {
            audioMutex.EnterWrite();
            aq.AddHead(pick(f));
            audioMutex.LeaveWrite();
        }
        
        packet.Drop();
    }
}

void MediaDecoder::LoadFirstFrame() {
    DoDecode();
    if(vq.GetCount() > 0) {
        MediaFrame& vf = vq.Tail();
        firstTimestamp = vf.ts;
        timer.Offset(vf.TS());
        WhenVideoFrame(vf);
    }
}

void MediaDecoder::StartThreads() {
    t.Run([&] {
        for(;;) {
            if(t.IsShutdownThreads()) return;
            if(!isStopped) DoDecode();
            Sleep(6);
        }
    }, false);
    
    t.Run([&] {
        for(;;) {
            if(t.IsShutdownThreads()) return;
            if(!isStopped) DoDispatch();
            Sleep(1);
        }
    }, false);
    
    t.Run([&] {
        for(;;) {
            if(t.IsShutdownThreads()) return;
            if(!isStopped) DoDispatch2();
            Sleep(1);
        }
    }, false);
    
}

void MediaDecoder::Run() {
    LoadFirstFrame();
    StartThreads();
}

MediaDecoder::~MediaDecoder() {
    StopThreads();
}
    
}