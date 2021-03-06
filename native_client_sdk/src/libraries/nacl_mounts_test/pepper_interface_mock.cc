/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "pepper_interface_mock.h"

PepperInterfaceMock::PepperInterfaceMock(PP_Instance instance)
    : instance_(instance),

    // Initialize interfaces.
#include "nacl_mounts/pepper/define_empty_macros.h"
#undef BEGIN_INTERFACE
#define BEGIN_INTERFACE(BaseClass, PPInterface, InterfaceString) \
    BaseClass##interface_(new BaseClass##Mock),
#include "nacl_mounts/pepper/all_interfaces.h"
#include "nacl_mounts/pepper/undef_macros.h"

    // Dummy value so we can ensure that no interface ends the initializer list.
    dummy_(0) {
}

PepperInterfaceMock::~PepperInterfaceMock() {

  // Delete interfaces.
#include "nacl_mounts/pepper/define_empty_macros.h"
#undef BEGIN_INTERFACE
#define BEGIN_INTERFACE(BaseClass, PPInterface, InterfaceString) \
    delete BaseClass##interface_;
#include "nacl_mounts/pepper/all_interfaces.h"
#include "nacl_mounts/pepper/undef_macros.h"

}

PP_Instance PepperInterfaceMock::GetInstance() {
  return instance_;
}

// Define Getter functions, constructors, destructors.
#include "nacl_mounts/pepper/define_empty_macros.h"
#undef BEGIN_INTERFACE
#define BEGIN_INTERFACE(BaseClass, PPInterface, InterfaceString) \
    BaseClass##Mock* PepperInterfaceMock::Get##BaseClass() { \
      return BaseClass##interface_; \
    } \
    BaseClass##Mock::BaseClass##Mock() { \
    } \
    BaseClass##Mock::~BaseClass##Mock() { \
    }
#include "nacl_mounts/pepper/all_interfaces.h"
#include "nacl_mounts/pepper/undef_macros.h"
