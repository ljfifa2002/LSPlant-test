#pragma once

#include "common.hpp"

namespace lsplant::art::jit {
class JitCodeCache {
    CREATE_MEM_FUNC_SYMBOL_ENTRY(void, MoveObsoleteMethod, JitCodeCache *thiz,
                                 ArtMethod *old_method, ArtMethod *new_method) {
        if (MoveObsoleteMethodSym) [[likely]] {
            MoveObsoleteMethodSym(thiz, old_method, new_method);
        } else {
            // fallback to set data
            new_method->SetData(old_method->GetData());
            old_method->SetData(nullptr);
        }
    }

    CREATE_MEM_HOOK_STUB_ENTRY("_ZN3art3jit12JitCodeCache19GarbageCollectCacheEPNS_6ThreadE", void,
                               GarbageCollectCache, (JitCodeCache * thiz, Thread *self), {
                                   auto movements = GetJitMovements();
                                   LOGD("Before jit cache gc, moving %zu hooked methods",
                                        movements.size());
                                   for (auto [target, backup] : movements) {
                                       MoveObsoleteMethod(thiz, target, backup);
                                   }
                                   backup(thiz, self);
                               });

    // Android 15 renamed GarbageCollectCache to DoCollection.
    // Same logic: move hooked methods before the JIT cache is GC'd.
    CREATE_MEM_HOOK_STUB_ENTRY("_ZN3art3jit12JitCodeCache12DoCollectionEPNS_6ThreadE", void,
                               DoCollection, (JitCodeCache * thiz, Thread *self), {
                                   auto movements = GetJitMovements();
                                   LOGD("Before jit cache gc (DoCollection), moving %zu hooked methods",
                                        movements.size());
                                   for (auto [target, backup] : movements) {
                                       MoveObsoleteMethod(thiz, target, backup);
                                   }
                                   backup(thiz, self);
                               });

public:
    static bool Init(const HookHandler &handler) {
        auto sdk_int = GetAndroidApiLevel();
        if (sdk_int >= __ANDROID_API_O__) [[likely]] {
            if (!RETRIEVE_MEM_FUNC_SYMBOL(
                    MoveObsoleteMethod,
                    "_ZN3art3jit12JitCodeCache18MoveObsoleteMethodEPNS_9ArtMethodES3_"))
                [[unlikely]] {
                return false;
            }
        }
        if (sdk_int >= __ANDROID_API_N__) [[likely]] {
            // Android 15 renamed GarbageCollectCache to DoCollection.
            // Try both; fail only if neither is found.
            if (!HookSyms(handler, GarbageCollectCache) &&
                !HookSyms(handler, DoCollection)) [[unlikely]] {
                return false;
            }
        }
        return true;
    }
};
}  // namespace lsplant::art::jit
