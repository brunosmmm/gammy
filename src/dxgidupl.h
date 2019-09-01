/**
 * Copyright (C) 2019 Francesco Fusco. All rights reserved.
 * License: https://github.com/Fushko/gammy#license
 */

#ifdef _WIN32
#ifndef DXGIDUPL_H
#define DXGIDUPL_H
#include <Windows.h>
#include <dxgi.h>
#include <dxgi1_2.h>
#include <d3d11.h>
#include <stdint.h>

class DXGIDupl
{
    ID3D11Device*			d3d_device;
    ID3D11DeviceContext*	d3d_context;
    IDXGIOutput1*			output1;
    IDXGIOutputDuplication* duplication;
    D3D11_TEXTURE2D_DESC	tex_desc;

    size_t bufLen;

public:
    DXGIDupl();

    bool initDXGI();
    bool getDXGISnapshot(uint8_t* buf) noexcept;
    void restartDXGI();

    ~DXGIDupl();
};

extern DXGIDupl dx;

#endif // DXGIDUPL_H
#endif
