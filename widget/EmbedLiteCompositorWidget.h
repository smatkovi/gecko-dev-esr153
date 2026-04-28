/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_EmbedLiteCompositorWidget_h__
#define mozilla_widget_EmbedLiteCompositorWidget_h__

#include "mozilla/widget/InProcessCompositorWidget.h"

namespace mozilla::widget {

class EmbedLiteCompositorWidget final : public InProcessCompositorWidget {
 public:
  explicit EmbedLiteCompositorWidget(
      const layers::CompositorOptions& aOptions, nsBaseWidget* aWidget);

  EmbedLiteCompositorWidget* AsEmbedLite() override { return this; }
  bool IsEmbedLiteOffscreen() const override { return true; }
};

}  // namespace mozilla::widget

#endif  // mozilla_widget_EmbedLiteCompositorWidget_h__
