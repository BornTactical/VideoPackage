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
class MediaTimer {
	int64_t start {0};
	int64_t stop  {0};
	bool running { false };
	
public:
	void    Start()   { start = av_gettime(); running = true;}
	void    Stop()    { stop = TS(); running = false; }
	void    Restart() { start = av_gettime() - stop; running = true; }
	void    Clear()   { start = 0; stop = 0; }
	int64_t TS()      { if(running) return av_gettime() - start; else return 0; }
	
	MediaTimer() {
		start = av_gettime();
	}
};

class Packet : Moveable<Packet> {
	AVPacket* packet { nullptr };
	
public:
	int   StreamIndex() const { return packet->stream_index; }
	int64 Duration()    const { return packet->duration; }
	int   Size()        const { return packet->size; }
	
	Packet() {
		packet = av_packet_alloc();
		if(!packet) throw Exc("Failed to allocate memory for AVPacket!");
	}
	
	~Packet() {
		av_packet_unref(packet);
		av_packet_free(&packet);
	}
	
	void Drop() {
		av_packet_unref(packet);
	}
	
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
	MediaFrameType type;
	uint64         ts;
	
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
	
	MediaFrame(AVFrame* framea) {
		frame = framea;
	}
	
	MediaFrame() {
		frame = av_frame_alloc();
		if(!frame) throw Exc("Failed to allocate memory for AVFrame!");
	}
	
	void Drop() {
		av_frame_unref(frame);
	}
	
	~MediaFrame() {
		if(frame != nullptr) {
			//av_frame_unref(frame);
			av_frame_free(&frame);
		}
	}
	
	MediaFrame(MediaFrame&& other) {
		streamIndex = other.streamIndex;
		frame = other.frame;
		other.frame = nullptr;
		ts = other.ts;
	}
	
	MediaFrame& operator=(MediaFrame&& other) {
		streamIndex = other.streamIndex;
		frame = other.frame;
		other.frame = nullptr;
		ts = other.ts;
		return *this;
	}
   
	MediaFrame(const MediaFrame& f, int deep) :
		frame(f.frame),
		streamIndex(f.streamIndex) {
			
		}
	
	String ToString() const {
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
	
	friend class CodecContext;
	friend class ScalerContext;
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
	
	MediaStream(AVStream* stream) : stream(stream) {
		
	}
	
	~MediaStream() {
	}
	
	MediaStream() = default;
	MediaStream(MediaStream&&) = default;
	MediaStream& operator=(MediaStream&&) = default;
	
	friend class CodecContext;
};

class CodecContext : Moveable<CodecContext> {
	AVCodecContext* ctx { nullptr };
	AVCodec* codec { nullptr };
	char* err;
	bool isDraining {false};
	
public:
	
	int Decode(Packet& packet, MediaFrame& frame) {
		int response = avcodec_send_packet(ctx, packet.packet);
		if(response < 0) throw Exc("Error sending packet for decoding!");
		
		response = avcodec_receive_frame(ctx, frame.frame);
		if(response == AVERROR_EOF || response == AVERROR(EAGAIN)) return response;
		else if(response == AVERROR_INVALIDDATA) throw Exc("Codec failed to open!");
		else if(response < 0) throw Exc("Decoding error!");

		return response;
	}
	
	int SendPacket(Packet& packet) {
		return avcodec_send_packet(ctx, packet.packet);
	}

	int ReceiveFrame(MediaFrame& frame) {
		return avcodec_receive_frame(ctx, frame.frame);
	}
	
	void Drain() {
		int response = avcodec_send_packet(ctx, nullptr);
	}
	
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
	
	String ToString() {
		return Format("Number: %d\nRate:%d:%d\nTime base:%d:%d",
			FrameNumber(),
			FrameRate().num, FrameRate().den,
			TimeBase().num, TimeBase().den
		);
	}
	
	bool IsValid() {
		return ctx != nullptr;
	}
	
	void Flush() {
		avcodec_flush_buffers(ctx);
	}
	
	CodecContext(MediaStream& stream) {
		codec = avcodec_find_decoder(stream.CodecId());
		if(!codec) throw Exc("Unsupported codec!");
		
	    ctx = avcodec_alloc_context3(codec);
		if(!ctx) throw Exc("Failed to allocated memory for AVCodecContext");
		if(avcodec_parameters_to_context(ctx, stream.Parms()) < 0)
			throw Exc("Failed to copy codec parameters to codec context");
		if(avcodec_open2(ctx, codec, nullptr) < 0)
			throw Exc("Failed to open codec through avcodec_open2");
	}
	
	CodecContext() = default;
	CodecContext(CodecContext&&) = default;
	CodecContext& operator=(CodecContext&&) = default;
	
	~CodecContext() {
		avcodec_close(ctx);
		avcodec_free_context(&ctx);
	}
};

static int IORead(void* data, uint8 *buf, int bufSize);
static int64_t IOSeek(void* data, int64_t pos, int whence);

class IOContext : public FileIn, MoveableAndDeepCopyOption<IOContext> {
	AVIOContext* ctx { nullptr };
	Buffer<uint8_t> buf;
	int bufferSize {4096};
	String fileName;
	
public:
	void Initialize() {
		buf.Clear();
		if(ctx != nullptr) avio_context_free(&ctx);
		fileName = "";
		
		buf.Alloc(4096 + AVPROBE_PADDING_SIZE, 0);
		ctx = avio_alloc_context(buf, bufferSize, 0, (void*)this, IORead, 0, IOSeek);
	}
	
	int Open(String fn) {
		Initialize();
		fileName = fn;
		return FileStream::Open(fn, FileStream::READ);
	}
	
	IOContext() = default;

	~IOContext() {
		if(IsOpen()) Close();
		
		if(ctx != nullptr) {
			avio_context_free(&ctx);
		}
	}

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
	IOContext ioCtx;
	AVFormatContext* ctx { nullptr };
	MediaStreams streams;
	
	AVFormatContext* CreateContext(String file) {
		ctx = avformat_alloc_context();
		if(!ctx) throw Exc("Error creating avformat_alloc_context!");
		if(avformat_open_input(&ctx, file, nullptr, nullptr) != 0) throw Exc("Error opening file!");
		if(avformat_find_stream_info(ctx, nullptr) < 0) throw Exc("Error getting stream info!");

		return ctx;
	}
	
	void CreateCodecParms() {
		for(int i = 0; i < ctx->nb_streams; i++) {
			streams.Create(MediaStream(ctx->streams[i]));
		}
	}
	
public:
	void Initialize() {
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
	
	String            Name()          const   { return ctx->iformat->name; }
	int64             Duration()      const   { return ctx->duration;      }
	int64             BitRate()       const   { return ctx->bit_rate;      }
	int               Count()         const   { return ctx->nb_streams;    }
	MediaStreams&     GetStreams()    const   { return const_cast<MediaStreams&>(streams); }
	int64             FileSize()      const   { return ioCtx.GetSize();    }
	int64             FilePos()       const   { return ioCtx.GetPos();     }
	
	MediaStream& operator[](int idx) {
		return streams[idx];
	}
	
	int FindBestVideoStream() {
		return av_find_best_stream(ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
	}
	
	int FindBestAudioStream() {
		return av_find_best_stream(ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
	}
	
	int ReadPacket(Packet& packet) {
		int response = av_read_frame(ctx, packet.packet);
		while(response > 0) response = av_read_frame(ctx, packet.packet);

		if(response >= 0) return response;
		if(response == AVERROR_EOF) return response;
		if(response < 0) throw Exc("Error reading frame!");
		return response;
	}
	
	void SeekBegin() {
		for(auto& stream : streams) {
			avio_seek(ioCtx.ctx, 0, SEEK_SET);
			avformat_seek_file(ctx, stream.Index(), 0, 0, stream.Duration(), 0);
		}
	}
	
	void CreateContext() {
		ctx = avformat_alloc_context();
		if(!ctx) throw Exc("Error creating avformat_alloc_context!");

	}
	
	void DestroyContext() {
		avformat_close_input(&ctx);
		avformat_free_context(ctx);
	}
	
	void Close() {
		avformat_close_input(&ctx);
	}
	
	bool IsOpen() {
		return ioCtx.IsOpen();
	}
	
	void Open(String filename) {
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
		SeekBegin();
	}
	
	FormatContext() = default;
    ~FormatContext() {
        if(ctx != nullptr) {
	        avformat_close_input(&ctx);
	        avformat_free_context(ctx);
        }
    }

    friend class MediaDecoder;
};

class ScalerContext {
	SwsContext* ctx { nullptr };
	
public:
	const void ScaleFrame(const MediaFrame& frame, const ImageBuffer& buf) {
		ASSERT(buf.GetSize().cx != 0);
		ASSERT(!buf.IsEmpty());
		
		int rgb32_stride[1] = { 4 * frame.Width() };
		uint8_t *rgb32[1] { (uint8_t*)buf.Begin() };
		sws_scale(ctx, frame.Data(), frame.LineSize(), 0, frame.Height(), rgb32, rgb32_stride);
	}
	
	ScalerContext(Size src, enum AVPixelFormat srcFmt, ImageBuffer& buf) {
		buf.Create(src.cx, src.cy);
		ctx = sws_getContext(src.cx, src.cy, srcFmt, src.cx, src.cy, AV_PIX_FMT_RGB32,
			     SWS_BICUBIC, nullptr, nullptr, nullptr);
	}
	
/*	ScalerContext(Size src, enum AVPixelFormat srcFmt, Size dest, enum AVPixelFormat destFmt,
	        int flags, SwsFilter* srcFilter = nullptr, SwsFilter* destFilter = nullptr, const double* param = nullptr) {
		ctx = sws_getCachedContext(ctx, src.cx, src.cy, srcFmt, dest.cx, dest.cy, destFmt, flags, srcFilter, destFilter, param);
	}
	*/
	
	ScalerContext(ScalerContext&& other) {
		ctx = other.ctx;
		other.ctx = nullptr;
	}
	
	ScalerContext& operator=(ScalerContext&& other) {
		ctx = other.ctx;
		other.ctx = nullptr;
		return *this;
	}
   
	ScalerContext() = default;
	
	~ScalerContext() {
		if(ctx != nullptr)
		sws_freeContext(ctx);
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
	bool          running       { false };
	
	Mutex         audioMutex;
	Mutex         videoMutex;
	Thread        t, t2;
	Size          videoSize;
	AVPixelFormat pixFmt;
	
	MediaTimer    timer;
	
	BiVector<MediaFrame> vq;
	BiVector<MediaFrame> aq;
	
	void Initialize() {
		Pause();
		StopThreads();
		codecs.Clear();
		vq.Clear();
		aq.Clear();
		timer.Clear();
		bestVideo = -1;
		bestAudio = -1;
		droppedFrames = 0;
		isStopped = true;
		running = false;
		videoSize = {0, 0};
	}

public:
	Event<const MediaFrame&>   WhenAudioFrame;
	Event<const MediaFrame&>   WhenVideoFrame;
	Event<const CodecContext&> WhenAudioInit;
	Event<const int64>         WhenFilePosition;
	
	Size          VideoSize()   const { return videoSize; }
	AVPixelFormat PixelFormat() const { return pixFmt;  }
	int64         FileSize()    const { return fc.FileSize(); }
	int64         FilePos()     const { return fc.FilePos(); }
	int64         Duration()    const { return fc.Duration(); }
		
	int DroppedFrames() const { return droppedFrames; }
	
	bool HasVideo() const {
		return bestVideo >= 0;
	}
	
	void StopThreads() {
		running = false;
		INTERLOCKED_(videoMutex) {
			INTERLOCKED_(audioMutex) {
			t.Wait();
			t2.Wait();
			}
		}
	}
	
	void Play() {
		if(isStopped) timer.Restart();
		isStopped = false;
	}
	
	void Pause() {
		timer.Stop();
		isStopped = true;
	}
	
	void Rewind() {
		Pause();
		INTERLOCKED_(videoMutex) {
			INTERLOCKED_(audioMutex) {
				aq.Clear();
				vq.Clear();
			}
		}
		fc.SeekBegin();
		timer.Clear();
	}
	
	bool IsOpen() {
		return fc.IsOpen();
	}
	
	int Open(String fn) {
		Initialize();
		fc.Open(fn);
		
		if(!fc.IsOpen()) return false;
		
		bestVideo = fc.FindBestVideoStream();
		if(bestVideo >= 0) {
			codecs.Create<CodecContext>(bestVideo, fc.streams[bestVideo]);
			MediaStream& s = fc.streams[bestVideo];
			videoSize = Size(s.Width(), s.Height());
			pixFmt    = s.PixelFormat();
		}
		
		bestAudio = fc.FindBestAudioStream();
		if(bestAudio >= 0) {
			codecs.Create<CodecContext>(bestAudio, fc.streams[bestAudio]);
			WhenAudioInit(codecs.Get(bestAudio));
		}
	
		return fc.IsOpen();
	}
	
	void Close() {
		fc.Close();
	}
	
	void DoDispatch() {
		auto beforeTime = timer.TS();
			
		if(vq.GetCount() > 0) {
			INTERLOCKED_(videoMutex) {
				MediaFrame& vf = vq.Tail();
				
				if(vf.TS() <= timer.TS()) {
					if(timer.TS() - vf.TS() > 60000) {
						droppedFrames++;
						vq.DropTail();
					}
					else {
						WhenVideoFrame(vf);
						vq.DropTail();
					}
				}
			}
		}
		
		if(aq.GetCount() > 0) {
			INTERLOCKED_(audioMutex) {
				MediaFrame& af = aq.Tail();
				
				WhenAudioFrame(af);
				aq.DropTail();
			}
		}
	}
	
	void DoDecode() {
		while(vq.GetCount() + aq.GetCount() < 100 ) {
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
			f.ts = av_rescale_q(f.BestEffortTS(), tb, (AVRational){1, 1000000});
			f.streamIndex = packet.StreamIndex();
	
			if(packet.StreamIndex() == bestVideo) {
				INTERLOCKED_(videoMutex) {
					vq.AddHead(pick(f));
				}
			}
			else if(packet.StreamIndex() == bestAudio) {
				 INTERLOCKED_(audioMutex) {
					aq.AddHead(pick(f));
				}
			}
			
			packet.Drop();
		}
	}
	
	void Run() {
		running = true;
		
		DoDecode();
		if(vq.GetCount() > 0) {
			MediaFrame& vf = vq.Tail();
			WhenVideoFrame(vf);
		}
		
		t.Start([&] {
			while(running) {
				if(!isStopped) {
					DoDecode();
					Sleep(1);
				}
				
				Sleep(1);
			}
		});
		
		t2.Start([&] {
			while(running) {
				if(!isStopped) {
					DoDispatch();
					Sleep(1);
				}
				
				Sleep(1);
			}
		});
	}
	
	~MediaDecoder() {
		StopThreads();
	}
	

	
	MediaDecoder() = default;
};


}

#endif
