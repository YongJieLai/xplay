//
// Created by 赖勇杰 on 2025/10/3.
//
#include "IResample.h"
#include "XLog.h"

void IResample::Update(XData data)
{

    XData d = this->Resample(data);
    //XLOGE("his->Resample(data) %d",d.size);
    if(d.size > 0)
    {
        this->Notify(d);
    }
}