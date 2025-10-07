#ifndef XPLAY_XSHADER_H  // 防止头文件重复包含
#define XPLAY_XSHADER_H

#include <mutex>  // 包含互斥锁头文件，用于线程安全

// 定义着色器类型枚举
enum XShaderType
{
    XSHADER_YUV420P = 0,    // YUV420P格式（三个独立平面：Y, U, V）
    XSHADER_NV12 = 25,      // NV12格式（Y平面 + UV交错平面）
    XSHADER_NV21 = 26        // NV21格式（Y平面 + VU交错平面）
};

// 着色器管理类
class XShader
{
public:
    // 初始化着色器程序
    // type: 着色器类型（默认为YUV420P）
    // 返回值: 成功返回true，失败返回false
    virtual bool Init(XShaderType type = XSHADER_YUV420P);

    // 关闭着色器并释放资源
    virtual void Close();

    // 更新纹理数据
    // index: 纹理索引（0=Y分量，1=U或UV分量，2=V分量）
    // width: 纹理宽度
    // height: 纹理高度
    // buf: 纹理数据指针
    // isa: 是否为双通道纹理（UV交错数据）
    virtual void GetTexture(unsigned int index, int width, int height,unsigned char *buf, bool isa = false);

    // 执行绘制命令
    virtual void Draw();

protected:
    unsigned int vsh = 0;        // 顶点着色器ID
    unsigned int fsh = 0;         // 片元着色器ID
    unsigned int program = 0;     // 着色器程序ID
    unsigned int texts[100] = {0}; // 纹理对象ID数组（支持最多100个纹理）
    std::mutex mux;              // 互斥锁，保证线程安全
};

#endif //XPLAY_XSHADER_H  // 头文件结束