#include <cstdint>
#include <cstddef>
#include "OpenEXR/ImathVec.h"
#include "OpenEXR/ImathBox.h"
#include "OpenEXR/ImfPixelType.h"
#include "OpenEXR/ImfChannelList.h"
#include "OpenEXR/ImfHeader.h"
#include "OpenEXR/ImfFrameBuffer.h"
#include "OpenEXR/ImfOutputFile.h"
#include "OpenEXR/ImfInputFile.h"

using namespace IMATH_NAMESPACE;
using namespace Imf;

extern "C" {
    // PixelType
    // This is a stand-in for an enum from the C++ library.
    enum CEXR_PixelType {
        UINT   = 0,		// unsigned int (32 bit)
        HALF   = 1,		// half (16 bit floating point)
        FLOAT  = 2,		// float (32 bit floating point)
    };

    // CompressionMethod
    // This is a stand-in for an enum from the C++ library.
    enum CEXR_CompressionMethod {
        NO_COMPRESSION  = 0,	// no compression
        RLE_COMPRESSION = 1,	// run length encoding
        ZIPS_COMPRESSION = 2,	// zlib compression, one scan line at a time
        ZIP_COMPRESSION = 3,	// zlib compression, in blocks of 16 scan lines
        PIZ_COMPRESSION = 4,	// piz-based wavelet compression
        PXR24_COMPRESSION = 5,	// lossy 24-bit float compression
        B44_COMPRESSION = 6,	// lossy 4-by-4 pixel block compression,
        				        // fixed compression rate
        B44A_COMPRESSION = 7,	// lossy 4-by-4 pixel block compression,
        				        // flat fields are compressed more
        DWAA_COMPRESSION = 8,       // lossy DCT based compression, in blocks
                                    // of 32 scanlines. More efficient for partial
                                    // buffer access.
        DWAB_COMPRESSION = 9,       // lossy DCT based compression, in blocks
                                    // of 256 scanlines. More efficient space
                                    // wise and faster to decode full frames
                                    // than DWAA_COMPRESSION.
    };

    enum CEXR_LineOrder {
        INCREASING_Y = 0,	// first scan line has lowest y coordinate
        DECREASING_Y = 1,	// first scan line has highest y coordinate
        RANDOM_Y = 2,       // only for tiled files; tiles are written
        			        // in random order
    };

    // Channel
    // This isn't a wrapper per se, but an separate representation for
    // passing to/from Rust.
    struct CEXR_Channel {
        CEXR_PixelType pixel_type;
        int x_sampling;
        int y_sampling;
        int p_linear; // bool
    };
};


//------------------------------------------------------------------------------
// Channel iterator
extern "C" {
    struct CEXR_ChannelIterator {
        void *begin;
        void *end;
    };

    void CEXR_ChannelIterator_delete(
        CEXR_ChannelIterator *iterator);

    const char * CEXR_ChannelIterator_next(
        CEXR_ChannelIterator* iterator);
};

void CEXR_ChannelIterator_delete(CEXR_ChannelIterator *iterator) {
    auto begin_ptr = reinterpret_cast<ChannelList::Iterator*>(iterator->begin);
    auto end_ptr = reinterpret_cast<ChannelList::Iterator*>(iterator->end);
    delete begin_ptr;
    delete end_ptr;
}

// Returns nullptr if no more channels
const char * CEXR_ChannelIterator_next(CEXR_ChannelIterator* iterator) {
    auto &begin = *reinterpret_cast<ChannelList::Iterator*>(iterator->begin);
    auto &end = *reinterpret_cast<ChannelList::Iterator*>(iterator->end);

    if (begin == end) {
        return nullptr;
    }

    auto name = begin.name();
    begin++;
    return name;
}


//------------------------------------------------------------------------------
// EXR header type.
extern "C" {
    struct CEXR_Header {
        void *header;
    };

    CEXR_Header CEXR_Header_new(
        int display_window_min_x,
        int display_window_min_y,
        int display_window_max_x,
        int display_window_max_y,
        int data_window_min_x,
        int data_window_min_y,
        int data_window_max_x,
        int data_window_max_y,
        float pixel_aspect_ratio,
        float screen_window_center_x,
        float screen_window_center_y,
        float screen_window_width,
        CEXR_LineOrder line_order,
        CEXR_CompressionMethod compression);

    void CEXR_Header_delete(
        CEXR_Header *header);

    void CEXR_Header_insert_channel(
        CEXR_Header *header,
        const char name[],
        const CEXR_Channel channel);

    int CEXR_Header_channel_exists(
        CEXR_Header *header,
        const char name[]);

    CEXR_Channel CEXR_Header_get_channel(
        CEXR_Header *header,
        const char name[]);

    CEXR_ChannelIterator CEXR_Header_new_channel_iterator(
        CEXR_Header *header);
};

CEXR_Header CEXR_Header_new(
    int display_window_min_x,
    int display_window_min_y,
    int display_window_max_x,
    int display_window_max_y,
    int data_window_min_x,
    int data_window_min_y,
    int data_window_max_x,
    int data_window_max_y,
    float pixel_aspect_ratio,
    float screen_window_center_x,
    float screen_window_center_y,
    float screen_window_width,
    CEXR_LineOrder line_order,
    CEXR_CompressionMethod compression
) {
    CEXR_Header header;

    header.header = new Header(
        Box2i(V2i(display_window_min_x, display_window_min_y), V2i(display_window_max_x, display_window_max_y)),
        Box2i(V2i(data_window_min_x, data_window_min_y), V2i(data_window_max_x, data_window_max_y)),
        pixel_aspect_ratio,
        V2f(screen_window_center_x, screen_window_center_y),
        screen_window_width,
        static_cast<LineOrder>(line_order),
        static_cast<Compression>(compression));
    return header;
}

void CEXR_Header_delete(CEXR_Header *header) {
    auto h = reinterpret_cast<Header*>(header->header);
    delete h;
}

void CEXR_Header_insert_channel(CEXR_Header *header, const char name[], const CEXR_Channel channel) {
    auto h = reinterpret_cast<Header*>(header->header);
    h->channels().insert(
        name,
        Channel(
            static_cast<PixelType>(channel.pixel_type),
            channel.x_sampling,
            channel.y_sampling,
            channel.p_linear));
}

int CEXR_Header_channel_exists(CEXR_Header *header, const char name[]) {
    auto h = reinterpret_cast<Header*>(header->header);
    h->channels().findChannel(name) != 0;
}

CEXR_Channel CEXR_Header_get_channel(CEXR_Header *header, const char name[]) {
    auto h = reinterpret_cast<Header*>(header->header);
    auto chan = h->channels().findChannel(name);

    CEXR_Channel channel;
    channel.pixel_type = static_cast<CEXR_PixelType>(chan->type);
    channel.x_sampling = chan->xSampling;
    channel.y_sampling = chan->ySampling;
    channel.p_linear = chan->pLinear;

    return channel;
}

CEXR_ChannelIterator CEXR_Header_new_channel_iterator(CEXR_Header *header) {
    auto h = reinterpret_cast<Header*>(header->header);

    CEXR_ChannelIterator channel_iter;
    channel_iter.begin = reinterpret_cast<void*>(new auto(h->channels().begin()));
    channel_iter.end = reinterpret_cast<void*>(new auto(h->channels().end()));

    return channel_iter;
}


//------------------------------------------------------------------------------
// FrameBuffer
extern "C" {
    struct CEXR_FrameBuffer {
        void *frame_buffer;
    };

    CEXR_FrameBuffer CEXR_FrameBuffer_new();

    void CEXR_FrameBuffer_delete(
        CEXR_FrameBuffer *frame_buffer);

    void CEXR_FrameBuffer_insert_slice(
        CEXR_FrameBuffer *frame_buffer,
        const char name[],
        char *base,
        size_t x_stride,
        size_t y_stride,
        int x_sampling,
        int y_sampling,
        double fill_value,
        int x_tile_coords, // bool
        int y_tile_coords // bool
        );
};

CEXR_FrameBuffer CEXR_FrameBuffer_new() {
    CEXR_FrameBuffer buffer;

    buffer.frame_buffer = reinterpret_cast<void*>(new FrameBuffer());

    return buffer;
}

void CEXR_FrameBuffer_delete(CEXR_FrameBuffer *frame_buffer) {
    auto buffer = reinterpret_cast<FrameBuffer*>(frame_buffer->frame_buffer);
    delete buffer;
}

void CEXR_FrameBuffer_insert_slice(
    CEXR_FrameBuffer *frame_buffer,
    const char name[],
    CEXR_PixelType pixel_type,
    char *base,
    size_t x_stride,
    size_t y_stride,
    int x_sampling,
    int y_sampling,
    double fill_value,
    int x_tile_coords, // bool
    int y_tile_coords // bool
) {
    auto buffer = reinterpret_cast<FrameBuffer*>(frame_buffer->frame_buffer);

    Slice slice;
    slice.type = static_cast<PixelType>(pixel_type);
    slice.base = base;
    slice.xStride = x_stride;
    slice.yStride = y_stride;
    slice.xSampling = x_sampling;
    slice.ySampling = y_sampling;
    slice.fillValue = fill_value;
    slice.xTileCoords = x_tile_coords;
    slice.yTileCoords = y_tile_coords;

    buffer->insert(name, slice);
}


//------------------------------------------------------------------------------
// OutputFile
extern "C" {
    struct CEXR_OutputFile {
        void *output_file;
    };

    CEXR_OutputFile CEXR_OutputFile_new(
        const char * file_name,
        const CEXR_Header *header,
        int num_threads);

    void CEXR_OutputFile_delete(
        CEXR_OutputFile *output_file);

    void CEXR_OutputFile_set_frame_buffer(
        CEXR_OutputFile* output_file,
        CEXR_FrameBuffer* frame_buffer);

    void CEXR_OutputFile_write_pixels(
        CEXR_OutputFile* output_file,
        int num_scan_lines);
};

CEXR_OutputFile CEXR_OutputFile_new(const char * file_name, const CEXR_Header *header, int num_threads) {
    CEXR_OutputFile output_file;

    output_file.output_file = reinterpret_cast<void*>(new OutputFile(
        file_name,
        *reinterpret_cast<Header*>(header->header),
        num_threads
    ));

    return output_file;
}

void CEXR_OutputFile_delete(CEXR_OutputFile *output_file) {
    auto outfile = reinterpret_cast<OutputFile*>(output_file->output_file);

    delete outfile;
}

void CEXR_OutputFile_set_frame_buffer(CEXR_OutputFile* output_file, CEXR_FrameBuffer* frame_buffer) {
    auto outfile = reinterpret_cast<OutputFile*>(output_file->output_file);
    auto framebuf = reinterpret_cast<FrameBuffer*>(frame_buffer->frame_buffer);

    outfile->setFrameBuffer(*framebuf);
}

void CEXR_OutputFile_write_pixels(CEXR_OutputFile* output_file, int num_scan_lines) {
    auto outfile = reinterpret_cast<OutputFile*>(output_file->output_file);

    outfile->writePixels(num_scan_lines);
}


//------------------------------------------------------------------------------
// InputFile
extern "C" {
    struct CEXR_InputFile {
        CEXR_Header header;
        void *input_file;
    };

    CEXR_InputFile CEXR_InputFile_new(
        const char file_name[],
        int num_threads);

    void CEXR_InputFile_delete(
        CEXR_InputFile *input_file);

    const CEXR_Header *CEXR_InputFile_header(
        const CEXR_InputFile *input_file);

    int CEXR_InputFile_version(
        const CEXR_InputFile *input_file);

    void CEXR_InputFile_set_frame_buffer(
        CEXR_InputFile* input_file,
        CEXR_FrameBuffer* frame_buffer);

    int CEXR_InputFile_is_complete(
        const CEXR_InputFile *input_file);

    void CEXR_InputFile_read_pixels(
        CEXR_InputFile *input_file,
        int scanline_1,
        int scanline_2);
};

CEXR_InputFile CEXR_InputFile_new(const char file_name[], int num_threads) {
    CEXR_InputFile input_file;
    auto in_file = new InputFile(file_name, num_threads);
    input_file.header.header = const_cast<void*>(reinterpret_cast<const void*>(&in_file->header()));
    input_file.input_file = reinterpret_cast<void*>(in_file);

    return input_file;
}

void CEXR_InputFile_delete(CEXR_InputFile *input_file) {
    auto in_file = reinterpret_cast<InputFile*>(input_file->input_file);
    delete in_file;
}

const CEXR_Header *CEXR_InputFile_header(const CEXR_InputFile *input_file) {
    return &(input_file->header);
}

int CEXR_InputFile_version(const CEXR_InputFile *input_file) {
    auto in_file = reinterpret_cast<InputFile*>(input_file->input_file);
    return in_file->version();
}

void CEXR_InputFile_set_frame_buffer(CEXR_InputFile* input_file, CEXR_FrameBuffer* frame_buffer) {
    auto in_file = reinterpret_cast<InputFile*>(input_file->input_file);
    auto framebuf = reinterpret_cast<FrameBuffer*>(frame_buffer->frame_buffer);

    in_file->setFrameBuffer(*framebuf);
}

int CEXR_InputFile_is_complete(const CEXR_InputFile *input_file) {
    auto in_file = reinterpret_cast<InputFile*>(input_file->input_file);
    in_file->isComplete();
}

void CEXR_InputFile_read_pixels(CEXR_InputFile *input_file, int scanline_1, int scanline_2) {
    auto in_file = reinterpret_cast<InputFile*>(input_file->input_file);
    in_file->readPixels(scanline_1, scanline_2);
}
