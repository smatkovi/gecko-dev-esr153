/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsContentHandlerApp.h"

#include <contentaction5/contentaction.h>
#include <QString>
#include <QUrl>

#include "nsIURI.h"

NS_IMPL_ISUPPORTS(nsContentHandlerApp, nsIHandlerApp)

nsContentHandlerApp::nsContentHandlerApp(const nsAString& aName,
                                         const nsACString& aType,
                                         bool aHandlesScheme)
    : mName(aName), mType(aType), mHandlesScheme(aHandlesScheme) {}

NS_IMETHODIMP nsContentHandlerApp::GetName(nsAString& aName) {
  aName.Assign(mName);
  return NS_OK;
}

NS_IMETHODIMP nsContentHandlerApp::SetName(const nsAString& aName) {
  mName.Assign(aName);
  return NS_OK;
}

NS_IMETHODIMP nsContentHandlerApp::Equals(nsIHandlerApp* aHandlerApp,
                                          bool* aResult) {
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = false;
  if (!aHandlerApp) {
    return NS_OK;
  }

  nsAutoString name;
  nsAutoString detailedDescription;
  nsresult rv = aHandlerApp->GetName(name);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aHandlerApp->GetDetailedDescription(detailedDescription);
  NS_ENSURE_SUCCESS(rv, rv);

  *aResult = mName.Equals(name) &&
             mDetailedDescription.Equals(detailedDescription);
  return NS_OK;
}

NS_IMETHODIMP nsContentHandlerApp::GetDetailedDescription(
    nsAString& aDetailedDescription) {
  aDetailedDescription.Assign(mDetailedDescription);
  return NS_OK;
}

NS_IMETHODIMP nsContentHandlerApp::SetDetailedDescription(
    const nsAString& aDetailedDescription) {
  mDetailedDescription.Assign(aDetailedDescription);
  return NS_OK;
}

NS_IMETHODIMP nsContentHandlerApp::LaunchWithURI(
    nsIURI* aURI, mozilla::dom::BrowsingContext*) {
  nsAutoCString spec;
  nsresult rv = aURI->GetAsciiSpec(spec);
  NS_ENSURE_SUCCESS(rv, rv);

  const QString uri = QString::fromUtf8(spec.get());
  QList<ContentAction::Action> actions;
  if (mHandlesScheme) {
    actions = ContentAction::Action::actionsForScheme(uri);
  } else {
    actions = ContentAction::Action::actionsForFile(
        QUrl(uri), QString::fromUtf8(mType.get()));
  }

  const QString name =
      QString::fromUtf8(NS_ConvertUTF16toUTF8(mName).get());
  for (const ContentAction::Action& action : actions) {
    if (action.name() == name) {
      action.trigger();
      break;
    }
  }

  return NS_OK;
}
