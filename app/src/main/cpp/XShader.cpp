#include "XShader.h"
#include "XLog.h"
#include <GLES2/gl2.h>  // OpenGL ES 2.0头文件

// 宏定义：将多行字符串转换为单行字符串
#define GET_STR(x) #x

// 顶点着色器GLSL代码
static const char *vertexShader = GET_STR(
        attribute vec4 aPosition; // 顶点位置属性（x,y,z,w）
        attribute vec2 aTexCoord; // 纹理坐标属性（u,v）
        varying vec2 vTexCoord;   // 传递给片元着色器的纹理坐标

        void main() {
            // 翻转V坐标（OpenGL纹理坐标原点在左下，通常图像原点在左上）
            vTexCoord = vec2(aTexCoord.x, 1.0 - aTexCoord.y);
            // 直接使用传入的顶点位置（已处理为标准化设备坐标）
            gl_Position = aPosition;
        }
);

// YUV420P格式的片元着色器
static const char *fragYUV420P = GET_STR(
        precision mediump float;    // 设置浮点数精度
        varying vec2 vTexCoord;     // 来自顶点着色器的纹理坐标
        uniform sampler2D yTexture; // Y分量纹理采样器
        uniform sampler2D uTexture; // U分量纹理采样器
        uniform sampler2D vTexture; // V分量纹理采样器

        void main() {
            vec3 yuv;  // 存储YUV值
            vec3 rgb;  // 存储转换后的RGB值

            // 从纹理中采样YUV分量
            yuv.r = texture2D(yTexture, vTexCoord).r; // 获取Y分量（红色通道）
            yuv.g = texture2D(uTexture, vTexCoord).r - 0.5; // 获取U分量并归一化
            yuv.b = texture2D(vTexture, vTexCoord).r - 0.5; // 获取V分量并归一化

            // YUV转RGB矩阵计算（BT.601标准）
            rgb = mat3(
                    1.0,     1.0,    1.0,     // Y分量系数
                    0.0,    -0.39465, 2.03211, // U分量系数
                    1.13983, -0.58060, 0.0     // V分量系数
            ) * yuv;

            // 输出RGBA颜色（完全不透明）
            gl_FragColor = vec4(rgb, 1.0);
        }
);

// NV12格式的片元着色器
static const char *fragNV12 = GET_STR(
        precision mediump float;    // 设置浮点数精度
        varying vec2 vTexCoord;     // 来自顶点着色器的纹理坐标
        uniform sampler2D yTexture; // Y分量纹理采样器
        uniform sampler2D uvTexture; // UV交错纹理采样器

        void main() {
            vec3 yuv;  // 存储YUV值
            vec3 rgb;  // 存储转换后的RGB值

            // 从纹理中采样YUV分量
            yuv.r = texture2D(yTexture, vTexCoord).r; // Y分量
            yuv.g = texture2D(uvTexture, vTexCoord).r - 0.5; // U分量（红色通道）
            yuv.b = texture2D(uvTexture, vTexCoord).a - 0.5; // V分量（Alpha通道）

            // YUV转RGB矩阵计算（BT.601标准）
            rgb = mat3(
                    1.0,     1.0,    1.0,
                    0.0,    -0.39465, 2.03211,
                    1.13983, -0.58060, 0.0
            ) * yuv;

            // 输出RGBA颜色
            gl_FragColor = vec4(rgb, 1.0);
        }
);

// NV21格式的片元着色器
static const char *fragNV21 = GET_STR(
        precision mediump float;    // 设置浮点数精度
        varying vec2 vTexCoord;     // 来自顶点着色器的纹理坐标
        uniform sampler2D yTexture; // Y分量纹理采样器
        uniform sampler2D uvTexture; // VU交错纹理采样器

        void main() {
            vec3 yuv;  // 存储YUV值
            vec3 rgb;  // 存储转换后的RGB值

            // 从纹理中采样YUV分量
            yuv.r = texture2D(yTexture, vTexCoord).r; // Y分量
            yuv.g = texture2D(uvTexture, vTexCoord).a - 0.5; // U分量（Alpha通道）
            yuv.b = texture2D(uvTexture, vTexCoord).r - 0.5; // V分量（红色通道）

            // YUV转RGB矩阵计算（BT.601标准）
            rgb = mat3(
                    1.0,     1.0,    1.0,
                    0.0,    -0.39465, 2.03211,
                    1.13983, -0.58060, 0.0
            ) * yuv;

            // 输出RGBA颜色
            gl_FragColor = vec4(rgb, 1.0);
        }
);

// 初始化着色器函数
static GLuint InitShader(const char *code, GLint type) {
    // 创建着色器对象
    GLuint sh = glCreateShader(type);
    if (sh == 0) {
        XLOGE("glCreateShader %d failed!", type);
        return 0;
    }

    // 加载着色器源代码
    glShaderSource(sh, 1, &code, 0);

    // 编译着色器
    glCompileShader(sh);

    // 检查编译状态
    GLint status;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &status);
    if (status == 0) {
        XLOGE("glCompileShader failed!");
        return 0;
    }

    XLOGE("glCompileShader success!");
    return sh;
}

// 关闭着色器程序，释放资源
void XShader::Close() {
    mux.lock();  // 加锁保证线程安全

    // 释放着色器程序
    if (program) glDeleteProgram(program);
    if (fsh) glDeleteShader(fsh);
    if (vsh) glDeleteShader(vsh);

    // 释放纹理对象
    for (int i = 0; i < sizeof(texts) / sizeof(unsigned int); i++) {
        if (texts[i]) {
            glDeleteTextures(1, &texts[i]);
        }
        texts[i] = 0;  // 重置纹理ID
    }

    mux.unlock();  // 解锁
}

// 初始化着色器程序
bool XShader::Init(XShaderType type) {
    Close();  // 先关闭可能存在的旧资源

    mux.lock();  // 加锁

    // 编译顶点着色器
    vsh = InitShader(vertexShader, GL_VERTEX_SHADER);
    if (vsh == 0) {
        mux.unlock();
        XLOGE("顶点着色器初始化失败!");
        return false;
    }

    // 根据格式选择片元着色器
    switch (type) {
        case XSHADER_YUV420P:
            fsh = InitShader(fragYUV420P, GL_FRAGMENT_SHADER);
            break;
        case XSHADER_NV12:
            fsh = InitShader(fragNV12, GL_FRAGMENT_SHADER);
            break;
        case XSHADER_NV21:
            fsh = InitShader(fragNV21, GL_FRAGMENT_SHADER);
            break;
        default:
            mux.unlock();
            XLOGE("不支持的着色器类型");
            return false;
    }

    if (fsh == 0) {
        mux.unlock();
        XLOGE("片元着色器初始化失败!");
        return false;
    }

    // 创建着色器程序
    program = glCreateProgram();
    if (program == 0) {
        mux.unlock();
        XLOGE("创建着色器程序失败!");
        return false;
    }

    // 附加着色器到程序
    glAttachShader(program, vsh);
    glAttachShader(program, fsh);

    // 链接程序
    glLinkProgram(program);
    GLint status = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (status != GL_TRUE) {
        mux.unlock();
        XLOGE("程序链接失败!");
        return false;
    }

    // 使用当前着色器程序
    glUseProgram(program);

    // 设置顶点数据（覆盖整个视口的矩形）
    static float vers[] = {
            1.0f, -1.0f, 0.0f,   // 右下
            -1.0f, -1.0f, 0.0f,  // 左下
            1.0f, 1.0f, 0.0f,    // 右上
            -1.0f, 1.0f, 0.0f    // 左上
    };

    // 获取并启用顶点位置属性
    GLuint apos = (GLuint)glGetAttribLocation(program, "aPosition");
    glEnableVertexAttribArray(apos);
    glVertexAttribPointer(apos, 3, GL_FLOAT, GL_FALSE, 12, vers);

    // 设置纹理坐标数据
    static float txts[] = {
            1.0f, 0.0f,  // 右下
            0.0f, 0.0f,  // 左下
            1.0f, 1.0f,  // 右上
            0.0f, 1.0f   // 左上
    };

    // 获取并启用纹理坐标属性
    GLuint atex = (GLuint)glGetAttribLocation(program, "aTexCoord");
    glEnableVertexAttribArray(atex);
    glVertexAttribPointer(atex, 2, GL_FLOAT, GL_FALSE, 8, txts);

    // 设置纹理单元绑定
    glUniform1i(glGetUniformLocation(program, "yTexture"), 0); // Y纹理绑定到0单元

    // 根据格式设置其他纹理单元
    switch (type) {
        case XSHADER_YUV420P:
            glUniform1i(glGetUniformLocation(program, "uTexture"), 1); // U纹理绑定到1单元
            glUniform1i(glGetUniformLocation(program, "vTexture"), 2); // V纹理绑定到2单元
            break;
        case XSHADER_NV21:
        case XSHADER_NV12:
            glUniform1i(glGetUniformLocation(program, "uvTexture"), 1); // UV纹理绑定到1单元
            break;
    }

    mux.unlock();  // 解锁
    XLOGI("着色器初始化成功！");
    return true;
}

// 执行绘制命令
void XShader::Draw() {
    mux.lock();  // 加锁
    if (!program) {
        mux.unlock();
        return;
    }

    // 绘制三角形条带（两个三角形组成矩形）
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    mux.unlock();  // 解锁
}

// 更新纹理数据
void XShader::GetTexture(unsigned int index, int width, int height, unsigned char *buf, bool isa) {
    // 确定纹理格式（单通道或双通道）
    unsigned int format = isa ? GL_LUMINANCE_ALPHA : GL_LUMINANCE;

    mux.lock();  // 加锁

    // 如果纹理不存在则创建
    if (texts[index] == 0) {
        glGenTextures(1, &texts[index]);  // 生成纹理ID

        // 绑定并配置纹理
        glBindTexture(GL_TEXTURE_2D, texts[index]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); // 缩小滤波
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); // 放大滤波

        // 分配纹理内存
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, NULL);
    }

    // 激活纹理单元并绑定纹理
    glActiveTexture(GL_TEXTURE0 + index);
    glBindTexture(GL_TEXTURE_2D, texts[index]);

    // 更新纹理数据
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, format, GL_UNSIGNED_BYTE, buf);

    mux.unlock();  // 解锁
}