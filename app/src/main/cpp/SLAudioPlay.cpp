#include "SLAudioPlay.h"  // 包含头文件
#include <SLES/OpenSLES.h>  // OpenSL ES标准头文件
#include <SLES/OpenSLES_Android.h>  // Android扩展头文件
#include "XLog.h"  // 日志模块

// OpenSL ES全局对象声明
static SLObjectItf engineSL = NULL;  // 引擎对象
static SLEngineItf eng = NULL;      // 引擎接口
static SLObjectItf mix = NULL;      // 混音器对象
static SLObjectItf player = NULL;   // 播放器对象
static SLPlayItf iplayer = NULL;     // 播放器控制接口
static SLAndroidSimpleBufferQueueItf pcmQue = NULL;  // 音频缓冲队列接口

// 构造函数
SLAudioPlay::SLAudioPlay() {
    buf = new unsigned char[1024 * 1024];  // 分配1MB音频缓冲区
}

// 析构函数
SLAudioPlay::~SLAudioPlay() {
    delete[] buf;  // 释放缓冲区
    buf = 0;
}

// 创建OpenSL ES引擎
static SLEngineItf CreateSL() {
    SLresult re;
    SLEngineItf en;

    // 1. 创建引擎对象
    re = slCreateEngine(&engineSL, 0, 0, 0, 0, 0);
    if(re != SL_RESULT_SUCCESS) return NULL;

    // 2. 实例化引擎对象
    re = (*engineSL)->Realize(engineSL, SL_BOOLEAN_FALSE);
    if(re != SL_RESULT_SUCCESS) return NULL;

    // 3. 获取引擎接口
    re = (*engineSL)->GetInterface(engineSL, SL_IID_ENGINE, &en);
    if(re != SL_RESULT_SUCCESS) return NULL;

    return en;
}

// 音频播放回调函数
void SLAudioPlay::PlayCall(void *bufq) {
    if(!bufq) return;

    SLAndroidSimpleBufferQueueItf bf = (SLAndroidSimpleBufferQueueItf)bufq;

    // 从音频数据队列获取数据
    XData d = GetData();
    if(d.size <= 0) {
        XLOGE("GetData() size is 0");
        return;
    }

    if(!buf) return;

    // 复制音频数据到缓冲区
    memcpy(buf, d.data, d.size);

    mux.lock();  // 加锁
    // 将数据加入OpenSL ES播放队列
    if(pcmQue && (*pcmQue)) {
        (*pcmQue)->Enqueue(pcmQue, buf, d.size);
    }
    mux.unlock();  // 解锁

    d.Drop();  // 释放数据
}

// OpenSL ES缓冲队列回调
static void PcmCall(SLAndroidSimpleBufferQueueItf bf, void *contex) {
    SLAudioPlay *ap = (SLAudioPlay *)contex;
    if(!ap) {
        XLOGE("PcmCall failed contex is null!");
        return;
    }
    // 触发播放回调
    ap->PlayCall((void *)bf);
}

// 关闭音频播放器
void SLAudioPlay::Close() {
    IAudioPlay::Clear();  // 清空音频数据队列

    mux.lock();  // 加锁

    // 停止播放
    if(iplayer && (*iplayer)) {
        (*iplayer)->SetPlayState(iplayer, SL_PLAYSTATE_STOPPED);
    }

    // 清空播放队列
    if(pcmQue && (*pcmQue)) {
        (*pcmQue)->Clear(pcmQue);
    }

    // 销毁播放器对象
    if(player && (*player)) {
        (*player)->Destroy(player);
    }

    // 销毁混音器
    if(mix && (*mix)) {
        (*mix)->Destroy(mix);
    }

    // 销毁引擎
    if(engineSL && (*engineSL)) {
        (*engineSL)->Destroy(engineSL);
    }

    // 重置所有指针
    engineSL = NULL;
    eng = NULL;
    mix = NULL;
    player = NULL;
    iplayer = NULL;
    pcmQue = NULL;

    mux.unlock();  // 解锁
}

// 启动音频播放
bool SLAudioPlay::StartPlay(XParameter out) {
    Close();  // 先关闭可能存在的旧实例

    mux.lock();  // 加锁

    // 1. 创建OpenSL ES引擎
    eng = CreateSL();
    if(eng) {
        XLOGI("CreateSL success！");
    } else {
        mux.unlock();
        XLOGE("CreateSL failed！");
        return false;
    }

    SLresult re = 0;

    // 2. 创建输出混音器
    re = (*eng)->CreateOutputMix(eng, &mix, 0, 0, 0);
    if(re != SL_RESULT_SUCCESS) {
        mux.unlock();
        XLOGE("CreateOutputMix failed!");
        return false;
    }

    // 实例化混音器
    re = (*mix)->Realize(mix, SL_BOOLEAN_FALSE);
    if(re != SL_RESULT_SUCCESS) {
        mux.unlock();
        XLOGE("Mixer Realize failed!");
        return false;
    }

    // 配置混音器输出
    SLDataLocator_OutputMix outmix = {SL_DATALOCATOR_OUTPUTMIX, mix};
    SLDataSink audioSink = {&outmix, 0};

    // 3. 配置音频参数
    // 缓冲队列定位器
    SLDataLocator_AndroidSimpleBufferQueue que = {
            SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,
            10  // 队列缓冲数量
    };

    // PCM音频格式配置
    SLDataFormat_PCM pcm = {
            SL_DATAFORMAT_PCM,                // 格式类型
            (SLuint32) out.channels,         // 声道数
            (SLuint32) out.sample_rate * 1000, // 采样率(转换为毫赫兹)
            SL_PCMSAMPLEFORMAT_FIXED_16,      // 采样位数(16位)
            SL_PCMSAMPLEFORMAT_FIXED_16,      // 容器大小
            SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT, // 声道布局(立体声)
            SL_BYTEORDER_LITTLEENDIAN         // 字节序(小端)
    };

    // 数据源配置
    SLDataSource ds = {&que, &pcm};

    // 4. 创建音频播放器
    const SLInterfaceID ids[] = {SL_IID_BUFFERQUEUE};
    const SLboolean req[] = {SL_BOOLEAN_TRUE};

    re = (*eng)->CreateAudioPlayer(eng, &player, &ds, &audioSink,
                                   sizeof(ids)/sizeof(SLInterfaceID), ids, req);
    if(re != SL_RESULT_SUCCESS) {
        mux.unlock();
        XLOGE("CreateAudioPlayer failed!");
        return false;
    }

    // 实例化播放器
    re = (*player)->Realize(player, SL_BOOLEAN_FALSE);
    if(re != SL_RESULT_SUCCESS) {
        mux.unlock();
        XLOGE("Player Realize failed!");
        return false;
    }

    // 获取播放控制接口
    re = (*player)->GetInterface(player, SL_IID_PLAY, &iplayer);
    if(re != SL_RESULT_SUCCESS) {
        mux.unlock();
        XLOGE("Get PLAY interface failed!");
        return false;
    }

    // 获取缓冲队列接口
    re = (*player)->GetInterface(player, SL_IID_BUFFERQUEUE, &pcmQue);
    if(re != SL_RESULT_SUCCESS) {
        mux.unlock();
        XLOGE("Get BUFFERQUEUE interface failed!");
        return false;
    }

    // 5. 设置回调函数
    (*pcmQue)->RegisterCallback(pcmQue, PcmCall, this);

    // 6. 设置播放状态为播放中
    (*iplayer)->SetPlayState(iplayer, SL_PLAYSTATE_PLAYING);

    // 7. 启动队列回调(发送空数据触发第一次回调)
    (*pcmQue)->Enqueue(pcmQue, "", 1);

    isExit = false;  // 标记播放器运行中
    mux.unlock();     // 解锁

    XLOGI("SLAudioPlay::StartPlay success!");
    return true;
}