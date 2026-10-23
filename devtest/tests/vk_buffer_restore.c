// Frame-counted Vulkan program: a host-visible buffer and its device-local copy
// must always hold the previous frame number, also after loading a state.
// Prints one "TEST:" line per frame, exits at frame END_FRAME.
// Loads Vulkan through vkGetInstanceProcAddr like game engines; libTAS hooks only that path.
#define VK_USE_PLATFORM_XLIB_KHR
#define VK_NO_PROTOTYPES
#include <X11/Xlib.h>
#include <dlfcn.h>
#include <vulkan/vulkan.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define END_FRAME 30

PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr_;
#define GLOBAL_FNS(X) X(vkCreateInstance)
#define INSTANCE_FNS(X)                                                                \
    X(vkEnumeratePhysicalDevices) X(vkGetPhysicalDeviceProperties)                     \
    X(vkGetPhysicalDeviceMemoryProperties) X(vkCreateXlibSurfaceKHR)                   \
    X(vkGetPhysicalDeviceSurfaceSupportKHR) X(vkGetPhysicalDeviceSurfaceCapabilitiesKHR) \
    X(vkCreateDevice) X(vkGetDeviceProcAddr)
#define DEVICE_FNS(X)                                                                  \
    X(vkGetDeviceQueue) X(vkCreateCommandPool) X(vkAllocateCommandBuffers)             \
    X(vkCreateFence) X(vkCreateSemaphore) X(vkCreateBuffer)                            \
    X(vkGetBufferMemoryRequirements) X(vkAllocateMemory) X(vkBindBufferMemory)         \
    X(vkMapMemory) X(vkCreateSwapchainKHR) X(vkDestroySwapchainKHR)                    \
    X(vkGetSwapchainImagesKHR) X(vkAcquireNextImageKHR) X(vkQueuePresentKHR)           \
    X(vkResetCommandBuffer) X(vkBeginCommandBuffer) X(vkEndCommandBuffer)              \
    X(vkCmdCopyBuffer) X(vkCmdClearColorImage) X(vkCmdPipelineBarrier)                 \
    X(vkQueueSubmit) X(vkWaitForFences) X(vkResetFences) X(vkDeviceWaitIdle)
#define DECLARE(fn) static PFN_##fn fn;
GLOBAL_FNS(DECLARE)
INSTANCE_FNS(DECLARE)
DEVICE_FNS(DECLARE)

static void *load(void *fn, const char *name) {
    if (!fn) {
        fprintf(stderr, "TEST: FAIL cannot load %s\n", name);
        exit(1);
    }
    return fn;
}
#define LOAD_GLOBAL(fn) fn = (PFN_##fn)load((void *)vkGetInstanceProcAddr_(NULL, #fn), #fn);
#define LOAD_INSTANCE(fn) fn = (PFN_##fn)load((void *)vkGetInstanceProcAddr_(instance, #fn), #fn);
#define LOAD_DEVICE(fn) fn = (PFN_##fn)load((void *)vkGetDeviceProcAddr(device, #fn), #fn);
#define MAX_IMAGES 8

#define CHECK(x)                                                                 \
    do {                                                                         \
        VkResult r_ = (x);                                                       \
        if (r_ != VK_SUCCESS) {                                                  \
            fprintf(stderr, "TEST: FAIL %s = %d (line %d)\n", #x, r_, __LINE__); \
            exit(1);                                                             \
        }                                                                        \
    } while (0)

static VkPhysicalDevice pd;
static VkDevice device;
static VkQueue queue;
static uint32_t queue_family;
static VkCommandPool pool;
static VkCommandBuffer cmdbuf;
static VkFence fence;
static VkSurfaceKHR surface;
static VkSwapchainKHR swapchain;
static VkImage images[MAX_IMAGES];
static uint32_t image_count;
static VkSemaphore acquired;
/* One per image: a present may still use the semaphore of the previous one. */
static VkSemaphore rendered[MAX_IMAGES];

static uint32_t find_mem_type(uint32_t bits, VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties mp;
    vkGetPhysicalDeviceMemoryProperties(pd, &mp);
    for (uint32_t i = 0; i < mp.memoryTypeCount; i++) {
        if ((bits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & props) == props) return i;
    }
    fprintf(stderr, "TEST: FAIL no memory type for 0x%x\n", props);
    exit(1);
}

static VkBuffer make_buffer(VkMemoryPropertyFlags props, void **map) {
    VkBufferCreateInfo bci = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = 4096,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    };
    VkBuffer buf;
    CHECK(vkCreateBuffer(device, &bci, NULL, &buf));
    VkMemoryRequirements reqs;
    vkGetBufferMemoryRequirements(device, buf, &reqs);
    VkMemoryAllocateInfo mai = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = reqs.size,
        .memoryTypeIndex = find_mem_type(reqs.memoryTypeBits, props),
    };
    VkDeviceMemory mem;
    CHECK(vkAllocateMemory(device, &mai, NULL, &mem));
    CHECK(vkBindBufferMemory(device, buf, mem, 0));
    if (map) CHECK(vkMapMemory(device, mem, 0, VK_WHOLE_SIZE, 0, map));
    return buf;
}

static void create_swapchain(void) {
    VkSurfaceCapabilitiesKHR caps;
    CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pd, surface, &caps));
    VkSwapchainKHR old = swapchain;
    VkSwapchainCreateInfoKHR sci = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface,
        .minImageCount = caps.minImageCount,
        .imageFormat = VK_FORMAT_B8G8R8A8_UNORM,
        .imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
        .imageExtent = caps.currentExtent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .preTransform = caps.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = VK_PRESENT_MODE_FIFO_KHR,
        .clipped = VK_TRUE,
        .oldSwapchain = old,
    };
    CHECK(vkCreateSwapchainKHR(device, &sci, NULL, &swapchain));
    if (old) vkDestroySwapchainKHR(device, old, NULL);
    image_count = MAX_IMAGES;
    CHECK(vkGetSwapchainImagesKHR(device, swapchain, &image_count, images));
}

static void barrier(VkImage image, VkImageLayout from, VkImageLayout to) {
    VkImageMemoryBarrier b = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .oldLayout = from,
        .newLayout = to,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
    };
    vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                         VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, NULL, 0, NULL, 1, &b);
}

int main(void) {
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "TEST: FAIL no display\n");
        return 1;
    }
    Window win = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy), 0, 0, 320, 240, 0, 0, 0);
    XMapWindow(dpy, win);
    XSync(dpy, False);

    void *libvulkan = dlopen("libvulkan.so.1", RTLD_NOW);
    if (!libvulkan) {
        fprintf(stderr, "TEST: FAIL dlopen libvulkan: %s\n", dlerror());
        return 1;
    }
    vkGetInstanceProcAddr_ =
        (PFN_vkGetInstanceProcAddr)load(dlsym(libvulkan, "vkGetInstanceProcAddr"), "vkGetInstanceProcAddr");
    GLOBAL_FNS(LOAD_GLOBAL)

    const char *instance_exts[] = {"VK_KHR_surface", "VK_KHR_xlib_surface"};
    VkApplicationInfo app = {.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                             .pApplicationName = "vk-buffer-restore",
                             .apiVersion = VK_API_VERSION_1_1};
    VkInstanceCreateInfo ici = {.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                                .pApplicationInfo = &app,
                                .enabledExtensionCount = 2,
                                .ppEnabledExtensionNames = instance_exts};
    VkInstance instance;
    CHECK(vkCreateInstance(&ici, NULL, &instance));
    INSTANCE_FNS(LOAD_INSTANCE)
    uint32_t count = 1;
    VkResult enumerated = vkEnumeratePhysicalDevices(instance, &count, &pd);
    if ((enumerated != VK_SUCCESS && enumerated != VK_INCOMPLETE) || count == 0) {
        fprintf(stderr, "TEST: FAIL no physical device (%d)\n", enumerated);
        return 1;
    }
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(pd, &props);
    fprintf(stderr, "INFO: device %s\n", props.deviceName);
    const char *expect = getenv("TEST_EXPECT_VK");
    if (expect && !strstr(props.deviceName, expect)) {
        fprintf(stderr, "TEST: FAIL device is not %s\n", expect);
        return 1;
    }

    VkXlibSurfaceCreateInfoKHR xsci = {.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,
                                       .dpy = dpy,
                                       .window = win};
    CHECK(vkCreateXlibSurfaceKHR(instance, &xsci, NULL, &surface));

    queue_family = 0;
    VkBool32 supported = VK_FALSE;
    CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(pd, queue_family, surface, &supported));
    if (!supported) {
        fprintf(stderr, "TEST: FAIL queue family 0 cannot present\n");
        return 1;
    }
    const float prio = 1.0f;
    VkDeviceQueueCreateInfo dqci = {.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                    .queueFamilyIndex = queue_family,
                                    .queueCount = 1,
                                    .pQueuePriorities = &prio};
    const char *device_exts[] = {"VK_KHR_swapchain"};
    VkDeviceCreateInfo dci = {.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                              .queueCreateInfoCount = 1,
                              .pQueueCreateInfos = &dqci,
                              .enabledExtensionCount = 1,
                              .ppEnabledExtensionNames = device_exts};
    CHECK(vkCreateDevice(pd, &dci, NULL, &device));
    DEVICE_FNS(LOAD_DEVICE)
    vkGetDeviceQueue(device, queue_family, 0, &queue);

    VkCommandPoolCreateInfo cpci = {.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                    .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                    .queueFamilyIndex = queue_family};
    CHECK(vkCreateCommandPool(device, &cpci, NULL, &pool));
    VkCommandBufferAllocateInfo cbai = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                        .commandPool = pool,
                                        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                        .commandBufferCount = 1};
    CHECK(vkAllocateCommandBuffers(device, &cbai, &cmdbuf));
    VkFenceCreateInfo fci = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    CHECK(vkCreateFence(device, &fci, NULL, &fence));
    VkSemaphoreCreateInfo semci = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    CHECK(vkCreateSemaphore(device, &semci, NULL, &acquired));
    for (uint32_t i = 0; i < MAX_IMAGES; i++) CHECK(vkCreateSemaphore(device, &semci, NULL, &rendered[i]));

    const VkMemoryPropertyFlags host =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    volatile uint32_t *upload, *readback;
    VkBuffer upload_buf = make_buffer(host, (void **)&upload);
    VkBuffer device_buf = make_buffer(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, NULL);
    VkBuffer readback_buf = make_buffer(host, (void **)&readback);

    create_swapchain();

    // Index 0 holds the last written frame; index 1 is never written after frame 0, so it checks
    // content that survives untouched across states.
    upload[0] = 0;
    upload[1] = 0xC0FFEE;

    for (uint32_t frame = 1;; frame++) {
        // The previous frame's upload went through the GPU into device_buf; read it back.
        CHECK(vkResetCommandBuffer(cmdbuf, 0));
        VkCommandBufferBeginInfo bi = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        CHECK(vkBeginCommandBuffer(cmdbuf, &bi));
        VkBufferCopy region = {.size = 8};
        vkCmdCopyBuffer(cmdbuf, device_buf, readback_buf, 1, &region);
        CHECK(vkEndCommandBuffer(cmdbuf));
        VkSubmitInfo si = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                           .commandBufferCount = 1,
                           .pCommandBuffers = &cmdbuf};
        CHECK(vkQueueSubmit(queue, 1, &si, fence));
        CHECK(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX));
        CHECK(vkResetFences(device, 1, &fence));

        uint32_t expected = frame - 1;
        uint32_t cpu = upload[0], cpu_untouched = upload[1];
        uint32_t gpu = readback[0], gpu_untouched = readback[1];
        if (frame > 1 && (cpu != expected || gpu != expected || cpu_untouched != 0xC0FFEE ||
                          gpu_untouched != 0xC0FFEE)) {
            fprintf(stderr,
                    "TEST: FAIL frame %u: upload %u/0x%x, device %u/0x%x, expected %u/0xc0ffee\n",
                    frame, cpu, cpu_untouched, gpu, gpu_untouched, expected);
            return 1;
        }
        fprintf(stderr, "TEST: frame %u ok\n", frame);
        if (frame == END_FRAME) break;

        upload[0] = frame;

        uint32_t index;
        VkResult acquire;
        while ((acquire = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, acquired,
                                                VK_NULL_HANDLE, &index)) ==
               VK_ERROR_OUT_OF_DATE_KHR) {
            fprintf(stderr, "TEST: swapchain out of date\n");
            create_swapchain();
        }
        if (acquire != VK_SUCCESS && acquire != VK_SUBOPTIMAL_KHR) CHECK(acquire);

        CHECK(vkResetCommandBuffer(cmdbuf, 0));
        CHECK(vkBeginCommandBuffer(cmdbuf, &bi));
        vkCmdCopyBuffer(cmdbuf, upload_buf, device_buf, 1, &region);
        barrier(images[index], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        VkClearColorValue color = {.float32 = {(frame % 8) / 8.0f, 0.2f, 0.4f, 1.0f}};
        VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vkCmdClearColorImage(cmdbuf, images[index], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &color, 1,
                             &range);
        barrier(images[index], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
        CHECK(vkEndCommandBuffer(cmdbuf));
        VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        VkSubmitInfo psi = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                            .waitSemaphoreCount = 1,
                            .pWaitSemaphores = &acquired,
                            .pWaitDstStageMask = &wait_stage,
                            .commandBufferCount = 1,
                            .pCommandBuffers = &cmdbuf,
                            .signalSemaphoreCount = 1,
                            .pSignalSemaphores = &rendered[index]};
        CHECK(vkQueueSubmit(queue, 1, &psi, fence));
        CHECK(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX));
        CHECK(vkResetFences(device, 1, &fence));

        VkPresentInfoKHR pi = {.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                               .waitSemaphoreCount = 1,
                               .pWaitSemaphores = &rendered[index],
                               .swapchainCount = 1,
                               .pSwapchains = &swapchain,
                               .pImageIndices = &index};
        VkResult presented = vkQueuePresentKHR(queue, &pi);
        if (presented == VK_ERROR_OUT_OF_DATE_KHR) {
            fprintf(stderr, "TEST: swapchain out of date\n");
            create_swapchain();
        } else if (presented != VK_SUBOPTIMAL_KHR) {
            CHECK(presented);
        }
    }

    vkDeviceWaitIdle(device);
    return 0;
}
