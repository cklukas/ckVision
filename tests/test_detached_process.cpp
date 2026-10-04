// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/core/detached_process.hpp"
#include "cvision/testing/cktest.hpp"

CK_TEST(detached_process_success_requires_verified_started_identity_and_clean_native_facts) {
    using namespace ckv::core;
    DetachedProcessResult result;
    CK_CHECK(!result.successful());
    result.state = DetachedProcessState::Started;
    CK_CHECK(!result.successful());
    result.process_id = 0xffffffffLL;
    CK_CHECK(result.successful());
    result.state = DetachedProcessState::ContainmentRetained;
    CK_CHECK(!result.successful());
    result.state = DetachedProcessState::Started;
    result.error = {ProcessErrorDomain::Win32, 5};
    CK_CHECK(!result.successful());
    result.error = {};
    result.cleanup = DetachedProcessCleanup::Terminated;
    CK_CHECK(!result.successful());
    result.cleanup = DetachedProcessCleanup::NotNeeded;
    result.cleanup_error = {ProcessErrorDomain::Win32, 1460};
    CK_CHECK(!result.successful());
}

CK_TEST(detached_process_refusal_retains_full_identity_and_independent_cleanup_failure) {
    using namespace ckv::core;
    DetachedProcessResult result;
    result.state = DetachedProcessState::ContainmentQueryFailed;
    result.process_id = 0xffffffffLL;
    result.error = {ProcessErrorDomain::Win32, 5};
    result.cleanup = DetachedProcessCleanup::Failed;
    result.cleanup_error = {ProcessErrorDomain::Win32, 1460};
    CK_CHECK(result.process_id == 0xffffffffLL);
    CK_CHECK(result.error.code == 5);
    CK_CHECK(result.cleanup_error.code == 1460);
    CK_CHECK(!result.successful());
}
