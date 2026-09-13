// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// JNI entry points for the Android app.
//
// This is not the emulator front end. It reports what the device can actually do, because three
// things the Android port depends on can only be measured on real hardware:
//
//   - the host page size, which decides whether GPU write tracking is even expressible
//     (video_core/buffer_cache/region_definitions.h tracks at 16 KB on Android),
//   - how much contiguous virtual address space the kernel hands out, since the desktop layout
//     wants far more than a 39-bit VA kernel has (core/address_space.cpp),
//   - whether the Vulkan driver has what the renderer hard-requires.

#include <string>
#include <string_view>
#include <vector>
#include <dlfcn.h>
#include <jni.h>
#include <sys/mman.h>
#include <unistd.h>

#include <vulkan/vulkan.h>

#include "common/path_util.h"
#include "common/scm_rev.h"
#include "common/types.h"

namespace {

void Line(std::string& out, const std::string& text) {
    out += text;
    out += "\n";
}

/// Largest mapping the kernel will hand out, by bisection. PROT_NONE and MAP_NORESERVE so nothing
/// is committed: this asks about address space, not memory.
u64 ProbeReservableAddressSpace() {
    u64 lo = 0;
    u64 hi = 1ULL << 48;
    while (hi - lo > (1ULL << 30)) {
        const u64 mid = lo + (hi - lo) / 2;
        void* p = mmap(nullptr, mid, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
        if (p == MAP_FAILED) {
            hi = mid;
        } else {
            munmap(p, mid);
            lo = mid;
        }
    }
    return lo;
}

void ReportVulkan(std::string& out) {
    void* lib = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
    if (lib == nullptr) {
        Line(out, "Vulkan: libvulkan.so not present");
        return;
    }

    auto get_proc =
        reinterpret_cast<PFN_vkGetInstanceProcAddr>(dlsym(lib, "vkGetInstanceProcAddr"));
    if (get_proc == nullptr) {
        Line(out, "Vulkan: vkGetInstanceProcAddr missing");
        return;
    }

    auto enumerate_version = reinterpret_cast<PFN_vkEnumerateInstanceVersion>(
        get_proc(nullptr, "vkEnumerateInstanceVersion"));
    u32 api = VK_API_VERSION_1_0;
    if (enumerate_version != nullptr) {
        enumerate_version(&api);
    }
    Line(out, "Vulkan loader: " + std::to_string(VK_VERSION_MAJOR(api)) + "." +
                  std::to_string(VK_VERSION_MINOR(api)));

    auto create_instance =
        reinterpret_cast<PFN_vkCreateInstance>(get_proc(nullptr, "vkCreateInstance"));
    VkApplicationInfo app{};
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = "shadPS4 device report";
    app.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo = &app;

    VkInstance instance{};
    if (create_instance == nullptr || create_instance(&ci, nullptr, &instance) != VK_SUCCESS) {
        Line(out, "Vulkan: cannot create a 1.3 instance -- the renderer requires 1.3");
        return;
    }

    auto enumerate_devices = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(
        get_proc(instance, "vkEnumeratePhysicalDevices"));
    auto get_props = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(
        get_proc(instance, "vkGetPhysicalDeviceProperties"));
    auto enumerate_ext = reinterpret_cast<PFN_vkEnumerateDeviceExtensionProperties>(
        get_proc(instance, "vkEnumerateDeviceExtensionProperties"));
    auto get_features2 = reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures2>(
        get_proc(instance, "vkGetPhysicalDeviceFeatures2"));

    u32 count = 0;
    enumerate_devices(instance, &count, nullptr);
    std::vector<VkPhysicalDevice> devices(count);
    enumerate_devices(instance, &count, devices.data());
    Line(out, "Physical devices: " + std::to_string(count));

    // What vk_instance.cpp refuses to start without.
    static const char* const Required[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
        VK_EXT_VERTEX_ATTRIBUTE_DIVISOR_EXTENSION_NAME,
        VK_EXT_ROBUSTNESS_2_EXTENSION_NAME,
    };

    for (VkPhysicalDevice device : devices) {
        VkPhysicalDeviceProperties props{};
        get_props(device, &props);
        Line(out, "");
        Line(out, std::string("  ") + props.deviceName);
        Line(out, "    API " + std::to_string(VK_VERSION_MAJOR(props.apiVersion)) + "." +
                      std::to_string(VK_VERSION_MINOR(props.apiVersion)) +
                      (props.apiVersion >= VK_API_VERSION_1_3 ? "   OK" : "   TOO OLD"));

        u32 ext_count = 0;
        enumerate_ext(device, nullptr, &ext_count, nullptr);
        std::vector<VkExtensionProperties> exts(ext_count);
        enumerate_ext(device, nullptr, &ext_count, exts.data());

        for (const char* needed : Required) {
            bool found = false;
            for (const auto& have : exts) {
                if (std::string_view{have.extensionName} == needed) {
                    found = true;
                    break;
                }
            }
            Line(out, std::string("    ") + needed + (found ? "   OK" : "   MISSING"));
        }

        if (get_features2 != nullptr) {
            VkPhysicalDeviceRobustness2FeaturesEXT robustness{};
            robustness.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT;
            VkPhysicalDeviceFeatures2 features{};
            features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
            features.pNext = &robustness;
            get_features2(device, &features);
            Line(out, std::string("    robustBufferAccess2   ") +
                          (robustness.robustBufferAccess2 ? "OK" : "MISSING"));
            Line(out, std::string("    robustImageAccess2    ") +
                          (robustness.robustImageAccess2 ? "OK" : "MISSING"));
            Line(out, std::string("    nullDescriptor        ") +
                          (robustness.nullDescriptor ? "OK" : "MISSING"));
        }
    }

    auto destroy_instance =
        reinterpret_cast<PFN_vkDestroyInstance>(get_proc(instance, "vkDestroyInstance"));
    destroy_instance(instance, nullptr);
}

} // Anonymous namespace

extern "C" JNIEXPORT jstring JNICALL
Java_net_shadps4_android_NativeBridge_deviceReport(JNIEnv* env, jclass, jstring user_dir) {
    const char* dir = env->GetStringUTFChars(user_dir, nullptr);
    Common::FS::SetUserDirectory(dir);
    env->ReleaseStringUTFChars(user_dir, dir);

    std::string out;
    Line(out, std::string("shadPS4 ") + Common::g_scm_desc);
    Line(out, std::string("branch ") + Common::g_scm_branch);
    Line(out, "");

    const long page_size = sysconf(_SC_PAGESIZE);
    Line(out, "Host page size: " + std::to_string(page_size) + " bytes");
    Line(out, std::string("  tracking granularity is 16384: ") +
                  (page_size <= 16384 ? "OK" : "TOO COARSE"));
    Line(out, "");

    const u64 reservable = ProbeReservableAddressSpace();
    Line(out, "Largest reservable mapping: " + std::to_string(reservable >> 30) + " GiB");
    Line(out, std::string("  128 GiB needed for the guest layout: ") +
                  (reservable >= (128ULL << 30) ? "OK" : "NOT AVAILABLE"));
    Line(out, "");

    ReportVulkan(out);
    return env->NewStringUTF(out.c_str());
}
