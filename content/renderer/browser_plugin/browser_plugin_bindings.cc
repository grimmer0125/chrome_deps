// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/browser_plugin/browser_plugin_bindings.h"

#include <cstdlib>
#include <string>

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/string16.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/utf_string_conversions.h"
#include "content/renderer/browser_plugin/browser_plugin.h"
#include "content/renderer/browser_plugin/browser_plugin_constants.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebBindings.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDOMMessageEvent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNode.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPluginContainer.h"
#include "third_party/npapi/bindings/npapi.h"
#include "v8/include/v8.h"

using WebKit::WebBindings;
using WebKit::WebElement;
using WebKit::WebDOMEvent;
using WebKit::WebDOMMessageEvent;
using WebKit::WebPluginContainer;
using WebKit::WebString;

namespace content {

namespace {

BrowserPluginBindings* GetBindings(NPObject* object) {
  return static_cast<BrowserPluginBindings::BrowserPluginNPObject*>(object)->
      message_channel;
}

int Int32FromNPVariant(const NPVariant& variant) {
  if (NPVARIANT_IS_INT32(variant))
    return NPVARIANT_TO_INT32(variant);

  if (NPVARIANT_IS_DOUBLE(variant))
    return NPVARIANT_TO_DOUBLE(variant);

  return 0;
}

std::string StringFromNPVariant(const NPVariant& variant) {
  if (!NPVARIANT_IS_STRING(variant))
    return std::string();
  const NPString& np_string = NPVARIANT_TO_STRING(variant);
  return std::string(np_string.UTF8Characters, np_string.UTF8Length);
}

bool StringToNPVariant(const std::string &in, NPVariant *variant) {
  size_t length = in.size();
  NPUTF8 *chars = static_cast<NPUTF8 *>(malloc(length));
  if (!chars) {
    VOID_TO_NPVARIANT(*variant);
    return false;
  }
  memcpy(chars, in.c_str(), length);
  STRINGN_TO_NPVARIANT(chars, length, *variant);
  return true;
}

//------------------------------------------------------------------------------
// Implementations of NPClass functions.  These are here to:
// - Implement src attribute.
//------------------------------------------------------------------------------
NPObject* BrowserPluginBindingsAllocate(NPP npp, NPClass* the_class) {
  return new BrowserPluginBindings::BrowserPluginNPObject;
}

void BrowserPluginBindingsDeallocate(NPObject* object) {
  BrowserPluginBindings::BrowserPluginNPObject* instance =
      static_cast<BrowserPluginBindings::BrowserPluginNPObject*>(object);
  delete instance;
}

bool BrowserPluginBindingsHasMethod(NPObject* np_obj, NPIdentifier name) {
  if (!np_obj)
    return false;

  BrowserPluginBindings* bindings = GetBindings(np_obj);
  if (!bindings)
    return false;

  return bindings->HasMethod(name);
}

bool BrowserPluginBindingsInvoke(NPObject* np_obj, NPIdentifier name,
                                 const NPVariant* args, uint32 arg_count,
                                 NPVariant* result) {
  if (!np_obj)
    return false;

  BrowserPluginBindings* bindings = GetBindings(np_obj);
  if (!bindings)
    return false;

  return bindings->InvokeMethod(name, args, arg_count, result);
}

bool BrowserPluginBindingsInvokeDefault(NPObject* np_obj,
                                        const NPVariant* args,
                                        uint32 arg_count,
                                        NPVariant* result) {
  NOTIMPLEMENTED();
  return false;
}

bool BrowserPluginBindingsHasProperty(NPObject* np_obj, NPIdentifier name) {
  if (!np_obj)
    return false;

  BrowserPluginBindings* bindings = GetBindings(np_obj);
  if (!bindings)
    return false;

  return bindings->HasProperty(name);
}

bool BrowserPluginBindingsGetProperty(NPObject* np_obj, NPIdentifier name,
                                      NPVariant* result) {
  if (!np_obj)
    return false;

  if (!result)
    return false;

  // All attributes from here on rely on the bindings, so retrieve it once and
  // return on failure.
  BrowserPluginBindings* bindings = GetBindings(np_obj);
  if (!bindings)
    return false;

  return bindings->GetProperty(name, result);
}

bool BrowserPluginBindingsSetProperty(NPObject* np_obj, NPIdentifier name,
                                      const NPVariant* variant) {
  if (!np_obj)
    return false;
  if (!variant)
    return false;

  // All attributes from here on rely on the bindings, so retrieve it once and
  // return on failure.
  BrowserPluginBindings* bindings = GetBindings(np_obj);
  if (!bindings)
    return false;

  return bindings->SetProperty(np_obj, name, variant);
}

bool BrowserPluginBindingsEnumerate(NPObject *np_obj, NPIdentifier **value,
                                    uint32_t *count) {
  NOTIMPLEMENTED();
  return true;
}

NPClass browser_plugin_message_class = {
  NP_CLASS_STRUCT_VERSION,
  &BrowserPluginBindingsAllocate,
  &BrowserPluginBindingsDeallocate,
  NULL,
  &BrowserPluginBindingsHasMethod,
  &BrowserPluginBindingsInvoke,
  &BrowserPluginBindingsInvokeDefault,
  &BrowserPluginBindingsHasProperty,
  &BrowserPluginBindingsGetProperty,
  &BrowserPluginBindingsSetProperty,
  NULL,
  &BrowserPluginBindingsEnumerate,
};

}  // namespace

// BrowserPluginMethodBinding --------------------------------------------------

class BrowserPluginMethodBinding {
 public:
  BrowserPluginMethodBinding(const char name[], uint32 arg_count)
      : name_(name),
        arg_count_(arg_count) {
  }

  virtual ~BrowserPluginMethodBinding() {}

  bool MatchesName(NPIdentifier name) const {
    return WebBindings::getStringIdentifier(name_.c_str()) == name;
  }

  uint32 arg_count() const { return arg_count_; }

  virtual bool Invoke(BrowserPluginBindings* bindings,
                      const NPVariant* args,
                      NPVariant* result) = 0;

 private:
  std::string name_;
  uint32 arg_count_;

  DISALLOW_COPY_AND_ASSIGN(BrowserPluginMethodBinding);
};

class BrowserPluginBindingBack : public BrowserPluginMethodBinding {
 public:
  BrowserPluginBindingBack()
      : BrowserPluginMethodBinding(browser_plugin::kMethodBack, 0) {
  }

  virtual bool Invoke(BrowserPluginBindings* bindings,
                      const NPVariant* args,
                      NPVariant* result) OVERRIDE {
    bindings->instance()->Back();
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserPluginBindingBack);
};

class BrowserPluginBindingCanGoBack : public BrowserPluginMethodBinding {
 public:
  BrowserPluginBindingCanGoBack()
      : BrowserPluginMethodBinding(browser_plugin::kMethodCanGoBack, 0) {
  }

  virtual bool Invoke(BrowserPluginBindings* bindings,
                      const NPVariant* args,
                      NPVariant* result) OVERRIDE {
    BOOLEAN_TO_NPVARIANT(bindings->instance()->CanGoBack(), *result);
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserPluginBindingCanGoBack);
};

class BrowserPluginBindingCanGoForward : public BrowserPluginMethodBinding {
 public:
  BrowserPluginBindingCanGoForward()
      : BrowserPluginMethodBinding(browser_plugin::kMethodCanGoForward, 0) {
  }

  virtual bool Invoke(BrowserPluginBindings* bindings,
                      const NPVariant* args,
                      NPVariant* result) OVERRIDE {
    BOOLEAN_TO_NPVARIANT(bindings->instance()->CanGoForward(), *result);
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserPluginBindingCanGoForward);
};

class BrowserPluginBindingForward : public BrowserPluginMethodBinding {
 public:
  BrowserPluginBindingForward()
      : BrowserPluginMethodBinding(browser_plugin::kMethodForward, 0) {
  }

  virtual bool Invoke(BrowserPluginBindings* bindings,
                      const NPVariant* args,
                      NPVariant* result) OVERRIDE {
    bindings->instance()->Forward();
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserPluginBindingForward);
};

// Note: This is a method that is used internally by the <webview> shim only.
// This should not be exposed to developers.
class BrowserPluginBindingGetRouteID : public BrowserPluginMethodBinding {
 public:
  BrowserPluginBindingGetRouteID()
      : BrowserPluginMethodBinding(browser_plugin::kMethodGetRouteId, 0) {
  }

  virtual bool Invoke(BrowserPluginBindings* bindings,
                      const NPVariant* args,
                      NPVariant* result) OVERRIDE {
    int route_id = bindings->instance()->guest_route_id();
    INT32_TO_NPVARIANT(route_id, *result);
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserPluginBindingGetRouteID);
};

class BrowserPluginBindingGetProcessID : public BrowserPluginMethodBinding {
 public:
  BrowserPluginBindingGetProcessID()
      : BrowserPluginMethodBinding(browser_plugin::kMethodGetProcessId, 0) {
  }

  virtual bool Invoke(BrowserPluginBindings* bindings,
                      const NPVariant* args,
                      NPVariant* result) OVERRIDE {
    int process_id = bindings->instance()->guest_process_id();
    INT32_TO_NPVARIANT(process_id, *result);
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserPluginBindingGetProcessID);
};

class BrowserPluginBindingGo : public BrowserPluginMethodBinding {
 public:
  BrowserPluginBindingGo()
      : BrowserPluginMethodBinding(browser_plugin::kMethodGo, 1) {
  }

  virtual bool Invoke(BrowserPluginBindings* bindings,
                      const NPVariant* args,
                      NPVariant* result) OVERRIDE {
    bindings->instance()->Go(Int32FromNPVariant(args[0]));
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserPluginBindingGo);
};

class BrowserPluginBindingReload : public BrowserPluginMethodBinding {
 public:
  BrowserPluginBindingReload()
      : BrowserPluginMethodBinding(browser_plugin::kMethodReload, 0) {
  }

  virtual bool Invoke(BrowserPluginBindings* bindings,
                      const NPVariant* args,
                      NPVariant* result) OVERRIDE {
    bindings->instance()->Reload();
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserPluginBindingReload);
};

class BrowserPluginBindingStop : public BrowserPluginMethodBinding {
 public:
  BrowserPluginBindingStop()
      : BrowserPluginMethodBinding(browser_plugin::kMethodStop, 0) {
  }

  virtual bool Invoke(BrowserPluginBindings* bindings,
                      const NPVariant* args,
                      NPVariant* result) OVERRIDE {
    bindings->instance()->Stop();
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserPluginBindingStop);
};

class BrowserPluginBindingTerminate : public BrowserPluginMethodBinding {
 public:
  BrowserPluginBindingTerminate()
      : BrowserPluginMethodBinding(browser_plugin::kMethodTerminate, 0) {
  }

  virtual bool Invoke(BrowserPluginBindings* bindings,
                      const NPVariant* args,
                      NPVariant* result) OVERRIDE {
    bindings->instance()->TerminateGuest();
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserPluginBindingTerminate);
};

// BrowserPluginPropertyBinding ------------------------------------------------

class BrowserPluginPropertyBinding {
 public:
  explicit BrowserPluginPropertyBinding(const char name[]) : name_(name) {}
  virtual ~BrowserPluginPropertyBinding() {}
  const std::string& name() const { return name_; }
  bool MatchesName(NPIdentifier name) const {
    return WebBindings::getStringIdentifier(name_.c_str()) == name;
  }
  virtual bool GetProperty(BrowserPluginBindings* bindings,
                           NPVariant* result) = 0;
  virtual bool SetProperty(BrowserPluginBindings* bindings,
                           NPObject* np_obj,
                           const NPVariant* variant) = 0;
  // Updates the DOM Attribute value with the current property value.
  void UpdateDOMAttribute(BrowserPluginBindings* bindings,
                          std::string new_value) {
    bindings->instance()->UpdateDOMAttribute(name(), new_value);
  }
 private:
  std::string name_;

  DISALLOW_COPY_AND_ASSIGN(BrowserPluginPropertyBinding);
};

class BrowserPluginPropertyBindingAutoSize
    : public BrowserPluginPropertyBinding {
 public:
  BrowserPluginPropertyBindingAutoSize()
      : BrowserPluginPropertyBinding(browser_plugin::kAttributeAutoSize) {
  }
  virtual bool GetProperty(BrowserPluginBindings* bindings,
                           NPVariant* result) OVERRIDE {
    bool auto_size = bindings->instance()->GetAutoSizeAttribute();
    BOOLEAN_TO_NPVARIANT(auto_size, *result);
    return true;
  }
  virtual bool SetProperty(BrowserPluginBindings* bindings,
                           NPObject* np_obj,
                           const NPVariant* variant) OVERRIDE {
    bool new_value;
    if (variant->type == NPVariantType_Bool) {
      new_value = NPVARIANT_TO_BOOLEAN(*variant);
    } else {
      new_value = LowerCaseEqualsASCII(std::string(
          NPVARIANT_TO_STRING(*variant).UTF8Characters), "true");
    }
    if (bindings->instance()->GetAutoSizeAttribute() != new_value) {
      UpdateDOMAttribute(bindings, new_value ? "true" : "false");
      bindings->instance()->ParseAutoSizeAttribute();
    }
    return true;
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserPluginPropertyBindingAutoSize);
};

class BrowserPluginPropertyBindingContentWindow
    : public BrowserPluginPropertyBinding {
 public:
  BrowserPluginPropertyBindingContentWindow()
      : BrowserPluginPropertyBinding(browser_plugin::kAttributeContentWindow) {
  }
  virtual bool GetProperty(BrowserPluginBindings* bindings,
                           NPVariant* result) OVERRIDE {
    NPObject* obj = bindings->instance()->GetContentWindow();
    if (obj) {
      result->type = NPVariantType_Object;
      result->value.objectValue = WebBindings::retainObject(obj);
    }
    return true;
  }
  virtual bool SetProperty(BrowserPluginBindings* bindings,
                           NPObject* np_obj,
                           const NPVariant* variant) OVERRIDE {
    return false;
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserPluginPropertyBindingContentWindow);
};

class BrowserPluginPropertyBindingMaxHeight
    : public BrowserPluginPropertyBinding {
 public:
  BrowserPluginPropertyBindingMaxHeight()
      : BrowserPluginPropertyBinding(browser_plugin::kAttributeMaxHeight) {
  }
  virtual bool GetProperty(BrowserPluginBindings* bindings,
                           NPVariant* result) OVERRIDE {
    int max_height = bindings->instance()->GetMaxHeightAttribute();
    INT32_TO_NPVARIANT(max_height, *result);
    return true;
  }
  virtual bool SetProperty(BrowserPluginBindings* bindings,
                           NPObject* np_obj,
                           const NPVariant* variant) OVERRIDE {
    int new_value = Int32FromNPVariant(*variant);
    if (bindings->instance()->GetMaxHeightAttribute() != new_value) {
      UpdateDOMAttribute(bindings, base::IntToString(new_value));
      bindings->instance()->ParseSizeContraintsChanged();
    }
    return true;
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserPluginPropertyBindingMaxHeight);
};

class BrowserPluginPropertyBindingMaxWidth
    : public BrowserPluginPropertyBinding {
 public:
  BrowserPluginPropertyBindingMaxWidth()
      : BrowserPluginPropertyBinding(browser_plugin::kAttributeMaxWidth) {
  }
  virtual bool GetProperty(BrowserPluginBindings* bindings,
                           NPVariant* result) OVERRIDE {
    int max_width = bindings->instance()->GetMaxWidthAttribute();
    INT32_TO_NPVARIANT(max_width, *result);
    return true;
  }
  virtual bool SetProperty(BrowserPluginBindings* bindings,
                           NPObject* np_obj,
                           const NPVariant* variant) OVERRIDE {
    int new_value = Int32FromNPVariant(*variant);
    if (bindings->instance()->GetMaxWidthAttribute() != new_value) {
      UpdateDOMAttribute(bindings, base::IntToString(new_value));
      bindings->instance()->ParseSizeContraintsChanged();
    }
    return true;
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserPluginPropertyBindingMaxWidth);
};

class BrowserPluginPropertyBindingMinHeight
    : public BrowserPluginPropertyBinding {
 public:
  BrowserPluginPropertyBindingMinHeight()
      : BrowserPluginPropertyBinding(browser_plugin::kAttributeMinHeight) {
  }
  virtual bool GetProperty(BrowserPluginBindings* bindings,
                           NPVariant* result) OVERRIDE {
    int min_height = bindings->instance()->GetMinHeightAttribute();
    INT32_TO_NPVARIANT(min_height, *result);
    return true;
  }
  virtual bool SetProperty(BrowserPluginBindings* bindings,
                           NPObject* np_obj,
                           const NPVariant* variant) OVERRIDE {
    int new_value = Int32FromNPVariant(*variant);
    if (bindings->instance()->GetMinHeightAttribute() != new_value) {
      UpdateDOMAttribute(bindings, base::IntToString(new_value));
      bindings->instance()->ParseSizeContraintsChanged();
    }
    return true;
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserPluginPropertyBindingMinHeight);
};

class BrowserPluginPropertyBindingMinWidth
    : public BrowserPluginPropertyBinding {
 public:
  BrowserPluginPropertyBindingMinWidth()
      : BrowserPluginPropertyBinding(browser_plugin::kAttributeMinWidth) {
  }
  virtual bool GetProperty(BrowserPluginBindings* bindings,
                           NPVariant* result) OVERRIDE {
    int min_width = bindings->instance()->GetMinWidthAttribute();
    INT32_TO_NPVARIANT(min_width, *result);
    return true;
  }
  virtual bool SetProperty(BrowserPluginBindings* bindings,
                           NPObject* np_obj,
                           const NPVariant* variant) OVERRIDE {
    int new_value = Int32FromNPVariant(*variant);
    if (bindings->instance()->GetMinWidthAttribute() != new_value) {
      UpdateDOMAttribute(bindings, base::IntToString(new_value));
      bindings->instance()->ParseSizeContraintsChanged();
    }
    return true;
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserPluginPropertyBindingMinWidth);
};

class BrowserPluginPropertyBindingName
    : public BrowserPluginPropertyBinding {
 public:
  BrowserPluginPropertyBindingName()
      : BrowserPluginPropertyBinding(browser_plugin::kAttributeName) {
  }
  virtual bool GetProperty(BrowserPluginBindings* bindings,
                           NPVariant* result) OVERRIDE {
    std::string name = bindings->instance()->GetNameAttribute();
    return StringToNPVariant(name, result);
    return true;
  }
  virtual bool SetProperty(BrowserPluginBindings* bindings,
                           NPObject* np_obj,
                           const NPVariant* variant) OVERRIDE {
    std::string new_value = StringFromNPVariant(*variant);
    if (bindings->instance()->GetNameAttribute() != new_value) {
      UpdateDOMAttribute(bindings, new_value);
      bindings->instance()->ParseNameAttribute();
    }
    return true;
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserPluginPropertyBindingName);
};

class BrowserPluginPropertyBindingPartition
    : public BrowserPluginPropertyBinding {
 public:
  BrowserPluginPropertyBindingPartition()
      : BrowserPluginPropertyBinding(browser_plugin::kAttributePartition) {
  }
  virtual bool GetProperty(BrowserPluginBindings* bindings,
                           NPVariant* result) OVERRIDE {
    std::string partition_id = bindings->instance()->GetPartitionAttribute();
    return StringToNPVariant(partition_id, result);
  }
  virtual bool SetProperty(BrowserPluginBindings* bindings,
                           NPObject* np_obj,
                           const NPVariant* variant) OVERRIDE {
    std::string new_value = StringFromNPVariant(*variant);
    std::string old_value = bindings->instance()->GetPartitionAttribute();
    if (old_value != new_value) {
      UpdateDOMAttribute(bindings, new_value);
      std::string error_message;
      if (!bindings->instance()->ParsePartitionAttribute(&error_message)) {
        WebBindings::setException(
            np_obj, static_cast<const NPUTF8 *>(error_message.c_str()));
        // Reset to old value on error.
        UpdateDOMAttribute(bindings, old_value);
        return false;
      }
    }
    return true;
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserPluginPropertyBindingPartition);
};

class BrowserPluginPropertyBindingSrc : public BrowserPluginPropertyBinding {
 public:
  BrowserPluginPropertyBindingSrc()
      : BrowserPluginPropertyBinding(browser_plugin::kAttributeSrc) {
  }
  virtual bool GetProperty(BrowserPluginBindings* bindings,
                           NPVariant* result) OVERRIDE {
    std::string src = bindings->instance()->GetSrcAttribute();
    return StringToNPVariant(src, result);
  }
  virtual bool SetProperty(BrowserPluginBindings* bindings,
                           NPObject* np_obj,
                           const NPVariant* variant) OVERRIDE {
    std::string new_value = StringFromNPVariant(*variant);
    std::string old_value = bindings->instance()->GetSrcAttribute();
    if (old_value != new_value && !new_value.empty()) {
      UpdateDOMAttribute(bindings, new_value);
      std::string error_message;
      if (!bindings->instance()->ParseSrcAttribute(&error_message)) {
        WebBindings::setException(
            np_obj, static_cast<const NPUTF8 *>(error_message.c_str()));
        // Reset to old value on error.
        UpdateDOMAttribute(bindings, old_value);
        return false;
      }
    }
    return true;
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserPluginPropertyBindingSrc);
};


// BrowserPluginBindings ------------------------------------------------------

BrowserPluginBindings::BrowserPluginNPObject::BrowserPluginNPObject() {
}

BrowserPluginBindings::BrowserPluginNPObject::~BrowserPluginNPObject() {
}

BrowserPluginBindings::BrowserPluginBindings(BrowserPlugin* instance)
    : instance_(instance),
      np_object_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  NPObject* obj =
      WebBindings::createObject(NULL, &browser_plugin_message_class);
  np_object_ = static_cast<BrowserPluginBindings::BrowserPluginNPObject*>(obj);
  np_object_->message_channel = weak_ptr_factory_.GetWeakPtr();

  method_bindings_.push_back(new BrowserPluginBindingBack);
  method_bindings_.push_back(new BrowserPluginBindingCanGoBack);
  method_bindings_.push_back(new BrowserPluginBindingCanGoForward);
  method_bindings_.push_back(new BrowserPluginBindingForward);
  method_bindings_.push_back(new BrowserPluginBindingGetProcessID);
  method_bindings_.push_back(new BrowserPluginBindingGetRouteID);
  method_bindings_.push_back(new BrowserPluginBindingGo);
  method_bindings_.push_back(new BrowserPluginBindingReload);
  method_bindings_.push_back(new BrowserPluginBindingStop);
  method_bindings_.push_back(new BrowserPluginBindingTerminate);

  property_bindings_.push_back(new BrowserPluginPropertyBindingAutoSize);
  property_bindings_.push_back(new BrowserPluginPropertyBindingContentWindow);
  property_bindings_.push_back(new BrowserPluginPropertyBindingMaxHeight);
  property_bindings_.push_back(new BrowserPluginPropertyBindingMaxWidth);
  property_bindings_.push_back(new BrowserPluginPropertyBindingMinHeight);
  property_bindings_.push_back(new BrowserPluginPropertyBindingMinWidth);
  property_bindings_.push_back(new BrowserPluginPropertyBindingName);
  property_bindings_.push_back(new BrowserPluginPropertyBindingPartition);
  property_bindings_.push_back(new BrowserPluginPropertyBindingSrc);
}

BrowserPluginBindings::~BrowserPluginBindings() {
  WebBindings::releaseObject(np_object_);
}

bool BrowserPluginBindings::HasMethod(NPIdentifier name) const {
  for (BindingList::const_iterator iter = method_bindings_.begin();
       iter != method_bindings_.end();
       ++iter) {
    if ((*iter)->MatchesName(name))
      return true;
  }
  return false;
}

bool BrowserPluginBindings::InvokeMethod(NPIdentifier name,
                                         const NPVariant* args,
                                         uint32 arg_count,
                                         NPVariant* result) {
  for (BindingList::iterator iter = method_bindings_.begin();
       iter != method_bindings_.end();
       ++iter) {
    if ((*iter)->MatchesName(name) && (*iter)->arg_count() == arg_count)
      return (*iter)->Invoke(this, args, result);
  }
  return false;
}

bool BrowserPluginBindings::HasProperty(NPIdentifier name) const {
  for (PropertyBindingList::const_iterator iter = property_bindings_.begin();
       iter != property_bindings_.end();
       ++iter) {
    if ((*iter)->MatchesName(name))
      return true;
  }
  return false;
}

bool BrowserPluginBindings::SetProperty(NPObject* np_obj,
                                        NPIdentifier name,
                                        const NPVariant* variant) {
  for (PropertyBindingList::iterator iter = property_bindings_.begin();
       iter != property_bindings_.end();
       ++iter) {
    if ((*iter)->MatchesName(name)) {
      if ((*iter)->SetProperty(this, np_obj, variant)) {
        return true;
      }
      break;
    }
  }
  return false;
}

bool BrowserPluginBindings::GetProperty(NPIdentifier name, NPVariant* result) {
  for (PropertyBindingList::iterator iter = property_bindings_.begin();
       iter != property_bindings_.end();
       ++iter) {
    if ((*iter)->MatchesName(name))
      return (*iter)->GetProperty(this, result);
  }
  return false;
}

}  // namespace content
