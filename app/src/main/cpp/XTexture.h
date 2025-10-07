#ifndef XPLAY_XTEXTURE_H  // 防止头文件重复包含
#define XPLAY_XTEXTURE_H

// 定义纹理格式枚举
enum XTextureType
{
    XTEXTURE_YUV420P = 0,  // YUV420P格式：Y平面 + U平面 + V平面（各占4:1:1）
    XTEXTURE_NV12 = 25,    // NV12格式：Y平面 + UV交错平面（iOS相机常用）
    XTEXTURE_NV21 = 26     // NV21格式：Y平面 + VU交错平面（Android相机常用）
};

// 纹理抽象接口类
class XTexture
{
public:
    // 静态工厂方法：创建具体纹理实例
    static XTexture *Create();

    // 初始化纹理系统
    // win: 平台相关的渲染窗口句柄（Android: Surface, iOS: CAEAGLLayer）
    // type: 纹理格式类型（默认为YUV420P）
    virtual bool Init(void *win, XTextureType type = XTEXTURE_YUV420P) = 0;

    // 渲染一帧视频数据
    // data: 视频数据数组（YUV分量指针数组）
    // width: 视频宽度
    // height: 视频高度
    virtual void Draw(unsigned char *data[], int width, int height) = 0;

    // 释放纹理资源（包含自销毁）
    virtual void Drop() = 0;

    // 虚析构函数（确保派生类正确析构）
    virtual ~XTexture(){};

protected:
    // 保护构造函数（只能通过Create()方法实例化）
    XTexture(){};
};

#endif //XPLAY_XTEXTURE_H  // 头文件结束