#include "../include/interpreter.h"
#include "../include/module_registry.h"
#include <dxgi1_2.h>
#include <windows.h>
#include <string>
#include <vector>
#include <stdexcept>
#include <sstream>

namespace gpu_lib {
namespace {
std::vector<IDXGIAdapter1*> enumerate_adapters() {
    std::vector<IDXGIAdapter1*> adapters;
    IDXGIFactory1* factory = nullptr;
    if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&factory)))) {
        return adapters;
    }

    for (UINT index = 0;; ++index) {
        IDXGIAdapter1* adapter = nullptr;
        if (FAILED(factory->EnumAdapters1(index, &adapter))) {
            break;
        }
        adapters.push_back(adapter);
    }
    factory->Release();
    return adapters;
}

void release_adapters(std::vector<IDXGIAdapter1*>& adapters) {
    for (IDXGIAdapter1* adapter : adapters) {
        adapter->Release();
    }
    adapters.clear();
}

std::string vendor_name(UINT vendorId) {
    switch (vendorId) {
        case 0x10DE: return "NVIDIA";
        case 0x1002: return "AMD";
        case 0x8086: return "Intel";
        default: return "Unknown";
    }
}

std::string to_string_flags(UINT flags) {
    std::ostringstream oss;
    oss << flags;
    return oss.str();
}

IDXGIAdapter1* adapter_at(int index) {
    auto adapters = enumerate_adapters();
    if (index < 0 || static_cast<size_t>(index) >= adapters.size()) {
        release_adapters(adapters);
        throw std::runtime_error("GPU adapter index out of range");
    }
    IDXGIAdapter1* adapter = adapters[index];
    for (size_t i = 0; i < adapters.size(); ++i) {
        if (static_cast<int>(i) != index) {
            adapters[i]->Release();
        }
    }
    adapters.clear();
    return adapter;
}
} // namespace

int adapter_count() {
    auto adapters = enumerate_adapters();
    int count = static_cast<int>(adapters.size());
    release_adapters(adapters);
    return count;
}

std::string to_utf8(const std::wstring& wide) {
    if (wide.empty()) {
        return "";
    }
    int size = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    if (size == 0) {
        return "";
    }
    std::string result(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), &result[0], size, nullptr, nullptr);
    return result;
}

std::string adapter_names() {
    auto adapters = enumerate_adapters();
    std::ostringstream oss;
    for (size_t i = 0; i < adapters.size(); ++i) {
        DXGI_ADAPTER_DESC1 desc{};
        adapters[i]->GetDesc1(&desc);
        if (i) {
            oss << "|";
        }
        oss << to_utf8(std::wstring(desc.Description));
    }
    release_adapters(adapters);
    return oss.str();
}

std::string adapter_name(int index) {
    IDXGIAdapter1* adapter = adapter_at(index);
    DXGI_ADAPTER_DESC1 desc{};
    adapter->GetDesc1(&desc);
    std::wstring ws(desc.Description);
    adapter->Release();
    return to_utf8(ws);
}

std::string adapter_vendor(int index) {
    IDXGIAdapter1* adapter = adapter_at(index);
    DXGI_ADAPTER_DESC1 desc{};
    adapter->GetDesc1(&desc);
    adapter->Release();
    return vendor_name(desc.VendorId);
}

double adapter_memory_mb(int index) {
    IDXGIAdapter1* adapter = adapter_at(index);
    DXGI_ADAPTER_DESC1 desc{};
    adapter->GetDesc1(&desc);
    adapter->Release();
    return static_cast<double>(desc.DedicatedVideoMemory) / (1024.0 * 1024.0);
}

std::string adapter_device_id(int index) {
    IDXGIAdapter1* adapter = adapter_at(index);
    DXGI_ADAPTER_DESC1 desc{};
    adapter->GetDesc1(&desc);
    adapter->Release();
    std::ostringstream oss;
    oss << "VID_" << std::uppercase << std::hex << desc.VendorId << "_DEV_" << desc.DeviceId;
    return oss.str();
}

std::string adapter_flags(int index) {
    IDXGIAdapter1* adapter = adapter_at(index);
    DXGI_ADAPTER_DESC1 desc{};
    adapter->GetDesc1(&desc);
    adapter->Release();
    return to_string_flags(desc.Flags);
}

std::string primary_adapter() {
    auto adapters = enumerate_adapters();
    if (adapters.empty()) {
        return "";
    }
    DXGI_ADAPTER_DESC1 desc{};
    adapters[0]->GetDesc1(&desc);
    std::wstring ws(desc.Description);
    release_adapters(adapters);
    return to_utf8(ws);
}

} // namespace gpu_lib

extern "C" __declspec(dllexport)
void register_module() {
    module_registry::registerModule("gpu", [](Interpreter& interp) {
                    auto parse_index = [&interp](const Value& value, const std::string& name) -> int {
                        double raw = interp.expectNumber(value, name + " expects adapter index number");
                        int idx = static_cast<int>(raw);
                        if (raw != static_cast<double>(idx)) {
                            throw std::runtime_error(name + " expects integer index");
                        }
                        return idx;
                    };
                    interp.registerModuleFunction("gpu", "adapter_count", [&interp](const std::vector<Value>& args) -> Value {
                        interp.expectArity(args, 0, "gpu.adapter_count");
                        return Value::fromNumber(static_cast<double>(gpu_lib::adapter_count()));
                    });
                    interp.registerModuleFunction("gpu", "adapter_names", [&interp](const std::vector<Value>& args) -> Value {
                        interp.expectArity(args, 0, "gpu.adapter_names");
                        return Value::fromString(gpu_lib::adapter_names());
                    });
                    interp.registerModuleFunction("gpu", "primary_adapter", [&interp](const std::vector<Value>& args) -> Value {
                        interp.expectArity(args, 0, "gpu.primary_adapter");
                        return Value::fromString(gpu_lib::primary_adapter());
                    });
                    interp.registerModuleFunction("gpu", "adapter_name", [&interp, parse_index](const std::vector<Value>& args) -> Value {
                        interp.expectArity(args, 1, "gpu.adapter_name");
                        int idx = parse_index(args[0], "gpu.adapter_name");
                        return Value::fromString(gpu_lib::adapter_name(idx));
                    });
                    interp.registerModuleFunction("gpu", "adapter_vendor", [&interp, parse_index](const std::vector<Value>& args) -> Value {
                        interp.expectArity(args, 1, "gpu.adapter_vendor");
                        int idx = parse_index(args[0], "gpu.adapter_vendor");
                        return Value::fromString(gpu_lib::adapter_vendor(idx));
                    });
                    interp.registerModuleFunction("gpu", "adapter_memory_mb", [&interp, parse_index](const std::vector<Value>& args) -> Value {
                        interp.expectArity(args, 1, "gpu.adapter_memory_mb");
                        int idx = parse_index(args[0], "gpu.adapter_memory_mb");
                        return Value::fromNumber(gpu_lib::adapter_memory_mb(idx));
                    });
                    interp.registerModuleFunction("gpu", "adapter_device_id", [&interp, parse_index](const std::vector<Value>& args) -> Value {
                        interp.expectArity(args, 1, "gpu.adapter_device_id");
                        int idx = parse_index(args[0], "gpu.adapter_device_id");
                        return Value::fromString(gpu_lib::adapter_device_id(idx));
                    });
                    interp.registerModuleFunction("gpu", "adapter_flags", [&interp, parse_index](const std::vector<Value>& args) -> Value {
                        interp.expectArity(args, 1, "gpu.adapter_flags");
                        int idx = parse_index(args[0], "gpu.adapter_flags");
                        return Value::fromString(gpu_lib::adapter_flags(idx));
                    });

    });
}
