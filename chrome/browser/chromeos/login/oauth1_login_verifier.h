// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_OAUTH1_LOGIN_VERIFIER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_OAUTH1_LOGIN_VERIFIER_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/net/gaia/gaia_oauth_consumer.h"
#include "chrome/browser/net/gaia/gaia_oauth_fetcher.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"

namespace net {
class URLRequestContextGetter;
}

namespace chromeos {

// Verifies OAuth1 access token by performing OAuthLogin. Fetches user cookies
// on successful OAuth authentication.
class OAuth1LoginVerifier : public base::SupportsWeakPtr<OAuth1LoginVerifier>,
                            public GaiaOAuthConsumer,
                            public GaiaAuthConsumer {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}
    virtual void OnOAuth1VerificationSucceeded(const std::string& user_name,
                                               const std::string& sid,
                                               const std::string& lsid,
                                               const std::string& auth) {}
    virtual void OnOAuth1VerificationFailed(const std::string& user_name) {}
    virtual void OnCookiesFetchWithOAuth1Succeeded(
        const std::string& user_name) {}
    virtual void OnCookiesFetchWithOAuth1Failed(const std::string& user_name) {}
  };

  OAuth1LoginVerifier(OAuth1LoginVerifier::Delegate* delegate,
                      net::URLRequestContextGetter* user_request_context,
                      const std::string& oauth1_token,
                      const std::string& oauth1_secret,
                      const std::string& username);
  virtual ~OAuth1LoginVerifier();

  bool is_done() {
    return step_ == VERIFICATION_STEP_FAILED ||
           step_ == VERIFICATION_STEP_COOKIES_FETCHED;
  }

  void StartOAuthVerification();
  void ContinueVerification();

 private:
  typedef enum {
    VERIFICATION_STEP_UNVERIFIED,
    VERIFICATION_STEP_OAUTH_VERIFIED,
    VERIFICATION_STEP_COOKIES_FETCHED,
    VERIFICATION_STEP_FAILED,
  } VerificationStep;

  // Kicks off GAIA session cookie retrieval process.
  void StartCookiesRetrieval();

  // Decides how to proceed on GAIA response and other errors. It can schedule
  // to rerun the verification process if detects transient network or service
  // errors.
  bool RetryOnError(const GoogleServiceAuthError& error);

  // GaiaOAuthConsumer implementation:
  virtual void OnOAuthLoginSuccess(const std::string& sid,
                                   const std::string& lsid,
                                   const std::string& auth) OVERRIDE;
  virtual void OnOAuthLoginFailure(
      const GoogleServiceAuthError& error) OVERRIDE;
  void OnCookieFetchFailed(const GoogleServiceAuthError& error);

  // GaiaAuthConsumer overrides.
  virtual void OnIssueAuthTokenSuccess(const std::string& service,
                                       const std::string& auth_token) OVERRIDE;
  virtual void OnIssueAuthTokenFailure(
      const std::string& service,
      const GoogleServiceAuthError& error) OVERRIDE;
  virtual void OnMergeSessionSuccess(const std::string& data) OVERRIDE;
  virtual void OnMergeSessionFailure(
      const GoogleServiceAuthError& error) OVERRIDE;

  OAuth1LoginVerifier::Delegate* delegate_;
  GaiaOAuthFetcher oauth_fetcher_;
  GaiaAuthFetcher gaia_fetcher_;
  std::string oauth1_token_;
  std::string oauth1_secret_;
  std::string sid_;
  std::string lsid_;
  std::string username_;
  int verification_count_;
  VerificationStep step_;

  DISALLOW_COPY_AND_ASSIGN(OAuth1LoginVerifier);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_OAUTH1_LOGIN_VERIFIER_H_
