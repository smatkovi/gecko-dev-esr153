/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 4; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLScreenBuffer.h"

#include "CompositorTypes.h"
#include "GLContext.h"
#include "gfx2DGlue.h"
#include "MozFramebuffer.h"
#include "SharedSurface.h"
#include "SharedSurfaceGL.h"
#include "mozilla/gfx/BuildConstants.h"

namespace mozilla::gl {

// -
// SwapChainPresenter

UniquePtr<SwapChainPresenter> SwapChain::Acquire(
    const gfx::IntSize& size, const gfx::ColorSpace2 colorSpace) {
  MOZ_ASSERT(mFactory);

  std::shared_ptr<SharedSurface> surf;
  if (!mPool.empty()) {
    // Try reuse
    const auto& existingDesc = mPool.front()->mDesc;
    auto newDesc = existingDesc;
    newDesc.size = size;
    newDesc.colorSpace = colorSpace;
    if (newDesc != existingDesc || !mPool.front()->IsValid()) {
      mPool = {};
    }
  }

  // When pooling is disabled, recycling of SharedSurfaces is managed by the
  // owner of the SwapChain by calling StoreRecycledSurface().
  if (!mPool.empty() && (!mPoolLimit || mPool.size() >= mPoolLimit)) {
    surf = mPool.front();
    mPool.pop();
  }
  if (!surf) {
    auto uniquePtrSurf = mFactory->CreateShared(size, colorSpace);
    if (!uniquePtrSurf) return nullptr;
    surf.reset(uniquePtrSurf.release());
  }
  if (mPoolLimit > 0) {
    mPool.push(surf);
    while (mPool.size() > mPoolLimit) {
      mPool.pop();
    }
  }

  auto ret = MakeUnique<SwapChainPresenter>(*this);
  const auto old = ret->SwapBackBuffer(surf);
  MOZ_ALWAYS_TRUE(!old);
  return ret;
}

void SwapChain::ClearPool() {
  mPool = {};
  mPrevFrontBuffer = nullptr;
}

bool SwapChain::StoreRecycledSurface(
    const std::shared_ptr<SharedSurface>& surf) {
  MOZ_ASSERT(mFactory);
  // Don't allow external recycled surfaces if SwapChain is managing own pool.
  MOZ_ASSERT(!mPoolLimit);
  if (mPoolLimit > 0 || !mFactory ||
      NS_WARN_IF(surf->mDesc.gl != mFactory->mDesc.gl)) {
    // Ensure we don't accidentally store an expired shared surface or from a
    // different context.
    return false;
  }
  mPool.push(surf);
  return true;
}

// -

SwapChainPresenter::SwapChainPresenter(SwapChain& swapChain)
    : mSwapChain(&swapChain) {
  MOZ_RELEASE_ASSERT(mSwapChain->mPresenter == nullptr);
  mSwapChain->mPresenter = this;
}

SwapChainPresenter::~SwapChainPresenter() {
  if (!mSwapChain) return;
  MOZ_RELEASE_ASSERT(mSwapChain->mPresenter == this);
  mSwapChain->mPresenter = nullptr;

  auto newFront = SwapBackBuffer(nullptr);
  if (newFront) {
    mSwapChain->mPrevFrontBuffer = mSwapChain->mFrontBuffer;
    mSwapChain->mFrontBuffer = newFront;
  }
}

std::shared_ptr<SharedSurface> SwapChainPresenter::SwapBackBuffer(
    std::shared_ptr<SharedSurface> back) {
  if (mBackBuffer) {
    mBackBuffer->UnlockProd();
    mBackBuffer->ProducerRelease();
    mBackBuffer->Commit();
  }
  auto old = mBackBuffer;
  mBackBuffer = back;
  if (mBackBuffer) {
    mBackBuffer->WaitForBufferOwnership();
    mBackBuffer->ProducerAcquire();
    mBackBuffer->LockProd();
  }
  return old;
}

GLuint SwapChainPresenter::Fb() const {
  if (!mBackBuffer) return 0;
  const auto& fb = mBackBuffer->mFb;
  if (!fb) return 0;
  return fb->mFB;
}

// -
// SwapChain

SwapChain::SwapChain()
    :  // We need to apply pooling on Android because of the AndroidSurface slow
       // destructor bugs. They cause a noticeable performance hit. See bug
       // #1646073.
      mPoolLimit(kIsAndroid ? 4 : 0) {}

SwapChain::~SwapChain() {
  if (mPresenter) {
    // Out of order destruction, but ok.
    (void)mPresenter->SwapBackBuffer(nullptr);
    mPresenter->mSwapChain = nullptr;
    mPresenter = nullptr;
  }
  if (mDestroyedCallback) {
    mDestroyedCallback();
  }
}

// -
// GLScreenBuffer

UniquePtr<GLScreenBuffer> GLScreenBuffer::Create(GLContext* const gl,
                                                 const gfx::IntSize& size) {
  auto screen = MakeUnique<GLScreenBuffer>(gl);
  screen->Morph(MakeUnique<SurfaceFactory_Basic>(*gl));
  if (!screen->Resize(size)) {
    return nullptr;
  }
  return screen;
}

GLScreenBuffer::GLScreenBuffer(GLContext* const)
    : mSwapChain(MakeUnique<SwapChain>()) {}

GLScreenBuffer::~GLScreenBuffer() = default;

void GLScreenBuffer::Morph(UniquePtr<SurfaceFactory> factory) {
  MOZ_ASSERT(factory);
  mPresenter = nullptr;
  mSwapChain->ClearPool();
  mSwapChain->mFactory = std::move(factory);
  if (!mSize.IsEmpty()) {
    (void)Resize(mSize);
  }
}

bool GLScreenBuffer::Resize(const gfx::IntSize& size) {
  if (size.IsEmpty() || !mSwapChain->mFactory) {
    return false;
  }
  if (mPresenter && size == mSize) {
    return true;
  }

  mPresenter = nullptr;
  mSize = size;
  mPresenter = mSwapChain->Acquire(size, gfx::ColorSpace2::SRGB);
  return bool(mPresenter);
}

bool GLScreenBuffer::PublishFrame(const gfx::IntSize& size) {
  if (!mPresenter && !Resize(size)) {
    return false;
  }

  mPresenter = nullptr;
  return Resize(size);
}

const std::shared_ptr<SharedSurface>& GLScreenBuffer::FrontBuffer() const {
  return mSwapChain->FrontBuffer();
}

GLuint GLScreenBuffer::Fb() const {
  return mPresenter ? mPresenter->Fb() : 0;
}

}  // namespace mozilla::gl
