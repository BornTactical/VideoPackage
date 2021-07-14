#ifndef _VideoPackage_VideoPackage2_h_
#define _VideoPackage_VideoPackage2_h_
#include <CtrlCore/CtrlCore.h>
#include <Core/Core.h>
using namespace Upp;

#include <AudioPackage/AudioPackage.h>

extern "C" {
	#include <libavcodec/avcodec.h>
	#include <libavformat/avformat.h>
	#include <libavutil/avutil.h>
	#include <libavutil/imgutils.h>
	#include <libavutil/time.h>
	#include <libswscale/swscale.h>
	#include <libswresample/swresample.h>
	#include <libavutil/rational.h>
}

#ifdef av_err2str
#undef av_err2str
av_always_inline char* av_err2str(int errnum)
{
    // static char str[AV_ERROR_MAX_STRING_SIZE];
    // thread_local may be better than static in multi-thread circumstance
    thread_local char str[AV_ERROR_MAX_STRING_SIZE];
    memset(str, 0, sizeof(str));
    return av_make_error_string(str, AV_ERROR_MAX_STRING_SIZE, errnum);
}
#endif

		
constexpr uint64 operator"" _KB(uint64 kilobytes) { return kilobytes * 1024; }
constexpr uint64 operator"" _MB(uint64 megabytes) { return megabytes * 1024 * 1024; }
constexpr uint64 operator"" _GB(uint64 gigabytes) { return gigabytes * 1024 * 1024 * 1024; }
	    
constexpr PaSampleFormat ToPaSampleFormat(AVSampleFormat& fmt) {
	PaSampleFormat mappedFormat = 0;
	
	switch (fmt) {
		case AV_SAMPLE_FMT_NONE: mappedFormat = 0;         break;
		case AV_SAMPLE_FMT_U8:   mappedFormat = paUInt8;   break;
		case AV_SAMPLE_FMT_S16:  mappedFormat = paInt16;   break;
		case AV_SAMPLE_FMT_S32:  mappedFormat = paInt32;   break;
		case AV_SAMPLE_FMT_FLT:  mappedFormat = paFloat32; break;
		case AV_SAMPLE_FMT_DBL:  mappedFormat = paFloat32; break;
		case AV_SAMPLE_FMT_U8P:  mappedFormat = paUInt8 | paNonInterleaved; break;
		case AV_SAMPLE_FMT_S16P: mappedFormat = paInt16 | paNonInterleaved; break;
		case AV_SAMPLE_FMT_S32P: mappedFormat = paInt32 | paNonInterleaved; break;
		case AV_SAMPLE_FMT_FLTP: mappedFormat = paFloat32 | paNonInterleaved; break;
		case AV_SAMPLE_FMT_DBLP: mappedFormat = paFloat32 | paNonInterleaved; break;
		case AV_SAMPLE_FMT_S64:  mappedFormat = paCustomFormat; break;
		case AV_SAMPLE_FMT_S64P: mappedFormat = paCustomFormat | paNonInterleaved; break;
		case AV_SAMPLE_FMT_NB:   mappedFormat = 0; break;
	}
	
	return mappedFormat;
}

namespace Upp {
class Packet : Moveable<Packet> {
	AVPacket* packet { nullptr };
	
public:
	int   StreamIndex() const { return packet->stream_index; }
	int64 Duration()    const { return packet->duration; }
	int   Size()        const { return packet->size; }
	Packet();
	~Packet();
	void Drop();
	
	friend class FormatContext;
	friend class CodecContext;
};


enum class MediaFrameType {
	Video,
	Audio
};

class MediaFrame : MoveableAndDeepCopyOption<MediaFrame> {
	AVFrame*       frame { nullptr };
	int            streamIndex   { -1 };
	uint64         ts { 0 };
	
public:
	int     PacketSize()          const { return frame->pkt_size;  }
	AVPixelFormat PixelFormat()   const { return (AVPixelFormat) frame->format;    }
	AVSampleFormat SampleFormat() const { return (AVSampleFormat) frame->format;   }
	int64   Pts()                 const { return frame->pts;       }
	int     KeyFrame()            const { return frame->key_frame; }
	int     CodedPictureNumber()  const { return frame->coded_picture_number; }
	char    PictureType()         const { return av_get_picture_type_char(frame->pict_type); }
	int     Width()               const { return frame->width;     }
	int     Height()              const { return frame->height;    }
	Size    GetSize()             const { return Size(frame->width, frame->height); }
	uint8** Data()                const { return frame->data;      }
	uint8** ExtendedData()        const { return frame->extended_data; }
	int*    LineSize()            const { return frame->linesize;  }
	int     DecodeErrorFlags()    const { return frame->decode_error_flags; }
	uint64  BestEffortTS()        const { return frame->best_effort_timestamp; }
	int     Samples()             const { return frame->nb_samples; }
	int     SampleRate()          const { return frame->sample_rate; }
	int     Channels()            const { return frame->channels; }
	uint64  ChannelLayout()       const { return frame->channel_layout;}
	int     BytesPerSample()      const { return av_get_bytes_per_sample((AVSampleFormat)frame->format); }
	int     StreamIndex()         const { return streamIndex; }
	bool    IsValid()             const { return frame != nullptr; }
	int     ByteSize()            const { return (Width() * Height() * 4) + (BytesPerSample() * Samples() * Channels()); };
	uint64  TS()                  const { return ts; };
	
	MediaFrame(AVFrame* framea);
	MediaFrame();
	void Drop();
	~MediaFrame();
	MediaFrame(MediaFrame&& other);
	MediaFrame& operator=(MediaFrame&& other);
	MediaFrame(const MediaFrame& f, int deep);
	String ToString() const;
	
	friend class CodecContext;
	friend class ScalerContext;
	friend class ResamplerContext;
	friend class MediaDecoder;
};

class MediaStream : Moveable<MediaStream> {
	AVStream* stream { nullptr };
	
public:
	int  Width()       const { return stream->codecpar->width;  }
	int  Height()      const { return stream->codecpar->height; }
	Size GetSize()     const { return Size(stream->codecpar->width, stream->codecpar->height); }
	int  SampleRate()  const { return stream->codecpar->sample_rate; }
	bool IsAudio()     const { return stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO; }
	bool IsVideo()     const { return stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO; }
	int  Index()       const { return stream->index; }
	int  Duration()    const { return stream->duration; }
	
	AVCodecParameters* Parms()       const { return stream->codecpar; };
	AVCodecID          CodecId()     const { return stream->codecpar->codec_id;   }
	AVMediaType        CodecType()   const { return stream->codecpar->codec_type; }
	AVRational&        TimeBase()    const { return stream->time_base;            }
	AVPixelFormat      PixelFormat() const { return (AVPixelFormat)stream->codecpar->format; }
	
	MediaStream(AVStream* stream) : stream(stream) { }
	~MediaStream() {}
	MediaStream() = default;
	MediaStream(MediaStream&&) = default;
	MediaStream& operator=(MediaStream&&) = default;
	
	friend class CodecContext;
};

class CodecContext : Moveable<CodecContext> {
	AVCodecContext* ctx        { nullptr };
	AVCodec*        codec      { nullptr };
	char*           err        { nullptr };
	bool            isDraining {  false  };
	
public:
	int64           FrameNumber()    const { return ctx->frame_number; }
	AVRational      FrameRate()      const { return ctx->framerate;    }
	AVRational      TimeBase()       const { return ctx->time_base;    }
	AVSampleFormat& SampleFormat()   const { return ctx->sample_fmt;   }
	int             SampleRate()     const { return ctx->sample_rate; }
	int             BytesPerSample() const { return av_get_bytes_per_sample(ctx->sample_fmt); }
	uint64          ChannelLayout()  const { return ctx->channel_layout;}
	int             Channels()       const { return ctx->channels; }
	int             FrameSize()      const { return ctx->frame_size; }
	String          CodecName()      const { return codec->name;       }
	int             CodedId()        const { return codec->id;         }
	AVMediaType     CodecType()      const { return ctx->codec_type;   }
	bool            IsAudio()        const { return ctx->codec_type == AVMEDIA_TYPE_AUDIO; }
	bool            IsVideo()        const { return ctx->codec_type == AVMEDIA_TYPE_VIDEO; }
	
	int    Decode(Packet& packet, MediaFrame& frame);
	int    SendPacket(Packet& packet);
	int    ReceiveFrame(MediaFrame& frame);
	void   Drain();
	String ToString();
	bool   IsValid();
	void   Flush();
	CodecContext(MediaStream& stream);
	CodecContext()                          = default;
	CodecContext(CodecContext&&)            = default;
	CodecContext& operator=(CodecContext&&) = default;
	~CodecContext();
};

static int     IORead(void* data, uint8  *buf, int bufSize);
static int64_t IOSeek(void* data, int64_t pos, int whence);

class IOContext : public FileIn, MoveableAndDeepCopyOption<IOContext> {
	AVIOContext*    ctx        { nullptr };
	Buffer<uint8_t> buf;
	int             bufferSize { 4096 };
	String          fileName;
	
public:
	void Initialize();
	int Open(String fn);
	IOContext() = default;
	~IOContext();

	friend class FormatContext;
	friend class MediaDecoder;
};

static int IORead(void* data, uint8_t *buf, int bufSize) {
	IOContext* ctx = (IOContext*)data;
	int64 len = ctx->Get64((uint8_t*)buf, bufSize);
		
	if(len == 0) {
		return AVERROR_EOF;
	}
	
	return len;
}

static int64_t IOSeek(void* data, int64_t pos, int whence) {
	IOContext* ctx = (IOContext*)data;
	
	if(whence == AVSEEK_SIZE)  {
		return ctx->GetSize();
	}
	else if(whence == SEEK_SET) {
		ctx->Seek(pos);
	}
	else if(whence == SEEK_CUR) {
		ctx->SeekCur(whence);
	}
	else if(whence == SEEK_END) {
		ctx->SeekEnd(whence);
	}
	
	return ctx->GetPos();
}

using MediaStreams = Vector<MediaStream>;
	
class FormatContext {
protected:
	IOContext        ioCtx;
	AVFormatContext* ctx { nullptr };
	MediaStreams     streams;
	AVFormatContext* CreateContext(String file);
	void             CreateCodecParms();
public:
	String            Name()          const   { return ctx->iformat->name; }
	int64             Duration()      const   { return ctx->duration;      }
	int64             BitRate()       const   { return ctx->bit_rate;      }
	int               Count()         const   { return ctx->nb_streams;    }
	MediaStreams&     GetStreams()    const   { return const_cast<MediaStreams&>(streams); }
	int64             FileSize()      const   { return ioCtx.GetSize();    }
	int64             FilePos()       const   { return ioCtx.GetPos();     }

	void Initialize();
	MediaStream& operator[](int idx);
	int FindBestVideoStream();
	int FindBestAudioStream();
	int ReadPacket(Packet& packet);
	void Seek(int64_t ts);
	void SeekBegin();
	void CreateContext();
	void DestroyContext();
	void Close();
	bool IsOpen();
	void Open(String filename);
	~FormatContext();
	FormatContext() = default;
    friend class MediaDecoder;
};

class ScalerContext {
	SwsContext* ctx { nullptr };
	
public:
	const void ScaleFrame(const MediaFrame& frame, const ImageBuffer& buf);
	ScalerContext(Size src, enum AVPixelFormat srcFmt, ImageBuffer& buf);
	ScalerContext(ScalerContext&& other);
	ScalerContext& operator=(ScalerContext&& other);
	ScalerContext() = default;
	~ScalerContext();
};

class VolumeFilter {
	
};

class ResamplerContext {
	SwrContext* ctx { nullptr };
	
public:
	ResamplerContext(const CodecContext& cc);
	void ConvertFrame(MediaFrame& frame);
	ResamplerContext(ResamplerContext&& other);
	ResamplerContext& operator=(ResamplerContext&& other);
	ResamplerContext() = default;
	~ResamplerContext();
};

class MediaTimer {
	uint64_t start  { 0 };
	uint64_t offset { 0 };
	uint64_t stop   { 0 };
	
public:
	void     Offset(uint64_t off) { offset = off; }
	void     Reset()              { start = av_rescale_q(av_gettime(), AV_TIME_BASE_Q, AVRational{1, 1000}); }
	uint64_t TS()                 { return av_rescale_q(av_gettime(), AV_TIME_BASE_Q, AVRational{1, 1000}) - start + offset; }
	
	MediaTimer() {
		Reset();
	}
};

using FrameEvent     = Event<const MediaFrame&>;
using FrameEvent     = Event<const MediaFrame&>;
using InitAudioEvent = Event<const CodecContext&>;

class MediaDecoder {
	ArrayMap<int, CodecContext> codecs;
	FormatContext fc;
	Packet        packet;
	int           bestVideo     {  -1   };
	int           bestAudio     {  -1   };
	int           droppedFrames {   0   };
	bool		  isStopped     { true  };
	Atomic        running       { false };
	
	Mutex         audioMutex;
	Mutex         videoMutex;
	Thread        t;
	Size          videoSize;
	AVPixelFormat pixFmt;
	uint64        reliableDuration { 0 };
	uint64        firstTimestamp   { 0 };
	uint64        lastTS           { 0 };

public:
	MediaTimer    timer;
	
	BiVector<MediaFrame> vq;
	BiVector<MediaFrame> aq;
	
	Event<MediaFrame&>         WhenAudioFrame;
	Event<const MediaFrame&>   WhenVideoFrame;
	Event<const CodecContext&> WhenAudioInit;
	Event<const int64>         WhenVideoPosition;
	
	Size          VideoSize()     const { return videoSize;      }
	AVPixelFormat PixelFormat()   const { return pixFmt;         }
	int64         FileSize()      const { return fc.FileSize();  }
	int64         FilePos()       const { return fc.FilePos();   }
	uint64        Duration()      const { return av_rescale_q(fc.Duration(), AV_TIME_BASE_Q, AVRational{1, 1000});  }
	int64         FirstTS()       const { return firstTimestamp; }
	int           DroppedFrames() const { return droppedFrames;  }
	bool          HasVideo()      const { return bestVideo >= 0; }

	void StopThreads();
	void Seek(int64_t ts);
	void Play();
	void Pause();
	void Rewind();
	bool IsOpen();
	int  Open(String fn);
	void Close();
	void DoDispatch();
	void DoDispatch2();
	void DoDecode();
	void LoadFirstFrame();
	void StartThreads();
	void Run();
	~MediaDecoder();
	MediaDecoder() = default;
};


}

#endif
