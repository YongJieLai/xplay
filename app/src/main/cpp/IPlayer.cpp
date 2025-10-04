//
// Created by 赖勇杰 on 2025/10/4.
//

#include "IPlayer.h"
#include "IDemux.h"
#include "IDecode.h"
#include "IAudioPlay.h"
#include "IVideoView.h"
#include "IResample.h"
#include "XLog.h"

// 获取播放器实例（单例模式）
IPlayer *IPlayer::Get(unsigned char index) {
    static IPlayer p[256];  // 静态实例数组（最多支持256个实例）
    return &p[index];        // 返回指定索引的实例
}

// 播放器主线程函数（负责音视频同步）
void IPlayer::Main() {
    while (!isExit) {  // 循环直到退出标志
        mux.lock();    // 加锁保证线程安全

        // 检查音频播放器和视频解码器是否有效
        if (!audioPlay || !vdecode) {
            mux.unlock();
            XSleep(2);  // 等待2ms
            continue;
        }
        // 音视频同步核心逻辑
        int apts = audioPlay->pts;  // 获取当前音频播放位置
        vdecode->synPts = apts;     // 将音频位置同步给视频解码器
        mux.unlock();
        XSleep(2);  // 每2ms同步一次
    }
}

// 关闭播放器并释放所有资源
void IPlayer::Close() {
    mux.lock();  // 加锁

    // 1. 停止所有线程
    XThread::Stop();  // 停止主线程
    if (demux) demux->Stop();
    if (vdecode) vdecode->Stop();
    if (adecode) adecode->Stop();
    if (audioPlay) audioPlay->Stop();

    // 2. 清空所有缓冲队列
    if (vdecode) vdecode->Clear();
    if (adecode) adecode->Clear();
    if (audioPlay) audioPlay->Clear();

    // 3. 关闭所有模块
    if (audioPlay) audioPlay->Close();
    if (videoView) videoView->Close();
    if (vdecode) vdecode->Close();
    if (adecode) adecode->Close();
    if (demux) demux->Close();

    mux.unlock();  // 解锁
}

// 获取当前播放进度（0.0~1.0）
double IPlayer::PlayPos() {
    double pos = 0.0;
    mux.lock();  // 加锁

    if (demux) {
        int total = demux->totalMs;  // 媒体总时长
        if (total > 0 && vdecode) {
            pos = (double)vdecode->pts / (double)total;  // 计算进度百分比
        }
    }

    mux.unlock();  // 解锁
    return pos;
}

// 设置暂停状态
void IPlayer::SetPause(bool isP) {
    mux.lock();  // 加锁

    // 设置所有模块的暂停状态
    XThread::SetPause(isP);  // 主线程
    if (demux) demux->SetPause(isP);
    if (vdecode) vdecode->SetPause(isP);
    if (adecode) adecode->SetPause(isP);
    if (audioPlay) audioPlay->SetPause(isP);

    mux.unlock();  // 解锁
}

// 跳转到指定位置
bool IPlayer::Seek(double pos) {
    if (!demux) return false;  // 检查解封装器

    SetPause(true);  // 暂停播放
    mux.lock();      // 加锁

    // 1. 清空所有缓冲
    if (vdecode) vdecode->Clear();
    if (adecode) adecode->Clear();
    if (audioPlay) audioPlay->Clear();

    // 2. 执行跳转
    bool re = demux->Seek(pos);  // 解封装器跳转
    if (!re || !vdecode) {
        mux.unlock();
        SetPause(false);
        return re;
    }

    // 3. 定位到精确帧
    int seekPts = pos * demux->totalMs;  // 目标位置(毫秒)
    while (!isExit) {
        XData pkt = demux->Read();  // 读取数据包
        if (pkt.size <= 0) break;

        if (pkt.isAudio) {  // 音频包处理
            if (pkt.pts < seekPts) {
                pkt.Drop();  // 丢弃早于目标位置的音频
                continue;
            }
            demux->Notify(pkt);  // 将有效音频包加入队列
            continue;
        }

        // 视频包处理
        vdecode->SendPacket(pkt);  // 发送给视频解码器
        pkt.Drop();

        // 解码视频帧
        XData data = vdecode->RecvFrame();
        if (data.size <= 0) continue;

        // 找到目标帧后停止
        if (data.pts >= seekPts) {
            break;
        }
    }

    mux.unlock();      // 解锁
    SetPause(false);   // 恢复播放
    return re;
}

// 打开媒体文件
bool IPlayer::Open(const char *path) {
    Close();  // 先关闭可能存在的旧实例
    mux.lock();  // 加锁

    // 1. 打开解封装器
    if (!demux || !demux->Open(path)) {
        mux.unlock();
        XLOGE("解封装器打开失败: %s", path);
        return false;
    }

    // 2. 打开视频解码器
    if (!vdecode || !vdecode->Open(demux->GetVPara(), isHardDecode)) {
        XLOGE("视频解码器打开失败: %s", path);
        // 注：解码失败不直接返回，尝试继续
    }

    // 3. 打开音频解码器
    if (!adecode || !adecode->Open(demux->GetAPara())) {
        XLOGE("音频解码器打开失败: %s", path);
        // 注：解码失败不直接返回，尝试继续
    }

    // 4. 配置音频重采样
    outPara = demux->GetAPara();  // 获取音频参数
    if (!resample || !resample->Open(demux->GetAPara(), outPara)) {
        XLOGE("音频重采样打开失败: %s", path);
    }

    mux.unlock();  // 解锁
    return true;
}

// 开始播放
bool IPlayer::Start() {
    mux.lock();  // 加锁

    // 1. 启动视频解码器
    if (vdecode) vdecode->Start();

    // 2. 启动解封装器
    if (!demux || !demux->Start()) {
        mux.unlock();
        XLOGE("解封装器启动失败!");
        return false;
    }

    // 3. 启动音频解码器
    if (adecode) adecode->Start();

    // 4. 启动音频播放器
    if (audioPlay) audioPlay->StartPlay(outPara);

    // 5. 启动主线程（音视频同步）
    XThread::Start();

    mux.unlock();  // 解锁
    return true;
}

// 初始化视频渲染窗口
void IPlayer::InitView(void *win) {
    if (videoView) {
        videoView->Close();        // 关闭现有渲染器
        videoView->SetRender(win); // 设置新窗口句柄
    }
}
