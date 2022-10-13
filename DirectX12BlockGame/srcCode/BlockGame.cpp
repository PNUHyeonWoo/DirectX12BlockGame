#include "DirectX12App.h"
#include "UtilLib/3Dphysics.h"
#include <ioStream>
#define _USE_MATH_DEFINES
#include <math.h>
#include <map>


using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace std;

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")


struct PassCB
{
    XMFLOAT4X4 ViewProj;
    XMFLOAT4 Eye;
    XMFLOAT4 LightDir;
    XMFLOAT4 LightPower;
    XMFLOAT4 Ambient;
};

struct Vertex
{
    XMFLOAT3 Pos;
    XMFLOAT3 Normal;
    XMFLOAT2 TexC;
};

struct myMatarial
{
    float Roughness = 0.25f;
    XMFLOAT3 FresnelR0 = { 0.01f,0.01f,0.01f };
};

struct FrameResource 
{
    ComPtr<ID3D12CommandAllocator> cmdAllocator;

    UploadBuffer<PassCB>* passCBUploadBuffer;

    UINT dirtyFlag = 3;

    UINT FenceIndex = 0;

};

struct DynamicBlock
{
    XMFLOAT3 BlockXYZ;
    int TextrueIndex;
};

class BlockGameApp : public DirectX12App 
{
public:
    BlockGameApp(HINSTANCE hin) :DirectX12App(hin) { mMainWindowCaption = L"BlockGame App"; }
    ~BlockGameApp()
    {
        if (mDevice != nullptr)
            FlushCommandQueue();
    }

    virtual bool Initialize()override;

private:
	void CreateRootSignature();
	void CreateInputLayout();
	void CompileShaders();
	void CreatePSO();

    void CreateFrameResource();
    void CreateBoxGeoToVBandIB();
    void CreateMatarials();
    void LoadTexturesAndCreateSrvHeapAndSrvs();

    void InitPassCB();

    virtual void onResize()override;

    virtual void Update(Timer& gt)override;
    void UpdateMouse();
    void UpdateMove(Timer& timer);
    void SetViewProjAndEye();
    void MouseClickEvent();

    virtual void Draw(Timer& gt)override;

    void DrawGround();
    void DrawDynamicBlocks();

    map<int, DynamicBlock>::iterator FindBlock(XMFLOAT3 BlockXYZ);
    void InsertBlock(XMFLOAT3 BlockXYZ, int TextureIndex);
    void RemoveBlock(XMFLOAT3 BlockXYZ);

    virtual LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)override;

    
    XMFLOAT3 GetD(float radius, float phi, float theta);
    void PlayerMoveWithCollisionCheck(XMFLOAT3 deltaMove);
    XMFLOAT3 GetPlayerBlockLow();
    XMFLOAT3 RaycastBox(char& c);

private:
	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

	ComPtr<ID3DBlob> mVSBlob;
	ComPtr<ID3DBlob> mPSBlob;
	
	ComPtr<ID3D12PipelineState> mPSO = nullptr;

    vector<FrameResource> mFrameResources;
    UINT mCurFrameResourceIndex = 0;
    UINT mNumFrameResources = 3;

    ComPtr<ID3D12Resource> mVertexBuffer;
    ComPtr<ID3D12Resource> mIndexBuffer;
    ComPtr<ID3D12Resource> mVertexTmpUploadBuffer;
    ComPtr<ID3D12Resource> mIndexTmpUploadBuffer;
    D3D12_VERTEX_BUFFER_VIEW mVBV;
    D3D12_INDEX_BUFFER_VIEW mIBV;
    UINT indexSize;

    ComPtr<ID3D12Resource> mMatatialBuffers[3];
    ComPtr<ID3D12Resource> mMatatialTmpUploadBuffers[3];

    ComPtr<ID3D12Resource> mTextureResource[3];
    ComPtr<ID3D12Resource> mTextureTmpUploadBuffers[3];
    ComPtr<ID3D12DescriptorHeap> mSrvHeap;

    map<int, DynamicBlock> DynamicBlocks;

    XMFLOAT3 playerPos = { 14.0f,1.5f,14.0f };
    float playerSpeed = 3.0f;
    XMFLOAT2 playerSight = { M_PI/2, 0.0f };
    XMFLOAT3 playerSightD = { 1.0f,0.0f,0.0f };
    PassCB cpuPassCB;
    bool wasdSpace[5] = { false ,false ,false ,false ,false };
    int LastMousePos[2] = {-1,-1};
    int newMousePos[2] = {-1,-1};
    bool isMouseClick[2] = {false,false};
    int selectTexture = 0;

    float nowGravitySpeed = 5.0f;
    float nowJump = 0.0f;

};


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
    PSTR cmdLine, int showCmd)
{
    try
    {
        BlockGameApp theApp(hInstance);
        if (!theApp.Initialize())
            return 0;

        return theApp.Run();
    }
    catch (DxException& e)
    {
        MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
        return 0;
    }
    catch (int exit)
    {
        return 0;
    }
}

LRESULT BlockGameApp::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {

    case WM_KEYDOWN :
        switch (wParam)
        {
        case 'W':
            wasdSpace[0] = true;
            break;
        case 'A':
            wasdSpace[1] = true;
            break;
        case 'S':
            wasdSpace[2] = true;
            break;
        case 'D':
            wasdSpace[3] = true;
            break;
        case VK_SPACE:
            wasdSpace[4] = true;
            break;
        case '1':
            selectTexture = 0;
            break;
        case '2':
            selectTexture = 1;
            break;
        case '3':
            selectTexture = 2;
            break;
        case VK_ESCAPE:
            throw 0;
        default:
            break;
        }
        break;

    case WM_KEYUP :
        switch (wParam)
        {
        case 'W':
            wasdSpace[0] = false;
            break;
        case 'A':
            wasdSpace[1] = false;
            break;
        case 'S':
            wasdSpace[2] = false;
            break;
        case 'D':
            wasdSpace[3] = false;
            break;
        case VK_SPACE:
            wasdSpace[4] = false;
            break;
        default:
            break;
        }
        break;

    case WM_MOUSEMOVE:
    {
        newMousePos[0] = LOWORD(lParam);
        newMousePos[1] = HIWORD(lParam);

        POINT cp{};
        GetCursorPos(&cp);
        RECT lp{};
        GetWindowRect(mHWND, &lp);

        int lx = cp.x - lp.left;
        int ly = cp.y - lp.top;

        if (lx < mWidth / 10 || mWidth * 9 / 10 < lx || ly < mHeight / 10 || mHeight * 9 / 10 < ly)
        {
            SetCursorPos(lp.left + mWidth/2, lp.top + mHeight/2);
            newMousePos[0] = -1;
            newMousePos[1] = -1;
            LastMousePos[0] = -1;
            LastMousePos[1] = -1;
        }

        break;
    }

    case WM_LBUTTONDOWN:
        isMouseClick[0] = true;
        break;
    case WM_RBUTTONDOWN:
        isMouseClick[1] = true;
        break;
    default:
        break;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
};


bool BlockGameApp::Initialize() 
{
    if (!DirectX12App::Initialize())
        return false;

    ThrowIfFailed(mCommandList->Reset(mCommonAllocator.Get(), nullptr));

    CreateRootSignature();
    CreateInputLayout();
    CompileShaders();
    CreatePSO();

    CreateFrameResource();
    CreateBoxGeoToVBandIB();
    CreateMatarials();
    LoadTexturesAndCreateSrvHeapAndSrvs();

    InitPassCB();
    ShowCursor(false);

    //test
    InsertBlock({ 20,1,16 }, 0);
    InsertBlock({ 20,1,15 }, 1);
    InsertBlock({ 20,1,14 }, 2);
    //

    ThrowIfFailed(mCommandList->Close());
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    FlushCommandQueue();

    return true;

}

void BlockGameApp::CreateRootSignature() 
{
    CD3DX12_DESCRIPTOR_RANGE texRange;
    texRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

    CD3DX12_ROOT_PARAMETER slotRootParameter[4];

    slotRootParameter[0].InitAsDescriptorTable(1, &texRange, D3D12_SHADER_VISIBILITY_PIXEL);
    slotRootParameter[1].InitAsConstantBufferView(0);
    slotRootParameter[2].InitAsConstantBufferView(1);
    slotRootParameter[3].InitAsConstants(3,2);

    CD3DX12_STATIC_SAMPLER_DESC samplers[1] = {CD3DX12_STATIC_SAMPLER_DESC(
        0,
        D3D12_FILTER_ANISOTROPIC,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        0.0f,
        8) };


    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(4, slotRootParameter,
        1, samplers,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

 
    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;
    HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
        serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

    if (errorBlob != nullptr)
    {
        ::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    }
    ThrowIfFailed(hr);

    ThrowIfFailed(mDevice->CreateRootSignature(
        0,
        serializedRootSig->GetBufferPointer(),
        serializedRootSig->GetBufferSize(),
        IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}
void BlockGameApp::CreateInputLayout() 
{
    mInputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };
}
void BlockGameApp::CompileShaders() 
{
    mVSBlob = d3dUtil::CompileShader(L"srcCode\\Shaders\\Default.hlsl", nullptr, "VS", "vs_5_0");
    mPSBlob = d3dUtil::CompileShader(L"srcCode\\Shaders\\Default.hlsl", nullptr, "PS", "ps_5_0");
}
void BlockGameApp::CreatePSO() 
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;

    ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

    opaquePsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
    opaquePsoDesc.pRootSignature = mRootSignature.Get();
    opaquePsoDesc.VS = { reinterpret_cast<BYTE*>(mVSBlob->GetBufferPointer()), mVSBlob->GetBufferSize() };
    opaquePsoDesc.PS = { reinterpret_cast<BYTE*>(mPSBlob->GetBufferPointer()), mPSBlob->GetBufferSize() };
    opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    opaquePsoDesc.SampleMask = UINT_MAX;
    opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    opaquePsoDesc.NumRenderTargets = 1;
    opaquePsoDesc.RTVFormats[0] = mBackBufferFormat;
    opaquePsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
    opaquePsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
    opaquePsoDesc.DSVFormat = mDepthStencilFormat;
    ThrowIfFailed(mDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&mPSO)));
}

void BlockGameApp::CreateFrameResource() 
{
    for (int i = 0; i < mNumFrameResources; i++)
    {
        FrameResource r{};
        ThrowIfFailed(mDevice->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(r.cmdAllocator.GetAddressOf())));
        r.passCBUploadBuffer = new UploadBuffer<PassCB>(mDevice.Get(), 1, true);
        mFrameResources.push_back(r);
    }
}
void BlockGameApp::CreateBoxGeoToVBandIB() 
{
    GeometryGenerator geoGen;
    GeometryGenerator::MeshData box = geoGen.CreateBox(1.0f, 1.0f, 1.0f, 0);

    std::vector<Vertex> vertices(box.Vertices.size());

    for (size_t i = 0; i < box.Vertices.size(); ++i)
    {
        vertices[i].Pos = box.Vertices[i].Position;
        vertices[i].Normal = box.Vertices[i].Normal;
        vertices[i].TexC = box.Vertices[i].TexC;
    }

    std::vector<std::uint16_t> indices = box.GetIndices16();

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
    const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);
    indexSize = indices.size();


    mVertexBuffer = d3dUtil::CreateDefaultBuffer(mDevice.Get(),
        mCommandList.Get(), vertices.data(), vbByteSize, mVertexTmpUploadBuffer);

    mIndexBuffer = d3dUtil::CreateDefaultBuffer(mDevice.Get(),
        mCommandList.Get(), indices.data(), ibByteSize, mIndexTmpUploadBuffer);

    mVBV.BufferLocation = mVertexBuffer->GetGPUVirtualAddress();
    mVBV.StrideInBytes = sizeof(Vertex);
    mVBV.SizeInBytes = vbByteSize;

    mIBV.BufferLocation = mIndexBuffer->GetGPUVirtualAddress();
    mIBV.Format = DXGI_FORMAT_R16_UINT;
    mIBV.SizeInBytes = ibByteSize;
}

void BlockGameApp::CreateMatarials() 
{
    myMatarial tmp = { 0.5f,{0.02f,0.02f,0.02f} };
    mMatatialBuffers[0] = d3dUtil::CreateDefaultBuffer(mDevice.Get(),
        mCommandList.Get(), &tmp, sizeof(myMatarial), mMatatialTmpUploadBuffers[0]);
    tmp = { 0.3f,{0.02f,0.02f,0.02f} };
    mMatatialBuffers[1] = d3dUtil::CreateDefaultBuffer(mDevice.Get(),
        mCommandList.Get(), &tmp, sizeof(myMatarial), mMatatialTmpUploadBuffers[1]);
    tmp = { 0.01f,{0.4f,0.4f,0.4f} };
    mMatatialBuffers[2] = d3dUtil::CreateDefaultBuffer(mDevice.Get(),
        mCommandList.Get(), &tmp, sizeof(myMatarial), mMatatialTmpUploadBuffers[2]);
}

void BlockGameApp::LoadTexturesAndCreateSrvHeapAndSrvs()
{
    ThrowIfFailed(CreateDDSTextureFromFile12(mDevice.Get(),
        mCommandList.Get(), wstring(L"Textures\\grass.dds").c_str(),
        mTextureResource[0], mTextureTmpUploadBuffers[0]));
    ThrowIfFailed(CreateDDSTextureFromFile12(mDevice.Get(),
        mCommandList.Get(), wstring(L"Textures\\bricks.dds").c_str(),
        mTextureResource[1], mTextureTmpUploadBuffers[1]));
    ThrowIfFailed(CreateDDSTextureFromFile12(mDevice.Get(),
        mCommandList.Get(), wstring(L"Textures\\ice.dds").c_str(),
        mTextureResource[2], mTextureTmpUploadBuffers[2]));

    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.NumDescriptors = 3;
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(mDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvHeap)));

    CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(mSrvHeap->GetCPUDescriptorHandleForHeapStart());

    for (int i = 0; i < 3; i++)
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = mTextureResource[i]->GetDesc().Format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.MipLevels = mTextureResource[i]->GetDesc().MipLevels;
        srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

        mDevice->CreateShaderResourceView(mTextureResource[i].Get(), &srvDesc, srvHandle);

        srvHandle.Offset(1, mCbvSrvUavDescriptorSize);
    }
}

void BlockGameApp::InitPassCB() 
{
    cpuPassCB.Ambient = { 0.4f,0.4f,0.4f,0.0f };

    XMVECTOR tmp = { -1.0f,-1.0f,0.0f,0.0f };
    XMStoreFloat4(&cpuPassCB.LightDir, XMVector4Normalize(tmp));
    cpuPassCB.LightPower = { 1.0f,1.0f,1.0f,1.0f };
}

void BlockGameApp::onResize()
{
    DirectX12App::onResize();
}

XMFLOAT3 operator*(XMFLOAT3 f3, float fl)
{
    return { f3.x * fl,f3.y * fl, f3.z * fl };
}

void BlockGameApp::Update(Timer& timer)
{
    UpdateMouse();
    UpdateMove(timer);
    SetViewProjAndEye();
    //cpuPassCB Update Complete
    MouseClickEvent();


    mCurFrameResourceIndex = (mCurFrameResourceIndex + 1) % mNumFrameResources;
    FrameResource* mCurFrameResource = &mFrameResources[mCurFrameResourceIndex];

    if (mFence->GetCompletedValue() < mCurFrameResource->FenceIndex)
    {
        HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
        ThrowIfFailed(mFence->SetEventOnCompletion(mCurFrameResource->FenceIndex, eventHandle));
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }
    
    mCurFrameResource->passCBUploadBuffer->CopyData(0, cpuPassCB);

    isMouseClick[0] = false;
    isMouseClick[1] = false;
}

void BlockGameApp::UpdateMouse() 
{
    int deltaMousePos[2] = { 0,0 };

    if (newMousePos[0] != -1)
    {
        if (LastMousePos[0] != -1)
        {
            deltaMousePos[0] = newMousePos[0] - LastMousePos[0];
            deltaMousePos[1] = newMousePos[1] - LastMousePos[1];
        }
        LastMousePos[0] = newMousePos[0];
        LastMousePos[1] = newMousePos[1];
    }
    playerSight.y -= (float)deltaMousePos[0] * M_PI / 1080;
    playerSight.x += (float)deltaMousePos[1] * M_PI / 1080;

    if (playerSight.y > M_PI)
        playerSight.y -= 2.0f * M_PI;
    else if(playerSight.y < -M_PI)
        playerSight.y += 2.0f * M_PI;

    if (playerSight.x < 0.1f)
        playerSight.x = 0.1f;
    else if (M_PI - 0.1f < playerSight.x)
        playerSight.x = M_PI - 0.1f;

    playerSightD = GetD(1.0f, playerSight.x, playerSight.y);

}

void BlockGameApp::UpdateMove(Timer& timer) 
{
    XMVECTOR deltaXYXMV = XMVector2Normalize(XMVectorSet(playerSightD.x, playerSightD.z, 0.0f, 0.0f));
    XMFLOAT2 deltaXY;
    XMStoreFloat2(&deltaXY, deltaXYXMV);
    deltaXY = { deltaXY.x * timer.DeltaTime() * playerSpeed ,deltaXY.y * timer.DeltaTime() * playerSpeed };
    
    if (((wasdSpace[0] ^ wasdSpace[2]) && (wasdSpace[1] ^ wasdSpace[3])))
    {
        float Csrt2 = 0.7071;
        deltaXY = { deltaXY.x * 0.7071f, deltaXY.y * 0.7071f };
    }

    if (wasdSpace[0])
        PlayerMoveWithCollisionCheck({ deltaXY.x,0.0f,deltaXY.y });
    if (wasdSpace[2])
        PlayerMoveWithCollisionCheck({ -deltaXY.x,0.0f,-deltaXY.y });
    if (wasdSpace[1])
        PlayerMoveWithCollisionCheck({ -deltaXY.y,0.0f,deltaXY.x}); 
    if (wasdSpace[3])
        PlayerMoveWithCollisionCheck({ deltaXY.y,0.0f,-deltaXY.x });

    XMFLOAT3 tmp = playerPos;
    PlayerMoveWithCollisionCheck({ 0.0f,-nowGravitySpeed * timer.DeltaTime(),0.0f});
    if (tmp.x == playerPos.x && tmp.y == playerPos.y && tmp.z == playerPos.z)
    { 
        if (wasdSpace[4])
            nowJump = 10.0f;
        else
            nowJump = 0.0f;
    }

    if (nowJump > 0.0f)
    {
        PlayerMoveWithCollisionCheck({ 0.0f,nowJump * timer.DeltaTime(),0.0f });
        nowJump -= timer.DeltaTime() * 10.0f;
    }
        

}

void BlockGameApp::SetViewProjAndEye()
{
    XMVECTOR pos = XMVectorSet(playerPos.x, playerPos.y + 0.5f, playerPos.z, 1.0f);
    XMVECTOR target = pos + XMLoadFloat3(&playerSightD);
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
    XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi, ScreenRatio(), 0.01f, 1000.0f);

    XMStoreFloat4x4(&cpuPassCB.ViewProj, XMMatrixTranspose(view * P));
    XMStoreFloat4(&cpuPassCB.Eye, pos);
}

void BlockGameApp::MouseClickEvent() 
{
    if (isMouseClick[1])
    {
        char c = 'a';
        XMFLOAT3 target = RaycastBox(c);
        
        if (target.x >= 0.0f)
        {
            switch (c)
            {
            case 'X':
                if (target.x < 30 && FindBlock({ target.x + 1 ,target.y,target.z }) == DynamicBlocks.end())
                    InsertBlock({ target.x + 1 ,target.y,target.z }, selectTexture);        
                break;
            case 'x':
                if (target.x > 1 && FindBlock({ target.x - 1 ,target.y,target.z }) == DynamicBlocks.end())
                    InsertBlock({ target.x - 1 ,target.y,target.z }, selectTexture);
                break;
            case 'Y':
                if (target.y < 30 && FindBlock({ target.x ,target.y+1,target.z }) == DynamicBlocks.end())
                    InsertBlock({ target.x ,target.y+1,target.z }, selectTexture);
                break;
            case 'y':
                if (target.y > 1 && FindBlock({ target.x ,target.y-1,target.z }) == DynamicBlocks.end())
                    InsertBlock({ target.x ,target.y-1,target.z }, selectTexture);
                break;
            case 'Z':
                if (target.z < 30 && FindBlock({ target.x  ,target.y,target.z + 1 }) == DynamicBlocks.end())
                    InsertBlock({ target.x ,target.y,target.z + 1 }, selectTexture);
                break;
            case 'z':
                if (target.z > 1 && FindBlock({ target.x  ,target.y,target.z - 1 }) == DynamicBlocks.end())
                    InsertBlock({ target.x ,target.y,target.z - 1 }, selectTexture);
                break;
            default:
                break;
            }
        }

    }
    else if (isMouseClick[0])
    {
        char c = 'a';
        XMFLOAT3 target = RaycastBox(c);

        if (target.x >= 0.0f && target.y != 0.0f)
            RemoveBlock(target);
    }
}

void BlockGameApp::Draw(Timer& timer)
{
    FrameResource* mCurFrameResource = &mFrameResources[mCurFrameResourceIndex];

    ThrowIfFailed(mCurFrameResource->cmdAllocator->Reset());


    ThrowIfFailed(mCommandList->Reset(mCurFrameResource->cmdAllocator.Get(), mPSO.Get()));

    mCommandList->RSSetViewports(1, &mViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);


    mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));


    mCommandList->ClearRenderTargetView(CurrentBackBufferViewHandle(), Colors::LightSteelBlue, 0, nullptr);
    mCommandList->ClearDepthStencilView(DepthStencilViewHandle(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);


    mCommandList->OMSetRenderTargets(1, &CurrentBackBufferViewHandle(), true, &DepthStencilViewHandle());

    mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

    mCommandList->IASetVertexBuffers(0, 1, &mVBV);
    mCommandList->IASetIndexBuffer(&mIBV);
    mCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvHeap.Get() };
    mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    mCommandList->SetGraphicsRootConstantBufferView(1, mCurFrameResource->passCBUploadBuffer->Resource()->GetGPUVirtualAddress());

    DrawGround();
    DrawDynamicBlocks();

    mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    ThrowIfFailed(mCommandList->Close());


    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);


    ThrowIfFailed(mSwapChain->Present(0, 0));
    mCurBackBufferIndex = (mCurBackBufferIndex + 1) % mBackBufferCount;


    mCurFrameResource->FenceIndex = ++mCurrentFence;

    mCommandQueue->Signal(mFence.Get(), mCurrentFence);
    
}

void BlockGameApp::DrawGround() 
{
    mCommandList->SetGraphicsRootDescriptorTable(0, CD3DX12_GPU_DESCRIPTOR_HANDLE(mSrvHeap->GetGPUDescriptorHandleForHeapStart()).Offset(0, mCbvSrvUavDescriptorSize));
    mCommandList->SetGraphicsRootConstantBufferView(2, mMatatialBuffers[0]->GetGPUVirtualAddress());

    for(int x = 0; x < 30; x++)
        for (int z = 0; z < 30; z++)
        {
            float tmp[3] = { (float)x,-0.5f,(float)z };
            mCommandList->SetGraphicsRoot32BitConstants(3, 3, &tmp, 0);

            mCommandList->DrawIndexedInstanced(
                indexSize,
                1, 0, 0, 0);
        }

}

void BlockGameApp::DrawDynamicBlocks() 
{
    for (map<int,DynamicBlock>::iterator it = DynamicBlocks.begin();it!=DynamicBlocks.end();it++)
    {
        DynamicBlock block = it->second;
        mCommandList->SetGraphicsRootDescriptorTable(0, CD3DX12_GPU_DESCRIPTOR_HANDLE(mSrvHeap->GetGPUDescriptorHandleForHeapStart()).Offset(block.TextrueIndex, mCbvSrvUavDescriptorSize));
        mCommandList->SetGraphicsRootConstantBufferView(2, mMatatialBuffers[block.TextrueIndex]->GetGPUVirtualAddress());

        float pos[3] = { (float)block.BlockXYZ.x-1.0f,(float)block.BlockXYZ.y - 0.5f,(float)block.BlockXYZ.z - 1.0f };
        mCommandList->SetGraphicsRoot32BitConstants(3, 3, &pos, 0);

        mCommandList->DrawIndexedInstanced(
            indexSize,
            1, 0, 0, 0);
    }

}

//BlcokXYZ 1~30 input

map<int,DynamicBlock>::iterator BlockGameApp::FindBlock(XMFLOAT3 BlockXYZ)
{
    return DynamicBlocks.find(BlockXYZ.x * 10000 + BlockXYZ.y * 100 + BlockXYZ.z);
}

void BlockGameApp::InsertBlock(XMFLOAT3 BlockXYZ, int TextureIndex) 
{
    XMFLOAT3 playerlowPoint = { playerPos.x - 0.3f,playerPos.y - 1.0f,playerPos.z - 0.3f };
    XMFLOAT3 playerhighPoint = { playerPos.x + 0.3f,playerPos.y + 0.9f,playerPos.z + 0.3f };

    if (is3DCubeAndCubeCollision(playerlowPoint, playerhighPoint, { BlockXYZ.x - 1.5f,BlockXYZ.y - 1.0f,BlockXYZ.z - 1.5f }, { BlockXYZ.x - 0.5f,BlockXYZ.y,BlockXYZ.z - 0.5f }))
        return;

    DynamicBlocks.insert(pair<int,DynamicBlock>(BlockXYZ.x * 10000 + BlockXYZ.y * 100 + BlockXYZ.z, DynamicBlock{ BlockXYZ,TextureIndex }));
}

void BlockGameApp::RemoveBlock(XMFLOAT3 BlockXYZ)
{
    DynamicBlocks.erase(BlockXYZ.x * 10000 + BlockXYZ.y * 100 + BlockXYZ.z);
}



XMFLOAT3 BlockGameApp::GetD(float radius, float phi, float theta)
{
    XMFLOAT3 result;
    result.x = radius * sinf(phi) * cosf(theta);
    result.z = radius * sinf(phi) * sinf(theta);
    result.y = radius * cosf(phi);
    XMStoreFloat3(&result,XMVector3Normalize(XMLoadFloat3(&result)));

    return result;
}


void BlockGameApp::PlayerMoveWithCollisionCheck(XMFLOAT3 move)
{
    XMFLOAT3 result = { playerPos.x + move.x,playerPos.y + move.y ,playerPos.z + move.z };
    
    XMFLOAT3 playerlowPoint = { result.x -0.3f,result.y - 1.0f,result.z - 0.3f };
    XMFLOAT3 playerhighPoint = { result.x + 0.3f,result.y + 0.9f,result.z + 0.3f };

    XMFLOAT3 groundlowPoint = { -0.5f,-1.0f,- 0.5f };
    XMFLOAT3 groundhighPoint = { 29.5f, 0.0f,29.5f };

    if (is3DCubeAndCubeCollision(playerlowPoint, playerhighPoint, groundlowPoint, groundhighPoint))
        return;

    XMFLOAT3 lowBlockXYZ = GetPlayerBlockLow();
    
    for(float x = lowBlockXYZ.x; x < lowBlockXYZ.x+9;x++)
        for (float y = lowBlockXYZ.y; y < lowBlockXYZ.y + 9; y++)
            for (float z = lowBlockXYZ.z; z < lowBlockXYZ.z + 9; z++)
            { 
                if (DynamicBlocks.find(x * 10000 + y * 100 + z) != DynamicBlocks.end() 
                    && is3DCubeAndCubeCollision(playerlowPoint, playerhighPoint, { x - 1.5f,y - 1.0f,z - 1.5f }, {x-0.5f,y,z-0.5f}))
                    return;
            }

    playerPos = result;
}

XMFLOAT3 BlockGameApp::GetPlayerBlockLow() 
{
    const int r = 4;
    return { max(1,round(playerPos.x) + 1 - r), max(1,ceil(playerPos.y) - r), max(1,round(playerPos.z) + 1 - r) };
}

bool ApCom(float a, float b, float r)
{
    return abs(a - b) <= r;
}

XMFLOAT3 BlockGameApp::RaycastBox(char& c) 
{
    XMFLOAT3 lowBlockXYZ = GetPlayerBlockLow();

    BoundingBox collision{ {0.0f,0.0f,0.0f},{0.5f,0.5f,0.5f} };
    float p = -1.0f;
    float minP = -1.0f;
    XMFLOAT3 result = { 0.0f, 0.0f ,0.0f };

    for (float x = lowBlockXYZ.x; x < lowBlockXYZ.x + 9; x++)
        for (float y = lowBlockXYZ.y; y < lowBlockXYZ.y + 9; y++)
            for (float z = lowBlockXYZ.z; z < lowBlockXYZ.z + 9; z++)
                if (DynamicBlocks.find(x * 10000 + y * 100 + z) != DynamicBlocks.end())
                {
                    collision.Center = { x - 1.0f,y - 0.5f,z - 1.0f };
                    if (collision.Intersects(XMLoadFloat4(&cpuPassCB.Eye), XMLoadFloat3(&playerSightD), p))
                    {
                        if (minP == -1.0f || minP > p)
                        {
                            minP = p;
                            result = { x,y,z };
                        }

                    }
                }
    collision.Center = { 14.5f,-0.5f, 14.5f };
    collision.Extents = { 15.0f,0.5f,15.0f };

    if (collision.Intersects(XMLoadFloat4(&cpuPassCB.Eye), XMLoadFloat3(&playerSightD), p))
    {
        if (minP == -1.0f || minP > p)
        {
            minP = p;
            result = { -2.0f,-2.0f,-2.0f };
            if (p > 5.0f)
                return { -1.0f,-1.0f, -1.0f };
        }

    }

    if (minP != -1.0f)
    {   
        c = 'n';
        XMFLOAT3 targetPoint = {};
        XMStoreFloat3(&targetPoint, XMLoadFloat4(&cpuPassCB.Eye) + XMLoadFloat3(&playerSightD) * minP);

        if (result.x == -2.0f)
        {
            result = { round(targetPoint.x) + 1,0.0f,round(targetPoint.z) + 1 };
            c = 'Y';
            return result;
        }

        if (ApCom(targetPoint.x, result.x - 0.5f, 0.01f))
            c = 'X';
        else if (ApCom(targetPoint.x, result.x - 1.5f, 0.01f))
            c = 'x';
        else if (ApCom(targetPoint.z, result.z - 0.5f, 0.01f))
            c = 'Z';
        else if (ApCom(targetPoint.z, result.z - 1.5f, 0.01f))
            c = 'z';
        else if (ApCom(targetPoint.y, result.y, 0.01f))
            c = 'Y';
        else if (ApCom(targetPoint.y, result.y - 1.0f, 0.01f))
            c = 'y';

        return result;
    }
    else
        return { -1.0f ,-1.0f ,-1.0f };

            
}