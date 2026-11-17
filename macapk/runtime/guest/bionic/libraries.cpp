// Runtime-owned system-library declarations, not ELF copies of Android's system libraries.
#include "libraries.h"
#include "threads.h"

#include <new>

namespace winapk::guest {

std::unique_ptr<BionicLibraries> BionicLibraries::Load(const std::wstring& path, LoadError* error) {
    try {
        auto result = std::make_unique<BionicLibraries>();
        const auto fixed = ThreadRuntime::Imports();
        result->imports_.assign(fixed.begin(), fixed.end());
        result->helper_ = guest::Load(path, fixed, error);
        if (!result->helper_) return nullptr;
        for (const char* name : {"snprintf", "vsnprintf", "__android_log_print", "__android_log_vprint"}) {
            void* address = result->helper_->Find(name);
            if (!address) {
                *error = {Category::SymbolUnresolved, "Bionic helper", std::string("missing export ") + name};
                return nullptr;
            }
            result->imports_.push_back({name, address});
        }
        result->libraries_ = {{"libc.so", {}, "LIBC"}, {"libdl.so", {}, "LIBC"}, {"liblog.so", {}, ""}};
        for (const auto& symbol : result->imports_) {
            const std::string_view name = symbol.name;
            const size_t provider = name.starts_with("__android_log_") ? 2 :
                name == "dlopen" || name == "dlsym" || name == "dlclose" || name == "dlerror" ? 1 : 0;
            result->libraries_[provider].symbols.push_back(symbol);
        }
        return result;
    } catch (const std::bad_alloc&) {
        *error = {Category::HostMemory, "Bionic libraries", "library declaration allocation failed"};
        return nullptr;
    }
}

}  // namespace winapk::guest
