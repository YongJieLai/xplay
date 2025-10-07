#ifndef XPLAY_IPLAYER_H  // 防止头文件重复包含
#define XPLAY_IPLAYER_H

#include <mutex>              // 互斥锁头文件
#include "XThread.h"          // 线程基类
#include "XParameter.h"        // 音频参数定义

// 前置声明各模块接口
class IDemux;    // 解复用器接口
class IAudioPlay; // 音频播放接口
class IVideoView; // 视频渲染接口
class IResample;  // 音频重采样接口
class IDecode;    // 解码器接口

// 播放器核心控制类
class IPlayer : public XThread {
public:
    // 获取播放器实例（单例模式）
    // index: 实例索引（支持多个播放器实例）
    static IPlayer *Get(unsigned char index = 0);

    // 打开媒体文件
    // path: 媒体文件路径
    virtual bool Open(const char *path);

    // 关闭播放器并释放资源
    virtual void Close();

    // 开始播放
    virtual bool Start();

    // 初始化视频渲染窗口
    // win: 平台相关的窗口句柄
    virtual void InitView(void *win);

    // 获取当前播放进度
    // 返回值: 0.0 ~ 1.0之间的进度值
    virtual double PlayPos();

    // 跳转到指定位置
    // pos: 0.0 ~ 1.0之间的位置值
    virtual bool Seek(double pos);

    // 设置暂停状态
    // isP: true暂停, false继续
    virtual void SetPause(bool isP);

    // 是否使用视频硬解码
    bool isHardDecode = true;

    // 音频输出参数配置
    XParameter outPara;

    // ===== 构建用注入的方式 =====
    // 通过接口指针实现依赖注入
    // 播放器核心不依赖具体实现，而是通过外部注入

    IDemux *demux = 0;      // 解复用器模块
    IDecode *vdecode = 0;   // 视频解码器模块
    IDecode *adecode = 0;   // 音频解码器模块
    IResample *resample = 0; // 音频重采样模块
    IVideoView *videoView = 0; // 视频渲染模块
    IAudioPlay *audioPlay = 0; // 音频播放模块
    // ===========================

protected:
    // 主线程函数（用于音视频同步）
    void Main();

    // 互斥锁（保证线程安全）
    std::mutex mux;

    // 保护构造函数（只能通过Get方法创建实例）
    IPlayer(){};
};

#endif //XPLAY_IPLAYER_H  // 头文件结束