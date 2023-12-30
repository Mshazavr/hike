#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <vector>
#include <stdexcept>
#include <cstdlib>
#include <format>
#include <string>
#include <optional>
#include <set>
#include <cstring>

constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;

const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else 
constexpr bool enableValidationLayers = true;
#endif

VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance, 
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, 
    const VkAllocationCallbacks* pAllocator, 
    VkDebugUtilsMessengerEXT* pDebugMessenger
) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }
    else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void DestroyDebugUtilsMessengerEXT(
    VkInstance instance, 
    VkDebugUtilsMessengerEXT debugMessenger, 
    const VkAllocationCallbacks* pAllocator
) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> surfaceFamily;

    bool isComplete() {
        return graphicsFamily.has_value() && surfaceFamily.has_value();
    }
};

struct SwapChainDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presents;
};

class HelloTriangleApplication {
public:
    void run() {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

private:
    GLFWwindow* window;
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;
    VkSurfaceKHR surface;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue surfaceQueue = VK_NULL_HANDLE;


    void initWindow() {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // tell it to not create openGL context
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        window = glfwCreateWindow(WIDTH, HEIGHT, "First Window", nullptr, nullptr);
    }

    void initVulkan() {
        createInstance();
        setupDebugMessenger();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
    }

    void mainLoop() {
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
        }
    }

    void cleanup() {
        if (enableValidationLayers) {
            DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
        }

        vkDestroyDevice(device, nullptr);
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
        glfwDestroyWindow(window);
        glfwTerminate();
    }

    VkDebugUtilsMessengerCreateInfoEXT getDebugMessengerCreateInfo() {
        return {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .messageSeverity = (
                //VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT
            ),
            .messageType = (
                VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT
            ),
            .pfnUserCallback = debugCallback,
            .pUserData = nullptr // could pass a pointer to <this> (HelloTriangleApplication) for instance
        };
    }

    void createInstance() {
        if (enableValidationLayers && !checkValidationLayersSupported()) {
            throw std::runtime_error("Validation Layers requeted but not supported.");
        }

        std::vector<const char*> extensions = getRequiredExtensions();
        if (!checkExtensionsSupported(extensions)) {
            throw std::runtime_error("GLFW requested extensions not suppoted.");
        }

        VkApplicationInfo appInfo{
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pApplicationName = "Hello Triangle",
            .applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0),
            .pEngineName = "No Engine",
            .engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0),
            .apiVersion = VK_API_VERSION_1_0
        };

        VkInstanceCreateInfo createInfo{
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pApplicationInfo = &appInfo,
            .enabledLayerCount = 0,
            .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
            .ppEnabledExtensionNames = extensions.data()
        };

        VkDebugUtilsMessengerCreateInfoEXT debugInfo = getDebugMessengerCreateInfo();
        if (enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
            createInfo.pNext = &debugInfo;
        }

        // The general pattern for vulkan instance creation function signature is:
        // - pointer to info struct 
        // - pointer to custom allocator (null in this case)
        // - pointer to the instance object to use
        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create vulkan instance");
        }
    }

    void setupDebugMessenger() {
        if (!enableValidationLayers) return;
        VkDebugUtilsMessengerCreateInfoEXT createInfo = getDebugMessengerCreateInfo();

        if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create debug messenger");
        }
    }

    void createSurface() {
        if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create surface.");
        }
    }

    std::vector<const char*> getRequiredExtensions() {
        uint32_t glfwExtensionCount;
        const char** glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector<const char*> requiredExtensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
        if (enableValidationLayers) {
            requiredExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }
        return requiredExtensions;
    }

    bool checkExtensionsSupported(const std::vector<const char *> &extensions) {
        // Check that glfw extensions are in supported list of extensions 
        uint32_t extensionCount;
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data());

        for (const auto &extension: extensions) {
            bool extensionSupported = false;
            for (const auto& availableExtension : availableExtensions) {
                if (strcmp(availableExtension.extensionName, extension) == 0) {
                    extensionSupported = true;
                    break;
                }
            }
            if (!extensionSupported) {
                std::cerr << std::format("Extension {} not supported", extension) << std::endl;
                return false;
            }
        }

        return true;
    }

    bool checkValidationLayersSupported(bool verbose=false) {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        std::vector<VkLayerProperties> layers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, layers.data());

        if (verbose) {
            std::cout << "Available Validation Layers: [" << std::endl;
            for (const auto& layer : layers) {
                std::cout << "\t" << layer.layerName << std::endl;
            }
            std::cout << "]" << std::endl;
        }
        for (const auto& requestedLayer : validationLayers) {
            bool layerSupported = false;
            for (const auto& layer : layers) {
                if (strcmp(requestedLayer, layer.layerName) == 0) {
                    layerSupported = true;
                    break;
                }
            }
            if (!layerSupported) {
                std::cerr << std::format("Validation Layer {} not supported", requestedLayer) << std::endl;
                return false;
            }
        }

        return true;
    }

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
        void *pUserData
    ) {
        std::cerr << "Validation Layer: " << pCallbackData->pMessage << std::endl;
        return VK_FALSE;
    }

    void pickPhysicalDevice() {
        uint32_t numDevices; 
        vkEnumeratePhysicalDevices(instance, &numDevices, nullptr);
        std::vector<VkPhysicalDevice> devices(numDevices);
        vkEnumeratePhysicalDevices(instance, &numDevices, devices.data());

        for (const auto& device : devices) {
            if (isDeviceSuitable(device)) {
                this->physicalDevice = device;
                return;
            }
        }

        throw std::runtime_error("No suitable device found.");
    }

    bool isDeviceSuitable(VkPhysicalDevice physicalDevice) {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(physicalDevice, &properties);

        VkPhysicalDeviceFeatures features;
        vkGetPhysicalDeviceFeatures(physicalDevice, &features);

        QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

        // For fun
        if (properties.deviceName == "NVIDIA GeForce RTX 3060") {
            throw std::runtime_error("GPU Name unexpected");
        }

        bool isDiscreteGPU = properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
        bool hasGeometryShader = features.geometryShader;
        bool extensionsSupported = deviceExtensionsSupported(physicalDevice); // checks swap chain extension
        bool suitableSwapChain = false;
        if (extensionsSupported) {
            SwapChainDetails swapChain = supportedSwapChainDetails(physicalDevice);
            suitableSwapChain = swapChain.formats.size() > 0 && swapChain.presents.size() > 0;
            //swapChain.presents[0];
        }

        return (
            isDiscreteGPU &&
            hasGeometryShader && 
            queueFamilyIndices.isComplete() && 
            extensionsSupported && 
            suitableSwapChain
        );
    }

    SwapChainDetails supportedSwapChainDetails(VkPhysicalDevice physicalDevice) {
        VkSurfaceCapabilitiesKHR capabilities;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities);

        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data());

        uint32_t presentCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentCount, nullptr);
        std::vector<VkPresentModeKHR> presents(presentCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentCount, presents.data());
        
        return {
            .capabilities = capabilities,
            .formats = std::move(formats),
            .presents = std::move(presents)
        };
    }

    bool deviceExtensionsSupported(VkPhysicalDevice physicalDevice) {
        uint32_t extensionsCount;
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionsCount, nullptr);
        std::vector<VkExtensionProperties> extensionProperties(extensionsCount);
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionsCount, extensionProperties.data());

        for (const auto& extension : deviceExtensions) {
            bool foundExtension = false;
            for (const auto& availableExtensionProperties : extensionProperties) {
                if (strcmp(extension, availableExtensionProperties.extensionName) == 0) {
                    foundExtension = true;
                    break;
                }
            }
            if (!foundExtension) return false;
        }
        return true;
    }

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice physicalDevice) {
        QueueFamilyIndices indices;

        uint32_t queueFamilyCount;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

        for (uint32_t i = 0; i < queueFamilyCount; ++i) {
            if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                indices.graphicsFamily = i;
            }

            VkBool32 surfaceSupported;
            vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &surfaceSupported);
            if (surfaceSupported) {
                indices.surfaceFamily = i;
            }

            if (indices.isComplete()) break;
        }

        return indices;
    }


    void createLogicalDevice() {
        VkPhysicalDeviceFeatures features = {};

        const float queuePriority = 1.0f;
        QueueFamilyIndices familyIndices = findQueueFamilies(physicalDevice);
        std::set<uint32_t> uniqueFamilyIndices = {
            familyIndices.graphicsFamily.value(),
            familyIndices.surfaceFamily.value()
        };
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        for (const auto& queueFamilyIndex : uniqueFamilyIndices) {
            queueCreateInfos.push_back({
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .queueFamilyIndex = queueFamilyIndex,
                .queueCount = 1,
                .pQueuePriorities = &queuePriority
            });
        }

        VkDeviceCreateInfo createInfo = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
            .pQueueCreateInfos = queueCreateInfos.data(),
            .enabledLayerCount = 0,
            .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
            .ppEnabledExtensionNames = deviceExtensions.data(),
            .pEnabledFeatures = &features
        };

        // Device specific validation layers are deprecated
        // Including for backwards compatability
        if (enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
        }

        if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create logical device.");
        }

        vkGetDeviceQueue(device, familyIndices.graphicsFamily.value(), 0, &graphicsQueue);
        vkGetDeviceQueue(device, familyIndices.surfaceFamily.value(), 0, &surfaceQueue);
    }
};

int main() {
    HelloTriangleApplication app;

    try {
        app.run();
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}