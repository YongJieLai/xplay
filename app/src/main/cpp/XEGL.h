#ifndef XPLAY_XEGL_H  // 防止头文件重复包含
#define XPLAY_XEGL_H

// EGL抽象接口类

// EGL（Embedded-System Graphics Library）是Khronos集团制定的标准接口，
// 核心作用是充当本地窗口系统（如Android的SurfaceFlinger、Linux的X Window）与跨平台图形API（如OpenGL ES、Vulkan）之间的桥梁。
//   1、上下文管理：创建并维护OpenGL ES的渲染状态容器（EGLContext），隔离不同线程或应用的图形环境；
//   2、表面绑定：将本地窗口（ANativeWindow）封装为EGLSurface，使OpenGL ES的渲染输出能定向到屏幕或离屏缓冲区；
//   3、缓冲交换：通过双缓冲机制（eglSwapBuffers）实现渲染/显示的分离，在垂直同步时交换前后台帧缓冲区，避免画面撕裂；
//   4、资源协商：解决不同GPU硬件的能力差异（如纹理格式支持），通过eglChooseConfig匹配最优的像素格式和渲染特性；
//   5、跨平台抽象：统一Android/iOS/Windows等系统的图形初始化流程，使OpenGL ES代码无需关注底层窗口管理细节。
//  在移动端视频渲染中，EGL是连接MediaCodec解码输出（Surface）与OpenGL ES着色器渲染的关键基础设施，直接影响渲染效率和画面稳定性
class XEGL {
public:
    // 初始化EGL环境
    // win: 平台相关的窗口句柄（Android: ANativeWindow*, iOS: CAEAGLLayer*）
    // 返回值: 成功返回true，失败返回false
    virtual bool Init(void *win) = 0;

    // 关闭并释放EGL资源
    virtual void Close() = 0;

    // 执行缓冲区交换（将渲染结果显示到屏幕）
    virtual void Draw() = 0;

    // 获取EGL实例（单例模式）
    static XEGL *Get();

protected:
    // 保护构造函数（只能通过子类实例化）
    XEGL(){}
};

#endif //XPLAY_XEGL_H  // 头文件结束