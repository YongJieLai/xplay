#ifndef XPLAY_SLAUDIOPLAY_H  // 防止头文件重复包含
#define XPLAY_SLAUDIOPLAY_H

#include "IAudioPlay.h"  // 包含音频播放接口基类

// SLAudioPlay类 - 基于OpenSL ES的Android音频播放实现
class SLAudioPlay : public IAudioPlay {
public:
    // 启动音频播放
    // out: 音频输出参数（声道数、采样率等）
    // 返回值: 成功返回true，失败返回false
    virtual bool StartPlay(XParameter out);

    // 关闭音频播放并释放资源
    virtual void Close();

    // OpenSL ES回调函数
    // bufq: OpenSL ES缓冲队列接口指针
    void PlayCall(void *bufq);
    // 构造函数
    SLAudioPlay();
    // 析构函数
    virtual ~SLAudioPlay();

protected:
    unsigned char *buf = 0;  // 音频数据缓冲区
    std::mutex mux;          // 线程安全互斥锁
};

#endif //XPLAY_SLAUDIOPLAY_H  // 头文件结束