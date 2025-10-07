#include "XTexture.h"   // 包含纹理抽象接口定义
#include "XLog.h"       // 日志模块
#include "XEGL.h"       // EGL环境管理
#include "XShader.h"    // 着色器管理

// CXTexture是XTexture接口的具体实现类
class CXTexture: public XTexture
{
public:
    XShader sh;         // 着色器管理对象，负责编译/链接着色器程序
    XTextureType type;  // 当前纹理格式（YUV420P/NV12/NV21）
    std::mutex mux;     // 互斥锁，保证线程安全

    // 资源释放方法（实现XTexture的纯虚函数）
    virtual void Drop()
    {
        mux.lock();             // 加锁保证线程安全
        XEGL::Get()->Close();   // 关闭EGL环境（释放Display/Surface/Context）
        sh.Close();             // 关闭着色器程序（释放GPU资源）
        mux.unlock();           // 解锁
        delete this;            // 自销毁对象（注意：对象必须在堆上分配）
    }

    // 初始化方法（实现XTexture的纯虚函数）
    virtual bool Init(void *win, XTextureType type)
    {
        mux.lock();                     // 加锁保证线程安全

        // 清理可能存在的旧资源
        XEGL::Get()->Close();           // 关闭现有EGL环境
        sh.Close();                     // 关闭现有着色器程序

        this->type = type;              // 保存纹理格式类型

        // 参数检查：渲染窗口必须有效
        if(!win)
        {
            mux.unlock();               // 解锁
            XLOGE("XTexture Init failed win is NULL"); // 记录错误日志
            return false;                // 返回失败
        }

        // 初始化EGL环境
        if(!XEGL::Get()->Init(win))     // 创建EGLDisplay/EGLSurface/EGLContext
        {
            mux.unlock();               // 解锁
            return false;                // 返回失败
        }

        // 初始化着色器程序（根据纹理格式选择不同的着色器）
        sh.Init((XShaderType)type);     // 编译/链接着色器，准备Uniform变量

        mux.unlock();                   // 解锁
        return true;                     // 返回成功
    }

    // 渲染方法（实现XTexture的纯虚函数）
    virtual void Draw(unsigned char *data[], int width, int height)
    {
        mux.lock();     // 加锁保证线程安全

        // 上传Y分量到纹理单元0
        // 参数：纹理单元索引，宽度，高度，数据指针
        sh.GetTexture(0, width, height, data[0]);  // Y分量

        // 根据纹理格式处理UV分量
        if(type == XTEXTURE_YUV420P)
        {
            // YUV420P格式：三个独立平面
            sh.GetTexture(1, width/2, height/2, data[1]);  // U分量（宽高减半）
            sh.GetTexture(2, width/2, height/2, data[2]);  // V分量（宽高减半）
        }
        else
        {
            // NV12/NV21格式：UV分量交错存储在一个平面
            // 参数：纹理单元索引，宽度，高度，数据指针，是否UV交错
            sh.GetTexture(1, width/2, height/2, data[1], true);  // UV分量（宽高减半）
        }

        sh.Draw();              // 执行着色器绘制命令（glDrawArrays）
        XEGL::Get()->Draw();    // 交换缓冲区（eglSwapBuffers）

        mux.unlock();           // 解锁
    }
};

// XTexture的工厂方法实现
XTexture *XTexture::Create()
{
    return new CXTexture();  // 创建CXTexture实例（堆分配）
}