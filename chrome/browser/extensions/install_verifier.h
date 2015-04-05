// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_INSTALL_VERIFIER_H_
#define CHROME_BROWSER_EXTENSIONS_INSTALL_VERIFIER_H_

#include <queue>
#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "extensions/browser/management_policy.h"
#include "extensions/common/extension.h"

namespace content {
class BrowserContext;
}

namespace net {
class URLRequestContextGetter;
}

namespace extensions {

class ExtensionPrefs;
class InstallSigner;
struct InstallSignature;

// This class implements verification that a set of extensions are either from
// the webstore or are whitelisted by enterprise policy.  The webstore
// verification process works by sending a request to a backend server to get a
// signature proving that a set of extensions are verified. This signature is
// written into the extension preferences and is checked for validity when
// being read back again.
//
// This class should be kept notified of runtime changes to the set of
// extensions installed from the webstore.
class InstallVerifier : public ManagementPolicy::Provider {
 public:
  InstallVerifier(ExtensionPrefs* prefs, content::BrowserContext* context);
  ~InstallVerifier() override;

  // Returns whether |extension| is of a type that needs verification.
  static bool NeedsVerification(const Extension& extension);

  // Determines if an extension claims to be from the webstore.
  static bool IsFromStore(const Extension& extension);

  // Initializes this object for use, including reading preferences and
  // validating the stored signature.
  void Init();

  // Returns the timestamp of our InstallSignature, if we have one.
  base::Time SignatureTimestamp();

  // Returns true if |id| is either verified or our stored signature explicitly
  // tells us that it was invalid when we asked the server about it.
  bool IsKnownId(const std::string& id) const;

  // Returns whether the given |id| is considered invalid by our verified
  // signature.
  bool IsInvalid(const std::string& id) const;

  // Attempts to verify a single extension and add it to the verified list.
  void VerifyExtension(const std::string& extension_id);

  // Attempts to verify all extensions.
  void VerifyAllExtensions();

  // Call this to add a set of ids that will immediately be considered allowed,
  // and kick off an aysnchronous request to Add.
  void AddProvisional(const ExtensionIdSet& ids);

  // Removes an id or set of ids from the verified list.
  void Remove(const std::string& id);
  void RemoveMany(const ExtensionIdSet& ids);

  // Returns whether an extension id is allowed by policy.
  bool AllowedByEnterprisePolicy(const std::string& id) const;

  // ManagementPolicy::Provider interface.
  std::string GetDebugPolicyProviderName() const override;
  bool MustRemainDisabled(const Extension* extension,
                          Extension::DisableReason* reason,
                          base::string16* error) const override;

 private:
  // We keep a list of operations to the current set of extensions.
  enum OperationType {
    ADD_SINGLE,         // Adding a single extension to be verified.
    ADD_ALL,            // Adding all extensions to be verified.
    ADD_ALL_BOOTSTRAP,  // Adding all extensions because of a bootstrapping.
    ADD_PROVISIONAL,    // Adding one or more provisionally-allowed extensions.
    REMOVE              // Remove one or more extensions.
  };

  // This is an operation we want to apply to the current set of verified ids.
  struct PendingOperation {
    OperationType type;

    // This is the set of ids being either added or removed.
    ExtensionIdSet ids;

    explicit PendingOperation(OperationType type);
    ~PendingOperation();
  };

  // Returns the set of IDs for all extensions that potentially need to be
  // verified.
  ExtensionIdSet GetExtensionsToVerify() const;

  // Bootstrap the InstallVerifier if we do not already have a signature, or if
  // there are unknown extensions which need to be verified.
  void MaybeBootstrapSelf();

  // Try adding a new set of |ids| to the list of verified ids.
  void AddMany(const ExtensionIdSet& ids, OperationType type);

  // Record the result of the verification for the histograms, and notify the
  // ExtensionPrefs if we verified all extensions.
  void OnVerificationComplete(bool success, OperationType type);

  // Removes any no-longer-installed ids, requesting a new signature if needed.
  void GarbageCollect();

  // Returns whether the given |id| is included in our verified signature.
  bool IsVerified(const std::string& id) const;

  // Returns true if the extension with |id| was installed later than the
  // timestamp of our signature.
  bool WasInstalledAfterSignature(const std::string& id) const;

  // Begins the process of fetching a new signature, based on applying the
  // operation at the head of the queue to the current set of ids in
  // |signature_| (if any) and then sending a request to sign that.
  void BeginFetch();

  // Saves the current value of |signature_| to the prefs;
  void SaveToPrefs();

  // Called with the result of a signature request, or NULL on failure.
  void SignatureCallback(scoped_ptr<InstallSignature> signature);

  ExtensionPrefs* prefs_;

  // The context with which the InstallVerifier is associated.
  content::BrowserContext* context_;

  // Have we finished our bootstrap check yet?
  bool bootstrap_check_complete_;

  // This is the most up-to-date signature, read out of |prefs_| during
  // initialization and updated anytime we get new id's added.
  scoped_ptr<InstallSignature> signature_;

  // The current InstallSigner, if we have a signature request running.
  scoped_ptr<InstallSigner> signer_;

  // A queue of operations to apply to the current set of allowed ids.
  std::queue<linked_ptr<PendingOperation> > operation_queue_;

  // A set of ids that have been provisionally added, which we're willing to
  // consider allowed until we hear back from the server signature request.
  ExtensionIdSet provisional_;

  base::WeakPtrFactory<InstallVerifier> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(InstallVerifier);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_INSTALL_VERIFIER_H_