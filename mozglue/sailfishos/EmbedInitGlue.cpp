/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedInitGlue.h"

#include "mozilla/Bootstrap.h"
#include "nsXPCOMPrivate.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include <string>

using namespace mozilla;

Bootstrap::UniquePtr gBootstrap;

static bool FindLibXul(const char* aDirectory, std::string& aXPCOMPath) {
  if (!aDirectory || !*aDirectory) {
    return false;
  }

  aXPCOMPath = aDirectory;
  if (aXPCOMPath.back() != '/') {
    aXPCOMPath += '/';
  }
  aXPCOMPath += XPCOM_DLL;

  struct stat buf;
  return stat(aXPCOMPath.c_str(), &buf) == 0;
}

static std::string ResolveXPCOMPath(int aArgc, char** aArgv) {
  std::string xpcomPath;
  std::string greHome;

  const char* configuredHome = getenv("GRE_HOME");
  if (FindLibXul(configuredHome, xpcomPath)) {
    greHome = configuredHome;
  }

  if (greHome.empty()) {
    char currentDirectory[PATH_MAX];
    if (getcwd(currentDirectory, sizeof(currentDirectory)) &&
        FindLibXul(currentDirectory, xpcomPath)) {
      greHome = currentDirectory;
    }
  }

  if (greHome.empty() && aArgc > 0 && aArgv && aArgv[0]) {
    std::string executable = aArgv[0];
    size_t separator = executable.rfind('/');
    if (separator != std::string::npos) {
      std::string executableDirectory =
          separator == 0 ? "/" : executable.substr(0, separator);
      if (FindLibXul(executableDirectory.c_str(), xpcomPath)) {
        greHome = executableDirectory;
      }
    }
  }

  const char* buildHome = getenv("BUILD_GRE_HOME");
  if (greHome.empty() && FindLibXul(buildHome, xpcomPath)) {
    greHome = buildHome;
  }

  if (greHome.empty()) {
    return {};
  }

  setenv("GRE_HOME", greHome.c_str(), 1);
  setenv("MOZILLA_FIVE_HOME", greHome.c_str(), 1);
  setenv("XRE_LIBXPCOM_PATH", xpcomPath.c_str(), 1);
  return xpcomPath;
}

bool LoadEmbedLite(int aArgc, char** aArgv) {
  std::string xpcomPath = ResolveXPCOMPath(aArgc, aArgv);
  if (xpcomPath.empty()) {
    printf("Couldn't find %s\n", XPCOM_DLL);
    return false;
  }

  BootstrapResult bootstrapResult = mozilla::GetBootstrap(xpcomPath.c_str());
  if (bootstrapResult.isErr()) {
    printf("Couldn't load XPCOM from %s\n", xpcomPath.c_str());
    return false;
  }
  gBootstrap = bootstrapResult.unwrap();
  return true;
}
