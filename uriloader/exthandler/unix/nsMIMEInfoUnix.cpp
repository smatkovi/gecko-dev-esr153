/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMIMEInfoUnix.h"
#include "nsGNOMERegistry.h"
#include "nsIGIOService.h"
#include "nsNetCID.h"
#include "nsIIOService.h"
#include "nsLocalFile.h"

#ifdef MOZ_ENABLE_CONTENTACTION
#  include <contentaction5/contentaction.h>
#  include <QByteArray>
#  include <QString>
#  include <QUrl>

#  include "nsComponentManagerUtils.h"
#  include "nsContentHandlerApp.h"
#  include "nsXPCOMCID.h"
#endif

#ifdef MOZ_ENABLE_DBUS
#  include "nsDBusHandlerApp.h"
#endif
#ifdef MOZ_WIDGET_QT
#  include "nsMIMEInfoQt.h"
#endif

nsresult nsMIMEInfoUnix::LoadUriInternal(nsIURI* aURI) {
  nsresult rv = nsGNOMERegistry::LoadURL(aURI);

#ifdef MOZ_WIDGET_QT
  if (NS_FAILED(rv)) {
    rv = nsMIMEInfoQt::LoadUriInternal(aURI);
  }
#endif

  return rv;
}

NS_IMETHODIMP nsMIMEInfoUnix::GetDefaultExecutable(nsIFile** aExecutable) {
  // This needs to be implemented before FirefoxBridge will work on Linux.
  // To implement this and be consistent, GetHasDefaultHandler and
  // LaunchDefaultWithFile should probably be made to be consistent.
  // Right now, they aren't. GetHasDefaultHandler reports true in cases
  // where calling LaunchDefaultWithFile will fail due to not finding the
  // right executable.

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMIMEInfoUnix::GetHasDefaultHandler(bool* _retval) {
  // if a default app is set, it means the application has been set from
  // either /etc/mailcap or ${HOME}/.mailcap, in which case we don't want to
  // give the GNOME answer.
  if (GetDefaultApplication()) {
    return nsMIMEInfoImpl::GetHasDefaultHandler(_retval);
  }

  *_retval = false;

  if (mClass == eProtocolInfo) {
    *_retval = nsGNOMERegistry::HandlerExists(mSchemeOrType.get());
  } else {
    RefPtr<nsMIMEInfoBase> mimeInfo =
        nsGNOMERegistry::GetFromType(mSchemeOrType);
    if (!mimeInfo) {
      nsAutoCString ext;
      nsresult rv = GetPrimaryExtension(ext);
      if (NS_SUCCEEDED(rv)) {
        mimeInfo = nsGNOMERegistry::GetFromExtension(ext);
      }
    }
    if (mimeInfo) *_retval = true;
  }

  if (*_retval) return NS_OK;

#ifdef MOZ_ENABLE_CONTENTACTION
  ContentAction::Action action;
  if (mClass == eProtocolInfo) {
    const QString uri =
        QString::fromUtf8(mSchemeOrType.get()) + QLatin1Char(':');
    action = ContentAction::Action::defaultActionForScheme(uri);
  } else {
    action = ContentAction::Action::defaultActionForFile(
        QUrl(), QString::fromUtf8(mSchemeOrType.get()));
  }
  *_retval = action.isValid();
#endif

  return NS_OK;
}

nsresult nsMIMEInfoUnix::LaunchDefaultWithFile(nsIFile* aFile) {
  // if a default app is set, it means the application has been set from
  // either /etc/mailcap or ${HOME}/.mailcap, in which case we don't want to
  // give the GNOME answer.
  if (GetDefaultApplication()) {
    return nsMIMEInfoImpl::LaunchDefaultWithFile(aFile);
  }

  nsAutoCString nativePath;
  nsresult rv = aFile->GetNativePath(nativePath);
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef MOZ_ENABLE_CONTENTACTION
  const QUrl localFileUri =
      QUrl::fromLocalFile(QString::fromUtf8(nativePath.get()));
  ContentAction::Action action =
      ContentAction::Action::defaultActionForFile(
          localFileUri, QString::fromUtf8(mSchemeOrType.get()));
  if (action.isValid()) {
    action.trigger();
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
#endif

  nsCOMPtr<nsIGIOService> giovfs = do_GetService(NS_GIOSERVICE_CONTRACTID);
  if (!giovfs) {
    return NS_ERROR_FAILURE;
  }

  // nsGIOMimeApp->Launch wants a URI string instead of local file
  nsCOMPtr<nsIIOService> ioservice =
      do_GetService(NS_IOSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIURI> uri;
  rv = ioservice->NewFileURI(aFile, getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIHandlerApp> app;
  if (NS_FAILED(
          giovfs->GetAppForMimeType(mSchemeOrType, getter_AddRefs(app))) ||
      !app) {
    return NS_ERROR_FILE_NOT_FOUND;
  }

  return app->LaunchWithURI(uri, nullptr);
}

#ifdef MOZ_ENABLE_CONTENTACTION
NS_IMETHODIMP nsMIMEInfoUnix::GetPossibleApplicationHandlers(
    nsIMutableArray** aPossibleAppHandlers) {
  if (!mPossibleApplications) {
    mPossibleApplications = do_CreateInstance(NS_ARRAY_CONTRACTID);
    if (!mPossibleApplications) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    QList<ContentAction::Action> actions;
    const bool handlesScheme = mClass == eProtocolInfo;
    if (handlesScheme) {
      const QString uri =
          QString::fromUtf8(mSchemeOrType.get()) + QLatin1Char(':');
      actions = ContentAction::Action::actionsForScheme(uri);
    } else {
      actions = ContentAction::Action::actionsForFile(
          QUrl(), QString::fromUtf8(mSchemeOrType.get()));
    }

    for (const ContentAction::Action& action : actions) {
      const QByteArray name = action.name().toUtf8();
      RefPtr<nsContentHandlerApp> app = new nsContentHandlerApp(
          NS_ConvertUTF8toUTF16(name.constData()), mSchemeOrType,
          handlesScheme);
      nsresult rv = mPossibleApplications->AppendElement(app);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  *aPossibleAppHandlers = mPossibleApplications;
  NS_IF_ADDREF(*aPossibleAppHandlers);
  return NS_OK;
}
#endif
