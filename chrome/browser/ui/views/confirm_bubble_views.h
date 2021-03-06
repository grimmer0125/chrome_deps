// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CONFIRM_BUBBLE_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_CONFIRM_BUBBLE_VIEWS_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/link_listener.h"

class ConfirmBubbleModel;

// A class that implements a bubble that consists of the following items:
// * one icon ("icon")
// * one title text ("title")
// * one message text ("message")
// * one optional link ("link")
// * two optional buttons ("ok" and "cancel")
//
// This bubble is convenient when we wish to ask transient, non-blocking
// questions. Unlike a dialog, a bubble menu disappears when we click outside of
// its window to avoid blocking user operations. A bubble is laid out as
// follows:
//
//   +------------------------+
//   | icon title           x |
//   | message                |
//   | link                   |
//   |          [OK] [Cancel] |
//   +------------------------+
//
class ConfirmBubbleViews : public views::BubbleDelegateView,
                           public views::ButtonListener,
                           public views::LinkListener {
 public:
  ConfirmBubbleViews(gfx::NativeView parent,
                     const gfx::Point& anchor_point,
                     ConfirmBubbleModel* model);

 protected:
  virtual ~ConfirmBubbleViews();

  // views::ButtonListener implementation.
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // views::LinkListener implementation.
  virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE;

  // views::BubbleDelegateView implementation.
  virtual gfx::Rect GetAnchorRect() OVERRIDE;
  virtual void Init() OVERRIDE;

 private:
  // The model to customize this bubble view.
  scoped_ptr<ConfirmBubbleModel> model_;

  DISALLOW_COPY_AND_ASSIGN(ConfirmBubbleViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_CONFIRM_BUBBLE_VIEWS_H_
