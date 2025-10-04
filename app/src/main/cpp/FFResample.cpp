//
// Created by 赖勇杰 on 2025/10/4.
//
#include "FFResample.h"  // 包含头文件
#include "XLog.h"        // 日志模块
#include <libavcodec/avcodec.h>  // FFmpeg编解码库

extern "C" {
#include <libswresample/swresample.h>  // FFmpeg音频重采样库
}

// 关闭重采样器并释放资源
void FFResample::Close() {
    mux.lock();  // 加锁保证线程安全
    if (actx) {
        swr_free(&actx);  // 释放重采样上下文
        actx = nullptr;    // 置空指针
    }
    mux.unlock();  // 解锁
}

// 初始化音频重采样器
bool FFResample::Open(XParameter in, XParameter out) {
    Close();  // 先关闭可能存在的旧实例

    mux.lock();  // 加锁

    // 1. 创建重采样上下文
    actx = swr_alloc();

    // 2. 配置重采样参数
    actx = swr_alloc_set_opts(actx,
            // 输出参数
                              av_get_default_channel_layout(out.channels),  // 输出声道布局
                              AV_SAMPLE_FMT_S16,                            // 输出采样格式(16位有符号整数)
                              out.sample_rate,                              // 输出采样率

            // 输入参数
                              av_get_default_channel_layout(in.para->channels),  // 输入声道布局
                              (AVSampleFormat)in.para->format,             // 输入采样格式
                              in.para->sample_rate,                         // 输入采样率

            // 高级选项(默认)
                              0, 0);

    // 3. 初始化重采样器
    int re = swr_init(actx);
    if (re != 0) {  // 初始化失败
        mux.unlock();
        XLOGE("swr_init failed! Error: %s", av_err2str(re));
        return false;
    }

    // 4. 保存输出参数
    outChannels = in.para->channels;  // 输出声道数
    outFormat = AV_SAMPLE_FMT_S16;    // 输出采样格式

    XLOGI("音频重采样器初始化成功!");
    mux.unlock();  // 解锁
    return true;
}

// 执行音频重采样
XData FFResample::Resample(XData indata) {
    // 检查输入数据有效性
    if (indata.size <= 0 || !indata.data)
        return XData();  // 返回空数据

    mux.lock();  // 加锁

    // 检查重采样器是否初始化
    if (!actx) {
        mux.unlock();
        return XData();
    }

    // 将输入数据转换为AVFrame结构
    AVFrame *frame = (AVFrame *)indata.data;

    // 计算输出数据大小
    int outsize = outChannels * frame->nb_samples *
                  av_get_bytes_per_sample((AVSampleFormat)outFormat);
    if (outsize <= 0) {
        mux.unlock();
        return XData();
    }

    // 分配输出缓冲区
    XData out;
    out.Alloc(outsize);

    // 设置输出缓冲区指针数组
    uint8_t *outArr[2] = {0};
    outArr[0] = out.data;

    // 执行重采样
    int len = swr_convert(actx,
                          outArr,                // 输出缓冲区
                          frame->nb_samples,     // 输出样本数
                          (const uint8_t **)frame->data,  // 输入数据
                          frame->nb_samples       // 输入样本数
    );

    // 检查重采样结果
    if (len <= 0) {
        mux.unlock();
        out.Drop();  // 释放分配的内存
        return XData();
    }

    // 保存时间戳
    out.pts = indata.pts;

    mux.unlock();  // 解锁
    return out;
}