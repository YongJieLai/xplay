//
// Created by 赖勇杰 on 2025/10/2.
// 10/3
//
#include "GLVideoView.h"
#include "XTexture.h"
#include "XLog.h"
void GLVideoView::SetRender(void *win)
{
    view = win;
}
void GLVideoView::Render(XData data)
{

    if(!view) return;
    if(!txt)
    {
        txt = XTexture::Create();

        txt->Init(view,(XTextureType)data.format);
    }
    txt->Draw(data.datas,data.width,data.height);
}