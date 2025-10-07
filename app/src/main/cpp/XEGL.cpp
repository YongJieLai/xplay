#include <android/native_window.h>  // Android原生窗口头文件
#include <EGL/egl.h>                // EGL库头文件
#include <mutex>                    // 互斥锁头文件
#include "XEGL.h"                   // EGL抽象接口
#include "XLog.h"                   // 日志模块

// CXEGL类实现XEGL接口
class CXEGL : public XEGL {
public:
    EGLDisplay display = EGL_NO_DISPLAY;  // EGL显示连接
    EGLSurface surface = EGL_NO_SURFACE;   // EGL渲染表面
    EGLContext context = EGL_NO_CONTEXT;   // EGL渲染上下文
    std::mutex mux;                       // 线程安全互斥锁

    // 交换缓冲区（将渲染结果显示到屏幕）
    virtual void Draw() {
        mux.lock();  // 加锁保证线程安全

        // 检查EGL环境是否有效
        if (display == EGL_NO_DISPLAY || surface == EGL_NO_SURFACE) {
            mux.unlock();
            return;
        }

        // 执行缓冲区交换（将后台缓冲区内容显示到屏幕）
        eglSwapBuffers(display, surface);

        mux.unlock();  // 解锁
    }

    // 关闭并释放EGL资源
    virtual void Close() {
        mux.lock();  // 加锁

        // 检查是否已初始化
        if (display == EGL_NO_DISPLAY) {
            mux.unlock();
            return;
        }

        // 解除当前上下文绑定
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

        // 销毁渲染表面
        if (surface != EGL_NO_SURFACE)
            eglDestroySurface(display, surface);

        // 销毁渲染上下文
        if (context != EGL_NO_CONTEXT)
            eglDestroyContext(display, context);

        // 终止EGL显示连接
        eglTerminate(display);

        // 重置状态
        display = EGL_NO_DISPLAY;
        surface = EGL_NO_SURFACE;
        context = EGL_NO_CONTEXT;

        mux.unlock();  // 解锁
    }

    // 初始化EGL环境
    virtual bool Init(void *win) {
        ANativeWindow *nwin = (ANativeWindow *)win;  // 转换Android原生窗口

        Close();  // 先关闭可能存在的旧环境

        mux.lock();  // 加锁

        // 1. 获取默认显示连接
        display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (display == EGL_NO_DISPLAY) {
            mux.unlock();
            XLOGE("eglGetDisplay failed!");
            return false;
        }
        XLOGE("eglGetDisplay success!");

        // 2. 初始化EGL
        if (EGL_TRUE != eglInitialize(display, 0, 0)) {
            mux.unlock();
            XLOGE("eglInitialize failed!");
            return false;
        }
        XLOGE("eglInitialize success!");

        // 3. 配置EGL属性
        EGLint configSpec[] = {
                EGL_RED_SIZE, 8,           // 红色通道8位
                EGL_GREEN_SIZE, 8,         // 绿色通道8位
                EGL_BLUE_SIZE, 8,          // 蓝色通道8位
                EGL_SURFACE_TYPE, EGL_WINDOW_BIT, // 窗口类型表面
                EGL_NONE                   // 结束标记
        };

        EGLConfig config = 0;
        EGLint numConfigs = 0;

        // 选择匹配的配置
        if (EGL_TRUE != eglChooseConfig(display, configSpec, &config, 1, &numConfigs)) {
            mux.unlock();
            XLOGE("eglChooseConfig failed!");
            return false;
        }
        XLOGE("eglChooseConfig success!");

        // 创建EGL窗口表面
        surface = eglCreateWindowSurface(display, config, nwin, NULL);
        if (surface == EGL_NO_SURFACE) {
            mux.unlock();
            XLOGE("eglCreateWindowSurface failed!");
            return false;
        }
        XLOGE("eglCreateWindowSurface success!");

        // 4. 创建EGL上下文
        const EGLint ctxAttr[] = {
                EGL_CONTEXT_CLIENT_VERSION, 2, // 使用OpenGL ES 2.0
                EGL_NONE
        };

        context = eglCreateContext(display, config, EGL_NO_CONTEXT, ctxAttr);
        if (context == EGL_NO_CONTEXT) {
            mux.unlock();
            XLOGE("eglCreateContext failed!");
            return false;
        }
        XLOGE("eglCreateContext success!");

        // 5. 绑定上下文到当前线程
        if (EGL_TRUE != eglMakeCurrent(display, surface, surface, context)) {
            mux.unlock();
            XLOGE("eglMakeCurrent failed!");
            return false;
        }
        XLOGE("eglMakeCurrent success!");

        mux.unlock();  // 解锁
        return true;
    }
};

// 单例模式获取EGL实例
XEGL *XEGL::Get() {
    static CXEGL egl;  // 静态实例（线程安全）
    return &egl;
}