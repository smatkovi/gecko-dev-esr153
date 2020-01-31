/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim:expandtab:shiftwidth=2:tabstop=2:cin:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsContentHandlerApp_h_
#define nsContentHandlerApp_h_

#include "nsIMIMEInfo.h"
#include "nsString.h"

class nsContentHandlerApp final : public nsIHandlerApp {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIHANDLERAPP

  nsContentHandlerApp(const nsAString& aName, const nsACString& aType,
                      bool aHandlesScheme);

 private:
  ~nsContentHandlerApp() = default;

  nsString mName;
  nsCString mType;
  nsString mDetailedDescription;
  bool mHandlesScheme;
};

#endif  // nsContentHandlerApp_h_
