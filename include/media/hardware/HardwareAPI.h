/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef HARDWARE_API_H_

#define HARDWARE_API_H_

#include <media/hardware/OMXPluginBase.h>
#include <media/hardware/MetadataBufferType.h>
#include <system/window.h>
#include <utils/RefBase.h>

#include <OMX_Component.h>

namespace android {

// This structure is used to enable Android native buffer use for either
// graphic buffers or secure buffers.
//
// TO CONTROL ANDROID GRAPHIC BUFFER USAGE:
//
// A pointer to this struct is passed to the OMX_SetParameter when the extension
// index for the 'OMX.google.android.index.enableAndroidNativeBuffers' extension
// is given.
//
// When Android native buffer use is disabled for a port (the default state),
// the OMX node should operate as normal, and expect UseBuffer calls to set its
// buffers.  This is the mode that will be used when CPU access to the buffer is
// required.
//
// When Android native buffer use has been enabled for a given port, the video
// color format for the port is to be interpreted as an Android pixel format
// rather than an OMX color format.  Enabling Android native buffers may also
// change how the component receives the native buffers.  If store-metadata-mode
// is enabled on the port, the component will receive the buffers as specified
// in the section below. Otherwise, unless the node supports the
// 'OMX.google.android.index.useAndroidNativeBuffer2' extension, it should
// expect to receive UseAndroidNativeBuffer calls (via OMX_SetParameter) rather
// than UseBuffer calls for that port.
//
// TO CONTROL ANDROID SECURE BUFFER USAGE:
//
// A pointer to this struct is passed to the OMX_SetParameter when the extension
// index for the 'OMX.google.android.index.allocateNativeHandle' extension
// is given.
//
// When native handle use is disabled for a port (the default state),
// the OMX node should operate as normal, and expect AllocateBuffer calls to
// return buffer pointers. This is the mode that will be used for non-secure
// buffers if component requires allocate buffers instead of use buffers.
//
// When native handle use has been enabled for a given port, the component
// shall allocate native_buffer_t objects containing  that can be passed between
// processes using binder. This is the mode that will be used for secure buffers.
// When an OMX component allocates native handle for buffers, it must close and
// delete that handle when it frees those buffers. Even though pBuffer will point
// to a native handle, nFilledLength, nAllocLength and nOffset will correspond
// to the data inside the opaque buffer.
struct EnableAndroidNativeBuffersParams {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_BOOL enable;
};

typedef struct EnableAndroidNativeBuffersParams AllocateNativeHandleParams;

// A pointer to this struct is passed to OMX_SetParameter() when the extension index
// "OMX.google.android.index.storeMetaDataInBuffers" or
// "OMX.google.android.index.storeANWBufferInMetadata" is given.
//
// When meta data is stored in the video buffers passed between OMX clients
// and OMX components, interpretation of the buffer data is up to the
// buffer receiver, and the data may or may not be the actual video data, but
// some information helpful for the receiver to locate the actual data.
// The buffer receiver thus needs to know how to interpret what is stored
// in these buffers, with mechanisms pre-determined externally. How to
// interpret the meta data is outside of the scope of this parameter.
//
// Currently, this is used to pass meta data from video source (camera component, for instance) to
// video encoder to avoid memcpying of input video frame data, as well as to pass dynamic output
// buffer to video decoder. To do this, bStoreMetaData is set to OMX_TRUE.
//
// If bStoreMetaData is set to false, real YUV frame data will be stored in input buffers, and
// the output buffers contain either real YUV frame data, or are themselves native handles as
// directed by enable/use-android-native-buffer parameter settings.
// In addition, if no OMX_SetParameter() call is made on a port with the corresponding extension
// index, the component should not assume that the client is not using metadata mode for the port.
//
// If the component supports this using the "OMX.google.android.index.storeANWBufferInMetadata"
// extension and bStoreMetaData is set to OMX_TRUE, data is passed using the VideoNativeMetadata
// layout as defined below. Each buffer will be accompanied by a fence. The fence must signal
// before the buffer can be used (e.g. read from or written into). When returning such buffer to
// the client, component must provide a new fence that must signal before the returned buffer can
// be used (e.g. read from or written into). The component owns the incoming fenceFd, and must close
// it when fence has signaled. The client will own and close the returned fence file descriptor.
//
// If the component supports this using the "OMX.google.android.index.storeMetaDataInBuffers"
// extension and bStoreMetaData is set to OMX_TRUE, data is passed using VideoGrallocMetadata
// (the layout of which is the VideoGrallocMetadata defined below). Camera input can be also passed
// as "CameraSource", the layout of which is vendor dependent.
//
// Metadata buffers are registered with the component using UseBuffer calls, or can be allocated
// by the component for encoder-metadata-output buffers.
struct StoreMetaDataInBuffersParams {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_BOOL bStoreMetaData;
};

// Meta data buffer layout used to transport output frames to the decoder for
// dynamic buffer handling.
struct VideoGrallocMetadata {
    MetadataBufferType eType;               // must be kMetadataBufferTypeGrallocSource
#ifdef OMX_ANDROID_COMPILE_AS_32BIT_ON_64BIT_PLATFORMS
    OMX_PTR pHandle;
#else
    buffer_handle_t pHandle;
#endif
};

// Legacy name for VideoGrallocMetadata struct.
struct VideoDecoderOutputMetaData : public VideoGrallocMetadata {};

struct VideoNativeMetadata {
    MetadataBufferType eType;               // must be kMetadataBufferTypeANWBuffer
#ifdef OMX_ANDROID_COMPILE_AS_32BIT_ON_64BIT_PLATFORMS
    OMX_PTR pBuffer;
#else
    struct ANativeWindowBuffer* pBuffer;
#endif
    int nFenceFd;                           // -1 if unused
};

// Meta data buffer layout for passing a native_handle to codec
struct VideoNativeHandleMetadata {
    MetadataBufferType eType;               // must be kMetadataBufferTypeNativeHandleSource

#ifdef OMX_ANDROID_COMPILE_AS_32BIT_ON_64BIT_PLATFORMS
    OMX_PTR pHandle;
#else
    native_handle_t *pHandle;
#endif
};

// A pointer to this struct is passed to OMX_SetParameter() when the extension
// index "OMX.google.android.index.prepareForAdaptivePlayback" is given.
//
// This method is used to signal a video decoder, that the user has requested
// seamless resolution change support (if bEnable is set to OMX_TRUE).
// nMaxFrameWidth and nMaxFrameHeight are the dimensions of the largest
// anticipated frames in the video.  If bEnable is OMX_FALSE, no resolution
// change is expected, and the nMaxFrameWidth/Height fields are unused.
//
// If the decoder supports dynamic output buffers, it may ignore this
// request.  Otherwise, it shall request resources in such a way so that it
// avoids full port-reconfiguration (due to output port-definition change)
// during resolution changes.
//
// DO NOT USE THIS STRUCTURE AS IT WILL BE REMOVED.  INSTEAD, IMPLEMENT
// METADATA SUPPORT FOR VIDEO DECODERS.
struct PrepareForAdaptivePlaybackParams {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_BOOL bEnable;
    OMX_U32 nMaxFrameWidth;
    OMX_U32 nMaxFrameHeight;
};

// A pointer to this struct is passed to OMX_SetParameter when the extension
// index for the 'OMX.google.android.index.useAndroidNativeBuffer' extension is
// given.  This call will only be performed if a prior call was made with the
// 'OMX.google.android.index.enableAndroidNativeBuffers' extension index,
// enabling use of Android native buffers.
struct UseAndroidNativeBufferParams {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_PTR pAppPrivate;
    OMX_BUFFERHEADERTYPE **bufferHeader;
    const sp<ANativeWindowBuffer>& nativeBuffer;
};

// A pointer to this struct is passed to OMX_GetParameter when the extension
// index for the 'OMX.google.android.index.getAndroidNativeBufferUsage'
// extension is given.  The usage bits returned from this query will be used to
// allocate the Gralloc buffers that get passed to the useAndroidNativeBuffer
// command.
struct GetAndroidNativeBufferUsageParams {
    OMX_U32 nSize;              // IN
    OMX_VERSIONTYPE nVersion;   // IN
    OMX_U32 nPortIndex;         // IN
    OMX_U32 nUsage;             // OUT
};

// An enum OMX_COLOR_FormatAndroidOpaque to indicate an opaque colorformat
// is declared in media/stagefright/openmax/OMX_IVCommon.h
// This will inform the encoder that the actual
// colorformat will be relayed by the GRalloc Buffers.
// OMX_COLOR_FormatAndroidOpaque  = 0x7F000001,

// A pointer to this struct is passed to OMX_SetParameter when the extension
// index for the 'OMX.google.android.index.prependSPSPPSToIDRFrames' extension
// is given.
// A successful result indicates that future IDR frames will be prefixed by
// SPS/PPS.
struct PrependSPSPPSToIDRFramesParams {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_BOOL bEnable;
};

// Structure describing a media image (frame)
// Currently only supporting YUV
// @deprecated. Use MediaImage2 instead
struct MediaImage {
    enum Type {
        MEDIA_IMAGE_TYPE_UNKNOWN = 0,
        MEDIA_IMAGE_TYPE_YUV,
    };

    enum PlaneIndex {
        Y = 0,
        U,
        V,
        MAX_NUM_PLANES
    };

    Type mType;
    uint32_t mNumPlanes;              // number of planes
    uint32_t mWidth;                  // width of largest plane (unpadded, as in nFrameWidth)
    uint32_t mHeight;                 // height of largest plane (unpadded, as in nFrameHeight)
    uint32_t mBitDepth;               // useable bit depth
    struct PlaneInfo {
        uint32_t mOffset;             // offset of first pixel of the plane in bytes
                                      // from buffer offset
        uint32_t mColInc;             // column increment in bytes
        uint32_t mRowInc;             // row increment in bytes
        uint32_t mHorizSubsampling;   // subsampling compared to the largest plane
        uint32_t mVertSubsampling;    // subsampling compared to the largest plane
    };
    PlaneInfo mPlane[MAX_NUM_PLANES];
};

struct MediaImage2 {
    enum Type : uint32_t {
        MEDIA_IMAGE_TYPE_UNKNOWN = 0,
        MEDIA_IMAGE_TYPE_YUV,
        MEDIA_IMAGE_TYPE_YUVA,
        MEDIA_IMAGE_TYPE_RGB,
        MEDIA_IMAGE_TYPE_RGBA,
        MEDIA_IMAGE_TYPE_Y,
    };

    enum PlaneIndex : uint32_t {
        Y = 0,
        U = 1,
        V = 2,
        R = 0,
        G = 1,
        B = 2,
        A = 3,
        MAX_NUM_PLANES = 4,
    };

    Type mType;
    uint32_t mNumPlanes;              // number of planes
    uint32_t mWidth;                  // width of largest plane (unpadded, as in nFrameWidth)
    uint32_t mHeight;                 // height of largest plane (unpadded, as in nFrameHeight)
    uint32_t mBitDepth;               // useable bit depth (always MSB)
    uint32_t mBitDepthAllocated;      // bits per component (must be 8 or 16)

    struct PlaneInfo {
        uint32_t mOffset;             // offset of first pixel of the plane in bytes
                                      // from buffer offset
        int32_t mColInc;              // column increment in bytes
        int32_t mRowInc;              // row increment in bytes
        uint32_t mHorizSubsampling;   // subsampling compared to the largest plane
        uint32_t mVertSubsampling;    // subsampling compared to the largest plane
    };
    PlaneInfo mPlane[MAX_NUM_PLANES];

    void initFromV1(const MediaImage&); // for internal use only
};

// A pointer to this struct is passed to OMX_GetParameter when the extension
// index for the 'OMX.google.android.index.describeColorFormat'
// extension is given.  This method can be called from any component state
// other than invalid.  The color-format, frame width/height, and stride/
// slice-height parameters are ones that are associated with a raw video
// port (input or output), but the stride/slice height parameters may be
// incorrect. bUsingNativeBuffers is OMX_TRUE if native android buffers will
// be used (while specifying this color format).
//
// The component shall fill out the MediaImage structure that
// corresponds to the described raw video format, and the potentially corrected
// stride and slice-height info.
//
// The behavior is slightly different if bUsingNativeBuffers is OMX_TRUE,
// though most implementations can ignore this difference. When using native buffers,
// the component may change the configured color format to an optimized format.
// Additionally, when allocating these buffers for flexible usecase, the framework
// will set the SW_READ/WRITE_OFTEN usage flags. In this case (if bUsingNativeBuffers
// is OMX_TRUE), the component shall fill out the MediaImage information for the
// scenario when these SW-readable/writable buffers are locked using gralloc_lock.
// Note, that these buffers may also be locked using gralloc_lock_ycbcr, which must
// be supported for vendor-specific formats.
//
// For non-YUV packed planar/semiplanar image formats, or if bUsingNativeBuffers
// is OMX_TRUE and the component does not support this color format with native
// buffers, the component shall set mNumPlanes to 0, and mType to MEDIA_IMAGE_TYPE_UNKNOWN.

// @deprecated: use DescribeColorFormat2Params
struct DescribeColorFormat2Params;
struct DescribeColorFormatParams {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    // input: parameters from OMX_VIDEO_PORTDEFINITIONTYPE
    OMX_COLOR_FORMATTYPE eColorFormat;
    OMX_U32 nFrameWidth;
    OMX_U32 nFrameHeight;
    OMX_U32 nStride;
    OMX_U32 nSliceHeight;
    OMX_BOOL bUsingNativeBuffers;

    // output: fill out the MediaImage fields
    MediaImage sMediaImage;

    DescribeColorFormatParams(const DescribeColorFormat2Params&); // for internal use only
};

// A pointer to this struct is passed to OMX_GetParameter when the extension
// index for the 'OMX.google.android.index.describeColorFormat2'
// extension is given. This is operationally the same as DescribeColorFormatParams
// but can be used for HDR and RGBA/YUVA formats.
struct DescribeColorFormat2Params {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    // input: parameters from OMX_VIDEO_PORTDEFINITIONTYPE
    OMX_COLOR_FORMATTYPE eColorFormat;
    OMX_U32 nFrameWidth;
    OMX_U32 nFrameHeight;
    OMX_U32 nStride;
    OMX_U32 nSliceHeight;
    OMX_BOOL bUsingNativeBuffers;

    // output: fill out the MediaImage2 fields
    MediaImage2 sMediaImage;

    void initFromV1(const DescribeColorFormatParams&); // for internal use only
};

// A pointer to this struct is passed to OMX_SetParameter or OMX_GetParameter
// when the extension index for the
// 'OMX.google.android.index.configureVideoTunnelMode' extension is  given.
// If the extension is supported then tunneled playback mode should be supported
// by the codec. If bTunneled is set to OMX_TRUE then the video decoder should
// operate in "tunneled" mode and output its decoded frames directly to the
// sink. In this case nAudioHwSync is the HW SYNC ID of the audio HAL Output
// stream to sync the video with. If bTunneled is set to OMX_FALSE, "tunneled"
// mode should be disabled and nAudioHwSync should be ignored.
// OMX_GetParameter is used to query tunneling configuration. bTunneled should
// return whether decoder is operating in tunneled mode, and if it is,
// pSidebandWindow should contain the codec allocated sideband window handle.
struct ConfigureVideoTunnelModeParams {
    OMX_U32 nSize;              // IN
    OMX_VERSIONTYPE nVersion;   // IN
    OMX_U32 nPortIndex;         // IN
    OMX_BOOL bTunneled;         // IN/OUT
    OMX_U32 nAudioHwSync;       // IN
    OMX_PTR pSidebandWindow;    // OUT
};

// Color description parameters. This is passed via OMX_SetConfig or OMX_GetConfig
// to video encoders and decoders when the
// 'OMX.google.android.index.describeColorAspects' extension is given.
//
// Video encoders: the framework uses OMX_SetConfig to specify color aspects
// of the coded video before the component transitions to idle state, as well
// as before an input frame with a different color aspect is sent:
// 1. The component should maintain an internal color aspect state, initialized
//   to Unspecified values.
// 2. Upon OMX_SetConfig, it SHOULD update its internal state for the aspects that are not
//   Unspecified in the config param.
// 3. If an aspect value cannot be encoded into the bitstream (including the Other value), that
//   aspect should be reset to the Unspecified value (in the internal state).
// 4. OMX_GetConfig SHOULD return the current internal state.
// 5. If changing the color aspects after the first input frame is not supported, and the config
//   params would actually cause a change, OMX_SetConfig should fail with the internal state
//   unchanged.
// 6. If changing a portion of the aspects after the first input frame is supported, OMX_SetConfig
//   should succeed with the portion of the internal state updated.
//
// Video decoders: the framework uses OMX_SetConfig to specify color aspects
// of the coded video parsed from the container before the component transitions
// to idle state.
// 1. The component should maintiain an internal color aspect state, initialized to Unspecified
//   values.
// 2. Upon OMX_SetConfig, it SHOULD update its internal state for the aspects that are not
//   Unspecified in the config param, regardless of whether such aspects could be supplied by the
//   component bitstream. (E.g. it should blindly support all enumeration values, even unknown
//   ones, and the Other value).
// 3. OMX_GetConfig SHOULD return the current internal state.
// 4. When the component processes color aspect information in the bitstream with a non-Unspecified
//   value, it should update its internal state with that information just before the frame
//   with the new information is outputted, and the component SHALL signal an
//   OMX_EventPortSettingsChanged event with data2 set to the extension index (or
//   OMX_IndexConfigCommonOutputCrop, as it is handled identically).
// 4a. Component shall not signal a separate event purely for color aspect change, if it occurs
//   together with a port definition (e.g. size) or crop change.
// 5. If the aspects a component encounters in the bitstream cannot be represented with the below
//   enumeration values, it should set those aspects to Other. Restricted values in the bitstream
//   should be treated as defined by the relevant bitstream specifications/standards, or as
//   Unspecified, if not defined.
//
// NOTE: this structure is expected to grow in the future if new color aspects are
// added to codec bitstreams. OMX component should not require a specific nSize
// though could verify that nSize is at least the size of the structure at the
// time of implementation. All new fields will be added at the end of the structure
// ensuring backward compatibility.

struct ColorAspects {
    // this is in sync with the range values in graphics.h
    enum Range : uint32_t {
        RangeUnspecified,
        RangeFull,
        RangeLimited,
        RangeOther = 0xff,
    };

    enum Primaries : uint32_t {
        PrimariesUnspecified,
        PrimariesBT709_5,       // Rec.ITU-R BT.709-5 or equivalent
        PrimariesBT470_6M,      // Rec.ITU-R BT.470-6 System M or equivalent
        PrimariesBT601_6_625,   // Rec.ITU-R BT.601-6 625 or equivalent
        PrimariesBT601_6_525,   // Rec.ITU-R BT.601-6 525 or equivalent
        PrimariesGenericFilm,   // Generic Film
        PrimariesBT2020,        // Rec.ITU-R BT.2020 or equivalent
        PrimariesOther = 0xff,
    };

    // this partially in sync with the transfer values in graphics.h prior to the transfers
    // unlikely to be required by Android section
    enum Transfer : uint32_t {
        TransferUnspecified,
        TransferLinear,         // Linear transfer characteristics
        TransferSRGB,           // sRGB or equivalent
        TransferSMPTE170M,      // SMPTE 170M or equivalent (e.g. BT.601/709/2020)
        TransferGamma22,        // Assumed display gamma 2.2
        TransferGamma28,        // Assumed display gamma 2.8
        TransferST2084,         // SMPTE ST 2084 for 10/12/14/16 bit systems
        TransferHLG,            // ARIB STD-B67 hybrid-log-gamma

        // transfers unlikely to be required by Android
        TransferSMPTE240M = 0x40, // SMPTE 240M
        TransferXvYCC,          // IEC 61966-2-4
        TransferBT1361,         // Rec.ITU-R BT.1361 extended gamut
        TransferST428,          // SMPTE ST 428-1
        TransferOther = 0xff,
    };

    enum MatrixCoeffs : uint32_t {
        MatrixUnspecified,
        MatrixBT709_5,          // Rec.ITU-R BT.709-5 or equivalent
        MatrixBT470_6M,         // KR=0.30, KB=0.11 or equivalent
        MatrixBT601_6,          // Rec.ITU-R BT.601-6 625 or equivalent
        MatrixSMPTE240M,        // SMPTE 240M or equivalent
        MatrixBT2020,           // Rec.ITU-R BT.2020 non-constant luminance
        MatrixBT2020Constant,   // Rec.ITU-R BT.2020 constant luminance
        MatrixOther = 0xff,
    };

    // this is in sync with the standard values in graphics.h
    enum Standard : uint32_t {
        StandardUnspecified,
        StandardBT709,                  // PrimariesBT709_5 and MatrixBT709_5
        StandardBT601_625,              // PrimariesBT601_6_625 and MatrixBT601_6
        StandardBT601_625_Unadjusted,   // PrimariesBT601_6_625 and KR=0.222, KB=0.071
        StandardBT601_525,              // PrimariesBT601_6_525 and MatrixBT601_6
        StandardBT601_525_Unadjusted,   // PrimariesBT601_6_525 and MatrixSMPTE240M
        StandardBT2020,                 // PrimariesBT2020 and MatrixBT2020
        StandardBT2020Constant,         // PrimariesBT2020 and MatrixBT2020Constant
        StandardBT470M,                 // PrimariesBT470_6M and MatrixBT470_6M
        StandardFilm,                   // PrimariesGenericFilm and KR=0.253, KB=0.068
        StandardOther = 0xff,
    };

    Range mRange;                // IN/OUT
    Primaries mPrimaries;        // IN/OUT
    Transfer mTransfer;          // IN/OUT
    MatrixCoeffs mMatrixCoeffs;  // IN/OUT
};

struct DescribeColorAspectsParams {
    OMX_U32 nSize;              // IN
    OMX_VERSIONTYPE nVersion;   // IN
    OMX_U32 nPortIndex;         // IN
    ColorAspects sAspects;      // IN/OUT
};

}  // namespace android

extern android::OMXPluginBase *createOMXPlugin();

#endif  // HARDWARE_API_H_
