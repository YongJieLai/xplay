//
// Created by 赖勇杰 on 2025/10/2.
//

#ifndef XPLAY_XDATA_H
#define XPLAY_XDATA_H


struct XData {
    unsigned char *data = nullptr;
    int size = 0;
    bool isAudio = false;
    void Drop();
};


#endif