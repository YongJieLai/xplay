//
// Created by 赖勇杰 on 2025/10/2.
// 10/3
//

#ifndef XPLAY_XEGL_H
#define XPLAY_XEGL_H

class XEGL
{
public:
    virtual bool Init(void *win) = 0;
    virtual void Draw() = 0;
    static XEGL *Get();

protected:
    XEGL(){}
};


#endif //XPLAY_XEGL_H
