//
// Created by 赖勇杰 on 2025/10/2.
//

#include "XEGL.h"
class CXEGL:public XEGL
{
public:
    virtual bool Init(void *win)
    {
        return true;
    }
};

XEGL *XEGL::Get()
{
    static CXEGL egl;
    return &egl;
}